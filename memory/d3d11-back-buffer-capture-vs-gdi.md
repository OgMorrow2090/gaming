---
name: GDI BitBlt is dead under DXVK + gamescope; use D3D11 swap-chain readback
description: From inside a GW2 Nexus addon, BitBlt(GetDC(NULL)) returns all-black on bazzite. Capture the IDXGISwapChain back buffer instead via APIDefs->SwapChain.
type: reference
---

GW2 on bazzite renders via Wine + DXVK → Vulkan → gamescope. The Wine GDI desktop surface that `BitBlt(GetDC(nullptr), ...)` reads from never sees any of that — it stays empty. **Confirmed empirically**: a 700×500 capture of the destroy popup came back as a fully-black BMP.

Likewise broken on the host side:

- `import -window root` on rootless Xwayland :1 captures only the cursor sprite — the game frame goes straight from DXVK to gamescope, bypassing X.
- `ffmpeg -f x11grab :1` same story — cursor only.
- `ffmpeg -f kmsgrab` errors with "No handle set on framebuffer" without `CAP_SYS_ADMIN`.
- `start /unix` from inside Wine fails ("Could not translate the specified Unix filename to a DOS filename") — separate issue, but reinforces that there's no easy escape from the sandbox.

## What works: D3D11 swap-chain readback via Nexus

We're loaded inside the GW2 process; Nexus exposes `APIDefs->SwapChain` (an `IDXGISwapChain*`). Standard D3D11 readback recipe:

1. `swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))` → `ID3D11Texture2D*`
2. `backBuffer->GetDevice(...)` → `ID3D11Device*` → `device->GetImmediateContext(...)`
3. Create staging texture mirroring back-buffer desc with `D3D11_USAGE_STAGING` + `D3D11_CPU_ACCESS_READ`
4. `ctx->CopyResource(staging, backBuffer)`
5. `ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)`
6. Copy pixels out (handle BGRA vs RGBA based on `bbDesc.Format`)

## Threading constraint

`ID3D11DeviceContext` (immediate) is **not thread-safe**. Run the readback on GW2's render thread via Nexus's `RT_PostRender` callback. Macro-thread code submits a request via atomics + waits on a flag; the render-callback fulfils on the next frame. Implementation: `modules/mystic-clicker/screen-capture.{h,cpp}`.

## Why not a Windows tesseract or static lib

Considered + rejected: bundling a Win32 tesseract.exe (~50 MB binary + traineddata) or static-linking libtesseract into the DLL. The host-side daemon pattern (`scripts/gw2-ocr-daemon.sh`, see `mystic-clicker-build-and-deploy.md`) keeps the addon small and uses bazzite's rpm-ostree-installed tesseract.
