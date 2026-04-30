---
name: Nexus / addon multi-deploy rules
description: Hard rules for deploying GW2 addon binaries and configs across THREE profiles on bazzite (Steam GW2 = Local, ~/Games/gw2-appletv/ = Apple TV stream, ~/Games/gw2-deck/ = Steam Deck stream). Symlinked files (Gw2.dat, bin64/, d3d11.dll) auto-propagate; the addons/ directory is COPIED per-profile and must be deployed manually to all three.
type: feedback
---

<!-- markdownlint-disable MD041 -->

## Three GW2 install dirs on bazzite, only some files are shared

Per `multi-gw2-installs.md`:

- **Symlinked across all three profiles** (single source of truth, patches flow through): `Gw2.dat`, `bin64/`, `d3d11.dll`
- **Copied** (independent per profile, must be deployed to each manually if change is universal): `Gw2-64.exe`, **the entire `addons/` directory** (Nexus, MysticClicker, MysticTrading, ArcDPS, etc.)

This means **any change to an `addons/` file does NOT automatically propagate** between profiles.

## Deploy targets

| Profile | Bazzite path | Repo backup path |
| --- | --- | --- |
| Local (Steam GW2 — desktop ultrawide + Steam Controller) | `~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/` | `configs/gw2-keybinds/local/` |
| Apple TV stream | `~/Games/gw2-appletv/addons/` | `configs/gw2-keybinds/apple-tv/` |
| Steam Deck stream | `~/Games/gw2-deck/addons/` | `configs/gw2-keybinds/deck/` |

## Hard rules

1. **Default to deploying to ALL THREE profiles**, then ask the user "should this be per-profile?". Most changes are universal (new keybind, addon update, ArcDPS config tweak) and should land in all three; only client-screen-specific changes (UI position for 1280×800 Deck vs 2K Apple TV vs ultrawide Local) should diverge per profile.

2. **When deploying a Nexus addon binary update** (Nexus.dll, ArcDPS.dll, or any other `*.dll` in `addons/`): deploy to **all three** `addons/` directories. Symlinking the binary IS an option for addons that have no per-profile state (consider it for stable utility addons).

3. **When deploying Nexus *settings* changes** (`InputBinds.json`, `GameBinds.xml`, `AddonConfig.json`, `Settings.json`):
   - Universal changes (new keybind, new addon config) → deploy to all three, update all three repo backup dirs
   - Per-profile changes (UI layout for ultrawide vs 2K vs Deck) → deploy to one, update only that profile's repo backup

4. **When deploying Mystic Clicker per-resolution configs** (`mystic-clicker-WIDTHxHEIGHT.cfg`): all three profiles have all per-resolution variants. Deploy any added/edited variant to all three `addons/MysticClicker/` directories.

5. **Always update the matching repo backup dir** whenever you deploy to a profile on bazzite. The repo at `configs/gw2-keybinds/{local,apple-tv,deck}/` is the source of truth — don't let bazzite drift from the repo.

6. **Never `cp -r` from one install dir to another** — clobbers the destination's diverged configs. Always file-by-file.

## Why this matters

User confirmed during the multi-install setup (2026-04-30): the whole point of multiple profiles is that **GW2 settings, Nexus UI positions, and Mystic Clicker click coordinates** can diverge per stream client (or local play) without manual re-editing every time they switch. If you blindly mirror files, that benefit is lost.

## How to apply

Before deploying any addon-related change:

1. Identify scope: universal or per-profile?
2. If universal: deploy to all three `addons/` dirs on bazzite, update all three repo backup dirs, single commit
3. If per-profile: deploy to the one targeted profile, update only that backup dir
4. Always commit + push the matching repo backup file(s) so the on-disk state and version control match
