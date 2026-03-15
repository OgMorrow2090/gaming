# Development Roadmap

Development plans for Guild Wars 2 Addons.

## ✅ Completed: Mystic Clicker Addon v1.0.0

### Phase 1: Foundation ✅

- [x] **Development Environment**
  - [x] Set up Visual Studio 2025 project (Platform Toolset v145)
  - [x] Configure Nexus API header
  - [x] Create basic DLL structure
  - [x] Test addon loading in game

- [x] **Core Keybind System**
  - [x] Implement `GetAddonDef()` export
  - [x] Register keybinds for Deposit Materials (Ctrl+D)
  - [x] Register keybinds for Sort Inventory (Ctrl+S)
  - [x] Register keybinds for Bouncy Chest (Ctrl+B)
  - [x] Register combo keybind Deposit+Sort (Ctrl+Q)
  - [x] Implement `ProcessKeybind()` handler

- [x] **Input Simulation**
  - [x] Implement `WndProc_SendToGameOnly()` wrapper
  - [x] Left-click simulation for deposit/sort
  - [x] Right-click simulation for bouncy chest
  - [x] Combo action with delay between clicks

- [x] **Position Capture System**
  - [x] Capture keybinds (Ctrl+Shift+D/S/B)
  - [x] Auto-save to config file
  - [x] Load positions on addon startup
  - [x] Create config directory if missing

### Phase 2: Polish (Future)

- [x] **Per-Resolution Config Support**
  - [x] Detect current screen resolution
  - [x] Save configs with resolution in filename
  - [x] Auto-load correct config on resolution change
  - [x] Support switching between devices (laptop/desktop)

- [ ] **UI Scaling Support**
  - [ ] Read MumbleLink UI size setting
  - [ ] Adjust button positions for different UI scales
  - [ ] Test across resolutions (1080p, 1440p, 4K)

- [ ] **Options Panel**
  - [ ] Create ImGui options interface
  - [ ] Display current position values
  - [ ] Reset to defaults button

- [ ] **Quick Access Integration**
  - [ ] Create addon icons (normal + hover)
  - [ ] Add shortcut to Nexus menu bar

### Phase 3: Advanced Features (Future)

- [x] **Mystic Forge Hotkeys** (v1.3.0)
  - [x] Mystic Forge combine button hotkey
  - [x] Mystic Forge refill button hotkey
  - [x] Mystic Forge combo (Forge + Refill)
  - [x] Yes Dialog confirmation hotkey

- [ ] **Inventory State Detection**
  - [ ] Detect if inventory is open
  - [ ] Show warning if inventory closed

- [ ] **Additional Hotkeys**
  - [ ] Custom action triggers
  - [ ] More combo sequences

---

## 📋 Backlog

### Future Addon Ideas

| Idea                       | Description                        | Priority |
| -------------------------- | ---------------------------------- | -------- |
| Auto-Deposit on Map Change | Trigger deposit when changing maps | Low      |
| Inventory Presets          | Save/restore inventory layouts     | Low      |
| Quick Sell                 | Hotkey to sell junk items          | Medium   |

---

## ✅ Completed

### February 2026

- [x] v1.1.0 per-resolution config support
- [x] Auto resolution detection
- [x] Multi-device support (laptop/desktop switching)
- [x] v1.0.0 release with full functionality
- [x] Deposit Materials hotkey (Ctrl+D)
- [x] Sort Inventory hotkey (Ctrl+S)
- [x] Bouncy Chest hotkey (Ctrl+B)
- [x] Deposit + Sort combo (Ctrl+Q)
- [x] Position capture system (Ctrl+Shift+D/S/B)
- [x] Config file persistence
- [x] Nexus keybind UI integration

### January 2026

- [x] Research Nexus API capabilities
- [x] Confirm feasibility of input simulation
- [x] Create repository structure
- [x] Document development approach
- [x] Create agents.md with Nexus development patterns
- [x] Create C++ source structure
- [x] Set up changelog with version tracking
- [x] Push initial repository to GitHub

---

Roadmap for Guild Wars 2 Addons - Updated February 2026
