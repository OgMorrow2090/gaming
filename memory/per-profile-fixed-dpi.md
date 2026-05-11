---
name: GW2 Wine DPI is per-profile and FIXED, not per-mode dynamic
description: Each GW2 profile (local/Apple TV/Deck) has its own Wine prefix and a single target stream resolution, so Wine LogPixels should be set ONCE per profile to match — not dynamically updated by Sunshine prep-cmd on every mode change.
type: feedback
---

The Apple TV non-Steam profile (appid 2879321470) and Steam Deck non-Steam profile (appid 3111887265) each have their own Wine prefix under `~/.local/share/Steam/steamapps/compatdata/<appid>/pfx`. Each profile is dedicated to one streaming target — Apple TV always streams at 4K, Deck always streams at 1280×800@90. So Wine LogPixels (which GW2 reads at startup to scale UI) should be set ONCE per profile, not dynamically updated by Sunshine prep-cmd on every mode change.

**The fixed mapping:**

| Profile | Appid | Always at | Wine LogPixels |
| --- | --- | --- | --- |
| GW2 (Apple TV) | 2879321470 | 4K (165 or 120) | **0x90 (144 DPI = 150% UI)** |
| GW2 (Steam Deck) | 3111887265 | 1280×800@90 | **0x60 (96 DPI = 100% UI)** |
| GW2 Steam (local) | 1284210 | LG native, user-controlled | User-managed; do not auto-touch |

**Why:** Established 2026-05-11 evening after the prep-cmd was removed from "GW2 - SteamOS" Sunshine app (commit 49f221e — was causing Deck stream letterbox). Without prep-cmd auto-updating Wine LogPixels per gamescope-mode, the Apple TV profile was stuck at 96 DPI (left over from a Deck stream session), so UI rendered at 100% on 4K — looked tiny. User correctly observed that since profiles are dedicated to specific clients, dynamic per-mode DPI swap was unnecessary complexity. Setting each profile's LogPixels once to match its dedicated target solves it permanently.

**How to apply:**

- **NEVER** wire any Sunshine prep-cmd or other automation to dynamically update Wine LogPixels across the two stream profiles based on `gamescope-mode`. It's not needed; it caused the letterbox bug; it desyncs profile state.
- If a profile's UI suddenly looks wrong-sized: check Wine LogPixels in `user.reg [Control Panel\\Desktop]`. The Apple TV profile should always be 0x90; the Deck profile should always be 0x60. Restore if drifted.
- Manual one-shot update if needed (GW2 must be CLOSED — Wine reads LogPixels at process start only):
  ```bash
  # Apple TV profile to 144 DPI (0x90):
  ssh Og@172.16.100.212 'sed -i -E "/^\[Control Panel..Desktop\] /,/^\$/ s/\"LogPixels\"=dword:[0-9a-f]+/\"LogPixels\"=dword:00000090/" /var/home/Og/.local/share/Steam/steamapps/compatdata/2879321470/pfx/user.reg'

  # Deck profile to 96 DPI (0x60):
  ssh Og@172.16.100.212 'sed -i -E "/^\[Control Panel..Desktop\] /,/^\$/ s/\"LogPixels\"=dword:[0-9a-f]+/\"LogPixels\"=dword:00000060/" /var/home/Og/.local/share/Steam/steamapps/compatdata/3111887265/pfx/user.reg'
  ```
- The `update_wine_dpi()` function in `scripts/sunshine-set-resolution.sh` is now considered legacy. Don't re-wire it into any prep-cmd. After the 2026-05-12 AMD migration, restructure or delete that script — fixed per-profile DPI makes its DPI-swap logic obsolete.
- GW2 in-game "UI Size" setting (Options → General → User Interface) is **ignored when GFXSettings.xml has `dpiScaling=true`**. GW2 scales exclusively from Wine LogPixels. So in-game DPI hotkeys / chord bindings won't make UI larger or smaller — only restarting GW2 with different LogPixels will.
