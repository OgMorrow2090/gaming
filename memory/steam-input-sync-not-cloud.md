---
name: Steam Input sync is separate from Steam Cloud toggle
description: Disabling Steam Cloud in the client UI does not stop Steam Input from re-syncing controller layouts — Valve bug #7801
type: project
---

Steam Input maintains its own server-side sync layer for controller configs that is **independent** of the Steam Cloud toggle in the client UI. Disabling Steam Cloud will not stop Steam Input from re-injecting workshop refs or re-pulling deleted layouts on next launch.

This is Valve bug #7801, unfixed as of 2026-04.

**Why:** confirmed during the v18.4 outage (2026-04-26) when ghost workshop refs (`workshop://3714576121`, `workshop://3552083229`) kept getting restored on Steam restart even with Cloud disabled. Workshop deletion via browser returned "hidden" errors that the user could not clear. Only signout/signin broke the cycle.

**How to apply:** when Steam Deck layouts misbehave or ghost configs return:

1. Don't trust "Cloud is off" as proof that nothing syncs.
2. Recovery sequence (only thing that consistently works):
   - Quit Steam fully.
   - Wipe local caches: `~/.steam/steam/userdata/<userid>/241100/remotecache.vdf`, `~/.steam/steam/steamapps/workshop/appworkshop_241100.acf`, `~/.steam/steam/userdata/<userid>/ugcmsgcache/`.
   - Sign out, sign back in.
3. Reference: `docs/vdf-editing-golden-rules.md` Rule #6.
