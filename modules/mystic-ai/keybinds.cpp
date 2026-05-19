/**
 * keybinds.cpp
 *
 * Routes Mystic AI's keybinds:
 *   MYSTIC_AI_CAPTURE   — start (or cancel) the freeze-frame drag-select read
 *   MYSTIC_AI_READ_BOOK — read the saved book region, no drag
 *   MYSTIC_AI_READ      — voice the current panel content (the Read action)
 *   MYSTIC_AI_TP_REGION — overview the saved TP region, no drag
 *
 * Each just posts a command to the render thread (see overlay.cpp). Presses are
 * debounced against Steam Input chord double-fires, the same way Mystic
 * Clicker's handler is — Steam Input + Nexus can deliver a chord's press event
 * twice at the same millisecond.
 */

#include "shared.h"
#include <cstring>
#include <unordered_map>
#include <string>
#include <chrono>

static const long long DEBOUNCE_MS = 300;

void ProcessKeybind(const char* aIdentifier, bool aIsRelease)
{
    if (aIsRelease) return;

    // Per-identifier debounce — drop a second press within DEBOUNCE_MS.
    static std::unordered_map<std::string, long long> lastFireMs;
    long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    std::string key(aIdentifier ? aIdentifier : "");
    auto it = lastFireMs.find(key);
    if (it != lastFireMs.end() && (nowMs - it->second) < DEBOUNCE_MS)
        return;
    lastFireMs[key] = nowMs;

    CheckResolutionChange();

    if (strcmp(aIdentifier, MYSTIC_AI_CAPTURE) == 0)
        ToggleCapture();
    else if (strcmp(aIdentifier, MYSTIC_AI_READ_BOOK) == 0)
        ReadBookRegion();
    else if (strcmp(aIdentifier, MYSTIC_AI_READ) == 0)
        ReadPanelAloud();
    else if (strcmp(aIdentifier, MYSTIC_AI_TP_REGION) == 0)
        ReadTpRegion();
}
