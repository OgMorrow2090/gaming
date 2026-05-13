---
name: project-bazzite-repurpose-plan
description: Long-term plan (~2028) to repurpose current bazzite PC as combined Docker server (replacing thing-pi) + headless Moonlight/Sunshine streaming server when a new TV-connected PC is purchased
metadata:
  type: project
---

When a new PC is purchased (~2 years from 2026-05), the current bazzite machine (i9-9900K, 64GB RAM, AMD RX 9070 XT) will be repurposed to serve two roles:

1. **Docker container host** — replaces thing-pi for all addams_family containers
2. **Headless Sunshine streaming server** — continues streaming GW2 to Deck / Apple TV via Moonlight

The new PC will be connected directly to the LG TV for local gaming — no Moonlight/Sunshine streaming needed on it.

**Why:** Consolidates thing-pi + streaming server into one box. The i9-9900K + 64GB is massively overkill for Docker alone but perfect for dual-purpose. GPU handles both game rendering and VAAPI encoding for Sunshine.

**How to apply:**

- **Headless Sunshine**: AMD GPU will need a virtual display solution (dummy HDMI plug or software framebuffer) since gamescope currently relies on a real display. Research headless gamescope or virtual EDID injection closer to the time.
- **Power draw**: i9-9900K idles ~40-60W vs Pi ~5W. Acceptable trade-off for the capability gain.
- **OS choice**: Could stay on Bazzite (immutable, already configured) or reinstall as Fedora Server / Debian. Bazzite's immutability is good for gaming but may complicate Docker workflows — evaluate when the time comes.
- **Network**: the box stays at its current LAN position (172.16.100.212). Deck and Apple TV Moonlight configs won't need updating.

Related: [[streaming-input-host-vs-client]], [[gamescope-nested-xwayland-constraint]]
