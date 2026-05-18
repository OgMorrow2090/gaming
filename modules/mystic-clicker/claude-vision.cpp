/**
 * claude-vision.cpp
 *
 * Claude-vision screen reader for Mystic Clicker. See claude-vision.h for the
 * design rationale.
 *
 * IPC protocol — matched by scripts/gw2-claude-daemon.py. Separate gw2-claude-*
 * namespace so it never collides with the tesseract OCR daemon:
 *
 *   DLL writes  /tmp/gw2-claude-input-<TS>.bmp     captured frame / region
 *   DLL writes  /tmp/gw2-claude-input-<TS>.prompt  the question for Claude
 *   DLL touches /tmp/gw2-claude-input-<TS>.ready   "request complete" trigger
 *   daemon writes /tmp/gw2-claude-output-<TS>.txt  Claude's answer
 *   daemon touches /tmp/gw2-claude-done-<TS>       completion marker
 *
 * /tmp is reached via Wine's Z: drive (Z:\ -> /), same as the OCR path. The
 * daemon converts the BMP to PNG (Pillow) before sending it to the API.
 *
 * RequestReadScreen() is a *continuous follow read*: it reads the cursor
 * region, then keeps re-reading it each time the on-screen text changes (a
 * book page turn, a scroll) until Stop(). The in-panel preset buttons use the
 * one-shot Request()/Worker() path instead.
 */

#include "shared.h"
#include "claude-vision.h"
#include "screen-capture.h"
#include "ocr.h"            // WriteBmp
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstdint>
#include <cstdio>

namespace {

std::mutex                            g_mtx;
ClaudeVision::State                   g_state = ClaudeVision::State::Idle;
std::string                           g_label;
std::string                           g_result;
std::string                           g_suffix;       // set by the worker once files are written
bool                                  g_filesReady = false;
bool                                  g_speaking   = false;  // daemon TTS in progress
std::chrono::steady_clock::time_point  g_start;

// Continuous "follow" read state. While g_continuousActive is true the
// ContinuousWorker thread owns the whole read lifecycle and Poll() defers to
// it. Stop() sets g_stopContinuous; the worker winds down and clears both.
bool                                  g_continuousActive = false;
bool                                  g_stopContinuous   = false;

// Generous — the daemon normally replies in well under 10s. This is the
// "something is wrong" threshold (daemon not running, no network, etc.).
constexpr int CLAUDE_TIMEOUT_MS = 45000;

// Capture the GW2 frame (whole screen, or a box at the cursor when atCursor)
// and write the request files for the daemon. One-shot path — used by the
// in-panel preset buttons via Request().
//
// Runs on a detached worker thread: CaptureBackBufferRegion() blocks until the
// next RT_PostRender hook fulfils it, so calling it on the render thread (where
// the UI runs) would deadlock that frame.
void Worker(std::string prompt, bool atCursor)
{
    // Unique suffix so rapid/concurrent requests never collide.
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    char suffix[32];
    snprintf(suffix, sizeof(suffix), "%lld", (long long)now);

    // The game window's client rect is the render resolution.
    int w = 2560, h = 1440;   // fallback if the window handle is unavailable
    if (GameWindow != nullptr)
    {
        RECT r;
        if (GetClientRect(GameWindow, &r))
        {
            w = r.right - r.left;
            h = r.bottom - r.top;
        }
    }

    // Capture region. Default: the whole frame. Cursor mode: a box anchored a
    // little up-left of the mouse, so the player can point at the start of the
    // text they want and get just that block read — not the whole screen.
    int capX = 0, capY = 0, capW = w, capH = h;
    if (atCursor)
    {
        POINT pt;
        if (GetCursorPos(&pt))
        {
            if (GameWindow != nullptr)
                ScreenToClient(GameWindow, &pt);

            const int PAD  = 24;    // margin up-left so the first glyph isn't clipped
            const int BOXW = 1500;  // big enough for a full book / dialog panel
            const int BOXH = 1000;
            capX = pt.x - PAD;  if (capX < 0) capX = 0;
            capY = pt.y - PAD;  if (capY < 0) capY = 0;
            capW = BOXW;  if (capX + capW > w) capW = w - capX;
            capH = BOXH;  if (capY + capH > h) capH = h - capY;
        }
    }

    std::vector<uint8_t> pixels;
    if (capW <= 0 || capH <= 0 ||
        !CaptureBackBufferRegion(capX, capY, capW, capH, pixels))
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_state  = ClaudeVision::State::Error;
        g_result = "screen capture failed";
        return;
    }

