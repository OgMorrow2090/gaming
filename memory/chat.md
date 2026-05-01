# Session Log

Append-only log of agent sessions on this repo. Read at session start for continuity. Entries are summaries — not full transcripts.

## 2026-04-29 (PM) — Per-mode resolution switching, gamescope xwayland constraint, GW2 - SteamOS hybrid app

Long second-half session continuing morning Sunshine work. User wanted per-stream-client resolution switching (Deck → 1280x800, Apple TV → higher) with GW2 settings auto-adapted, plus a clean way to access Big Picture mid-session to change res.

### Major outcome — gamescope's nested xwayland resists size-locking

Hours of investigation into why GW2 always rendered at 2K on the Deck stream regardless of gamescope `-W -H`:

- **`-W/-H` controls the gamescope output canvas; `-w/-h` is supposed to set the nested xwayland init size.** Without `-w/-h` xwayland defaults to the EDID's largest mode.
- **Even with `-w 1280 -h 800` the xwayland gets bumped up to 2560x1440 once Steam/Wine start.** Steam logs literally say `Desktop state changed: desktop: size: 2560,1440`. Steam ignores gamescope's nested-size flags.
- **`xrandr --output gamescope --mode 1280x800` from outside returns `BadMatch`** — gamescope's nested xwayland refuses post-init RRSetScreenSize. Even attempting `--newmode/--addmode` fails on the nested compositor.
- **`--force-windows-fullscreen` only forces window draw size, not xwayland reported size** — Steam keeps redrawing at 2K, conflicting with gamescope's clamping → visible flicker. Useless for our case.
- **`--generate-drm-mode fixed` doesn't force the connector to deck mode either** — gamescope still picks the highest CVT-derivable mode (2560x1440@144) over EDID DTDs.

Net result: with the EVanlak HDMI dongle EDID + NVIDIA proprietary driver, **gamescope's nested xwayland is hardlocked at 2K**. GW2 in `windowed_fullscreen` always renders at 2K canvas. Reverted all these flags (added then removed in same session).

### Why morning auto-detect "worked fine" — `dpiScaling=true` was the secret sauce

User said the AM session worked but PM testing didn't. Diff showed the PM script forced `dpiScaling=false`. With dpiScaling=true (GW2 default), GW2 honours Wine LogPixels (96/144/192 per gamescope mode) and **scales UI proportionally** to the Wine-reported DPI. After Sunshine downscales the 2K-rendered stream to 1280x800 for the Deck, the per-mode UI scaling lands correctly — UI on Deck looks Deck-sized despite the underlying 2K render. Forcing dpiScaling=false killed that compensation. Reverted.

### Final Sunshine app layout

Stripped per-launch GW2 mode flips because they fought Steam Big Picture and GW2 in unstable ways. New design:

- **SteamOS Game Mode** — auto-detect prep-cmd; lands in Big Picture; default controller layout. Use this to set the gamescope mode for the session.
- **GW2 - SteamOS** (NEW) — auto-detect prep-cmd + `sunshine-wait-gw2.sh` cmd that waits up to 30 min for user to launch GW2 from Big Picture, then waits for GW2 to exit, then returns. Steam Input maps the AppName "GW2 - SteamOS" to the GW2 controller layout. Workflow: connect → Big Picture (change res if needed) → launch GW2 → play → Alt+F4 → stream auto-ends.
- **Guild Wars 2** — direct-launch via `sunshine-launch-gw2.sh`. No prep-cmd. Use when you already have the right gamescope mode and want straight-to-game.
- **Active Session** — no prep-cmd, no cmd, just attaches.
- **Kill Game** / **Reboot** — utility apps unchanged.

The script `sunshine-set-resolution.sh` still does the auto-detect + Wine LogPixels swap + GFXSettings RESOLUTION swap + verticalSync=false + SIGKILL pre-edit, but is only wired up to SteamOS Game Mode and GW2 - SteamOS now.

### Steam Deck side: matched shortcut + controller layout for the new app

