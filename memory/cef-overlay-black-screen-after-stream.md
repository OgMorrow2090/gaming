---
name: cef-overlay-black-screen-after-stream
description: After switching LG TV input back to bazzite direct HDMI (from any source — Apple TV, regular TV, etc.), a steamwebhelper CEF overlay covers Big Picture with black screen + cursor. May self-recover after ~15 min; otherwise restart gamescope-session.
metadata:
  type: project
---

After switching the LG TV input back to bazzite's direct HDMI — from **any** source, not just Moonlight streaming — sometimes shows **cursor + pure black screen**. Right-clicking reveals a Chromium context menu ("Forward", "Print...") confirming the topmost gamescope surface is a **steamwebhelper CEF overlay**, not Big Picture.

Big Picture and gamescope are alive underneath — the CEF window is just sitting on top, rendering blank.

**Occurrences:**
- 2026-05-13 (twice) — after ending Apple TV Moonlight stream + switching TV input
- 2026-05-13 (third time) — after ~3 hours of regular TV watching (no Moonlight/Sunshine involved), switching TV input back to bazzite. **This confirms the trigger is TV input switching, not streaming.**

**Self-recovery observed:** On the third occurrence, user clicked "Print..." (opening the CEF print dialog showing LPR printer options), then left for ~15 minutes. When they returned, BPM had restored itself — the CEF overlay had dismissed on its own. No gamescope restart or reboot needed.

**Why:** Likely a surface-stacking race in gamescope: when the TV input switches away and back, the HDMI hotplug/EDID renegotiation triggers gamescope to re-composite its surfaces, and a dormant CEF overlay (Steam browser, notification, or overlay) ends up on top instead of the Big Picture main window. The self-recovery may be gamescope's surface management eventually correcting the z-order, or the CEF overlay timing out.

**How to apply:**

- **Wait first (~15 min):** The overlay may self-recover. Opening the print dialog (right-click → Print...) doesn't hurt and may have helped by giving the CEF window focus then losing it.

- **Recovery if it doesn't self-resolve:**
  ```bash
  ssh Og@172.16.100.212 'systemctl --user restart gamescope-session-plus@steam.service'
  ```
  Wait ~15s for Big Picture to reboot. Much faster than a full system reboot.

- **Diagnostic tell:** if you can right-click and see "Forward" / "Print..." / "Back" in the context menu, it's a CEF overlay — not a display pipeline failure. Don't waste time on EDID/gamescope-mode troubleshooting.

- **Next step:** Wait for DP-to-HDMI dongle arrival — the hotplug/EDID handshake may behave differently over DisplayPort→HDMI vs native HDMI, potentially eliminating the issue. Don't investigate CEC or gamescope workarounds until tested with the new cable path.

- **If issue persists after DP dongle, investigate:**
  - Whether CEC (HDMI-CEC) signals from the LG TV are triggering the issue — try disabling CEC on bazzite
  - Whether disabling Steam's built-in browser overlay reduces the chance of a CEF window being available to steal the top surface
  - Whether a gamescope update addresses the surface z-order race

Related: [[moonlight-stream-stuck-diagnostic]], [[nvidia-gamescope-state-flicker]]
