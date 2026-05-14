---
name: cec-controller-wake-script
description: CEC wake script uses silence-detection on hidraw0 (not button filtering) because Steam exclusively grabs all button data. Triggers on controller sleep→wake transition only.
metadata:
  type: project
---

The CEC wake script (`~/bin/cec-controller-wake.sh` on bazzite) is Python, not bash. It detects when the Steam Controller wakes from sleep by monitoring hidraw0 for a silence→data transition.

**Why silence-detection, not button filtering:**
- Steam/gamescope exclusively grabs ALL button data — bytes 7-8 in `0x42` reports are always `00,00`
- The Steam button is intercepted by controller firmware and produces no standard HID report at all
- Neither hidraw (5 devices) nor evdev (Mouse, Keyboard, Xbox 360 pad virtual devices) expose any button presses while gamescope runs
- Only `0x7b` sensor/IMU reports (13 bytes, ~2/sec) are visible, and they flow continuously while controller is awake, stop when it sleeps

**Key design decisions:**
- `ever_seen_data` gate: CEC only fires after data has been observed at least once, then silence, then data resumes. Prevents false triggers on boot where no prior data baseline exists.
- No boot grace period needed (the `ever_seen_data` gate handles it)
- 30-second silence threshold = controller asleep
- 60-second cooldown between CEC sends

**What it does NOT prevent:** TV switching to bazzite on reboot/gamescope start. That's gamescope's display pipeline initializing the HDMI output — independent of CEC commands. User accepts this.

**How to apply:**
- Config backup: `configs/bazzite/cec-controller-wake.sh`
- Service unit: `configs/bazzite/cec-controller-wake.service`
- Deploy: `scp` to `~/bin/cec-controller-wake.sh`, `systemctl --user restart cec-controller-wake.service`

Related: [[cef-overlay-black-screen-after-stream]], [[streaming-input-host-vs-client]]
