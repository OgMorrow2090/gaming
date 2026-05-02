# Changelog

<!-- markdownlint-disable MD024 -->

All notable changes to Guild Wars 2 Addons will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.6.12] - 2026-05-02 — Mystic Clicker: TP + Bank combos use DLL-press-`I` (intermittency fix)

### Fixed

- **`SimulateTradingPostCombo` / `SimulateBankCombo`** — migrated to the same `WaitForChordModifiersRelease` + `OpenInventoryDllAndDoubleClick` pattern Teleport Friend / Wizard Gobbler / Lounge Pass / Merchant already use. Symptom was TP chord opening then closing inventory (sometimes opening), making the captured-slot double-click land on a closed panel. Root cause: the old `OpenInventoryAndDoubleClick` helper assumed the VDF chord pressed `I`, but the chords (bare F7 / F8) don't — and Steam Input's chord double-emit could land conflicting events. Now the DLL releases held modifiers and synthesizes `I` itself, exactly once per dispatch.

### Files

- `modules/mystic-clicker/input-sim.cpp` — TP and Bank combos rewritten
- `modules/mystic-clicker/entry.cpp` — version bump 3.6.11 → 3.6.12

## [controller v19.3] - 2026-05-02 — Utility Wheel: empty center, Settings moved to slot 16

### Changed

- **Utility Wheel (group id 63)** — `touch_menu_button_0` (center) removed; `Open Settings` (`F11`) moved to a new `touch_menu_button_16` on the ring. Center is now a no-op so accidental release in the middle of the wheel doesn't fire Settings — was getting in the way of every wheel trigger.
- Snapshot saved as `configs/steam-controller/moonlight-gw2-og-v19.3.vdf`. Brackets balanced (921=921), groups 56, bindings 294 (unchanged: removed 1, added 1).

### Files

- `configs/steam-controller/moonlight-gw2-og-template.vdf` — title v19.3, Utility Wheel center removed + slot 16 added
- `configs/steam-controller/moonlight-gw2-og-v19.3.vdf` — new snapshot

## [3.6.11] - 2026-05-02 — Mystic Clicker GRACEFUL_QUIT macro + controller v19.2

### Added

- **Mystic Clicker `GRACEFUL_QUIT` keybind** (default `ALT+SHIFT+Q`) — replaces controller Menu-button Long_Press = `Alt+F4` force-quit. Flow: send `Esc` (open in-game menu) → 200 ms wait → click captured "Exit Game" position. GW2 closes via the normal shutdown path so Nexus runs `Unload` and `InputBinds.json` saves cleanly. Mitigates the InputBinds drift hypothesis where Alt+F4 force-quit skipped the Nexus save.
- New capture target **"Exit Game"** in the Mystic Clicker Capture window — captures the in-game menu's "Exit to Character Select" button per-resolution.
- New config keys `ExitGameX` / `ExitGameY` persisted to per-resolution `.cfg` (zeroed on resolution reset).
- New Nexus binding identifier `CAPTURE_EXIT_GAME` (registered, no default key) so the capture entry surfaces in Nexus settings.

### Changed

- **Controller layout v19.2** — `button_escape` Long_Press: `LEFT_ALT + F4` → `LEFT_ALT + LEFT_SHIFT + Q` (label `Graceful Quit`). Snapshot saved as `configs/steam-controller/moonlight-gw2-og-v19.2.vdf`.

### Files

- `modules/mystic-clicker/shared.h` — `GRACEFUL_QUIT` + `CAPTURE_EXIT_GAME` constants; `g_ExitGameX/Y`; `SimulateGracefulQuit()` proto
- `modules/mystic-clicker/entry.cpp` — register/deregister + version bump 3.6.10 → 3.6.11
- `modules/mystic-clicker/keybinds.cpp` — dispatch `GRACEFUL_QUIT → SimulateGracefulQuit()`
- `modules/mystic-clicker/input-sim.cpp` — `SimulateGracefulQuit()` implementation
- `modules/mystic-clicker/capture-ui.cpp` — "Exit Game" capture target row
- `modules/mystic-clicker/config.cpp` — read/write/reset `ExitGameX/Y`
- `configs/gw2-keybinds/nexus-inputbinds.json` — `GRACEFUL_QUIT` (Code 16, Alt+Shift) + `CAPTURE_EXIT_GAME` (Code 0)
- `configs/steam-controller/moonlight-gw2-og-template.vdf` — title v19.2, button_escape Long_Press updated
- `configs/steam-controller/moonlight-gw2-og-v19.2.vdf` — new snapshot

## [3.6.10] - 2026-05-01 — Mystic Clicker capture window + Mystic Trading delivery box close on ESC

### Added

