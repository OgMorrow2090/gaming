#pragma once

#include <string>
#include <vector>
#include <cstdint>

// "Copy Text" action — OCR an already-captured region (the drag-select crop)
// with the host-side tesseract daemon and put the recognised text on the
// Windows clipboard, so the player can paste it (e.g. into GW2's item-destroy
// confirmation box). No Claude API call — this is the cheap, local path.

namespace CopyText {

enum class State { Idle, Working, Done, Error };

// OCR `bgrPixels` (24-bit BGR, top-down, w x h) and copy the recognised text
// to the clipboard. Non-blocking — the OCR round-trip runs on a worker thread.
void Request(std::vector<uint8_t> bgrPixels, int w, int h);

// Forget the last result. Called when a new capture starts.
void Reset();

State       GetState();
std::string GetResult();   // copied text when Done, error message when Error

}  // namespace CopyText
