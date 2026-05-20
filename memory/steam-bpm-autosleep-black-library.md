---
name: BPM library goes black after 1-hour AC auto-suspend; restart steamwebhelper to recover
description: Steam Big Picture (gamepadui, -steamdeck mode) auto-suspends after 1 hour even on AC. On wake the library SPA fails to repaint while the Quick Access side panel renders fine. Diagnostic + recovery + permanent fix.
metadata:
  type: project
---

<!-- markdownlint-disable MD041 MD032 -->

## Symptom

User presses Steam button (or returns to the bazzite TV input) and sees BPM with
the **library page entirely black** — but the **Quick Access side panel**
(Reboot, System Settings, etc.) paints normally. UI isn't dead, only the main
library tab. Not a CEF crash, not a gamescope crash, not a TV-input switch bug.

## Root cause

Steam BPM, when launched with `-steamdeck -steampal -gamepadui` (the bazzite
HTPC default), inherits the Steam Deck's auto-sleep policy and suspends after
60 minutes of inactivity **even when on AC power**. On resume the SteamRoot
SPA's library WebContents fails to repaint, while the side-panel React root
(separate tree) repaints fine.

Evidence in `~/.local/share/Steam/logs/`:

- `steamui_system.txt`:
  `Switching to power state: [k_ESystemPowerState_Sleep]`
  `reason: 'ComputeNextPowerState: active: 3600 < 3600 (k_EACState_Connected)'`
- `webhelper_js.txt`: `Received suspend request` → `PrepareForSystemSuspend succeeded`
- Wake later: `Switching to power state: [k_ESystemPowerState_Normal]`
- Stale steamwebhelper has `-steamid=0` (lost user binding); a fresh respawn has
  `-steamid=76561198025059559`.

Tracked upstream: [bazzite#1175](https://github.com/ublue-os/bazzite/issues/1175)
— "Steam UI icons and buttons disappear after sleep in Game Mode".

## Recovery (lightest touch)

```bash
ssh Og@172.16.100.212 'pkill -f "^./steamwebhelper -nocrashdialog"'
```

Steam respawns steamwebhelper in ~4 s. Game mode stays up, controller stays
paired, TV input stays put — only the CEF browser process is replaced. Repaint
is immediate.

Heavier recovery (only if pkill doesn't repaint):
`systemctl --user restart gamescope-session-plus@steam.service` — full BPM bounce.

## Permanent fix

**Steam button → ⚙ Settings → Power → "Time Until Sleep" → Never.**
This is the official Valve setting; persists in BPM's IndexedDB
(`config/htmlcache/Default/Local Storage/leveldb/`). The
`com.steampowered.SteamOSManager1` dbus interface does NOT expose a sleep-timer
property — only WifiBackend, TdpLimit, Storage. So the in-UI toggle is the only
sanctioned write path. Once "Never" is set the 3600s timer no longer arms and
BPM stays awake indefinitely on AC.

## Why don't logs show a CEF crash?

Because CEF didn't crash. The browser process keeps running across suspend/
resume; only the page's render state is wedged. `cef_log.txt` shows no error
between the suspend at 14:50 and the wake at 15:36 — just the regular 30-min
`cast_crl` heartbeat.

## How to recognize this state vs the other BPM black-screen

- This one: side panels work, library black, no recent CEF crash, suspend/resume
  pair in `steamui_system.txt` reason `active: 3600 < 3600`.
- The other one (CEF overlay after TV input switch): everything is black
  including the side panels; see [[cef-overlay-black-screen-after-stream]].

Related: [[cef-overlay-black-screen-after-stream]],
[[cec-controller-wake-script]] (the wake script just makes you LOOK at bazzite
where BPM was already silently suspended; it doesn't cause the suspend).