- **Mystic Clicker**: capture window registers `GUI_RegisterCloseOnEscape` so pressing ESC closes the panel just like any other Nexus window. Symmetric `GUI_DeregisterCloseOnEscape` in `AddonUnload`.
- **Mystic Trading**: Delivery Box now also closes on ESC. Previously only Dashboard and FlipColumn responded — Delivery Box was an explicit gap noted in a `// (but NOT delivery box)` comment. Comment updated; all three MT windows now consistent.

### Files

- `modules/mystic-clicker/entry.cpp` — register/deregister + version bump 3.6.9 → 3.6.10
- `modules/mystic-trading/entry.cpp` — register/deregister Delivery Box + version build bump 20260420 → 20260501

## [3.6.9] - 2026-05-01 — Mystic Clicker: Bouncy Meta Progress capture

### Added

- **New capture point: "Bouncy Meta Progress"** — fits between Bouncy Accept and Bouncy Meta Complete in the OPEN_CHEST_COMBO flow. Some bouncy chest meta events show a separate progress dialog mid-flow that needs clicking before the final completion dialog appears; without this capture step, the combo would skip it and stall.
- New config keys `BouncyMetaProgressX`/`BouncyMetaProgressY` persisted to / loaded from `mystic-clicker.cfg` and per-resolution variants.
- Capture-UI row inserted between "Bouncy Accept" and "Bouncy Meta Complete"; existing Meta Complete description updated from "(combo step 3)" → "(combo step 4)".
- Combo sequencing in `SimulateOpenChestCombo`: Right-click chest → 500ms → Accept (optional) → 100ms → **Meta Progress (optional, NEW)** → 100ms → Meta Complete (optional). All clicks remain optional; whichever positions are captured fire.

### Files

- `modules/mystic-clicker/shared.h` — extern declarations
- `modules/mystic-clicker/config.cpp` — globals, primary load, save, reset, secondary load (5 touch-points)
- `modules/mystic-clicker/capture-ui.cpp` — new row in `s_Targets[]`
- `modules/mystic-clicker/input-sim.cpp` — combo flow click between Accept and Meta Complete
- `modules/mystic-clicker/entry.cpp` — version bump 3.6.8 → 3.6.9

### Deploy reminder

Per `memory/nexus-multi-deploy-rules.md`, after GitHub Actions builds the new DLL it must be deployed to **all three** profile install dirs (`~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/`, `~/Games/gw2-appletv/addons/`, `~/Games/gw2-deck/addons/`). User then captures the Meta Progress position once per profile (or copies the `.cfg` files between profiles since click positions are screen-resolution-dependent).

## [3.6.8] - 2026-04-26 — Mystic Clicker + Controller v18.4.8

### Fixed

- **Friend chord still opened then immediately closed inventory** even with v3.6.7's Nexus-side debounce. Log analysis showed Steam Input is double-emitting the **entire VDF chord** (both `I` and `F6`) on Full_Press in this layout, 50–80 ms apart. Nexus debounce caught the second `F6` dispatch (`Debounced duplicate dispatch of TELEPORT_FRIEND_COMBO`), but **GW2 still saw `I` pressed twice** at the OS level — and two `I` presses toggle inventory open then closed.
- Fix: convert Friend to the **DLL-press-`I` pattern** (matches Wizard Gobbler / Portal / Lounge / Merchant). VDF chord now emits `F6` alone — no `I`. The DLL synthesizes `I` exactly once per dispatch via `OpenInventoryDllAndDoubleClick`. The chord has no modifier so `WaitForChordModifiersRelease` returns instantly — no perceptible delay.
- **Mystic Clicker DLL `RegisterWithString` default for `RESET_WINDOWS` updated** from `CTRL+SHIFT+HOME` → `CTRL+SHIFT+INSERT` to match v18.4.7's wheel-slot-2 chord change.

### Notes

After deploying v18.4.7 the user reported: "Screen rescue shows no skill error[,] still does nothing[. No] window change." Translation: chord moved off Home (skill collision gone) but Nexus isn't dispatching the new `Ctrl+Shift+Insert` chord — the on-disk InputBinds.json change to Code 45 hasn't been picked up because **Nexus only reads InputBinds.json on startup**. The user must do a full GW2 restart (not just an addon reload) for the Reset Windows binding to take effect. The DLL default change in this release ensures any future install picks up Insert directly without manual rebinding.

The Steam Input double-emit-Full_Press behavior also affects Trading Post and Bank chords (`I+F7`, `I+F8`) on dpad_east, but the user reports those work — likely because the second `I` toggle happens within the same frame as the first and isn't visually obvious. Leaving those alone for now (don't fix what isn't broken). If they ever exhibit the symptom, convert them to the same DLL-press-`I` pattern.