    // Wine maps Z: -> Linux root, so Z:/tmp/foo == /tmp/foo for the daemon.
    std::string inBase     = std::string("Z:/tmp/gw2-claude-input-") + suffix;
    std::string bmpPath    = inBase + ".bmp";
    std::string promptPath = inBase + ".prompt";
    std::string readyPath  = inBase + ".ready";

    if (!WriteBmp(bmpPath, pixels, capW, capH))
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_state  = ClaudeVision::State::Error;
        g_result = "failed to write capture BMP";
        return;
    }
    {
        std::ofstream pf(promptPath, std::ios::binary);
        pf << prompt;
    }
    // Touch the .ready marker LAST — the daemon treats it as "request complete"
    // so it can never pick up a half-written BMP or prompt.
    {
        std::ofstream rf(readyPath);
    }

    if (APIDefs)
    {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Claude vision request sent (%dx%d at %d,%d, suffix %s)",
                 capW, capH, capX, capY, suffix);
        APIDefs->Log(LOGL_INFO, "MysticClicker", msg);
    }

    std::lock_guard<std::mutex> lk(g_mtx);
    g_suffix     = suffix;
    g_filesReady = true;
}

// ===================================================================
// Continuous "follow" read — watch the cursor region and re-read it as
// the text changes (book page turn, scroll) until Stop().
// ===================================================================

// Thumbnail grid for cheap change detection. Small enough that a page turn or
// scroll moves it a lot while a cursor blink / particle barely registers.
constexpr int THUMB_W = 64;
constexpr int THUMB_H = 40;

// Mean per-cell brightness diff (0-255): above CHANGE = "the text changed";
// below SETTLE between two captures = "the page has stopped moving".
// First-cut values — easy to tune after testing on a real in-game book.
constexpr int CHANGE_THRESHOLD = 12;
constexpr int SETTLE_THRESHOLD = 5;

// Downscale a captured BGRA/BGR region to a THUMB_W x THUMB_H grey grid.
void MakeThumb(const std::vector<uint8_t>& px, int w, int h,
               std::vector<uint8_t>& out)
{
    out.assign((size_t)THUMB_W * THUMB_H, 0);
    if (w <= 0 || h <= 0) return;
    size_t bpp = px.size() / ((size_t)w * (size_t)h);
    if (bpp < 3) return;                       // unexpected layout — bail safely
    for (int ty = 0; ty < THUMB_H; ++ty)
    for (int tx = 0; tx < THUMB_W; ++tx)
    {
        int x0 = (int)((long long)tx * w / THUMB_W);
        int x1 = (int)((long long)(tx + 1) * w / THUMB_W);
        int y0 = (int)((long long)ty * h / THUMB_H);
        int y1 = (int)((long long)(ty + 1) * h / THUMB_H);
        if (x1 <= x0) x1 = x0 + 1;
        if (y1 <= y0) y1 = y0 + 1;
        int sx = (x1 - x0) / 8 + 1;             // sample-step to bound the cost
        int sy = (y1 - y0) / 8 + 1;
        unsigned long long sum = 0;
        int cnt = 0;
        for (int y = y0; y < y1; y += sy)
        for (int x = x0; x < x1; x += sx)
        {
            size_t idx = ((size_t)y * (size_t)w + (size_t)x) * bpp;
            if (idx + 2 >= px.size()) continue;            // defensive bound
            sum += (unsigned)px[idx] + px[idx + 1] + px[idx + 2];
            ++cnt;
        }
        out[(size_t)ty * THUMB_W + tx] = cnt ? (uint8_t)(sum / (3 * cnt)) : 0;
    }
}

