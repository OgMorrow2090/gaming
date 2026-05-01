---
name: Steam Input host depends on Moonlight client type — Deck does it locally, Apple TV doesn't
description: Where Steam Input runs determines which machine's Steam Controller Configs CLOUD layout governs streaming. Deck = client-side, Apple TV = host-side.
type: project
---

<!-- markdownlint-disable MD041 -->

For streaming GW2 from bazzite to two clients, the controller layout that's actually applied lives on a **different machine depending on the client**:

| Client | What runs on client | What bazzite sees | Layout that matters lives on… |
|---|---|---|---|
| Steam Deck (172.16.100.95) | Steam (with Steam Input) launching Moonlight as a non-Steam shortcut | Keyboard + mouse events (Deck already translated controller → keys) | **Deck's** `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight/<filename>.vdf`, controlled by Deck's `configset_controller_neptune.vdf` for appname `moonlight` and `moonlight - gw2 steamos` |
| Apple TV 4K | Moonlight tvOS app (no Steam) | Raw HID gamepad events | **Bazzite's** Steam Input config for the foreground app — `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/<appname>/<filename>.vdf`, with `configset_controller_neptune.vdf` keyed by appname `guild wars 2 (apple tv)` |

**Why:** caught on 2026-05-01 — deployed v18.4.9 to bazzite + added per-profile CLOUD subdirs (`guild wars 2 (apple tv)`, `guild wars 2 (steam deck)`) on bazzite, but Deck stream still showed v18.4.8. User clarified: "Big Picture on bazzite over Moonlight says no controller found" — bazzite literally never sees the Deck's gamepad because the Deck's Steam Input intercepts and forwards keys. The bazzite-side `guild wars 2 (steam deck)` CLOUD layout is dead code for the Deck stream.

## How to apply

Whenever changing controller bindings, deploy to **all relevant hosts**:

1. **Always** SCP to **Deck** at `deck@172.16.100.95` → `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight/og v<X.Y.Z>_0.vdf` and bump Deck's `configset_controller_neptune.vdf` `moonlight` + `moonlight - gw2 steamos` entries.
2. **Always** SCP to **bazzite** at `Og@172.16.100.212` → per-profile subdir for any non-Steam GW2 profile that's launched directly under bazzite (Apple TV stream, future direct-play paths) + bump `configset_controller_neptune.vdf` for that appname.
3. After both deploys, restart Steam on **whichever host's layout you changed** (kill `steamwebhelper` + parent `steam`, bust `htmlcache/{Cache,Code Cache,GPUCache}`, let systemd respawn).

## Lookup chain by stream client

**Deck → bazzite Big Picture:**
1. Deck physical input → Deck's Steam Input reads layout from Deck's `Steam Controller Configs/.../config/moonlight/og v<X.Y.Z>_0.vdf` (looked up via Deck's `configset_controller_neptune.vdf` "moonlight" entry)
2. Deck's Steam Input emits keyboard/mouse → Moonlight client → bazzite host as keyboard/mouse over network
3. Bazzite's Steam Input sees a keyboard, **not a gamepad** — bazzite-side `guild wars 2 (steam deck)` configset entry is unused

**Apple TV → bazzite Big Picture:**
1. Apple TV controller → Moonlight tvOS app → bazzite host as raw HID gamepad events
2. Bazzite's Steam Input sees a virtual controller → applies layout per foreground app's configset entry
3. For "Guild Wars 2 (Apple TV)" non-Steam shortcut, bazzite's `configset_controller_neptune.vdf` "guild wars 2 (apple tv)" template path governs the layout

So: **Deck stream depends on Deck-side layouts. Apple TV stream depends on bazzite-side layouts.** Update accordingly.
