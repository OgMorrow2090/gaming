---
paths:
  - 'modules/**/*.cpp'
  - 'include/Nexus.h'
---

# Nexus API Patterns

How a Mystic addon talks to the Raidcore Nexus loader. The full API surface is
declared in `include/Nexus.h`. Every module under `modules/<addon>/` follows the
same shape; read the real code rather than copying snippets.

## Addon definition export

Each addon exports `GetAddonDef()`, which Nexus calls to register it. The
signature must be unique and negative for non-Raidcore addons (Nexus stores it
as a `uint32_t`, so the modules cast the negative literal).

| Addon | `GetAddonDef()` | Signature |
|-------|-----------------|-----------|
| mystic-clicker | `modules/mystic-clicker/entry.cpp:51` | `-54321` (`entry.cpp:54`) |
| mystic-trading | `modules/mystic-trading/entry.cpp:57` | `-54322` (`entry.cpp:59`) |
| mystic-ai | `modules/mystic-ai/entry.cpp:50` | `-54323` (`entry.cpp:54`) |

When adding a new addon, pick the next free signature and document it in that
addon's `entry.cpp` comment block (see `modules/mystic-ai/entry.cpp:52`).

## AddonLoad and the ImGui context

`AddonLoad()` receives the `AddonAPI_t*`, stashes it in the module's global
`APIDefs` pointer, then adopts the Nexus-owned ImGui context and allocator
before any rendering. Reference implementation:

- `modules/mystic-trading/entry.cpp:81` (`SetCurrentContext` + `SetAllocatorFunctions` at `entry.cpp:87`)
- `modules/mystic-ai/entry.cpp:80`

Always set the ImGui context and allocator functions before calling any ImGui
function, or the addon will crash on first render.

## Keybind registration and handling

Keybinds use the `InputBinds_*` API. Identifiers are defined as `constexpr`
string constants in each module's `shared.h`, registered in `AddonLoad()`, and
deregistered in `AddonUnload()`.

- Identifier constants: `modules/mystic-trading/shared.h:22`, `modules/mystic-clicker/shared.h:43`, `modules/mystic-ai/shared.h:17`
- Registration: `modules/mystic-trading/entry.cpp:110` (`InputBinds_RegisterWithString`), `modules/mystic-ai/entry.cpp:119`, mystic-clicker `entry.cpp:143`
- Deregistration: `modules/mystic-trading/entry.cpp:135`, mystic-clicker `entry.cpp:262`
- Handler: `ProcessKeybind()` at `modules/mystic-clicker/keybinds.cpp:35`, `modules/mystic-trading/keybinds.cpp:9`, `modules/mystic-ai/keybinds.cpp:23`

The handler must early-return on key release and act only on press; see the
top of `modules/mystic-clicker/keybinds.cpp:35`.

Keybind naming has its own hard rules (reserved identifiers, prefixes) — see
`.claude/rules/keybind-naming.md`.

## Input simulation

Mystic Clicker drives the game window directly. Real call sites:

- `SimulateKeyPress` (single down+up): `modules/mystic-clicker/input-sim.cpp:131`
- `SendInput` real OS-level events: `modules/mystic-clicker/item-name.cpp:370` onward
- `ClientToScreen` coordinate mapping: `modules/mystic-clicker/input-sim.cpp:94`

Every simulated input must be a single 1:1 action per keypress. Multi-action
behaviour is a ToS concern — see `.claude/rules/tos-safety.md`.

## MumbleLink data

GW2 exposes the MumbleLink shared-memory struct via `DataLink_Get("DL_MUMBLE_LINK")`.
Mystic Clicker reads it to detect chat focus and the character name:

- Struct + accessor: `modules/mystic-clicker/item-name.cpp:62` (`DataLink_Get` at `item-name.cpp:85`)

Always null-check the pointer from `DataLink_Get` before dereferencing.

## Safety rules for API use

- Always validate every pointer returned by an API call before use (textures,
  resources, MumbleLink, ImGui context).
- Always deregister keybinds and free resources in `AddonUnload()`.
- Never block the render thread; use async work for any delay.
- Use Nexus logging (`APIDefs->Log(ELogLevel_*, "<channel>", ...)`) for
  diagnostics instead of writing files.

## Reference documentation

- Nexus Wiki: <https://github.com/RaidcoreGG/Nexus/wiki>
- API reference: <https://github.com/RaidcoreGG/Nexus/wiki/API>
- Addon quickstart: <https://github.com/RaidcoreGG/Nexus/wiki/Addon-Quickstart>
- Example addon: <https://github.com/RaidcoreGG/GW2-Compass>
