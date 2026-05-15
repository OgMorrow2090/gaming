---
name: controller-wake-tv-script
description: WebOS network API replaces CEC for TV input switching on controller wake. Silence-detection on hidraw0 unchanged. Direct HDMI has no CEC adapter — AMD GPU native HDMI doesn't expose /dev/cec*.
metadata:
  type: project
---

The controller wake script (`~/bin/controller-wake-tv.py` on bazzite) switches the LG TV to bazzite's HDMI input via the **WebOS SSAP API** over the local network when the Steam Controller wakes from sleep.

**Why WebOS instead of CEC:**
- Switched from UGREEN DP-to-HDMI dongle to direct HDMI 2.1 (GPU native port) after discovering dongle didn't pass audio
- AMD GPU's native HDMI port does NOT expose a CEC adapter — no `/dev/cec*` device exists
- LG TV firmware update (2026-05-14) fixed HDMI 2.1 negotiation, enabling 4K@120Hz + FreeSync + audio on direct HDMI — dongle no longer needed
- WebOS API (`aiowebostv` library) provides the same input-switching capability over LAN

**TV connection details:**
- IP: 172.16.100.44 (WebOS/4.1.0)
- MAC: 6c:15:db:8d:a5:a6
- Client key: 2c37f5beb73ad7247415b32263c9501f
- Bazzite input: HDMI_2
- Apple TV input: HDMI_1

**Silence-detection (unchanged from CEC version):**
- Monitors `/dev/hidraw0` for Steam Controller data (0x42 reports at ~267/sec when awake)
- `ever_seen_data` gate: only triggers after data observed → silence → data resumes
- 30-second silence threshold = controller powered off
- 60-second cooldown between triggers
- Controller takes ~35 seconds to actually power down after holding Steam button

**TV WOL prerequisite:**

- LG TV must have **"WoL"** enabled (Settings → General → Devices) — without this, the WiFi radio powers down completely in standby and WOL magic packets never arrive
- Confirmed working 2026-05-15: TV off → WOL → TV powers on → WebOS switches input

**Dependencies (pip install --user):**
- `aiowebostv` — WebOS WebSocket client
- `wakeonlan` — WOL magic packet (sent before WebOS connect in case TV is in standby)

**How to apply:**
- Script: `scp configs/bazzite/controller-wake-tv.py Og@172.16.100.212:~/bin/`
- Service: `scp configs/bazzite/controller-wake-tv.service Og@172.16.100.212:~/.config/systemd/user/`
- Enable: `systemctl --user enable --now controller-wake-tv.service`
- Old CEC script removed from bazzite (2026-05-14)

Related: [[streaming-input-host-vs-client]]
