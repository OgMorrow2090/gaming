---
name: Nexus / addon dual-deploy rules
description: Hard rules for deploying GW2 addon binaries and configs across both profiles (Steam GW2 = Deck, ~/Games/gw2-appletv/ = Apple TV) on bazzite. Some files auto-propagate via symlink; others (addons/) must be deployed to both manually.
type: feedback
---

<!-- markdownlint-disable MD041 -->

## Two GW2 install dirs on bazzite, only some files are shared

Per `dual-gw2-installs.md`:

- **Symlinked** (single source of truth, patches flow through both): `Gw2.dat`, `bin64/`, `d3d11.dll`
- **Copied** (independent per profile, must be deployed to both manually if change is universal): `Gw2-64.exe`, **the entire `addons/` directory** (Nexus, MysticClicker, MysticTrading, ArcDPS, etc.)

This means **any change to an `addons/` file does NOT automatically propagate** between profiles.

## Deploy targets

| Profile | Bazzite path | Repo backup path |
| --- | --- | --- |
| Deck (Steam GW2) | `~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/` | `configs/gw2-keybinds/deck/` (Nexus) |
| Apple TV (non-Steam shortcut) | `~/Games/gw2-appletv/addons/` | `configs/gw2-keybinds/apple-tv/` (Nexus) |

## Hard rules

1. **Default to deploying to BOTH profiles**, then ask the user "should this be per-profile?". Most changes are universal (new keybind, addon update, ArcDPS config tweak) and should land in both; only UI-position / screen-size-specific changes should diverge per profile.

2. **When deploying a Nexus addon binary update** (Nexus.dll, ArcDPS.dll, or any other `*.dll` in `addons/`): deploy to BOTH `addons/` directories. Symlinking the binary IS an option for addons that have no per-profile state (consider it for stable utility addons).

3. **When deploying Nexus *settings* changes** (`InputBinds.json`, `GameBinds.xml`, `AddonConfig.json`, `Settings.json`):
   - Universal changes (new keybind, new addon config) → deploy to both, update both repo backup dirs
   - Per-profile changes (UI layout for 2K vs Deck panel) → deploy to one, update only that profile's repo backup

4. **When deploying Mystic Clicker per-resolution configs** (`mystic-clicker-WIDTHxHEIGHT.cfg`): both profiles have all per-resolution variants. Deploy any added/edited variant to both `addons/MysticClicker/` directories.

5. **Always update the matching repo backup dir** whenever you deploy to a profile on bazzite. The repo at `configs/gw2-keybinds/{deck,apple-tv}/` is the source of truth — don't let bazzite drift from the repo.

6. **Never `cp -r` from one install dir to the other** — that clobbers the destination's diverged configs. Always file-by-file.

## Why this matters

User confirmed during the dual-install setup (2026-04-30): the whole point of dual profiles is that **GW2 settings, Nexus UI positions, and Mystic Clicker click coordinates** can diverge per stream client without manual re-editing every time they switch. If you blindly mirror files, that benefit is lost.

## How to apply

Before deploying any addon-related change:

1. Identify scope: universal or per-profile?
2. If universal: deploy to both `addons/` dirs on bazzite, update both repo backup dirs, single commit
3. If per-profile: deploy to the one targeted profile, update only that backup dir
4. Always commit + push the matching repo backup file(s) so the on-disk state and version control match