- Added `Moonlight - GW2 SteamOS` to `shortcuts.vdf` via custom binary VDF appender (no `vdf` Python module on SteamOS) — appid 3161606423
- Removed redundant `Moonlight - GW2` shortcut (was the direct-launch path before this session's split)
- Force-killed Steam (`pkill -9`) before editing so it didn't overwrite shortcuts.vdf with its in-memory copy on shutdown — Steam writes this file on clean shutdown, will clobber external edits if running. **Critical pattern for any future shortcut edits.**
- Added `"moonlight - gw2 steamos"` entry in `configset_controller_neptune.vdf` pointing at `template CLOUD_moonlight/og v18.4.8_0` so the new shortcut applies the GW2 controller layout
- Copied GW2 VoE artwork (4 grid PNG variants) from old appid `2959110740` to new appid `3161606423`. Removed the orphaned old artwork files.
- Removed the now-orphaned `"moonlight - gw2"` configset entry

### Bazzite side cleanup

- Removed unused `gw2-2k.png` / `gw2-4k.png` covers (per-mode app icons that were briefly added then removed)
- Live `~/.config/sunshine/apps.json` matches `configs/bazzite/sunshine-apps.json` ✓

### Files changed this session

- `configs/bazzite/sunshine-apps.json` — final 6-app layout
- `configs/bazzite/gamescope-wrapper.sh` — back to simple `-W -H -r -o` (added then removed `-w/-h`, `--force-windows-fullscreen`, `--generate-drm-mode fixed`)
- `scripts/sunshine-set-resolution.sh` — Apple TV → 2k120 (was 4k120), SIGKILL GW2 before edits, optional MODE arg via `$1` (kept), per-mode Wine LogPixels (kept), per-mode RESOLUTION (kept), verticalSync=false (kept). Removed: forced screenMode, forced dpiScaling.
- `scripts/sunshine-wait-gw2.sh` (NEW) — Sunshine cmd that waits for GW2 to start (30m grace), then waits for it to exit, then returns
- `configs/steam-deck/configset_controller_neptune.vdf` (NEW backup) — current state of the layout pointer table
- `configs/steam-deck/moonlight-shortcuts-summary.txt` (NEW backup) — human-readable summary of the 5 Moonlight shortcuts and their Sunshine app targets

### Apple TV side — pending

User mentioned wanting the same dual-app pattern on Apple TV (Big Picture + GW2 - SteamOS hybrid). Apple TV uses different controller hardware so the layout pointer file is `configset_controller_xbox.vdf` (or whichever matches the connected MFi controller). The Sunshine apps are already there; just need the controller pointer mapping next session.

### New memory entries (PM session)

- `gamescope-nested-xwayland-constraint.md` — gamescope nested xwayland defaults to EDID max and resists post-init resize
- `gw2-windowed-fullscreen-dpiscaling-trick.md` — keep `dpiScaling=true` so per-mode Wine LogPixels does the UI scaling

## 2026-04-29 — Moonlight shortcut split + configset pointer fix

Cross-repo session driven from `itinyk-app` working dir. User rebuilt the Steam Deck library to have multiple Moonlight non-Steam shortcuts (Moonlight - GW2, Moonlight - SteamOS, Moonlight - Reboot Bazzite, Moonlight - Kill Game) so each could carry its own Steam Input layout. The rename of the original "Moonlight" shortcut to "Moonlight - GW2" broke v18.4.8 controller bindings — Steam appeared to revert to v18.4.2 even though `og v18.4.8_0.vdf` was deployed correctly.

### Major outcome — configset_controller_neptune.vdf is the actual pointer

After hours of editing autosave files, Personal cloud entries, ugcmsgcache, and the per-appid dirs, the bug was finally traced to `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/configset_controller_neptune.vdf`. This file maps each app/shortcut (by AppName lowercased) to its layout source — `template` mode points at a local SSH-editable VDF; `workshop` mode points at an immutable Workshop UGC item.

When the rename happened, Steam auto-created a new `"moonlight - gw2"` entry pointing at a default Workshop UGC (which happened to be the v18.4.2 (v18.3 base) item). The old `"moonlight"` entry still correctly pointed at `CLOUD_moonlight/og v18.4.8_0`. Fix: rewrote the new entry to use the same `template` pointer.

Documented in `memory/configset-controller-pointer.md`. Future controller deploys must verify (and fix) configset entries after any shortcut rename.

### Library / Sunshine work that DID succeed

- **Steam Deck shortcuts.vdf rewritten**: programmatically renamed slot 0 ("Moonlight" → "Moonlight - GW2") preserving its appid; added 3 new shortcuts (SteamOS, Reboot Bazzite, Kill Game) each with stable md5-derived appids
- **Cover artwork** placed in `userdata/<id>/config/grid/` for all 4 Moonlight entries (reused Sunshine's existing covers from Bazzite)
- **Each shortcut** uses `flatpak run com.moonlight_stream.Moonlight stream 172.16.100.212 "<Sunshine app name>" --quit-after` so they auto-launch the right Sunshine app
- **Bazzite Flatpak system updates** applied (Mesa 25.3.5, Chrome 147, Firefox 150, runtime updates)

### Outstanding

- **EVanlak EDID dummy plug** purchased to fix Bazzite's 1280×800 virtual display (no real monitor connected; GW2 renders into a tiny canvas during Moonlight stream). Will plug in next session. Custom Deck/Apple TV resolutions still work because Sunshine renders at client-requested res; the EDID just provides the "valid resolutions" envelope.
- **Resolution-based GW2 profile swap** (Deck vs Apple TV → different GFXSettings + Nexus DPI) deferred until EDID plug installed.
- **Bazzite-deck-nvidia is on F43 stable**; F44 deck images lag desktop by a few days. Auto-update will pick up F44 once Universal Blue ships it.

### Files heavily edited (Deck only this session)

- `~/.local/share/Steam/userdata/64793831/config/shortcuts.vdf` — added 3 shortcuts, renamed 1 (preserved appid)
- `~/.local/share/Steam/userdata/64793831/config/grid/` — 8 PNG cover files placed
- `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/configset_controller_neptune.vdf` — added `"moonlight - gw2"` `template` entry (the actual fix)
- `Steam Controller Configs/.../config/2959110740/`, `.../1284210/`, `.../moonlight/` controller_neptune.vdf — overwrites that turned out to be no-ops (configset was the real pointer)

### New memory entries

- `configset-controller-pointer.md` — the appname→layout pointer file and why renaming shortcuts breaks SSH deploys

## 2026-04-26 — Mystic Clicker v3.6.0 → v3.6.8 + Controller v18.4 → v18.4.8

Long session driven by user-reported bugs after v18.4 was promoted from broken Python-regex VDF rewrite to working layout. Eight DLL releases (3.6.0 → 3.6.8) and eight controller revisions (v18.4.0 → v18.4.8) in a single afternoon. Each fix surfaced the next deeper bug.

### Major outcomes

- **v18.4 outage recovery**: a prior session used Python regex to rewrite `dpad_south`/`dpad_east` blocks in the template VDF; the regex consumed Long_Press/Double_Press activator subgroups across multiple groups and the file went 63 121 → 37 170 bytes. Steam Deck silently dropped the layout (Personal tab empty). Recovered by starting from the v18.3 base and applying chord changes via line-anchored Edit calls only. Documented in `docs/vdf-editing-golden-rules.md` (10 rules + recovery playbook).
- **Renamed** `configs/steam-controller/moonlight-gw2-og-v16.1.vdf` → `moonlight-gw2-og-template.vdf`. The `v16.1` in the filename had been misleading since v17.x.
- **New chord collisions discovered and fixed:**
  - `Ctrl+F11` for Merchant collided with `KB_CRAFTY_TOGGLE` in Nexus InputBinds → moved to `Alt+F11`.
  - `Ctrl+Shift+Home` for Reset Windows fired GW2's `SkillProfession5` (user has Home bound to that skill in `GameBinds.xml`; GW2 ignores chord modifiers) → moved to `Ctrl+Shift+Insert`. Insert is unbound across all of the user's GW2 keybinds.
- **Two Steam Input quirks identified and worked around in DLL:**
  - Modifier-hold via uinput (Shift/Alt asserted for the entire physical chord hold) → replaced fixed `Sleep(500)` defers with `WaitForChordModifiersRelease()` poll on `GetAsyncKeyState`. Adapts to actual hold time.
  - Full_Press chord output double-emitted at OS level on `dpad_south` (50–80 ms apart) → Friend chord (and similar Full_Press inventory combos sharing a dpad block with Long_Press / Double_Press) must omit `key_press I` from VDF and let DLL synthesize it via SendInput exactly once per dispatch.
- **Per-combo debounce in `keybinds.cpp`** (300 ms window) catches the second Nexus dispatch when Steam Input double-emits a chord. Each suppressed call logs `Debounced duplicate dispatch of <combo> (<delta>ms since last)` at DEBUG.
- **Window rescue updated**: clamps to 70% of viewport (was display-size only) so oversized windows actually shrink to a usable size on the Deck. Also logs every press regardless of whether anything was rescued.

### Files heavily edited

- `configs/steam-controller/moonlight-gw2-og-template.vdf` — chord reorganization, version bumps, double_tap_time settings, multiple chord-key changes
- `configs/gw2-keybinds/nexus-inputbinds.json` — `MERCHANT_COMBO` Code 87 + Alt; `RESET_WINDOWS` / `MT_RESET_WINDOWS` Code 71 → 45
- `modules/mystic-clicker/input-sim.cpp` — `WaitForChordModifiersRelease` helper; converted Friend / Wizard combos / Merchant to detached-thread + DLL-press-`I` pattern
- `modules/mystic-clicker/keybinds.cpp` — per-identifier 300 ms debounce
- `modules/mystic-clicker/rescue.cpp` — 70% viewport clamp + always-log
- `modules/mystic-clicker/entry.cpp` — version bumps to 3.6.8, RESET_WINDOWS default → CTRL+SHIFT+INSERT
- `docs/vdf-editing-golden-rules.md` — 10 + 1 rules; new Rule #11 covers chord-collision check via Python snippet
- `CHANGELOG.md` — v3.6.2 through v3.6.8 entries
- New memory entries: `chord-emission-quirks.md`, `gw2-keybind-collisions.md`

### Infrastructure changes

- Pushed `nexus-inputbinds.json` updates live to Bazzite via SSH (Mystic Clicker's `MERCHANT_COMBO` and `RESET_WINDOWS` bindings). User must do a full GW2 restart (not just Nexus reload) for InputBinds.json changes to take effect — Nexus reads the file only on startup.
- Deployed VDFs v18.4.3 → v18.4.8 to Steam Deck via SSH overwrite of the Personal save (`url=usercloud://moonlight/og v18.4.N_0`). Each version under a new filename so the previous Personal stays available as a rollback.
- Pulled Mystic Clicker DLL v3.6.5 → v3.6.8 from GitHub Actions artifact and `scp`'d to Bazzite addons dir.

### Open items / next session

- Trading Post and Bank chords on `dpad_east` (`I+F7` / `I+F8`) likely have the same Steam Input Full_Press double-emit bug as Friend did, but the user reports they work — possibly because the second `I` toggle happens within the same frame. If they ever exhibit the open-then-close symptom, convert to the same DLL-press-`I` pattern as Friend.
- The user must do a full GW2 restart on Bazzite to pick up the `Ctrl+Shift+Insert` binding for Reset Windows. Until then, wheel slot 2 emits the new chord but Nexus's in-memory binding is still the old `Ctrl+Shift+Home`.
- Window rescue 70% viewport clamp + always-log are in v3.6.8 but were never logged because the chord wasn't dispatching (per above). Verify after user's restart.
- The `<button>"36"`/`35`/`34`/`33` skill bindings in user's GameBinds.xml are documented in memory but worth re-checking if user re-imports a fresh template.

## 2026-05-01 — Controller v18.4.9 + ESC-close on MC capture & MT delivery

Driven by user reports: "I don't see new controller build on steamdeck" and "add ESC to close capture window".

### Major outcomes

- **Controller v18.4.8 → v18.4.9**: L1 + D-Pad Up Tap = MT Fliplist (Alt+D, "Flip"). MT Delivery dropped from this slot. Long-Press = MT Dashboard unchanged. Double_Press removed entirely. Validation: 298 → 296 bindings, brackets balanced 924/924.
- **Architecture lesson recorded** at `memory/streaming-input-host-vs-client.md`: for the Deck stream, Steam Input runs on the **Deck** (translates physical input to keys *before* Moonlight transmits) — bazzite never sees a gamepad. Apple TV stream is the opposite (raw HID over Moonlight tvOS app → bazzite-side Steam Input applies). My initial v18.4.9 deploy went only to bazzite per-appname CLOUD subdirs, which the Deck-side stream never reads. Fix: also SCP'd to `deck@172.16.100.95:.../moonlight/og v18.4.9_0.vdf` and bumped Deck's `configset_controller_neptune.vdf` "moonlight" + "moonlight - gw2 steamos" entries.
- **MC v3.6.9 → v3.6.10**: capture window now closes on ESC via `GUI_RegisterCloseOnEscape("Mystic Clicker - Capture", &g_ShowCaptureWindow)`; symmetric Deregister in unload.
- **MT 20260420 → 20260501**: Delivery Box now closes on ESC. Previously Dashboard + FlipColumn registered but Delivery Box was an explicit `// (but NOT delivery box)` gap. All three MT windows now consistent.
- **GW2 UI persistence model corrected**: chat box, minimap layout, hero panel are **server-side / account level**, NOT in `Local.dat` as I initially claimed. What stays per-profile: `GFXSettings.Gw2-64.exe.xml`, Nexus addon configs, Wine reg (DPI + time format). Inventory state (compact toggle, sort order) is also per-install. Documented in conversation only — no memory file because the test was empirical.

### Files

- `configs/steam-controller/moonlight-gw2-og-template.vdf` — title v18.4.9, url, drop Double_Press, swap Full_Press F→D (line-anchored Edits, no regex)
- `configs/steam-controller/moonlight-gw2-og-v18.4.9.vdf` — new snapshot
- `configs/steam-deck/configset_controller_neptune.vdf` — bumped to v18.4.9, added `guild wars 2 (apple tv)` and `(steam deck)` entries
- `modules/mystic-clicker/entry.cpp` — Build 9 → 10, Register/Deregister CloseOnEscape
- `modules/mystic-trading/entry.cpp` — Build 20260420 → 20260501, Delivery Box ESC register
- `CHANGELOG.md` — v3.6.10 entry
- `memory/streaming-input-host-vs-client.md` (new) + `memory/MEMORY.md` (index entry)

### Infrastructure changes

- **Bazzite (Og@172.16.100.212)**: configset rewritten with per-appname `template` entries (moonlight, guild wars 2 (apple tv), guild wars 2 (steam deck)); new CLOUD subdirs created for both new GW2 profiles; v18.4.9 VDF deployed in each; Steam fully restarted (kill steamwebhelper + parent + bust htmlcache); both DLLs deployed to all three profile addons dirs (`Steam install`, `gw2-appletv`, `gw2-deck`) with `.bak-20260501-092609` rotation.
- **Steam Deck (deck@172.16.100.95)**: v18.4.9 SCP'd to `moonlight/og v18.4.9_0.vdf`; configset bumped (`moonlight` + `moonlight - gw2 steamos`); Steam respawned via steamwebhelper + parent kill + htmlcache bust.

### Open items / next session

- Mystic Clicker Bouncy Meta Progress capture position (added in v3.6.9 prior session) still needs to be set in-game per profile.
- 12-hour clock fix on Apple TV + Steam Deck profiles: applied via Wine reg `iTime`/`sShortTime`/`sTimeFormat` patch earlier today; user said "will need test after 12 as AM moment so will work either way". Verify after noon.
- Build "Bouncy Meta Progress" capture coordinates per profile or copy `.cfg` files between profiles (positions are resolution-dependent).

## 2026-05-01 (evening) — Controller v19.0/v19.1 + Deck profile InputBinds catastrophic-drift recovery (4+ hour debug)

Session started with user reporting auto-sort inventory (DPad-Right hold) was firing Community LFG instead. What followed was a **dual-failure-mode** debugging session that took ~4 hours to resolve. Documented in detail at [nexus-inputbinds-per-profile-drift.md](nexus-inputbinds-per-profile-drift.md) Dual-failure section.

### Root cause (of the symptom)

Two independent failures stacked:

1. **Controller layout side**: Steam Input UI auto-saved a local copy of `controller_neptune.vdf` with `Ctrl+C` on `dpad_east` Long_Press instead of `Ctrl+Q`. The file path was `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight - gw2 steamos/controller_neptune.vdf` with `description: "#SettingsController_AutosaveDescription"` (Steam Input's autosave marker). User firmly denies making this edit; mechanism likely a Steam Input UI side-effect of opening the layout editor or a sync/merge event. **This broke both Apple TV and Deck streams** because Apple TV uses Deck-as-controller per [streaming-input-host-vs-client.md](streaming-input-host-vs-client.md) (older note about bazzite-side Steam Input governing Apple TV is wrong for this setup).

2. **Nexus side**: Deck profile's `InputBinds.json` had **23 entries** drifted vs canonical (Apple TV / repo source-of-truth). Many bindings cleared (`Code=0`), several mismapped. Cause of the magnitude unknown — see open-bug section in nexus-inputbinds-per-profile-drift.md for ruled-out theories.

Surgical fixes (only `DEPOSIT_AND_SORT` + `0xFC56DD9F_ToggleLFG`) didn't converge because each only addressed one of the two failures. The breakthrough was recognizing the dual-failure mode and:

1. Replacing controller layout with clean canonical (deployed v19.0, then v19.1 with `Double_Press` removed from `dpad_east`)
2. Wholesale-copying Apple TV profile's clean `InputBinds.json` over the Deck profile's drifted state

After both layers fixed: auto-sort works, Trading Post works, Bank works, Mystic Forge works, all chord bindings restored.

### Major outcomes

- **Controller v18.4.9 → v19.0 → v19.1**: title bumps to make active layout visible in Steam Input UI; `Double_Press` `S+U+M` removed from `dpad_east` (was dead code — three keys aren't a usable GW2 chord). Validation: 296 → 293 bindings.
- **bazzite Sunshine recovery**: a separate Sunshine-can't-find-display issue surfaced when SDDM was restarted. Root cause: bazzite's immutable `/usr` (composefs) doesn't allow `setcap` on `/usr/bin/sunshine`, AND `Og` was not in `video`/`render` groups (system groups live in read-only `/usr/lib/group`; user-side memberships need to be added to `/etc/group`). Fix: appended `video:x:39:Og` and `render:x:105:Og` to `/etc/group`, then `sudo systemctl restart sddm` to refresh the user session's group membership. Sunshine can now access `/dev/dri/card0`.
- **gw2-deck native install identified on the Deck** at `/home/deck/.local/share/Steam/steamapps/common/Guild Wars 2/` (78 GB, appid 1284210). Used as a streaming-isolation test environment by copying the Apple TV InputBinds + v19.1 controller layout there too.
- **Memory honest correction**: my initial memory entry attributing the 23-binding drift to "passive collision-resolution" was over-reach. Updated to acknowledge cause-unknown with theories tested.

### Files

- `configs/steam-controller/moonlight-gw2-og-template.vdf` — title v19.1, Double_Press removed from dpad_east, line-anchored Edits per VDF rules
- `configs/steam-controller/moonlight-gw2-og-v19.0.vdf` — snapshot
- `configs/steam-controller/moonlight-gw2-og-v19.1.vdf` — snapshot (current canonical)
- `memory/nexus-inputbinds-per-profile-drift.md` — added Dual-failure mode section, Open bug section (cause unknown), wholesale-vs-surgical decision rule, capture-diagnostics recipe for next time

### Infrastructure changes

- **bazzite (Og@172.16.100.212)**:
  - `/etc/group`: appended `video:x:39:Og` and `render:x:105:Og`
  - `sudo systemctl restart sddm` (to refresh user session group membership for Sunshine)
  - `Documents/Guild Wars 2/nexus-configs/backup-2026-04-30-19-45.zip` was deleted by Addon_Config_Backup_Tool's auto-rotation (kept `backup-2026-04-30-20-27.zip`)
  - `~/Games/gw2-deck/addons/Nexus/InputBinds.json` ← wholesale-copied from `~/Games/gw2-appletv/addons/Nexus/InputBinds.json` (canonical state); backup at `InputBinds.json.bak-pre-applewipe-20260501-215910`
  - `~/Games/gw2-deck/addons/Nexus/InputBinds.json.bak-20260501-193659` — earlier surgical-restore backup (before wholesale)
  - `~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/Nexus/InputBinds.json` ← also wholesale-copied from Apple TV
  - `~/.local/share/Steam/userdata/64793831/config/configset_controller_neptune.vdf` — restored from repo (was missing entirely on bazzite; cause unknown)
  - `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight/og v18.4.9_0.vdf` — added (was missing); also v19.0 + v19.1 deployed to all per-appname CLOUD subdirs
- **Steam Deck (deck@172.16.100.95)**:
  - `~/.local/share/Steam/userdata/64793831/config/configset_controller_neptune.vdf` — restored from repo (was missing)
  - `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight - gw2 steamos/controller_neptune.vdf` — overwritten with v19.0 then v19.1; backup at `.bak-locally-edited-20260501-202007` (the Ctrl+C autosave that started this whole mess)
  - `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/1284210/controller_neptune.vdf` — overwritten with v19.1 for native Deck GW2 testing; backup at `.bak-pre-test-*`
  - `~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/Nexus/InputBinds.json` ← wholesale-copied from Apple TV (Deck native install)
  - Multiple `pkill steam.sh` cycles to force config reloads

### Open bugs / unresolved

- **🐛 Cause of 23-binding drift on Deck profile InputBinds is unknown.** Theories ruled out: passive Nexus drift, OneDrive sync, Steam Cloud sync, Addon_Config_Backup_Tool restore. Theories on the table: addon update bulk-resetting identifiers, stale file overwrite during multi-profile setup on Apr 30, accumulated silent collision resolutions over weeks. **If recurs**, capture diagnostic snapshots BEFORE wholesale-restoring (recipe in [nexus-inputbinds-per-profile-drift.md](nexus-inputbinds-per-profile-drift.md) open-bug section).
- **🐛 Cause of `controller_neptune.vdf` autosave with `Ctrl+C` instead of `Ctrl+Q` is unknown.** User firmly denies editing the controller layout. The file had `description: "#SettingsController_AutosaveDescription"` and `progenitor: usercloud://moonlight - gw2 steamos/controller_neptune` indicating Steam Input UI autosaved it. Mechanism unclear. **If recurs**, capture the file's progenitor + url + description fields and any Steam Input UI events from the day it changed.
- **🐛 Cause of `configset_controller_neptune.vdf` going missing on both bazzite and Deck simultaneously is unknown.** This is the appname→layout pointer file. Was missing entirely on both machines at session start. Restored from repo backup. Mechanism for both being deleted simultaneously unclear.
- **dpad_east Long_Press not firing** — initially suspected Steam Input regression from today's SteamOS update (`/etc/os-release` modified May 1 01:28). Ultimately resolved to be the dual-failure mode (controller emitting wrong key + Nexus binding cleared). Dual fix worked. Whether SteamOS update contributed is unverified — left as ambient possibility.
- **Long-term mitigation for InputBinds drift** — user hasn't decided between sync script / diff script / per-launch validation. For now, the manual recovery recipe is the standing fix.

### What worked (for next-time triage)

- **Smoke test using `Q` typed in chat** — temporarily change Long_Press binding to plain `Q` and check if the keystroke shows up in GW2 chat. If `Q` doesn't appear, the Long_Press isn't firing at the controller layer (rules out Nexus). If `Q` appears, Long_Press fires fine and the issue is downstream (binding misalignment).
- **Test Device Inputs on the Deck** for hardware verification (Settings → Controller → Test Device Inputs). Rules out physical button issues.
- **Tail Nexus.log live with grep filter** during user testing to capture exactly which addon dispatch fires:
  ```bash
  ssh Og@172.16.100.212 'timeout 60 tail -F "$HOME/Games/gw2-deck/addons/Nexus/Nexus.log" 2>&1 | grep --line-buffered -E "MysticClicker|DEPOSIT|Combo|LFG"'
  ```
- **Wholesale-copy from Apple TV profile** is the canonical recovery — Apple TV's InputBinds.json reliably matches `configs/gw2-keybinds/nexus-inputbinds.json` repo source-of-truth.
