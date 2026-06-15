---
paths:
  - 'modules/**/*.cpp'
  - 'include/Nexus.h'
---

# ArenaNet ToS Safety

These addons stay inside ArenaNet's third-party policy by simulating a single,
real, user-initiated action per keypress. Keep them there.

## Allowed

- UI convenience addons and keybind remapping.
- One action per keypress (1:1 input) — the user presses a key, one in-game
  action fires.

## Not allowed

- Automating gameplay, or any action that runs without a per-action keypress.
- Multiple distinct game actions from a single keypress as unattended automation
  (combat rotations, farming loops, etc.).

## Combo binds: the one nuance

Mystic Clicker has "combo" binds that fire a short fixed sequence on one press
(e.g. `OPEN_CHEST_COMBO`, `MYSTIC_FORGE_COMBO` in
`modules/mystic-clicker/shared.h:48`). These are user-attended convenience
macros over UI clicks, gated behind a deliberate keypress — see the sequence
note at `modules/mystic-clicker/input-sim.cpp:1314`. They must remain
user-triggered and must not loop or run unattended. When in doubt, keep it 1:1.

The clipboard helper deliberately does NOT auto-paste into the game; the user
pastes manually. See the rationale at `modules/mystic-clicker/item-name.cpp:644`.
Do not "fix" this by adding an automatic paste.

## Addon safety (defensive coding)

- Always null-check every pointer returned by a Nexus API call before use.
- Always clean up resources and deregister keybinds in `AddonUnload()`.
- Never block the render thread; use async work for any delay.
- Validate keybind identifiers and config values before acting on them.

## Security audit (run at session cleanup)

Before the final commit of any session that created or modified C++, audit the
changed code (also on request: "security audit" / "harden"):

1. Null-pointer checks on every API return value.
2. Buffer safety: validate string lengths, use safe string functions.
3. Memory management: clean up all resources in `AddonUnload()`, no leaks.
4. Input validation on keybind identifiers and config values.
5. ToS: confirm no unattended automation or looping multi-action macros (1:1).
6. DLL safety: no thread blocking, proper async handling.

Report findings as Critical / High / Medium / Low with the fixes applied.
