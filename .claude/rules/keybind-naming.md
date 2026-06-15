---
paths:
  - 'modules/**/*.cpp'
  - 'include/Nexus.h'
---

# Keybind Naming

Rules for choosing Nexus InputBind identifier strings so addons do not break
each other or collide with in-game binds.

## Reserved identifiers

Some single-word identifiers collide with Nexus internals and silently block
keyboard input for every other addon (BlishHUD, Nexus GG, etc.).

| Reserved | Use instead | Notes |
|----------|-------------|-------|
| `VENDOR` | `VENDOR_BUY` | Causes input blocking for all other addons |

The corrected `VENDOR_BUY` constant lives at `modules/mystic-clicker/shared.h:55`
and is registered at `modules/mystic-clicker/entry.cpp:143`.

## Naming conventions

- Define every identifier as a `constexpr const char*` in the module's
  `shared.h` (e.g. `modules/mystic-trading/shared.h:22`,
  `modules/mystic-clicker/shared.h:43`, `modules/mystic-ai/shared.h:17`).
- Use descriptive, unique names. Prefix per-addon where helpful — mystic-trading
  uses an `MT_` prefix on its emitted identifier strings (`shared.h:22`),
  mystic-ai uses a `MYSTIC_AI_` prefix (`shared.h:17`).
- Avoid bare single common words that might be reserved internally.

## Verify before binding

- Test a new keybind against BlishHUD chat shortcuts to confirm it does not
  block input. If a new keybind breaks other addons, rename it.
- A Nexus chord can reach GW2 with its modifiers dropped, firing a bare-key game
  action as well as the addon. Grep `configs/gw2-keybinds/gamebinds.xml` for the
  bare key before choosing a chord. Full explanation:
  `memory/gw2-keybind-collisions.md`.

## Source-symbol naming (C++)

| Type | Convention | Example |
|------|------------|---------|
| Files | lowercase with hyphens | `input-sim.cpp` |
| Functions | PascalCase | `ProcessKeybind()` |
| Variables | camelCase | `isInventoryOpen` |
| Identifier constants | SCREAMING_SNAKE | `DEPOSIT_MATERIALS` |
| Texture IDs | `TEX_` prefix | `TEX_ADDON_ICON` |