Versions: VDF v18.4.8 (revision 2408), Mystic Clicker DLL 3.6.8.

## [3.6.7] - 2026-04-26 — Mystic Clicker + Controller v18.4.7

### Fixed

- **R1+DPad Down → Friend "doesn't open inventory" — actually a chord double-fire.** Nexus.log shows two `Teleport Friend Combo: open inventory + double-click` entries at the **same millisecond timestamp** for every press (15:02:26.16 ×2, 15:02:33.847 ×2, 15:02:51.711 ×2). The pattern affects all Full_Press chords in v18.4.x layouts (Friend, Wizard Gobbler, TP, Bank). Pressing `I` twice toggles the inventory **closed** on the same combo that just opened it, so the second double-click lands on a closed panel. Added per-identifier debounce in `ProcessKeybind` — drops any dispatch within 300 ms of the same identifier's last fire. Logs the suppressed dispatch at DEBUG level for verification.
- **Utility Wheel slot 2 (Reset Windows) also fired GW2's Skill Profession 5** ("skill recharging" message). User has `SkillProfession5` bound to `button="36"` (Home key) in `GameBinds.xml`, and GW2 ignores the Ctrl+Shift modifiers — bare Home press triggers the skill. Switched the rescue chord from `Ctrl+Shift+Home` → `Ctrl+Shift+Insert` (Insert is unbound across all of the user's GW2 keybinds) and updated both `RESET_WINDOWS` + `MT_RESET_WINDOWS` Nexus bindings to Code 45 (Insert). The `Ctrl+Shift+Insert` chord is also unique to Mystic Clicker — no other Nexus addon claims it.

### Notes

The user's GW2 GameBinds.xml binds **Home / End / PageUp / PageDown** all to Skill Profession 2-5 — pressing any of those keys (regardless of modifier state) fires the skill. When picking chord keys for Steam Input wheel slots, check `gamebinds.xml` for `button="36"` (Home), `button="35"` (End), `button="33"` (PgUp), `button="34"` (PgDn) and avoid them. F11 / F12 / Insert / Delete / Numpad 0 / 1 / 5 / 7 / 9 / + / - / / are all free in this user's setup.

Versions: VDF v18.4.7 (revision 2407), Mystic Clicker DLL 3.6.7.

## [3.6.6] - 2026-04-26 — Mystic Clicker DLL only

### Fixed

- **Long_Press inventory combos (Wizard Portal Scroll) still opened Wizard Vault** even with v3.6.5's 800 ms defer. Root cause: Steam Input keeps Shift asserted via uinput for the **entire physical hold**, which on a Long_Press routinely exceeds 800 ms (user's natural hold is 500 – 2000 ms). Any fixed `Sleep(N)` is a guess.
- Replaced the fixed-time defer in all four detached-thread combos (Wizard Gobbler, Wizard Portal Scroll, Lounge Pass, Merchant) with `WaitForChordModifiersRelease()` — polls `GetAsyncKeyState(VK_LSHIFT/RSHIFT/LMENU/RMENU/LCONTROL/RCONTROL)` every 50 ms (3 s timeout) and exits as soon as the OS sees the modifier release. Adapts to actual hold time so quick taps don't feel laggy and long holds don't leak `Shift+I`/`Alt+I` to GW2.
- Each combo now logs the modifier state on entry and the wait duration on exit (`Wizard Portal Scroll: modifiers held at start=0x1, released after 1450ms (timeout=no)`), so future debugging can see hold patterns directly in `Nexus.log`.

### Notes

DLL-only release — no VDF or Nexus binding changes. v18.4.6 controller layout stays current.

Versions: Mystic Clicker DLL 3.6.6.

## [3.6.5] - 2026-04-26 — Mystic Clicker + Controller v18.4.6

### Fixed

- **R1+DPad Left Double_Press → Lounge Pass opened Wizard Vault instead.** Same `Shift+I → WV` race we fixed for Long_Press in v3.5.2, but on Double_Press the second physical tap can run longer than 500 ms — uinput is still asserting Shift when the DLL synthesizes `I`. Bumped `SimulateLoungePassCombo`'s detached-thread defer from 500 ms → 800 ms so the held virtual modifier fully drains. Same change applied to `SimulateMerchantCombo` (also Double_Press, also `Alt+F11`).
- **R1+DPad Right Double_Press → Merchant didn't always trigger** because the user's natural double-tap rhythm exceeded Steam Input's default `double_tap_time` window. Added explicit `"double_tap_time" "500"` to all three v18.4 Double_Press activators (Merchant on R1+Right, Leave Party on R1+Down, Lounge Pass on R1+Left), giving 500 ms between taps.
- **Window rescue (Utility Wheel slot 2) didn't change window sizes** — `rescue.cpp` only clamped to display size, so a 1920×1080 window on the Deck became a 1280×800 window filling the whole screen. Now clamps to 70% of viewport (896×560 on Deck) so oversized windows actually shrink to a usable size. Also logs every press now (even when no windows needed rescuing) so the user can confirm the chord reached the addon.

