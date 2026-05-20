---
name: gw2-favorites-cleanup script reconciles addon favorites against the live GW2 account API
description: Host-side script + systemd timer on bazzite that drops NexusGameWiki and CraftyLegend favorites already unlocked in /v2/account/skins or /v2/account/legendaryarmory. Refuses to mutate while GW2 is running (Nexus rewrites library.json on shutdown).
metadata:
  type: project
---

<!-- markdownlint-disable MD041 MD032 -->

When Guild Wars 2 closes, the wardrobe / legendary armory have just refreshed
and stale favorites accumulate in the Wiki + Legendary addons — items the
player has since unlocked but the addons still list as "to do". This bazzite-
side script reconciles each favorites file against the live GW2 account API
and drops anything already complete.

## Files

- **Script**: `~/scripts/gw2-favorites-cleanup.py` (canonical in repo at `configs/bazzite/gw2-favorites-cleanup.py`)
- **Service**: `~/.config/systemd/user/gw2-favorites-cleanup.service` (`Type=oneshot`)
- **Timer**: `~/.config/systemd/user/gw2-favorites-cleanup.timer` (`OnUnitInactiveSec=30min`, `Persistent=true`)
- **API key**: `GW2_API_KEY=…` in `~/.config/gw2-claude/config.env` (chmod 600). Uses the *gw2efficiency* key out of 1Password Home → "SC - ArenaNet - Guildwars 2" → "gw2efficiency - API" — scopes include `account`, `unlocks`, `inventories`, plus extras.
- **Logs**: syslog tag `gw2-favorites-cleanup` — view with `journalctl -t gw2-favorites-cleanup`.

## What it does

| Addon | File | Detection rule |
|---|---|---|
| **NexusGameWiki** | `addons/NexusGameWiki/library.json` `favorites[]` | Parse the favorite's wiki pageId for `{{Weapon\|Armor\|Item\|Equipment\|Trinket\|Backpiece\|Skin infobox \| id = NNNNN}}` → look up `/v2/items/<id>` for `default_skin` → if skin in `/v2/account/skins`, remove. |
| **CraftyLegend** | `addons/CraftyLegend/favourites.json` `favourites[]` (array of item ids) | `/v2/items/<id>` for `default_skin` → if skin in `/v2/account/skins`, remove. **Or** if the item id itself is in `/v2/account/legendaryarmory`, remove. |

Every mutation backs the file up first (`*.bak-<epoch>`); drops are logged line-by-line to syslog. If GW2 is alive when the timer fires, the script no-ops in ~5 ms (`pgrep -x Gw2-64.exe`).

## Known gap — legendary precursor containers

CraftyLegend favorites can be **Legendary "Container" items** (e.g. item `105497` = *Aetheric Anchor*, the voucher that unlocks the *Ancora* legendary spear). For those:
- `default_skin` is `None` (a container isn't worn)
- The container's own item id is **never** in `/v2/account/legendaryarmory` — only the *resulting* legendary (e.g. Ancora = item `105496`) goes into the armory after the player consumes the container.

So the current logic correctly keeps the favorite while the player is still working toward the legendary, but **also** won't auto-remove it after they finish. To close this, extend `cleanup_craftylegend()` with a precursor → legendary lookup:

1. If `item.type == "Container"` and `item.rarity == "Legendary"`,
2. Fetch the wiki page (search by item name),
3. Find the first `[[<page>]]` link that resolves to an item via its `{{Item infobox \| id = …}}`,
4. Check that final id against the armory.

Three lines of logic + one wiki call per Legendary-container favorite. Deferred until the user actually finishes a legendary precursor — gives a real test case to verify against.

## How to apply

- Run on demand: `systemctl --user start gw2-favorites-cleanup.service`
- See last run: `journalctl -t gw2-favorites-cleanup --since today --no-pager`
- Restore a wrong removal: each run writes `library.json.bak-<epoch>` / `favourites.json.bak-<epoch>` alongside the original — `mv` the latest backup back.
- Tune cadence: `OnUnitInactiveSec=` in `gw2-favorites-cleanup.timer`. Default 30 min is a compromise between "fire shortly after GW2 closes" and "don't poll the API for nothing". Cheap no-op while GW2 is up.

Related: [[multi-gw2-installs]] (archived — single install only now), [[gw2-claude-vision]] (the existing host-side daemon that shares the same `config.env`).
