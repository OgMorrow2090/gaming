---
name: Multi GW2 installs — Local + Apple TV + Steam Deck profiles
description: HISTORICAL — multi-install setup was consolidated to single Steam GW2 install on 2026-05-12. Kept as a reference for the wiring patterns (binary VDF, compat-tool mapping, prefix seeding) in case profiles ever get recreated.
type: project
---

<!-- markdownlint-disable MD041 -->

> **ARCHIVED 2026-05-12** — both non-Steam shortcuts (Apple TV `2879321470` and Steam Deck `3111887265`) were deleted in commit `feb5225` along with their Wine prefixes, addon dirs, and Sunshine entries (~1.2 GB freed). Reasons: streaming flow is now Apple-TV-only via the existing `GW2 - SteamOS` Sunshine app + main Steam GW2 (Steam Deck went native — see `/home/deck/...` install on the deck itself), and the dual-Proton tracking became more friction than the per-profile state isolation was worth. The repo backup dirs `configs/gw2-keybinds/{apple-tv,deck}/` are now historical snapshots. Everything below is the original setup memo, preserved as a recipe.

---

Created 2026-04-30. User wanted parallel GW2 settings (Nexus keybinds, Mystic Clicker resolution config, GFXSettings, addon UI positions) per stream client without manually re-editing GW2 every time. Started as dual (Steam GW2 = Deck, Apple TV non-Steam) and was extended to triple (Steam GW2 repurposed for local play, separate non-Steam shortcuts for Apple TV and Deck) so the Steam GW2 install can be tuned for ultrawide local play with Steam Controller without affecting either stream profile.

## Layout

| Path | Type | Profile | App ID |
| --- | --- | --- | --- |
| `~/.local/share/Steam/steamapps/common/Guild Wars 2/` | Steam-managed | **Local** (desktop ultrawide + Steam Controller) | `1284210` |
| `~/Games/gw2-appletv/` | Manual | **Apple TV** (Moonlight stream, 2K + 144 DPI) | `2879321470` |
| `~/Games/gw2-deck/` | Manual | **Steam Deck** (Moonlight stream, 1280×800 + 96 DPI) | `3111887265` |

### Shared content (symlinked from each `~/Games/*/` dir to the Steam install)

| File | Type | Why |
| --- | --- | --- |
| `Gw2.dat` (81 GB) | symlink | Content patches flow through automatically |
| `bin64/` | symlink | Steam-patched binaries flow through |
| `d3d11.dll` (7.6 MB) | symlink | Nexus addon loader; Nexus updates flow through |
| `Gw2-64.exe` (43 MB) | **copy** | Separate binary identity; `sync-gw2-exe.sh` re-copies after Steam patches |
| `addons/` (~150 MB) | **independent copy** | Diverges per profile (Nexus, MysticClicker, ArcDPS, etc.) |

### Per-profile state (always isolated)

| Path | Purpose |
| --- | --- |
| `~/.local/share/Steam/steamapps/compatdata/1284210/pfx/` | Local profile Wine prefix (GFXSettings + user.reg) |
| `~/.local/share/Steam/steamapps/compatdata/2879321470/pfx/` | Apple TV profile Wine prefix |
| `~/.local/share/Steam/steamapps/compatdata/3111887265/pfx/` | Steam Deck profile Wine prefix |

Net unique disk cost per non-Steam profile: ~200 MB (43 MB exe + ~150 MB addons). Gw2.dat (81 GB) and bin64 (204 MB) stay single-source via symlink across all three.

## How patches flow

- **Gw2.dat content patches** — Steam writes; symlinks resolve at access time → all profiles read new content. **No action needed.**
- **bin64/ updates** — flow through symlinks. **No action needed.**
- **d3d11.dll (Nexus updates)** — flow through symlinks. **No action needed.**
- **Gw2-64.exe launcher patches** — copy is independent per profile; Steam only patches its own. Run `scripts/sync-gw2-exe.sh` to re-copy to **both** non-Steam installs. The script handles only the Apple TV install currently — extend it to also cover `~/Games/gw2-deck/Gw2-64.exe` if you patch the launcher.

## Sunshine flow

The existing **"GW2 - SteamOS"** Sunshine app handles all three. Moonlight from any client lands the user in Big Picture; the user picks the matching profile from the library:

- **Local play** (no Moonlight) → launch **"Guild Wars 2"** directly from desktop / Big Picture
- **Apple TV stream** → "GW2 - SteamOS" → pick **"Guild Wars 2 (Apple TV)"** in Big Picture
- **Steam Deck stream** → "GW2 - SteamOS" → pick **"Guild Wars 2 (Steam Deck)"** in Big Picture

The prep-cmd (`sunshine-set-resolution.sh`) auto-swaps Wine LogPixels per client width on the **two stream profiles only** (Apple TV + Deck). The local profile's prefix is **never touched by prep-cmd** so user can configure it independently for ultrawide.

