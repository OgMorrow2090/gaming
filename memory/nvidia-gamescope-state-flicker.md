---
name: NVIDIA + gamescope display state accumulates → HDMI flicker
description: After many gamescope-session restarts / EDID swaps / vkms module reloads, NVIDIA proprietary driver enters a stale state that causes purple/red HDMI flicker on 4K@165. Cold reboot is the reliable reset.
type: project
---

NVIDIA proprietary driver (595.58.x) keeps internal display-pipeline state (EDID cache, HDMI link parameters, color/timing negotiation) across `gamescope-session-plus@steam` restarts. Heavy churn in a single uptime — mode switches, custom-EDID kernel arg toggles, vkms module load/unload, HDR flag flips — accumulates stale state. Eventually a HDMI link train comes up with mismatched parameters → purple/red colour-cycle flicker at 4K@165, regardless of cable or mode config.

**Cold reboot wipes driver state + EDID cache + HDMI link state → clean re-negotiation. If a soft reboot doesn't clear it, do a FULL POWER DRAIN: PSU rocker off, hold case power button 10s, wait 30s, power back on. Confirmed 2026-05-11 evening — soft reboot was insufficient after heavy driver churn; capacitor drain was required.**

**Second discovery 2026-05-11 evening: changing `gamescope-mode` while a Moonlight stream is active reliably re-triggers the flicker class.** Sunshine's KMS capture holds DRM resources; restarting gamescope-session-plus@steam mid-stream forces NVIDIA to renegotiate the HDMI link while Sunshine still expects its old framebuffer reference. **Always end the Moonlight stream BEFORE running `~/bin/gamescope-mode <new>`.**

**Why:** Observed 2026-05-11 during the Bazzite-on-LG-G5 setup session. Did dozens of mode switches + custom-EDID experiments + vkms toggles + Evanlak plug/unplug without rebooting. 4K@165 started flickering. Soft reboot fixed it briefly, more experiments re-broke it, second soft reboot didn't fix. Full PSU-off capacitor drain restored it. Then discovered that even on the clean stack, mid-stream mode changes re-trigger.

**How to apply:**

- If 4K@165 (or any high-bandwidth mode) starts flickering on the LG G5 after session-experiment churn:
  1. First: soft reboot (`sudo systemctl reboot`)
  2. If still flickering: full power drain (PSU rocker off + hold case button 10s + wait 30s + power on)
  3. Don't waste time on EDID injection / `--immediate-flips` tweaks / HDR strip experiments if symptom emerged mid-session
- **Never change `gamescope-mode` while a Moonlight stream is active.** Workflow: end the stream (Alt+F4 in Moonlight client) → wait ~5s for Sunshine to close the session → `~/bin/gamescope-mode <new>` → start Moonlight again. Skipping the end-stream step reliably re-triggers flicker.
- Avoid stacking custom EDID + vkms + HDR toggles + mode switches in the same uptime. If experimenting with display infrastructure, reboot between distinct experiments to keep driver state clean.
- When the flicker symptom is "purple/red colour cycle that comes and goes" (not "permanent garbled output"), that signature points to NVIDIA driver state, not a cable or EDID problem.
- This entire memo class becomes obsolete on AMD (RX 9070 XT migration 2026-05-12) — amdgpu doesn't have these state-management failure modes. Keep this memo for historical reference + in case the GPU is ever swapped back to NVIDIA.
