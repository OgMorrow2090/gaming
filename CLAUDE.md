# Claude Code Instructions

Read and follow `agents.md` in this repository root for full project context, conventions, and workflows.

## Session Start

When the user says "start", "start session", or "begin session", read and follow the centralized checklist:
`/Users/shaunchittenden/Developer/GitHub/itinyk/dev-toolkit/session-start-checklist.md`

## Session Cleanup

When the user says "cleanup", "session end", or "synchronize", read and follow the centralized checklist:
`/Users/shaunchittenden/Developer/GitHub/itinyk/dev-toolkit/session-cleanup-checklist.md`

**Additionally**: Always update `memory/MEMORY.md` with any new stable patterns, conventions, or architectural decisions discovered during the session. This is separate from `memory/chat.md` (which logs session activity). MEMORY.md should capture reusable knowledge for future sessions. All memory files live in the `memory/` directory inside the repo and must be committed.

**Memory merge rule**: Always write memory files to the repo `memory/` directory — never to `~/.claude/projects/*/memory/`. If local memory is found at that path, merge any unique content into the repo `memory/` directory, then delete the local copy. The repo is the single source of truth for all memory.

**Never exclude memory from commits**: All `.md` files in `memory/` (MEMORY.md, chat.md, planning docs, project notes) must always be committed and pushed. These files are shared across devices — never leave them as local-only or excluded from git.

## Key Rules

- Source of truth: the Git repository for all code and configurations
- Always commit and push after every change — never leave uncommitted work
- Lowercase filenames only
- Platform: Windows only (DLL-based Nexus addon)
- Language: C++ with Visual Studio 2022
- **Dual GW2 installs on bazzite**: changes to GW2 addon files (`addons/Nexus/*`, `addons/MysticClicker/*`, etc.) must deploy to BOTH profile dirs (`~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/` AND `~/Games/gw2-appletv/addons/`) unless the change is explicitly per-profile (UI position, screen-size-specific). Symlinked files (`Gw2.dat`, `bin64/`, `d3d11.dll`) auto-propagate; the entire `addons/` dir does NOT. See `memory/nexus-dual-deploy-rules.md`.
