---
name: GW2 dpiScaling=true is what makes per-mode UI sizing land right after gamescope's 2K-downscale
description: With gamescope's nested xwayland hardlocked at 2K, GW2 always renders at 2K canvas. dpiScaling=true + per-mode Wine LogPixels (96/144/192) is the compensation that makes UI proportions look correct on each stream client. Forcing dpiScaling=false breaks this.
type: project
---

<!-- markdownlint-disable MD041 -->

Companion memory to `gamescope-nested-xwayland-constraint.md`. Given that gamescope's nested xwayland is hardlocked at 2K, the only way to make GW2's UI look correctly-sized on the Deck stream (vs Apple TV stream) is to **let GW2 scale UI based on Wine's reported DPI**, and swap Wine's DPI per gamescope mode.

## How the chain works

1. `sunshine-set-resolution.sh` runs as Sunshine prep-cmd (on SteamOS Game Mode and GW2 - SteamOS apps)
2. Detects client width via `SUNSHINE_CLIENT_WIDTH` and picks a gamescope mode:
   - 1280 → deck → DPI 96
   - 1920 → 1080p144 → DPI 96
   - 2560 / 3840 → 2k120 → DPI 144
3. Writes `LogPixels=dword:00000060/90/c0` to `~/.local/share/Steam/steamapps/compatdata/1284210/pfx/user.reg`
4. Wine reads user.reg fresh on each Gw2-64.exe launch
5. With `dpiScaling=true` in GFXSettings, GW2 asks Wine for DPI and scales its UI elements accordingly:
   - 96 DPI = 100% UI (smaller elements, fits Deck's 1280x800 panel post-downscale)
   - 144 DPI = 150% UI (bigger elements for Apple TV at 2K)
   - 192 DPI = 200% UI (was for 4K mode, currently unused since Apple TV maps to 2k120)
6. GW2 renders at xwayland's 2K size (cannot be changed; see constraint memo)
7. gamescope downscales 2K render to `-W -H` canvas (1280x800 for deck client)
8. Sunshine streams canvas to client at native client res (1280x800 for Deck)

The DPI scaling counteracts the 2K-canvas-with-100%-UI-then-downscale-to-deck = tiny UI problem.

## What goes wrong if you change `dpiScaling`

`dpiScaling=false` makes GW2 ignore Wine DPI and use its in-game "User Interface Size" setting only. Without per-Wine-DPI scaling, GW2's UI renders at fixed proportions on the 2K canvas. After downscale to 1280x800 for the Deck stream, UI elements are roughly half their intended size — looks "stuck at 2K" to the user. Symptom matches what the Deck-stream rendering looks like.

## What goes wrong if you change `screenMode`

`screenMode=fullscreen` (exclusive) makes GW2 call Wine's SetDisplayMode itself, attempting to resize xwayland to RESOLUTION value. **This actually fails** in the gamescope nested context (see constraint memo) and produces inconsistent behavior between launches. `windowed_fullscreen` is the correct value.

## How to apply

The script `scripts/sunshine-set-resolution.sh` already has the correct logic:
- Updates Wine LogPixels per mode (`update_wine_dpi`)
- Updates GW2 GFXSettings RESOLUTION (cosmetic — RESOLUTION value isn't actually honored in windowed_fullscreen but harmless)
- Forces `verticalSync=false` (DXVK FIFO uncap)
- **Does NOT touch `dpiScaling` or `screenMode`** — leave them at GW2 defaults (true / windowed_fullscreen)

If a future session adds `screenMode` or `dpiScaling` rewrites to the script: **revert immediately**. We have already burned hours on this.
