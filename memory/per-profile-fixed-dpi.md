---
name: GW2 Wine DPI is per-profile and FIXED, not per-mode dynamic
description: Each GW2 profile (local/Apple TV/Deck) has its own Wine prefix and a single target stream resolution, so Wine LogPixels should be set ONCE per profile to match — not dynamically updated by Sunshine prep-cmd on every mode change.
type: feedback
---

The Apple TV non-Steam profile (appid 2879321470) and Steam Deck non-Steam profile (appid 3111887265) each have their own Wine prefix under `~/.local/share/Steam/steamapps/compatdata/<appid>/pfx`. Each profile is dedicated to one streaming target — Apple TV always streams at 4K, Deck always streams at 1280×800@90. So Wine LogPixels (which GW2 reads at startup to scale UI) should be set ONCE per profile, not dynamically updated by Sunshine prep-cmd on every mode change.

**The fixed mapping:**

| Profile | Appid | Always at | Wine LogPixels |
| --- | --- | --- | --- |
| GW2 (Apple TV) | 2879321470 | 4K (Apple TV native) | **0x90 (144 DPI = 150% UI)** |
| GW2 (Steam Deck) | 3111887265 | 4K source → 720p Deck downscale (3×) | **0x120 (288 DPI = 300% UI)** |
| GW2 Steam (local) | 1284210 | LG native, user-controlled | User-managed; do not auto-touch |

**2026-05-12 update**: changed Deck profile from 0x60 (96 DPI for native
1280×800 streaming) to **0x120 (288 DPI)** because the architecture moved
from "switch gamescope to deck mode (1280×800) before streaming" to "stay
at 4K always, compensate via Wine DPI scaling for the 3× downscale to Deck".

The original 0x60 setting was correct when streaming source = 1280×800 native
(deck gamescope mode). After AMD migration we settled on "single gamescope
mode at 4K + per-profile DPI to compensate" because:

1. gamescope mode switching crashes Sunshine mid-stream
2. virtual HDMI-A-2 (deck mode) requires LG to go dark
3. simpler is better

288 DPI gives a Deck stream UI that looks ~100% perceived size on the Deck's
1280×800 screen after Sunshine downscales the 4K source. Adjust if too big or
too small via hex values 0x100 (256/267%), 0x120 (288/300%), 0x150 (336/350%).

**Why:** Established 2026-05-11 evening after the prep-cmd was removed from "GW2 - SteamOS" Sunshine app (commit 49f221e — was causing Deck stream letterbox). Without prep-cmd auto-updating Wine LogPixels per gamescope-mode, the Apple TV profile was stuck at 96 DPI (left over from a Deck stream session), so UI rendered at 100% on 4K — looked tiny. User correctly observed that since profiles are dedicated to specific clients, dynamic per-mode DPI swap was unnecessary complexity. Setting each profile's LogPixels once to match its dedicated target solves it permanently.

**How to apply:**

- **NEVER** wire any Sunshine prep-cmd or other automation to dynamically update Wine LogPixels across the two stream profiles based on `gamescope-mode`. It's not needed; it caused the letterbox bug; it desyncs profile state.
- If a profile's UI suddenly looks wrong-sized: check Wine LogPixels in `user.reg [Control Panel\\Desktop]`. The Apple TV profile should always be 0x90; the Deck profile should always be 0x120 (post 2026-05-12). Restore if drifted.
- Manual one-shot update if needed (GW2 must be CLOSED — Wine reads LogPixels at process start only):

  ```bash
  # Apple TV profile to 144 DPI (0x90):
  ssh Og@172.16.100.212 'sed -i -E "/^\[Control Panel..Desktop\] /,/^\$/ s/\"LogPixels\"=dword:[0-9a-f]+/\"LogPixels\"=dword:00000090/" /var/home/Og/.local/share/Steam/steamapps/compatdata/2879321470/pfx/user.reg'

  # Deck profile to 288 DPI (0x120) — for 4K source → 720p Deck downscale:
  ssh Og@172.16.100.212 'sed -i -E "/^\[Control Panel..Desktop\] /,/^\$/ s/\"LogPixels\"=dword:[0-9a-f]+/\"LogPixels\"=dword:00000120/" /var/home/Og/.local/share/Steam/steamapps/compatdata/3111887265/pfx/user.reg'
  ```

- The `update_wine_dpi()` function in `scripts/sunshine-set-resolution.sh` is now considered legacy. Don't re-wire it into any prep-cmd. After the 2026-05-12 AMD migration, restructure or delete that script — fixed per-profile DPI makes its DPI-swap logic obsolete.
- GW2 in-game "UI Size" setting (Options → General → User Interface) is **ignored when GFXSettings.xml has `dpiScaling=true`**. GW2 scales exclusively from Wine LogPixels. So in-game DPI hotkeys / chord bindings won't make UI larger or smaller — only restarting GW2 with different LogPixels will.
