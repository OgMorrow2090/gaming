#pragma once

#include <string>
#include <vector>
#include <cstdint>

// IPC client for the host-side gw2-claude-daemon — the Claude-vision screen
// reader at the heart of Mystic AI.
//
// Mystic AI captures a screen region, drops gw2-claude-* request files in /tmp
// (reached via Wine's Z: drive), and the daemon — running outside the Steam
// sandbox — sends the frame to the Claude API and writes the answer back. The
// daemon also speaks the answer aloud (Piper / ElevenLabs TTS).
//
// Two ways to start a request:
//   RequestPixels — the caller already has the pixels (the drag-select crop).
//   RequestRegion — capture a fixed screen rectangle ourselves first (the
//                   static book-region read).
//
// Both are non-blocking. RequestRegion's capture runs on a worker thread
// because CaptureBackBufferRegion() blocks until the next RT_PostRender — doing
// it on the render thread would deadlock that frame. Poll() — called every
// render frame — picks up the daemon's reply.

namespace ClaudeVision {

enum class State { Idle, Waiting, Done, Error };

// Send an already-captured 24-bit BGR region (top-down, row-major) to the
// daemon. `label` names the request in the UI; `prompt` is the question.
// Non-blocking; ignored if a request is already in flight.
void RequestPixels(const char* label, const char* prompt,
                   std::vector<uint8_t> bgrPixels, int w, int h);

// Capture a fixed screen rectangle, then send it. Non-blocking — the capture
// runs on a worker thread.
void RequestRegion(const char* label, const char* prompt,
                   int x, int y, int w, int h);

// Fire-and-forget: ask the daemon to voice `text` aloud right now. Writes a
// "@action:say" request (a .prompt + .ready, no .bmp) on a detached worker
// thread. Does NOT touch the request state machine and is never polled — the
// daemon writes no reply. Used by the Read button to read panel content on
// demand without disturbing what the panel shows.
void Speak(const std::string& text);

// True while the daemon is reading an answer aloud (TTS in progress).
bool IsSpeaking();

// Cancel an in-flight read and stop any speech.
void Stop();

// Advance the state machine — check for the daemon's reply. Call once per
// render frame. Cheap when idle.
void Poll();

State       GetState();
std::string GetLabel();    // label of the in-flight / last request
std::string GetResult();   // answer text when Done, error message when Error
int         GetElapsedMs();// milliseconds since the current request started

}  // namespace ClaudeVision
