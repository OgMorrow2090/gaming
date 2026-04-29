---
name: start
description: Bootstrap a guildwars2 session — Bazzite/Sunshine/Moonlight configs for shaun-bazzite
disable-model-invocation: true
---

Start a session for the **guildwars2** project.

## Project quick context

- Gaming PC configs — Bazzite OS, Sunshine streaming server, Moonlight client setup, Steam Deck controller scripts.
- **Target host: shaun-bazzite (172.16.100.212).** Deploys via SSH/rsync directly to that host — there is no container layer.
- Many scripts are interactive (xdotool / UI manipulation / controller mappings) — be aware they assume a graphical session is up; remote SSH-only invocation will fail without `DISPLAY=:0`.
- Bazzite is image-based — system changes that should persist must go via `rpm-ostree` or layered overlays, not direct `/etc` edits.
- Local Mac has `chrome-devtools-mcp` + `mobile-mcp` (iPhone 17 Pro sim) installed for any cross-cutting web UI verification — full setup at `Shauns-MacBook-Air-72196` only; Mac mini parity pending.

## Steps

1. Run the central checklist: `/Users/shaunchittenden/Developer/GitHub/itinyk/dev-toolkit/session-start-checklist.md`.
2. **Also scan `memory/MEMORY.md` for deferred multi-session projects.** These don't show up in the most recent session's "Open Items" — surface them explicitly in the status report.
3. After the checklist, suggest: `/rename guildwars2`.
