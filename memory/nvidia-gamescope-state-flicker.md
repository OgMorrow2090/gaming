---
name: NVIDIA + gamescope display state accumulates → HDMI flicker
description: After many gamescope-session restarts / EDID swaps / vkms module reloads, NVIDIA proprietary driver enters a stale state that causes purple/red HDMI flicker on 4K@165. Cold reboot is the reliable reset.
type: project
---

NVIDIA proprietary driver (595.58.x) keeps internal display-pipeline state (EDID cache, HDMI link parameters, color/timing negotiation) across `gamescope-session-plus@steam` restarts. Heavy churn in a single uptime — mode switches, custom-EDID kernel arg toggles, vkms module load/unload, HDR flag flips — accumulates stale state. Eventually a HDMI link train comes up with mismatched parameters → purple/red colour-cycle flicker at 4K@165, regardless of cable or mode config.

**Cold reboot wipes driver state + EDID cache + HDMI link state → clean re-negotiation → flicker gone.**

**Why:** Observed 2026-05-11 during the Bazzite-on-LG-G5 setup session. Did dozens of mode switches + custom-EDID experiments + vkms toggles without rebooting. 4K@165 started flickering. After cold reboot with `~/.config/gamescope-mode=4k165` pinned: 4K@165 + HDR + `--immediate-flips` + VRR ran cleanly, no flicker. The cable (HDMI 2.1) and gamescope config were innocent.

**How to apply:**
- If 4K@165 (or any high-bandwidth mode) starts flickering on the LG G5 after a lot of session-experiment churn, **cold reboot first** before debugging the cable/EDID/mode. Don't waste time on EDID injection or `--immediate-flips` tweaks if the symptom emerged mid-session.
- Avoid stacking custom EDID + vkms + HDR toggles + mode switches in the same uptime. If you need to experiment with display infrastructure, reboot between distinct experiments to keep driver state clean.
- When the flicker symptom is "purple/red colour cycle that comes and goes" (not "permanent garbled output"), that signature points to NVIDIA driver state, not a cable or EDID problem.
- Document any new mode-switching workflow with a "reboot to clear state" note if it touches kernel args or DRM module loads.
