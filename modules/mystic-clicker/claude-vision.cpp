/**
 * claude-vision.cpp
 *
 * Claude-vision screen reader for Mystic Clicker (Phase 2 of the GW2/Claude
 * integration). See claude-vision.h for the design rationale.
 *
 * IPC protocol — matched by scripts/gw2-claude-daemon.py. Separate gw2-claude-*
 * namespace so it never collides with the tesseract OCR daemon:
 *
 *   DLL writes  /tmp/gw2-claude-input-<TS>.bmp     captured full-screen frame
 *   DLL writes  /tmp/gw2-claude-input-<TS>.prompt  the question for Claude
 *   DLL touches /tmp/gw2-claude-input-<TS>.ready   "request complete" trigger
 *   daemon writes /tmp/gw2-claude-output-<TS>.txt  Claude's answer
 *   daemon touches /tmp/gw2-claude-done-<TS>       completion marker
 *
 * /tmp is reached via Wine's Z: drive (Z:\ -> /), same as the OCR path. The
 * daemon converts the BMP to PNG (Pillow) before sending it to the API.
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

// Generous — the daemon normally replies in well under 10s. This is the
// "something is wrong" threshold (daemon not running, no network, etc.).
constexpr int CLAUDE_TIMEOUT_MS = 45000;

// Capture the GW2 frame (whole screen, or a box at the cursor when atCursor)
// and write the request files for the daemon.
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
            const int BOXW = 960;   // generous enough for a tooltip / dialog box
            const int BOXH = 620;
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
    Request("Read at Cursor",
        "This image is a region of the Guild Wars 2 screen, captured around "
        "the point the player pointed at. Read the text in it - a tooltip, "
        "dialogue, quest text, mail, or item listing - in full, top to bottom. "
        "If text is clearly cut off at an edge, say so briefly at the end.",
        true);   // crop to a box at the mouse cursor
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
    if (g_state == State::Waiting)
        g_state = State::Idle;   // also cancel a read that hasn't returned yet
}

}  // namespace ClaudeVision
