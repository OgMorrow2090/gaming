#pragma once

// Per-slot screenshot thumbnails for the capture UI: when a slot's click point
// is captured, also grab a 384x192 region of the back buffer around it and
// save it to disk. The capture window then shows the thumbnail on hover, with
// a small red crosshair at the click point so you can see WHERE inside the
// grab the click lands — handy for remembering which "Accept N" is which.
//
// Wide-rectangle aspect (2:1) matches the GW2 UI elements being captured —
// dialog buttons with adjacent label text — so the surrounding context that
// makes a slot identifiable lands inside the grab.

struct Texture_t;

namespace Thumbs {

constexpr int WIDTH  = 384;  // captured + stored at this size — wide rectangle
constexpr int HEIGHT = 192;  // matches GW2 dialog button layouts (2:1)

// Capture a SIZE x SIZE back-buffer region centered on (clickX, clickY) (game
// window client coords), clamped to the client area, and write it as a 24-bit
// BMP to addons/MysticClicker/thumb-<slot>-<W>x<H>.bmp. Runs on a detached
// worker thread — CaptureBackBufferRegion blocks until the next RT_PostRender,
// so it must not run on the render thread. After the write, bumps the slot's
// version so the next Get() mints a fresh Nexus identifier and reloads.
void Capture(const char* slotId, int clickX, int clickY);

// Return the Nexus texture for this slot's thumbnail, or nullptr if no file
// has been saved yet (or the texture is still loading async — Nexus loads
// off-thread on the first call). Cheap to call every frame.
Texture_t* Get(const char* slotId);

}  // namespace Thumbs
