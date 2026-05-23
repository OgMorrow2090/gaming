---
name: amdgpu exposes the GPU's HDMI port as TWO connectors (HDMI-A-1 + DP-2) for FRL/HDMI-2.0 dual-mode
description: RX 9070 XT (and other RDNA3/4 cards) have an on-die DP→HDMI PCON converter on their physical HDMI port. Linux amdgpu enumerates this as two separate DRM connectors — `HDMI-A-1` is the TMDS / HDMI 2.0 fallback path, `DP-2` is the HDMI 2.1 FRL path via PCON. Only one is "connected" at a time depending on which mode the link trained at. Same physical socket, same cable, switched dynamically.
metadata:
  type: project
---

<!-- markdownlint-disable MD041 MD032 -->

## What you see

The cable goes from the GPU's **HDMI socket** to the TV. No external adapter.
Yet `for c in /sys/class/drm/card1-*; do echo $(basename $c): $(cat $c/status); done`
shows:

```
card1-DP-1: disconnected
card1-DP-2: connected         ← HDMI 2.1 / FRL trained
card1-HDMI-A-1: disconnected  ← HDMI 2.0 / TMDS fallback (not active)
card1-HDMI-A-2: disconnected
```

…or, when FRL fails:

```
card1-DP-2: disconnected
card1-HDMI-A-1: connected     ← TMDS-only fallback active
```

It's the **same physical port**. The kernel reports whichever protocol path
won link training. Audio works in both modes because the wire is still a
plain HDMI cable carrying HDMI audio packets — the PCON conversion is
internal to the GPU.

The boot log line `amdgpu: [drm] DP-HDMI FRL PCON supported` confirms
the on-die PCON. This is **not** the external DP→HDMI active-adapter
pathway (which historically dropped audio).

## Why this matters — the symptom that bit us 2026-05-23

User's friend plugged an Xbox into the TV's HDMI socket. Cable later
returned to bazzite. From the user's POV bazzite was on the same TV port
with the same HDMI cable, but Steam BPM only offered 4K@60 in the refresh
dropdown — 4K@120 had vanished. Two-hour archaeology ensued:

- xrandr (under gamescope nested XWayland) only showed `3840x2160 59.98*+`
- gamescope log said `drm: selecting mode 3840x2160@60Hz`
- The DRM connector reported as `HDMI-A-1` (not `DP-2`), with the EDID's
  HDMI Forum VSDB showing TMDS-only (no FRL block visible at first glance)
- This led me down a wrong path of "the TV's HDMI port is stuck in HDMI 2.0
  cached state after the Xbox handshake" → cable replug → wall power cycle
  → factory-reset thoughts → kernel-level "amdgpu doesn't support direct
  HDMI FRL" theory — all wrong

Actual root cause: the Xbox handshake dropped the FRL link state. After
the cable came back to bazzite, FRL never re-trained (until later in the
session by accident), so amdgpu kept the connection on the `HDMI-A-1`
TMDS fallback. The DP-2/FRL pathway was simply not active — there was no
"stuck cache" to clear, no firmware bug, no kernel limitation.

The fix that worked: `systemctl --user restart gamescope-session-plus@steam.service`
re-triggered the connector probe at a moment when FRL was able to train.
The connector flipped from `HDMI-A-1` to `DP-2`, mode list expanded to
include 4K@120 and 4K@165, gamescope selected `-r 120` and locked in.

## Recovery recipe (next time this happens)

1. **Don't go digging into TV settings first.** Check the DRM connector state:
   ```bash
   ssh bazzite 'for c in /sys/class/drm/card1-*; do
     echo "$(basename $c): $(cat $c/status 2>/dev/null)"
   done'
   ```
   If `HDMI-A-1: connected` and `DP-2: disconnected` → FRL didn't train.
   The TV is fine, the cable is fine, the GPU is fine. It's just on the
   slow path.

2. **Restart gamescope session.** Plain old:
   ```bash
   systemctl --user restart gamescope-session-plus@steam.service
   ```
   Wait ~8 s, then re-check the connector state. If `DP-2: connected`
   you're done — `xrandr` will now show the high-refresh modes and
   gamescope-wrapper's `-r 120/144/165` will stick.

3. **If still stuck:** force a kernel-side hotplug:
   ```bash
   echo 1 | sudo tee /sys/kernel/debug/dri/0000:03:00.0/HDMI-A-1/trigger_hotplug
   ```
   Then restart gamescope again. (Sometimes two passes are needed.)

4. **If still stuck:** reboot bazzite. Brings the link up from scratch
   and forces FRL re-training at kernel init.

5. **Genuinely last resort:** unplug the HDMI cable from the GPU side
   for 30 s. (Not the TV side — TV-side replugs were unreliable in the
   2026-05-23 session.) Force a fresh source-side HPD cycle, then reboot
   or restart gamescope.

## Things that DON'T need doing for this symptom

- TV factory reset
- TV wall power cycle / drain capacitor
- LG WebOS HDMI Deep Color toggle off/on  ← these were red herrings;
  the TV's per-port settings were never the issue
- Cable change (Ultra High Speed HDMI cable is fine; same cable works)
- Switching TV HDMI ports (port 1 vs port 2 — irrelevant from GPU side)

These were all attempted on 2026-05-23 and none of them moved the needle.
The fix is GPU-side: get amdgpu to re-train FRL on the link.

## Diagnostic anchor — the EDID looks different per pathway

Important debugging gotcha: the EDID exposed at
`/sys/class/drm/card1-HDMI-A-1/edid` is the **fallback HDMI 2.0** EDID
— it has `Maximum TMDS Character Rate: 600 MHz` and no FRL block visible
in the basic VSDB. This is **NOT proof the TV is publishing HDMI 2.0
only**. The TV publishes the full HDMI 2.1 EDID; you only see it via
`/sys/class/drm/card1-DP-2/edid` when FRL has trained and `DP-2` is the
active connector. Read both.

## Related

- [[nvidia-gamescope-state-flicker]] — older NVIDIA-era issue, totally
  different cause but same surface symptom of "rates disappear from BPM"
- [[gw2-windowed-fullscreen-dpiscaling-trick]] — adjacent display-state
  area for GW2 specifically
