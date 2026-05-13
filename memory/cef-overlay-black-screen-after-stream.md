---
name: cef-overlay-black-screen-after-stream
description: After ending Apple TV Moonlight stream and switching TV input back to bazzite direct HDMI, a steamwebhelper CEF overlay covers Big Picture with black screen + cursor. Restart gamescope-session to fix (no reboot needed).
metadata:
  type: project
---

After a Moonlight streaming session from Apple TV, switching the LG TV input back to bazzite's direct HDMI sometimes shows **cursor + pure black screen**. Right-clicking reveals a Chromium context menu ("forward", "print") — confirming the topmost gamescope surface is a **steamwebhelper CEF overlay**, not Big Picture.

Big Picture and gamescope are alive underneath — the CEF window is just sitting on top, rendering blank.

**Why:** Observed twice (2026-05-13). Likely a surface-stacking race: when Sunshine releases the KMS/DRM capture after the Moonlight session ends, gamescope re-composites its surfaces and a dormant CEF overlay (Steam browser, notification, or overlay) ends up on top instead of the Big Picture main window.

**How to apply:**

- **Recovery (no reboot needed):**
  ```bash
  ssh Og@172.16.100.212 'systemctl --user restart gamescope-session-plus@steam.service'
  ```
  Wait ~15s for Big Picture to reboot. Much faster than a full system reboot.

- **Diagnostic tell:** if you can right-click and see "forward" / "print" / "back" in the context menu, it's a CEF overlay — not a display pipeline failure. Don't waste time on EDID/gamescope-mode troubleshooting.

- If this becomes frequent, investigate whether ending the Sunshine session cleanly before switching TV input helps (end stream in Moonlight client → wait 5s → switch TV input).

Related: [[moonlight-stream-stuck-diagnostic]], [[nvidia-gamescope-state-flicker]]
