/**
 * claude-vision.cpp
 *
 * IPC client for the host-side gw2-claude-daemon. See claude-vision.h for the
 * design rationale.
 *
 * Protocol — matched by scripts/gw2-claude-daemon.py:
 *   DLL writes    /tmp/gw2-claude-input-<TS>.bmp     captured region
 *   DLL writes    /tmp/gw2-claude-input-<TS>.prompt  the question for Claude
 *   DLL touches   /tmp/gw2-claude-input-<TS>.ready   "request complete" trigger
 *   daemon writes /tmp/gw2-claude-output-<TS>.txt    Claude's answer
 *   daemon touches /tmp/gw2-claude-done-<TS>         completion marker
 *   daemon touches /tmp/gw2-claude-speaking          while TTS plays
 *   DLL touches   /tmp/gw2-claude-stop               kill TTS now
 *
 * /tmp is reached via Wine's Z: drive (Z:\ -> /). The daemon converts the BMP
 * to PNG before sending it to the Claude API.
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
// "something is wrong" threshold (daemon down, no network, etc.).
constexpr int CLAUDE_TIMEOUT_MS = 45000;

std::string MakeSuffix()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    char s[32];
    snprintf(s, sizeof(s), "%lld", (long long)now);
    return std::string(s);
}

// Write the three daemon request files for an already-captured 24-bit BGR
// region. The .ready marker is touched LAST so the daemon never picks up a
// half-written frame. Returns false if the BMP write fails.
bool WriteRequest(const std::string& suffix, const std::string& prompt,
                  const std::vector<uint8_t>& px, int w, int h)
{
    // Wine maps Z: -> Linux root, so Z:/tmp/foo == /tmp/foo for the daemon.
    std::string base = std::string("Z:/tmp/gw2-claude-input-") + suffix;
    if (!WriteBmp(base + ".bmp", px, w, h))
        return false;
    {
        std::ofstream pf(base + ".prompt", std::ios::binary);
        pf << prompt;
    }
    {
        std::ofstream rf(base + ".ready");   // touched LAST
    }
    return true;
}

void LogSent(const std::string& label, int w, int h, const std::string& suffix)
{
    if (!APIDefs) return;
    char msg[256];
    snprintf(msg, sizeof(msg), "Claude request sent: %s (%dx%d, suffix %s)",
             label.c_str(), w, h, suffix.c_str());
    APIDefs->Log(LOGL_INFO, "MysticAI", msg);
}

// Worker for RequestPixels — pixels already captured, just write the files.
void PixelWorker(std::string label, std::string prompt,
                 std::vector<uint8_t> px, int w, int h)
{
    std::string suffix = MakeSuffix();
    bool ok = (w > 0 && h > 0) && WriteRequest(suffix, prompt, px, w, h);
    std::lock_guard<std::mutex> lk(g_mtx);
    if (ok)
    {
        g_suffix     = suffix;
        g_filesReady = true;
        LogSent(label, w, h, suffix);
    }
    else
    {
        g_state  = ClaudeVision::State::Error;
        g_result = "failed to write the capture file";
    }
}

// Worker for RequestRegion — capture the rectangle ourselves, then write the
// files. CaptureBackBufferRegion blocks until the next RT_PostRender, so this
// must not run on the render thread.
void RegionWorker(std::string label, std::string prompt,
                  int x, int y, int w, int h)
{
    std::vector<uint8_t> px;
    if (w <= 0 || h <= 0 || !CaptureBackBufferRegion(x, y, w, h, px))
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_state  = ClaudeVision::State::Error;
        g_result = "screen capture failed";
        return;
    }
    std::string suffix = MakeSuffix();
    bool ok = WriteRequest(suffix, prompt, px, w, h);
    std::lock_guard<std::mutex> lk(g_mtx);
    if (ok)
    {
        g_suffix     = suffix;
        g_filesReady = true;
        LogSent(label, w, h, suffix);
    }
    else
    {
        g_state  = ClaudeVision::State::Error;
        g_result = "failed to write the capture file";
    }
}

// Worker for Speak — fire-and-forget. Writes a "@action:say" request: just a
// .prompt + .ready marker, no .bmp. The daemon voices the text and writes no
// reply, so this never touches g_state / g_suffix / g_filesReady and is never
// polled.
void SayWorker(std::string text)
{
    std::string base = std::string("Z:/tmp/gw2-claude-input-") + MakeSuffix();
    {
        std::ofstream pf(base + ".prompt", std::ios::binary);
        pf << "@action:say\n" << text;
    }
    {
        std::ofstream rf(base + ".ready");   // touched LAST
    }
}

// Common request setup. Returns false (so the caller drops the request) if a
// read is already in flight.
bool BeginRequest(const char* label)
{
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_state == ClaudeVision::State::Waiting)
        return false;
    g_state      = ClaudeVision::State::Waiting;
    g_label      = label ? label : "";
    g_result.clear();
    g_suffix.clear();
    g_filesReady = false;
    g_start      = std::chrono::steady_clock::now();
    return true;
}

}  // namespace

namespace ClaudeVision {

void RequestPixels(const char* label, const char* prompt,
                   std::vector<uint8_t> bgrPixels, int w, int h)
{
    if (!BeginRequest(label)) return;
    std::thread(PixelWorker, std::string(label ? label : ""),
                std::string(prompt ? prompt : ""),
                std::move(bgrPixels), w, h).detach();
}

void RequestRegion(const char* label, const char* prompt,
                   int x, int y, int w, int h)
{
    if (!BeginRequest(label)) return;
    std::thread(RegionWorker, std::string(label ? label : ""),
                std::string(prompt ? prompt : ""),
                x, y, w, h).detach();
}

void Speak(const std::string& text)
{
    if (text.empty()) return;   // nothing to voice
    std::thread(SayWorker, text).detach();
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
        if (!g_filesReady)             return;   // worker still capturing / writing
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

State GetState()  { std::lock_guard<std::mutex> lk(g_mtx); return g_state; }

std::string GetLabel()  { std::lock_guard<std::mutex> lk(g_mtx); return g_label; }

std::string GetResult() { std::lock_guard<std::mutex> lk(g_mtx); return g_result; }

int GetElapsedMs()
{
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_state == State::Idle) return 0;
    return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_start).count();
}

bool IsSpeaking() { std::lock_guard<std::mutex> lk(g_mtx); return g_speaking; }

void Stop()
{
    // Touch the stop marker — the daemon kills any TTS playback on its next
    // poll. Wine maps Z: -> Linux root, so this is /tmp/gw2-claude-stop.
    { std::ofstream sf("Z:/tmp/gw2-claude-stop"); }

    std::lock_guard<std::mutex> lk(g_mtx);
    g_speaking = false;
    if (g_state == State::Waiting)
        g_state = State::Idle;   // cancel an in-flight read that hasn't returned
}

}  // namespace ClaudeVision
