# Changelog

<!-- markdownlint-disable MD024 -->

All notable changes to Guild Wars 2 Addons will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
