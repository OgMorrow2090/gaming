# Changelog

<!-- markdownlint-disable MD024 -->

All notable changes to Guild Wars 2 Addons will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
