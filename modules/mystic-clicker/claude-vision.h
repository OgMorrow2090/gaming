#pragma once

#include <string>

// Claude-vision screen reader for Mystic Clicker — Phase 2 of the GW2/Claude
// integration.
//
// Lets the player ask Claude to read and understand the current GW2 screen
// (vendor lists, tooltips, dialog, prices) with no keyboard — preset buttons
// in the capture window. Companion to the host-side gw2-claude-daemon.py.
//
// Flow: a button calls Request(); a detached worker thread captures the frame
// and drops gw2-claude-* files in /tmp; the daemon (outside the Steam sandbox)
// sends the frame to the Claude API and writes the answer back; Poll() —
// called every render frame — picks it up.
//
// Why a worker thread: CaptureBackBufferRegion() blocks until the next
// RT_PostRender. Calling it directly from the render thread (where the UI
// runs) would deadlock that frame, so Request() offloads it. Poll() is cheap
// and render-thread-safe.

namespace ClaudeVision {

enum class State { Idle, Waiting, Done, Error };

// Start a request. `label` is the button name (shown in the UI); `prompt` is
// the question sent to Claude. Non-blocking; ignored if one is already in
// flight.
void Request(const char* label, const char* prompt);

// Convenience: the "Read Screen" preset (transcribe everything). Shared by the
// in-window button and the CLAUDE_READ_SCREEN keybind so the prompt lives in
// one place.
void RequestReadScreen();

// Advance the state machine — check for the daemon's reply. Call once per
// render frame. Cheap when idle.
void Poll();

State       GetState();
std::string GetLabel();    // button name of the in-flight / last request
std::string GetResult();   // answer text when Done, error message when Error
int         GetElapsedMs();// milliseconds since the current request started

}  // namespace ClaudeVision