### Notes

The two Double_Press combos that drive inventory items both run the held-modifier-drain pattern with an 800 ms defer (longer than the 500 ms used by Long_Press combos). Pattern: when an inventory-item combo lives on a Double_Press activator and the chord carries `Shift` or `Alt`, the DLL must wait at least 800 ms before synthesizing `I` — uinput key-hold during the second physical tap can outlast a 500 ms drain.

Versions: VDF v18.4.6 (revision 2406), Mystic Clicker DLL 3.6.5.

## [3.6.4] - 2026-04-26 — Mystic Clicker + Controller v18.4.5

### Fixed

- **R1+DPad Down → Friend didn't open inventory before clicking the captured slot.** `SimulateTeleportFriendCombo` does not press `I` itself — it relies on the VDF chord emitting `I` alongside the keycode. v18.3 had Friend on R1+DPad Right Full_Press emitting `I+F6`; v18.4.x moved Friend to R1+DPad Down and only emitted `F6`, so the 600ms wait + double-click landed on a closed inventory and did nothing. Fix: add `key_press I, Open Inventory` back to the dpad_south Full_Press chord. (F6 is bare, so `I` reaches GW2 cleanly with no `Shift+I`→WV bug.)
- **R1+DPad Right Double_Press → Merchant had the same bug**, but unlike Friend the chord carries `Alt`, so adding `key_press I` to the VDF wouldn't help (GW2 sees `Alt+I`, which is unbound). Fix: switch `SimulateMerchantCombo` to use `OpenInventoryDllAndDoubleClick` on a detached `std::thread` with a 500ms pre-delay — the same pattern already used by Wizard Gobbler / Portal Scroll / Lounge Pass to drain the held virtual modifier before the DLL synthesizes a clean `I`.

### Notes

The two inventory-combo patterns in this codebase:

1. **Bare-chord pattern** (Friend, Trading Post, Bank): VDF emits `I + F<n>`. GW2 sees `I` → opens inventory. Nexus catches `F<n>` → DLL waits 600ms → double-click captured slot. DLL function uses `OpenInventoryAndDoubleClick`.
2. **Modifier-chord pattern** (Wizard Gobbler / Portal Scroll / Lounge Pass / Merchant): chord uses `Shift` or `Alt`, so VDF must NOT emit `I` (would fire `Shift+I` = WV or be ignored as `Alt+I`). DLL runs on a detached `std::thread`, sleeps 500ms to let the held virtual modifier drain, then `ReleaseChordModifiers()` and synthesizes `I` via SendInput. DLL function uses `OpenInventoryDllAndDoubleClick`.

Pattern selection is determined by whether the chord carries a modifier — not by which button the chord lives on. When moving a combo across DPad slots, the new chord's modifier set must match the DLL function's pattern, or the inventory-open step silently fails.

Versions: VDF v18.4.5 (revision 2405), Mystic Clicker DLL 3.6.4.

## [3.6.3] - 2026-04-26 — Mystic Clicker + Controller v18.4.4

### Fixed

- **R1+DPad Right Double_Press → Merchant Combo opened Settings + Wizard Vault instead** — Nexus log showed no `Merchant Combo` event because the v18.4.3 chord (`Shift+F11+I`) didn't match what Nexus had stored for `MERCHANT_COMBO` (`F11` alone). Meanwhile GW2 caught `F11` (Game Options) and `Shift+I` (Wizard Vault toggle) — the same `Shift+I → WV` bug that bit Wizard Gobbler in v3.5.1. Fix: strip `key_press I` from the chord (DLL opens inventory itself with the 500 ms detached-thread defer) and move the chord off `Shift+F11` entirely. First tried `Ctrl+F11`, which collided with `KB_CRAFTY_TOGGLE` — settled on **`Alt+F11`** which is unbound. Updated `configs/gw2-keybinds/nexus-inputbinds.json` (deployed live to Bazzite) and the DLL `RegisterWithString` default so first-run users get the same key.

### Added

- **Utility Wheel slot 0 → Open Settings** (`F11`) — bare F11 is GW2's default Game Options keybind and is unbound in Nexus, so a single emit opens Settings cleanly.
- **Golden Rule #11: check Nexus for chord collisions before picking a key** — added to `docs/vdf-editing-golden-rules.md` with a one-line Python snippet that lists everything bound to a given scancode. Would have caught the `Ctrl+F11` / `KB_CRAFTY_TOGGLE` collision before deploy.

