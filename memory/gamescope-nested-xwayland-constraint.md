---
name: gamescope nested xwayland is hardlocked at EDID max — cannot be resized per-mode
description: gamescope's `-W/-H/-w/-h` and `--force-windows-fullscreen` cannot prevent Steam/Wine from xrandr-bumping the nested xwayland to the EDID-derived max size. xrandr from outside returns BadMatch. This is the architecture wall behind any "stream content renders at 2K regardless of gamescope mode" symptom.
type: project
---

<!-- markdownlint-disable MD041 -->

When running gamescope-session-plus on bazzite with the EVanlak HDMI dongle EDID and the NVIDIA proprietary driver, **gamescope's nested xwayland is hardlocked at the EDID-derived max size (2560x1440 in our case) regardless of any flags.** This is the constraint behind the "GW2 always renders at 2K even in deck mode" symptom and the per-mode resolution switching pain.

## What I tried (all failed)

| Flag / approach | Effect | Result |
|---|---|---|
| `-W 1280 -H 800` (output size) | Sets the gamescope DRM/composition canvas size | Works for canvas, but xwayland separate |
| `-w 1280 -h 800` (nested init size) | Should set xwayland init size | Set initially; Steam/Wine bump it back to 2K once they start |
| `--force-windows-fullscreen` | Forces window draw size to nested display | Steam still draws at 2K, conflicts with clamp every redraw → visible flicker |
| `--generate-drm-mode fixed` | Should pick from EDID-listed modes only | gamescope still picks 2560x1440@144 (CVT-derived) over EDID DTDs |
| `xrandr --output gamescope --mode 1280x800` (post-init) | Force xwayland resize | Returns `X Error: BadMatch on RRSetScreenSize` |
| `xrandr --newmode + --addmode` then `--output --mode` | Add mode then switch | Same BadMatch on screen size change |

## Why Steam ignores gamescope flags

Steam logs literally show:
```
steam[xxxxx]: Desktop state changed: desktop: { pos: 0,0 size: 2560,1440 }
```
right after Big Picture launches, regardless of what `-w/-h` were set to. Steam queries xwayland's exposed monitor (which derives from EDID, not from gamescope's nested-size flags) and resizes its window/desktop assumption to that. `--force-windows-fullscreen` then fights this every redraw — visible flicker.

## What this means for stream rendering

- gamescope canvas size = `-W -H` (e.g. 1280x800 deck mode)
- xwayland reported size = ~always 2560x1440 (or whatever EDID's max is)
- GW2 in `windowed_fullscreen` renders at xwayland's reported size = 2K
- gamescope downscales the 2K render to its `-W -H` canvas
- Sunshine captures the canvas (1280x800 or whatever) and streams to the client
- Net: GW2 is rendered at 2K, downscaled twice. **The stream is at the right pixel size, but the content was rendered for 2K** — so UI elements that are sized for 2K end up looking proportionally smaller after downscale to a 1280x800 panel.

## The compensation that actually works

`dpiScaling=true` in GW2's GFXSettings.xml + per-mode Wine LogPixels (96 / 144 / 192) is what makes the UI proportions land correctly on each client despite the 2K-canvas constraint. See `gw2-windowed-fullscreen-dpiscaling-trick.md`.

## When this matters

Any time a future agent considers:
- Adding gamescope flags to "lock" xwayland size
- Trying to xrandr-resize gamescope's nested displays
- Changing GW2's `screenMode` from `windowed_fullscreen` to something else "to fix per-mode adapt"

— stop and re-read this memory. None of those paths actually work on this hardware/driver combo.

## How to apply

If the symptom is "GW2 renders at wrong size on a particular client":
- DO NOT add `--force-windows-fullscreen`, `-w/-h`, `--generate-drm-mode fixed`, or post-init xrandr resize attempts
- DO check that GFXSettings.xml has `dpiScaling=true` and Wine LogPixels matches the active mode (96 / 144 / 192)
- DO check that `verticalSync=false` so DXVK doesn't FIFO-cap the framerate at the EDID's reported max refresh
- The per-mode DPI swap is in `scripts/sunshine-set-resolution.sh` and is the source of truth for what UI scale GW2 should use per stream client