// Mean absolute per-cell difference of two thumbnails (0-255); 999 if mismatched.
int ThumbDiff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
{
    if (a.empty() || a.size() != b.size()) return 999;
    long long sum = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        int d = (int)a[i] - (int)b[i];
        sum += (d < 0) ? -d : d;
    }
    return (int)(sum / (long long)a.size());
}

bool ContinuousStopped()
{
    std::lock_guard<std::mutex> lk(g_mtx);
    return g_stopContinuous;
}

bool DaemonSpeaking()
{
    return GetFileAttributesA("Z:/tmp/gw2-claude-speaking")
           != INVALID_FILE_ATTRIBUTES;
}

// Sleep up to `ms`, in small chunks, bailing early on Stop(). Returns false if
// a stop was requested (so callers can break out promptly).
bool Nap(int ms)
{
    int slept = 0;
    while (slept < ms)
    {
        if (ContinuousStopped()) return false;
        int chunk = (ms - slept < 100) ? (ms - slept) : 100;
        std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
        slept += chunk;
    }
    return !ContinuousStopped();
}

// Capture the fixed region and downscale to a thumbnail. No request files.
bool CaptureThumb(int x, int y, int w, int h, std::vector<uint8_t>& thumb)
{
    std::vector<uint8_t> px;
    if (!CaptureBackBufferRegion(x, y, w, h, px)) return false;
    MakeThumb(px, w, h, thumb);
    return !thumb.empty();
}

// Capture the region, write the daemon request files, return the new suffix
// and the thumbnail of what was captured.
bool SendContinuousRead(int x, int y, int w, int h, const std::string& prompt,
                        std::string& outSuffix, std::vector<uint8_t>& outThumb)
{
    std::vector<uint8_t> px;
    if (!CaptureBackBufferRegion(x, y, w, h, px)) return false;

    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    char suffix[32];
    snprintf(suffix, sizeof(suffix), "%lld", (long long)now);
    std::string inBase = std::string("Z:/tmp/gw2-claude-input-") + suffix;

    if (!WriteBmp(inBase + ".bmp", px, w, h)) return false;
    { std::ofstream pf(inBase + ".prompt", std::ios::binary); pf << prompt; }
    { std::ofstream rf(inBase + ".ready"); }            // touch LAST

    MakeThumb(px, w, h, outThumb);
    outSuffix = suffix;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_suffix = suffix;
        g_start  = std::chrono::steady_clock::now();
        g_state  = ClaudeVision::State::Waiting;
    }
    return true;
}

