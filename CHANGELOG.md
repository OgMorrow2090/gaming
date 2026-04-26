# Changelog

<!-- markdownlint-disable MD024 -->

All notable changes to Guild Wars 2 Addons will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
