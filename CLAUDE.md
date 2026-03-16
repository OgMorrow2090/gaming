# Claude Code Instructions

Read and follow `agents.md` in this repository root for full project context, conventions, and workflows.

## Session Start

When the user says "start", "start session", or "begin session", read and follow the centralized checklist:
`/Users/shaunchittenden/Developer/GitHub/itinyk/dev-toolkit/session-start-checklist.md`

## Session Cleanup

When the user says "cleanup", "session end", or "synchronize", read and follow the centralized checklist:
`/Users/shaunchittenden/Developer/GitHub/itinyk/dev-toolkit/session-cleanup-checklist.md`

**Additionally**: Always update `memory/MEMORY.md` with any new stable patterns, conventions, or architectural decisions discovered during the session. This is separate from `memory/chat.md` (which logs session activity). MEMORY.md should capture reusable knowledge for future sessions. All memory files live in the `memory/` directory inside the repo and must be committed.

**Memory merge rule**: If local memory exists at `~/.claude/projects/*/memory/`, it is stale. Merge any unique content into the repo `memory/` directory, then delete the local copy. The repo is the single source of truth for all memory.

## Key Rules

- Source of truth: the Git repository for all code and configurations
- Always commit and push after every change — never leave uncommitted work
- Lowercase filenames only
- Platform: Windows only (DLL-based Nexus addon)
- Language: C++ with Visual Studio 2022
