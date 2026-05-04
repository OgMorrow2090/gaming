---
name: Moonlight stream stuck at GW2 character select — diagnostic + likely causes
description: When Moonlight shows GW2 frozen at character select with bazzite cursor visible and Nexus icons rendered but unclickable, GW2 itself is fine — the failure is in streaming/focus. Diagnostic commands and three contributing causes found 2026-05-04.
type: project
---

User-visible symptom: Moonlight stream from bazzite shows GW2 stuck at character select. Background black, no character previews load, Nexus addon icons visible at top but unclickable, system (gamescope/bazzite) cursor visible — **not** GW2's themed cursor. Always reproduces "at the same place" when it happens, but recurrence is intermittent.

GW2 itself is **not** crashed in this state. Confirmed via:

```bash
# Process alive, busy on workers
ps -eo pid,pcpu,pmem,etime,wchan:25,comm | grep -E "Gw2|wineserver"
# 478% CPU, main thread parked in ntsync_schedule (normal between-frames sync wait)

# Network connections to ArenaNet game servers established + idle
ss -tn state established | grep ":6112"
# Recv-Q 0, Send-Q 0 — no backlog, server side fine

# Addons cycling normally → game's main loop running, renderer hooked
tail ~/Games/gw2-appletv/addons/Nexus/Nexus.log
# MysticTrading [DEBUG] Cycle N every ~30s
```

If those three checks all pass, the game is alive and rendering — the freeze is upstream, in streaming/focus.

## The cursor tell

**System cursor (X11/gamescope) on screen = GW2 does not have input focus.** GW2 captures and themes its pointer when focused; the themed cursor disappearing means clicks are being absorbed by another surface (Steam Big Picture / CefHost.exe overlay).

## Three contributing causes found 2026-05-04

### 1. Dual Moonlight sessions (the big one for Apple-TV-as-screen + Deck-as-controller)

```bash
grep "active sessions" ~/.config/sunshine/sunshine.log | tail
# active sessions: 2 ← bad
```

When both Apple TV and Deck have Moonlight connected (e.g. using Deck as TV controller while streaming GW2 to TV), Sunshine stacks two streaming sessions. Encoder forks across both. Input routes to whichever client connected last but the screen surface focus stays elsewhere → GW2 receives no clicks.

Fix: only one Moonlight client connected at a time. Will become moot when the dedicated Steam Controller for bazzite arrives (~2026-05-09) and Deck is no longer needed as a controller.

### 2. Per-profile GFXSettings can drift

Apple TV profile (`compatdata/2879321470/`) had `<RESOLUTION Width="1280" Height="800">` baked in — Deck dimensions, not Apple TV's 2560×1440. Wine DPI was correct (LogPixels 0x90 = 144) but the in-game resolution wasn't, so character-select viewport math could break. Fixed by editing `GFXSettings.Gw2-64.exe.xml` directly.

GFXSettings can drift if the user opens Options on the wrong profile (changes save to whichever Wine prefix is running). Verify after any in-game Options change:

```bash
grep RESOLUTION ~/.local/share/Steam/steamapps/compatdata/2879321470/pfx/drive_c/users/steamuser/AppData/Roaming/Guild\ Wars\ 2/GFXSettings.Gw2-64.exe.xml
```

Expected: 2560×1440 for Apple TV (appid 2879321470), 1280×800 for Deck (appid 1284210).

### 3. Sunshine service unit is `sunshine-gaming.service`, not `sunshine.service`

```bash
systemctl --user restart sunshine.service        # FAILS — Unit is masked
systemctl --user restart sunshine-gaming.service # CORRECT
```

bazzite ships `sunshine.service` masked and uses `sunshine-gaming.service` (custom unit, "stable config"). Use `systemctl --user list-units --all | grep -i sunshine` to confirm if the unit name changes.

## Recovery sequence when symptom appears

```bash
# 1. Kill GW2 cleanly
ssh Og@172.16.100.212 'pkill -TERM -f Gw2-64.exe; sleep 4; pkill -KILL -f Gw2-64.exe || true'

# 2. Restart Sunshine to drop any ghost sessions
ssh Og@172.16.100.212 'systemctl --user restart sunshine-gaming'

# 3. Verify per-profile GFXSettings still match expected resolution (see §2)

# 4. Reconnect Moonlight from ONE client only — don't open the second one
```

## Open / unresolved

Symptom is intermittent and not always reproducible. The 2026-05-04 fix landed during a session that was already broken (mid-stream), so we couldn't fully verify which of the three contributing causes was the dominant trigger. Likely all three compounded. **If symptom recurs after Steam Controller arrives (eliminating cause 1), focus on causes 2 and 3 — and dig deeper into Big Picture / CefHost.exe surface stacking in gamescope.**