### Changed

- **Renamed `configs/steam-controller/moonlight-gw2-og-v16.1.vdf` → `moonlight-gw2-og-template.vdf`.** The `v16.1` in the filename had been misleading for many releases — the actual VDF version is in the `title` field inside the file. The repo template now has no version in its name; per-release deploys still go to the Deck under `og v<N.M.P>_0.vdf`.
- DLL bumped to **3.6.3**, controller VDF bumped to **v18.4.4** (revision 2404).

## [3.6.2] - 2026-04-26 — Mystic Clicker + Controller v18.4.3

### Fixed

- **Controller v18.4 VDF was structurally rejected by the Steam Deck parser** — Personal/Templates tabs went empty, ghost workshop refs kept getting restored on every Steam restart, and the user's saved layout could not be applied. Root cause: an earlier session used Python regex to rewrite `dpad_south` and `dpad_east` blocks in `moonlight-gw2-og-v16.1.vdf`. The greedy regex consumed Long_Press/Double_Press activator subgroups across multiple groups, dropping ~25 of them. The file went from 63 121 → 37 170 bytes (3543 → 1768 lines, 296 → 158 bindings, 56 → 31 effective groups). Brackets still balanced and the file looked legal, but Steam silently dropped it.
- **Recovery (v18.4.3)**: started fresh from the v18.3 base on disk (3543 lines, 56 groups, 296 bindings) and applied the v18.4 chord changes via two surgical line-level edits. Final file: 3551 lines, 63 305 bytes, 56 groups, 298 bindings (+2 for the new Merchant chord), bracket balance 918/918. Deployed to the Deck as `og v18.4.3_0.vdf` with `url=usercloud://moonlight/og v18.4.3_0` and `export_type=personal_local`.

### Added

- **R1+DPad Right Double_Press → Merchant Combo** (`SHIFT+F11`) — opens inventory and double-clicks the captured Merchant slot. New capture `CAPTURE_MERCHANT` already existed in v3.6.0; this commit wires it to a chord.
- **R1+DPad Right reorganized**: Full=TP (F7) / Long=Bank (F8) / **Double=Merchant** (Shift+F11, new).
- **R1+DPad Down reorganized**: Full=Friend (F6) / Long=Waypoint (Shift+F9) / Double=LeaveParty (Shift+F10). Friend was previously bound on R1+DPad Right Full.
- **`docs/vdf-editing-golden-rules.md`** — full reference doc with hard rules, validation checklist, and recovery playbook to prevent this class of outage. Memory entries (`memory/vdf-editing-rules.md`, `memory/steam-input-sync-not-cloud.md`) point at it.

### Notes

