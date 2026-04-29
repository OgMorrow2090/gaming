# Session Log

Append-only log of agent sessions on this repo. Read at session start for continuity. Entries are summaries — not full transcripts.

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
