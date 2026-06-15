# Guild Wars 2 Addons — AI Agent Instructions

<!-- markdownlint-disable MD022 MD024 MD031 MD032 MD036 MD040 MD060 -->

This repo holds three Guild Wars 2 Nexus addons plus the Bazzite/Steam Deck
gaming-PC configs that run them. Start by reading `readme.md` for full project
context before changing code.

## Critical rules

- Always edit in the Git repo first, then deploy. The repository is the single
  source of truth for all code and configs; never edit live Bazzite/Steam Deck
  files directly (see Build and deploy below).
- Always commit and push after every change; `git pull --rebase` first.
- Lowercase filenames only, hyphens for spaces (`input-sim.cpp`, `mystic-ai.dll`).

## The three modules

Each addon lives under `modules/<addon>/` and ships as one Windows DLL. They
share a common shape (`entry.cpp` exports `GetAddonDef()`, `keybinds.cpp` holds
`ProcessKeybind()`, `shared.h` holds globals + identifier constants).

| Module | Directory | What it does |
|--------|-----------|--------------|
| mystic-clicker | `modules/mystic-clicker/` | Hotkeys that click GW2 inventory/UI buttons (deposit, sort, open chest, vendor) |
| mystic-trading | `modules/mystic-trading/` | Trading-post dashboard / flip-list / delivery windows |
| mystic-ai | `modules/mystic-ai/` | On-screen capture + OCR + Claude vision read-aloud helpers |

Each module has a matching `<module>.vcxproj` at the repo root; the solution is
`guildwars2-addons.sln`. The Nexus API header is `include/Nexus.h`.

### Common file layout per module

| File | Purpose |
|------|---------|
| `entry.cpp` | DLL entry, `GetAddonDef()` export, `AddonLoad`/`AddonUnload` |
| `keybinds.cpp` | `ProcessKeybind()` handler |
| `shared.h` | Global state, `APIDefs` pointer, identifier constants |
| `config.cpp` | Config file handling (where present) |
| `assets/` / `resources.rc` | Quick Access icons and embedded resources |

## Platform and technical context

- The addons are Windows DLLs: GW2 and the Raidcore Nexus loader are Windows
  binaries, so the DLLs are compiled for Windows (`x64`, MSVC v143).
- The deploy target is a Linux gaming PC — `shaun-bazzite`
  (`172.16.100.212`), an image-based Bazzite host. GW2 + Nexus + the addon DLLs
  run there under Proton. There is no container layer; deploy is SSH/scp direct
  to the host. A Steam Deck (`172.16.100.95`) is a secondary target.
- Language: C++17 with C compatibility. UI framework: ImGui (provided by Nexus).

## Build and deploy

Build runs in CI, not on a local Windows box.

- Push to `main` triggers `.github/workflows/build.yml` on a `windows-latest`
  runner, which builds each `mystic-*.vcxproj` as `Release|x64` via MSBuild and
  uploads `bin/Release/<module>.dll` as a per-module artifact.
- Deploy pulls that artifact and copies the DLL onto the Bazzite host (and the
  Steam Deck) over SSH. Run from a LAN machine (Mac or wednesday) — GitHub
  runners cannot reach the `172.16.100.x` private LAN.

```bash
./scripts/deploy-mystic-clicker-dll.sh        # latest CI build → bazzite + deck
./scripts/deploy-mystic-ai-dll.sh             # same for mystic-ai
./scripts/deploy-mystic-clicker-dll.sh --local <path/to.dll>   # use a local build
```

Deploy scripts refuse to run while GW2 is open on any reachable target, back up
each existing DLL with a timestamp, and verify the SHA256 per target. A Mac
launchd watcher (`scripts/watch-mystic-clicker.sh`) auto-deploys new CI builds
every ~5 min. Nexus loads DLLs from the `addons/` root only, never from
subdirectories — see `memory/nexus-dll-shadow-load-from-addons-root.md`.

Nexus configs (InputBinds, AddonConfig, GameBinds) are deployed separately from
their canonical copies in `configs/gw2-keybinds/`:

```bash
./scripts/deploy-nexus-config.sh --all        # InputBinds + AddonConfig + GameBinds
```

Addon-file changes generally deploy to every GW2 profile dir, not just one —
see `CLAUDE.md` and `memory/nexus-multi-deploy-rules.md`.

## Topic rules (load on demand)

Detailed conventions live in `.claude/rules/` and load automatically for
matching paths:

| Rule file | Covers | Loads for |
|-----------|--------|-----------|
| `.claude/rules/nexus-api-patterns.md` | `GetAddonDef`, `AddonLoad`, keybind registration, input sim, MumbleLink, API safety | `modules/**/*.cpp`, `include/Nexus.h` |
| `.claude/rules/tos-safety.md` | ArenaNet ToS (1:1 input), combo-bind nuance, defensive coding, security audit | `modules/**/*.cpp`, `include/Nexus.h` |
| `.claude/rules/keybind-naming.md` | Reserved identifiers, naming conventions, chord/game-bind collisions | `modules/**/*.cpp`, `include/Nexus.h` |

## Session start and cleanup

- Start / cleanup follow the centralized dev-toolkit checklists referenced from
  `CLAUDE.md`. On cleanup: run the security audit (`.claude/rules/tos-safety.md`),
  remove debug/temp code, update `changelog.md` / `readme.md` / `roadmap.md`,
  bump `AddonDef.Version` if code changed, build, deploy if changed, then commit
  and push everything including `memory/`.
- Never create VS Code tasks.

## Reference documentation

| Document | Purpose |
|----------|---------|
| [Nexus Wiki](https://github.com/RaidcoreGG/Nexus/wiki) | Official API documentation |
| [Nexus API](https://github.com/RaidcoreGG/Nexus/wiki/API) | Function reference |
| [Addon Quickstart](https://github.com/RaidcoreGG/Nexus/wiki/Addon-Quickstart) | Getting started guide |
| [GW2 Compass](https://github.com/RaidcoreGG/GW2-Compass) | Example addon |
