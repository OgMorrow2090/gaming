#pragma once

#include <vector>
#include <cstdint>

// In-process D3D11 frame capture from GW2's swap chain back buffer.
//
// Why this exists:
//   GW2 renders via DXVK → Vulkan → gamescope on bazzite. Wine's GDI desktop
//   surface (what BitBlt(GetDC(nullptr)) reads from) doesn't see any of that
//   — captures come back all-black. The only reliable way to get the actual
//   rendered frame from inside the addon is to read the D3D11 back buffer
//   directly, which Nexus exposes via APIDefs->SwapChain.
//
// Threading:
//   ID3D11DeviceContext (immediate) is NOT thread-safe. The actual readback
//   has to run on GW2's render thread, which we do via a Nexus RT_Render
//   callback. CaptureBackBuffer() (called from any thread) just sets a
//   request flag and waits for the next frame's hook to fulfil it.

// Register the render-callback hook with Nexus. Call once at addon load.
void RegisterScreenCaptureHook();

// Deregister the hook. Call at unload.
void DeregisterScreenCaptureHook();

// Synchronous capture: wait for the next frame, copy a region of the back
// buffer into outPixels (24-bit BGR, top-down, row-major). Times out at
// timeoutMs (typical: 100-300ms — covers >5 frames at 30fps).
//
// (x, y) and (w, h) are in screen pixels. Out-of-bounds areas are filled
// with black.
//
// Returns true on success.
bool CaptureBackBufferRegion(int x, int y, int w, int h,
                             std::vector<uint8_t>& outPixels,
                             int timeoutMs = 300);