// Wait for the daemon to answer suffix S and finish speaking it; clean up the
// per-request /tmp files. Returns false if stopped or timed out.
bool WaitForContinuousRead(const std::string& suffix)
{
    std::string inBase = std::string("Z:/tmp/gw2-claude-input-")  + suffix;
    std::string outTxt = std::string("Z:/tmp/gw2-claude-output-") + suffix + ".txt";
    std::string doneMk = std::string("Z:/tmp/gw2-claude-done-")   + suffix;

    auto t0 = std::chrono::steady_clock::now();
    while (GetFileAttributesA(doneMk.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        if (ContinuousStopped()) return false;
        auto el = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        if (el > CLAUDE_TIMEOUT_MS) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    // Consumed — drop the per-request files so /tmp doesn't fill over a long read.
    DeleteFileA((inBase + ".bmp").c_str());
    DeleteFileA((inBase + ".prompt").c_str());
    DeleteFileA(outTxt.c_str());
    DeleteFileA(doneMk.c_str());

    // Let speech start (the daemon touches the marker just after 'done'), then
    // wait for it to finish.
    auto t1 = std::chrono::steady_clock::now();
    while (!DaemonSpeaking())
    {
        if (ContinuousStopped()) return false;
        auto el = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t1).count();
        if (el > 3000) break;                  // TTS off, or nothing to say
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    while (DaemonSpeaking())
    {
        if (ContinuousStopped()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return true;
}

// The continuous follow reader. Reads the cursor region, then watches it and
// re-reads each time the text settles into something new — until Stop().
void ContinuousWorker(std::string prompt)
{
    try
    {
        // Fix the watched region once, from the cursor at trigger time.
        int w = 2560, h = 1440;
        if (GameWindow != nullptr)
        {
            RECT r;
            if (GetClientRect(GameWindow, &r))
            {
                w = r.right - r.left;
                h = r.bottom - r.top;
            }
        }
        int capX = 0, capY = 0, capW = w, capH = h;
        POINT pt;
        if (GetCursorPos(&pt))
        {
            if (GameWindow != nullptr) ScreenToClient(GameWindow, &pt);
            const int PAD  = 24;
            const int BOXW = 1500;
            const int BOXH = 1000;
            capX = pt.x - PAD;  if (capX < 0) capX = 0;
            capY = pt.y - PAD;  if (capY < 0) capY = 0;
            capW = BOXW;  if (capX + capW > w) capW = w - capX;
            capH = BOXH;  if (capY + capH > h) capH = h - capY;
        }

        if (capW > 0 && capH > 0)
        {
            std::vector<uint8_t> lastThumb;
            std::string suffix;

            // First read.
            if (SendContinuousRead(capX, capY, capW, capH, prompt, suffix, lastThumb))
            {
                while (!ContinuousStopped())
                {
                    if (!WaitForContinuousRead(suffix)) break;   // stopped / timeout

                    // Watch the region; re-read once it settles into new text.
                    bool fired = false;
                    while (!ContinuousStopped() && !fired)
                    {
                        if (!Nap(700)) break;

                        std::vector<uint8_t> cur;
                        if (!CaptureThumb(capX, capY, capW, capH, cur)) continue;
                        if (lastThumb.empty()) { lastThumb = cur; continue; }
                        if (ThumbDiff(cur, lastThumb) <= CHANGE_THRESHOLD) continue;

                        // Looks changed — confirm it has settled (not mid-scroll).
                        if (!Nap(450)) break;
                        std::vector<uint8_t> cur2;
                        if (!CaptureThumb(capX, capY, capW, capH, cur2)) continue;
                        if (ThumbDiff(cur, cur2) >= SETTLE_THRESHOLD) continue;  // still moving

                        // Settled new content — read it.
                        std::string ns;
                        std::vector<uint8_t> nt;
                        if (SendContinuousRead(capX, capY, capW, capH, prompt, ns, nt))
                        {
                            suffix    = ns;
                            lastThumb = nt;
                            fired     = true;
                        }
                    }
                }
            }
        }
    }
    catch (...)
    {
        // A detached thread must never let an exception escape (std::terminate).
    }

    std::lock_guard<std::mutex> lk(g_mtx);
    g_continuousActive = false;
    g_stopContinuous   = false;
    if (g_state == ClaudeVision::State::Waiting)
        g_state = ClaudeVision::State::Idle;
}

}  // namespace

namespace ClaudeVision {

void Request(const char* label, const char* prompt, bool atCursorCrop)
{
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (g_state == State::Waiting) return;   // one request at a time
        g_state      = State::Waiting;
        g_label      = label ? label : "";
        g_result.clear();
        g_suffix.clear();
        g_filesReady = false;
        g_start      = std::chrono::steady_clock::now();
    }
    std::thread(Worker, std::string(prompt ? prompt : ""), atCursorCrop).detach();
}

void RequestReadScreen()
{
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (g_state == State::Waiting) return;   // a read / follow is already running
        g_state            = State::Waiting;
        g_label            = "Read at Cursor";
        g_result.clear();
        g_suffix.clear();
        g_filesReady       = false;
        g_start            = std::chrono::steady_clock::now();
        g_continuousActive = true;
        g_stopContinuous   = false;
    }
    // Continuous follow read: reads the cursor region, then keeps reading it
    // each time the text changes (book page turn / scroll) until Stop().
    std::thread(ContinuousWorker, std::string(
        "This image is a region of the Guild Wars 2 screen, captured around "
        "the point the player pointed at. Read the text in it - a tooltip, "
        "dialogue, quest text, book page, mail, or item listing - in full, top "
        "to bottom. If text is clearly cut off at an edge, say so briefly.")).detach();
}

void Poll()
{
    // The daemon touches this while TTS is talking. Check it every frame
    // regardless of request state — speech outlives the Done transition.
    bool speaking = (GetFileAttributesA("Z:/tmp/gw2-claude-speaking")
                     != INVALID_FILE_ATTRIBUTES);

    std::string                           suffix;
    std::chrono::steady_clock::time_point  start;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_speaking = speaking;
        if (g_continuousActive) return;   // ContinuousWorker owns the lifecycle
        if (g_state != State::Waiting) return;
        if (!g_filesReady)             return;   // worker still capturing
        suffix = g_suffix;
        start  = g_start;
    }

    std::string inBase = std::string("Z:/tmp/gw2-claude-input-")  + suffix;
    std::string outTxt = std::string("Z:/tmp/gw2-claude-output-") + suffix + ".txt";
    std::string doneMk = std::string("Z:/tmp/gw2-claude-done-")   + suffix;

    if (GetFileAttributesA(doneMk.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        std::string text;
        std::ifstream f(outTxt, std::ios::binary);
        if (f.is_open())
        {
            std::stringstream ss;
            ss << f.rdbuf();
            text = ss.str();
        }
        else
        {
            text = "ERROR: daemon reply file unreadable";
        }
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            g_result = text;
            g_state  = State::Done;
        }
        // Clean up artifacts (the daemon already removed the .ready marker).
        DeleteFileA((inBase + ".bmp").c_str());
        DeleteFileA((inBase + ".prompt").c_str());
        DeleteFileA(outTxt.c_str());
        DeleteFileA(doneMk.c_str());
        return;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    if (elapsed > CLAUDE_TIMEOUT_MS)
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_state  = State::Error;
        g_result = "timed out - is gw2-claude-daemon.service running on bazzite?";
    }
}

State GetState()
{
    std::lock_guard<std::mutex> lk(g_mtx);
    return g_state;
}

std::string GetLabel()
{
    std::lock_guard<std::mutex> lk(g_mtx);
    return g_label;
}

std::string GetResult()
{
    std::lock_guard<std::mutex> lk(g_mtx);
    return g_result;
}

int GetElapsedMs()
{
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_state == State::Idle) return 0;
    return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_start).count();
}

bool IsSpeaking()
{
    std::lock_guard<std::mutex> lk(g_mtx);
    return g_speaking;
}

void Stop()
{
    // Touch the stop marker — the daemon kills any TTS playback on its next
    // poll. Wine maps Z: -> Linux root, so this is /tmp/gw2-claude-stop.
    { std::ofstream sf("Z:/tmp/gw2-claude-stop"); }

    std::lock_guard<std::mutex> lk(g_mtx);
    g_speaking = false;
    if (g_continuousActive)
    {
        // Signal the follow reader to wind down — it clears the state itself.
        g_stopContinuous = true;
    }
    else if (g_state == State::Waiting)
    {
        g_state = State::Idle;   // cancel a one-shot read that hasn't returned
    }
}

}  // namespace ClaudeVision
