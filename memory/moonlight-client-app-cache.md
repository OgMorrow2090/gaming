---
name: moonlight-client-app-cache
description: Moonlight caches the Sunshine app list in its local config — stale cache masks server-side changes; CLI `list` reads cache not server
metadata:
  type: project
---

Moonlight stores a local cache of each paired host's app list in its config file:

- **Deck**: `~/.var/app/com.moonlight_stream.Moonlight/config/Moonlight Game Streaming Project/Moonlight.conf`
- Format: `[hosts]` section, entries like `1\apps\1\name=SteamOS`, `1\apps\1\id=1594984826`, `1\apps\size=6`

The `moonlight list <host>` CLI command reads this cache, NOT the server. Restarting Sunshine does NOT update the Moonlight client cache. The cache refreshes only when the Moonlight GUI connects to the host.

**Why:** Discovered 2026-05-13 when trimming Sunshine apps from 6 to 3. Sunshine API confirmed correct 3-app list, but `moonlight list` kept returning old 6 apps. Root cause was stale `1\apps\*` entries in the Deck's Moonlight config.

**How to apply:** When changing Sunshine app configuration on bazzite:

1. Restart `sunshine-gaming.service` on bazzite
2. On each Moonlight client, either:
   - Open Moonlight GUI and navigate to the host (triggers fresh fetch), OR
   - Delete `1\apps\*` entries from the config file (forces refetch on next GUI connect)
3. Don't trust `moonlight list` for verification — use `curl -u sunshine:admin123 https://localhost:47990/api/apps` on bazzite instead

Other clients (Apple TV, Mac mini, MacBook Air) may also have stale caches — clear when issues arise.

Related: [[steamos-add-to-steam-for-non-steam-games]]