## Reversibility

If the multi-install causes problems:

1. Delete `~/Games/gw2-appletv/` and/or `~/Games/gw2-deck/` (no harm to Steam install — symlinks are inert)
2. Remove the non-Steam shortcuts from Steam library (or revert `shortcuts.vdf` from `.bak`)
3. Revert `config.vdf` `CompatToolMapping` entries from `.bak`

The Steam install at `~/.local/share/Steam/steamapps/common/Guild Wars 2/` is never modified by this setup.

## Critical rules for future sessions

- **Never `cp -r` between profile dirs** — clobbers diverged Nexus configs, Mystic Clicker positions, etc. Always file-by-file.
- **Never replace symlinks with copies** — breaks patch flow for Gw2.dat / bin64/ / d3d11.dll, doubles disk use, creates a sync chore.
- **Symlinks must stay relative-resolvable**: if the Steam install moves (different library), update the four symlinks per non-Steam install: `Gw2.dat`, `bin64`, `d3d11.dll`.
- **Account login is single-instance**: ArenaNet enforces one login per account; second launch boots the first. Never run two profiles simultaneously.
- **Local profile's Wine prefix is user-managed**: prep-cmd does NOT modify `compatdata/1284210/pfx/user.reg`. If you tune Wine DPI for local ultrawide play, it stays — prep-cmd won't overwrite.

## How non-Steam shortcuts were wired (2026-04-30)

Done via SSH binary VDF editing. Kill all Steam helpers (`steamwebhelper`, `steam-runtime-launcher-service`, `pressure-vessel`), then run:

```bash
python3 scripts/add-gw2-shortcut.py \
    --name "Guild Wars 2 (Steam Deck)" \
    --exe /var/home/Og/Games/gw2-deck/Gw2-64.exe \
    --start-dir /var/home/Og/Games/gw2-deck/ \
    --proton proton_11
```

The script parses `~/.local/share/Steam/userdata/64793831/config/shortcuts.vdf`, computes a stable appid via `crc32(exe + name) | 0x80000000`, appends new entry, updates `config.vdf` `CompatToolMapping` for that appid → desired Proton tool. Idempotent (refuses to add duplicate name). Same script can wire any future profile.

### Critical VDF format gotcha

Steam's binary VDF requires a **trailing `0x08` byte at file end** beyond the closing brace of the outermost section. The original `shortcuts.vdf` ends with `08 08 08`:

- `0x08` close `tags` (innermost empty section)
- `0x08` close last shortcut entry
- `0x08` close-of-root marker — **this is what Steam checks for**

If you parse + re-emit without the final `0x08`, Steam logs `CSteamDoc::LoadShortcuts: failed to load shortcut file` and **deletes** the file. Always append `b"\x08"` after `emit_vdf_binary(parsed)`.

### The shortcuts.vdf is sometimes malformed (not our fault)

bazzite's `shortcuts.vdf` has the `Reboot` shortcut as a **separate root-level section keyed `"1"`** instead of nested inside `shortcuts`. Probably from an earlier external edit by a different tool. Our parser handles this correctly (treats root as a dict of multiple top-level sections). Do not "fix" the structure — Steam tolerates it.

### Files missed in initial install + post-first-launch fixups

**Initial dir creation missed `d3d11.dll`** — Nexus's loader DLL lives at the install root (NOT in `bin64/`). Without it, Nexus addons silently fail to load (no overlay, no addon menu). Fixed for both non-Steam installs by symlinking like `Gw2.dat`/`bin64/`. Future installs: include `d3d11.dll` in the symlink set.

**Fresh Wine prefix has no `[Control Panel\\Desktop]` LogPixels** — Proton auto-creates `[Software\\Wine\\Fonts]` LogPixels (Wine internal font DPI), but the first-launch prefix has no `[Control Panel\\Desktop]` LogPixels (Windows-emulated user DPI — the one GW2 reads). The prep-cmd's sed-replace was a no-op. Fixed: `update_wine_dpi_one()` does section-scoped detection and inserts the LogPixels line if absent. **Critical**: the existence check MUST be section-scoped (`awk '/^\[Control Panel..Desktop\] /,/^$/'`) — checking the whole file matches the unrelated `[Software\\Wine\\Fonts]` LogPixels and bypasses the insert.

**GFXSettings.xml in fresh prefix is default** — first-launch creates a default GFXSettings.xml. Seed it from another profile if you want to start from a known-good baseline:

```bash
cp ~/.local/share/Steam/steamapps/compatdata/1284210/pfx/drive_c/users/steamuser/AppData/Roaming/Guild\ Wars\ 2/GFXSettings.Gw2-64.exe.xml \
   ~/.local/share/Steam/steamapps/compatdata/3111887265/pfx/drive_c/users/steamuser/AppData/Roaming/Guild\ Wars\ 2/GFXSettings.Gw2-64.exe.xml
```