- Steam Input sync layer is **separate** from the Steam Cloud toggle in the client UI (Valve bug #7801). Disabling Cloud does not stop Steam Input from re-syncing controller layouts. Confirmed during recovery — only signout/signin combined with local cache wipe (`appworkshop_241100.acf`, `ugcmsgcache/`, `userdata/.../remotecache.vdf`) consistently broke the ghost-layout cycle.
- DLL `MERCHANT_COMBO` default changed from `(null)` to `SHIFT+F11`.
- Mystic Clicker DLL bumped to **3.6.2**. GitHub Actions builds it on push.

## [3.5.2] - 2026-04-21 — Mystic Clicker + Controller v17.7

### Added

- **Five new combos on R1+DPad Left and R1+DPad Down** (v3.5.0/v17.6):
  - R1+DPad Left Full → **Wizard Gobbler** (Shift+F1)
  - R1+DPad Left Long → **Wizard Portal Scroll** (Shift+F2)
  - R1+DPad Left Double → **Lounge Pass** (Shift+F3)
  - R1+DPad Down Full → **Waypoint Combo** (Shift+F4) — single-click captured chat waypoint → map auto-opens → double-click captured map waypoint to travel
  - R1+DPad Down Long → **Leave Party Combo** (Shift+F5) — right-click captured party/squad bar → single-click captured Leave button

### Fixed

- **Wizard Gobbler / Portal Scroll / Lounge Pass opened Wizard Vault instead of inventory** (v3.5.1): the VDF emitted `I + LEFT_SHIFT + F#` simultaneously. GW2 observed `Shift+I` (WV toggle) while Nexus caught `Shift+F#`. Fix: strip `key_press I` from the 3 R1+DPad Left activators; new `OpenInventoryDllAndDoubleClick()` helper releases chord modifiers and synthesizes a clean `I` via SendInput scan code (same pattern WV combo uses for Shift+I).
- **Long_Press / Double_Press variants still opened WV** after v3.5.1 fix (v3.5.2): Steam Input's Long_Press and Double_Press activators keep the chord keys held via uinput for the duration of the physical button press — our DLL's SendInput Shift-KEYUP got overridden by the still-asserting uinput hold. Fix: the 3 inventory combos now run on a detached `std::thread` with a 500ms pre-delay, letting the user's button release drain the virtual Shift before we press `I`.
- **Waypoint Combo map double-click missed the waypoint** — 700ms post-chat-click sleep wasn't enough for the map to settle on the pin. Bumped to 1200ms.

### Notes

Architecture split between the 3 R1+DPad Right portable-item combos (bare-F6/F7/F8 chord, VDF emits `I` directly — no Shift conflict) and the 3 R1+DPad Left portable-item combos (Shift+F1/F2/F3 chord, DLL synthesizes `I` after a 500ms defer). Keep them that way — they use the same `OpenInventoryAndDoubleClick` / `OpenInventoryDllAndDoubleClick` helpers but resolve different Linux/Sunshine uinput quirks.

## [3.4.1] - 2026-04-20 — Mystic Clicker + Controller v17.5

### Added

- **Personal Marker** moved from VDF mouse_button binding into DLL. New `PERSONAL_MARKER` keybind on bare F9 triggers a single SendInput batch (Alt-down → LeftClick-down → LeftClick-up → Alt-up) at the current cursor position. Reliable through Sunshine/uinput where Steam Input's `mouse_button LEFT` wasn't registering the Alt modifier in sync.
- **Three portable-item combos** on R1+DPad Right variants. Each emits `key_press I` natively (opens inventory) + a bare F-key chord that fires the DLL to double-click the captured icon:
  - R1+DPad Right Full → Teleport to Friend (F6)
  - R1+DPad Right Double → Trading Post (F7)
  - R1+DPad Right Hold → Bank (F8)
- **Controller v17.3–v17.5** ships the R1+DPad Right bindings, new Personal Marker chord on R1+Y, and removed a duplicate empty `button_y` block that was shadowing Personal Marker.

### Fixed

- **WV Combo** now triple-clicks Collect per tab (catches multi-collect states), single-clicks Complete, and returns to Daily tab at the end so the next opening starts on Daily.
- **MT lock bypass for rescue/resolution change**: locked Delivery / FlipList windows no longer stay stuck when a resolution change or Ctrl+Shift+Home rescue fires — `NoMove/NoResize/NoTitleBar` flags are temporarily stripped for that frame so the reposition actually takes effect.
- **ClampWindowPos** now caps window size to the viewport, preventing a 4K-sized window from rendering oversized on an 800p display.

### Notes

After multiple misfires on chord choice (Ctrl+Alt+F1/F2/F3 = Linux TTY switch; Ctrl+Shift+Alt+F1/F2/F3 = 4-binding drop; Ctrl+Shift+PAGEUP/DOWN/INSERT and F13/F14/F15 = key names not in Steam Input's VDF parser), settled on **bare F6/F7/F8/F9** with `key_press I` alongside the chord. Steam Input definitely knows F1-F12; Nexus distinguishes bare-F6 from Ctrl+Shift+F6 by modifier bits.

## [3.2.0] - 2026-04-20 — Mystic Clicker + Mystic Trading v0.5.0 + Controller v17.2

### Added

- **Drag-anywhere** (both modules): click-and-hold any empty area to move a panel. No longer limited to the title bar.
- **Per-resolution window positions** (both modules): each window remembers pos + size per resolution. Switch 4K → 1280×800 and it auto-loads the right layout. New state files: extended `mystic-clicker-{WxH}.cfg`, new `mystic-trading-windows-{WxH}.cfg`.
- **Auto-clamp** on resolution change keeps at least 60px of each window on-screen.
- **Rescue hotkey Ctrl+Shift+Home** snaps all addon windows back to (100, 100) at default size. 200ms arming window so all 3 MT panels consume it in one frame.
- **Controller v17.2**: Utility Wheel slot 2 fires Ctrl+Shift+Home (Reset Windows).

### Changed

- Saves for window pos/size are 1-second debounced so drags don't hammer the disk.

## [3.1.3] - 2026-04-20 — Mystic Clicker

### Added

- **Wizard Vault Special Tab** capture + combo step — combo now loops through Daily → Weekly → Special, each with Collect + Complete clicks. All three tabs optional.

## [3.1.2] - 2026-04-20 — Mystic Clicker

### Added

- **Capture panel sort**: uncaptured targets on top, captured below, both A-Z. Re-sorts each frame so newly-captured entries drop to the bottom immediately.

### Fixed

- **LFG Combo not firing**: Nexus keybind store on Bazzite had `LFG_COMBO` with Code=0 (unbound) and `GENERAL_ACCEPT` stealing Ctrl+Shift+F6. Fixed InputBinds.json directly — LFG_COMBO now bound to scan code 64 (F6), GENERAL_ACCEPT cleared. Nexus uses scan codes, not VK codes.

## [3.1.1] - 2026-04-20 — Mystic Clicker

### Added

- **Wizard Vault Daily Tab** capture + combo step — WV remembers the last-selected tab, so the combo now clicks Daily Tab first (when captured) to ensure Daily is always processed before switching to Weekly.

## [3.1.0] - 2026-04-20 — Mystic Clicker + Controller v17.1

### Added

- **Accept Combo** expanded from 10 to 15 slots (Accept 11-15).
- **Wizard Vault Weekly Tab** capture — combo now clicks Daily → Collect/Complete → Weekly Tab → Collect/Complete.
- **LFG Combo** — press Y (opens LFG) → click Search tab. Bound to Ctrl+Shift+F6.
- **Controller v17.1**: DPad Left Full Press now fires LFG Combo (was raw Y press).

### Changed

- **Click intervals** reduced from 300ms to 100ms between all combo clicks for faster flow.

### Migrated

- Accept 10's captured position moved to Accept 15 slot (one-time cfg migration on Bazzite 1280x800).

## [3.0.0] - 2026-04-19 — Mystic Clicker + Controller v17.0

### Added

- **Accept Combo**: 8 → 10 slots with Bouncy Meta Complete (3rd bouncy capture for meta-completion dialogs).
- **Right-click-to-clear** in capture window to reset a single captured position.
- **Bouncy Open + Bouncy Accept** as separate captures from general Accept combo.
- **Mail / Guild Hall / Wizard Vault combos** with keypress + click patterns using SendInput (KEYEVENTF_SCANCODE) to handle Nexus chord modifier timing.

### Fixed

- **Scan code requirement**: WM_KEYDOWN without scan code in lParam is silently dropped by GW2 — affected Guild Hall Combo (G key). Now constructs `(scan << 16) | 0x01` via `MapVirtualKey(vk, MAPVK_VK_TO_VSC)`.
- **Nexus chord modifier timing**: Nexus keybind callback fires while physical Ctrl/Shift still held — combos that send SendInput chords (Shift+I for WV, Shift+0 for Mail) first release VK_LCONTROL/RCONTROL/LSHIFT/RSHIFT via SendInput KEYUP before sending the intended chord.

## [0.3.0] - 2026-03-18 — Mystic Trading

### Changed

- **Dashboard (Alt+T)**: Simplified to single column — Wallet, Bank, Materials only
- **Delivery Box (Alt+D)**: Simple toggle on/off, removed auto-show logic
- **Delivery Box**: Removed duplicate inner heading (window title bar is sufficient)
- **Flip List (Alt+F)**: Removed duplicate "Profitable Flips" inner heading

### Added

- **Window transparency slider** in Options > Appearance (0.3–1.0, default 0.92)
- **Linux gaming docs**: Added `docs/linux-gaming.md` for Bazzite desktop setup

### Removed

- Flips and Delivery Box sections from main dashboard (use Alt+F and Alt+D instead)

## [2.0.0] - 2026-03-15

### Changed

- **Renamed addon to Mystic Clicker** - formerly "Inventory Hotkeys"
- **New addon directory**: `MysticClicker` (was `InventoryHotkeys`)
- **New config filenames**: `mystic-clicker-{width}x{height}.cfg` (was `inventory-hotkeys-*.cfg`)
- **New DLL name**: `mystic-clicker.dll` (was `inventory-hotkeys.dll`)
- **Reorganized source code** into `src/mystic-clicker/` subfolder for multi-module support

### Added

- **Automatic config migration** - On first load, copies all saved position configs from the old `InventoryHotkeys` directory to `MysticClicker` with renamed filenames. No manual action needed.

### Technical

- Solution and project files renamed to `mystic-clicker.sln` / `mystic-clicker.vcxproj`
- Source files moved to `src/mystic-clicker/` to prepare for additional addon modules

## [1.4.0] - 2026-02-05

### Fixed

- **BlishHUD/Nexus GG compatibility** - Fixed keybind conflict that prevented other addons from receiving keyboard input
- **Reserved name conflict** - Renamed "VENDOR" to "VENDOR_BUY" to avoid Nexus internal name collision

### Changed

- **Updated default keybinds** - All actions now use CTRL+key, captures use CTRL+SHIFT+key
  - CTRL+D: Deposit Materials
  - CTRL+S: Sort Inventory
  - CTRL+B: Open Chest
  - CTRL+Q: Deposit & Sort
  - CTRL+J: Sell Junk
  - CTRL+O: Trading Post
  - CTRL+U: Vendor (changed from CTRL+V to avoid paste conflict)
  - CTRL+E: Exit Instance
  - CTRL+Y: Yes Dialog
  - CTRL+F: Mystic Forge
  - CTRL+R: Mystic Refill
  - CTRL+M: Mystic Forge Combo
  - CTRL+T: TP Remove

### Technical

- Discovered "VENDOR" is a reserved/conflicting identifier in Nexus
- All 24 keybinds (13 actions + 11 captures) now work without breaking other addons

## [1.3.0] - 2026-02-04

### Added

- **Yes Dialog Hotkey** (Ctrl+P) - Click the Yes button on confirmation dialogs
- **Mystic Forge Hotkey** (Ctrl+F) - Click the Mystic Forge combine button
- **Mystic Refill Hotkey** (Ctrl+R) - Click the Mystic Forge refill button
- **Mystic Forge Combo** (Ctrl+A) - Forge then Refill with 100ms delay
- **Vendor Hotkey** (Ctrl+V) - Click vendor button
- **Sell Junk Hotkey** (Ctrl+J) - Click sell junk button at vendors
- **Trading Post Hotkey** (Ctrl+O) - Click trading post button
- **TP Remove Hotkey** (Ctrl+T) - Click "Take" button in Trading Post pickup
- **Capture keybinds** for all new hotkeys (Ctrl+Shift+P/F/R/V/J/O/T)

### Changed

- **Cleaner keybind names** - Removed "KB_" prefix from all keybind identifiers
  - e.g., "KB_DEPOSIT_MATERIALS" → "DEPOSIT_MATERIALS"
  - Requires re-binding keys in Nexus after update

### Technical

- Added 14 new position variables for new hotkeys
- All positions saved per-resolution in config files

## [1.2.0] - 2026-02-04

### Added

- **Exit Instance Hotkey** (Ctrl+E) - Click the exit instance button
- **5 Generic Hotkeys** - User-assignable click hotkeys for any UI element
  - Generic 1-5 with capture keys Ctrl+Shift+1-5
  - Unassigned by default - configure in Nexus Options
- **Capture Exit Instance** (Ctrl+Shift+E) - Save exit instance button location

### Technical

- Added 12 new position variables for exit instance and generic hotkeys
- All positions saved per-resolution in config files

## [1.1.0] - 2026-02-04

### Added

- **Per-Resolution Config Files** - Positions now save to resolution-specific files
  - e.g., `inventory-hotkeys-1920x1080.cfg`, `inventory-hotkeys-2560x1440.cfg`
- **Auto Resolution Detection** - Detects current game window resolution on startup
- **Resolution Change Detection** - Automatically switches config when resolution changes
- **Multi-Device Support** - Switch between laptop and desktop without reconfiguring

### Changed

- Config files now include resolution in filename
- Alert messages show current resolution when loading/saving

## [1.0.0] - 2026-02-03

### Added

- **Deposit Materials Hotkey** (Ctrl+D) - Click deposit materials button
- **Sort/Compact Hotkey** (Ctrl+S) - Click sort/compact button
- **Bouncy Chest Hotkey** (Ctrl+B) - Right-click to open bouncy chests
- **Deposit + Sort Combo** (Ctrl+Q) - Performs deposit then sort in sequence
- **Position Capture System** - Keybinds to save button positions at cursor
  - Ctrl+Shift+D for deposit button
  - Ctrl+Shift+S for sort button
  - Ctrl+Shift+B for bouncy chest
- **Config Persistence** - Saves positions to `inventory-hotkeys.cfg`
- **Nexus Integration** - Full keybind customization via Nexus Options UI
- **Visual Studio 2025 Support** - Built with Platform Toolset v145

### Technical

- Nexus API v6 integration
- `WndProc_SendToGameOnly` for input simulation
- `GUI_SendAlert` for user feedback
- Auto-creates config directory if missing

## [0.1.0] - 2026-01-01

### Initial Release

- Initial repository structure
- Project documentation (readme.md, agents.md, roadmap.md)
- Development setup guide
- Research documentation for Nexus addon development

---

## Version History Summary

| Version | Date       | Highlights                                      |
| ------- | ---------- | ----------------------------------------------- |
| 2.0.0   | 2026-03-15 | Renamed to Mystic Clicker, config migration     |
| 1.3.0   | 2026-02-04 | Mystic Forge, Vendor, Trading Post hotkeys      |
| 1.2.0   | 2026-02-04 | Exit instance hotkey, 5 generic hotkeys         |
| 1.1.0   | 2026-02-04 | Per-resolution configs, multi-device support    |
| 1.0.0   | 2026-02-03 | Full release with all core features             |
| 0.1.0   | 2026-01-01 | Initial structure and research                  |

---

Changelog for Guild Wars 2 Addons