### prep-cmd updates the two STREAM prefixes (not local)

`scripts/sunshine-set-resolution.sh::update_wine_dpi()` iterates over:

- `GW2_APPID_APPLETV=2879321470`
- `GW2_APPID_DECK=3111887265`

`GW2_APPID_LOCAL=1284210` is documented in the script as a constant but **NOT iterated** — local profile's prefix is left for the user to manage manually. The prep-cmd runs before the user picks a profile in Big Picture, so we set DPI on both stream prefixes; whichever they pick has the right DPI for the active client. **If you ever change a non-Steam shortcut's exe path or name, the appid changes — update the matching constant in the script.**

### Computed appid is stable

- Apple TV: `crc32('"/var/home/Og/Games/gw2-appletv/Gw2-64.exe"' + 'Guild Wars 2 (Apple TV)') | 0x80000000 = 2879321470 (0xab9ef57e)`
- Steam Deck: `crc32('"/var/home/Og/Games/gw2-deck/Gw2-64.exe"' + 'Guild Wars 2 (Steam Deck)') | 0x80000000 = 3111887265 (0xb97ba1a1)`

As long as the exe path and name don't change, the appid stays stable, so `CompatToolMapping` and `compatdata/<appid>/` paths remain valid.

## Seeding a new profile from a baseline (verified working 2026-04-30)

When the user wants a new profile to start with the same GW2 + Nexus state as an existing profile (rather than tuning from scratch), pre-seed the new prefix BEFORE first launch:

1. **Nexus addon configs** — already present in the cloned `addons/` dir (we copy this at install creation). If the source profile's configs have drifted since the clone, sync individual files: `InputBinds.json`, `GameBinds.xml`, `AddonConfig.json`, `Settings.json`. NEVER `cp -r addons/` (clobbers other diverged state).

1. **GFXSettings.xml** — copy from source profile's Wine prefix into the new prefix path, creating the directory tree if it doesn't exist yet:

```bash
SRC=/home/Og/.local/share/Steam/steamapps/compatdata/<src-appid>/pfx/drive_c/users/steamuser/AppData/Roaming/Guild\ Wars\ 2
DST=/home/Og/.local/share/Steam/steamapps/compatdata/<dst-appid>/pfx/drive_c/users/steamuser/AppData/Roaming/Guild\ Wars\ 2
mkdir -p "$DST"
cp -f "$SRC/GFXSettings.Gw2-64.exe.xml" "$DST/"
```

Wine's `wineboot` (run on first Steam launch) creates standard prefix scaffolding (drive_c, system DLLs, etc.) but does NOT overwrite user files in `AppData/Roaming/`. Pre-seeded files survive prefix init.

1. **Local.dat** — copy if you want to skip ArenaNet login on first launch (token usually valid ~30 days). Same `cp` pattern. Optional. If the token is expired by launch time, GW2 falls back to login prompt — no harm.

After pre-seeding + first launch:

- GW2 reads seeded GFXSettings → renders with baseline-tuned settings
- Login skipped if Local.dat is fresh
- Nexus loads (d3d11.dll symlinked, addons/Nexus configs identical to source)
- prep-cmd's Wine DPI swap will set the right `[Control Panel\\Desktop]` LogPixels on next stream connect

User confirmed this workflow worked end-to-end for the Steam Deck profile (cloned from Steam GW2 baseline at 2026-04-30 19:24 install creation, then GFXSettings + Local.dat pre-seeded into compatdata/3111887265/pfx/.../Roaming/Guild Wars 2/ at 19:42).

## Universal change → deploy to all three

When the user makes a change to one profile that's meant to be universal (a new Nexus addon DLL, a fixed keybind, an updated ArcDPS config, a tweaked Mystic Clicker per-resolution config), **default to deploying to all three profile install dirs** AND syncing all three repo backup dirs in `configs/gw2-keybinds/{local,apple-tv,deck}/`. See `nexus-multi-deploy-rules.md` for the full rule. Only diverge per-profile when the change is explicitly screen-size or controller-layout specific (e.g., addon UI position for ultrawide vs Deck).

## Repo backup strategy

Three per-profile snapshot directories are the source of truth — sync any change to the matching profile on bazzite to its repo dir:

- `configs/gw2-keybinds/local/` ↔ `~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/Nexus/` (Steam GW2 install)
- `configs/gw2-keybinds/apple-tv/` ↔ `~/Games/gw2-appletv/addons/Nexus/`
- `configs/gw2-keybinds/deck/` ↔ `~/Games/gw2-deck/addons/Nexus/`

Files backed up per profile: `InputBinds.json`, `GameBinds.xml`, `AddonConfig.json`, `Settings.json`. Mystic Clicker `.cfg` files live in `addons/MysticClicker/` per profile (currently shared per-resolution variants; will diverge if user tunes click coordinates per-profile).
