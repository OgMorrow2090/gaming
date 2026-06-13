---
name: Nexus loads addon DLLs from addons/ ROOT only — NOT subdirectories
description: Nexus scans `addons/*.dll` flat, not recursively. A DLL placed in `addons/MysticAI/mystic-ai.dll` is silently never loaded. Subdirectories are reserved for per-addon config and assets read via Paths_GetAddonDirectory, not for the addon binary. Every sibling addon (mystic-clicker, mystic-trading, ArcDPS, …) sits at the root.
metadata:
  type: project
---

<!-- markdownlint-disable MD041 MD032 -->

## The rule

**`addons/<name>.dll`** — the DLL lives at the addons root.

**`addons/<Name>/`** — a same-named subdir for per-addon assets, config,
icons, etc. The addon code reads from here via `Paths_GetAddonDirectory()`.

Nexus does **not** scan subdirectories for DLLs. A DLL placed under a
subdir is silently never loaded; there is no warning, no log line in
Nexus's loader, the addon just isn't there.

## How this bit us (2026-05-21)

The original Mystic AI Phase 1 plan assumed `addons/MysticAI/mystic-ai.dll`
was the right path because Mystic AI has its own subdir for assets. The
deploy script `scripts/deploy-mystic-ai-dll.sh` and three different
session-internal hand-deploys all wrote to that subdir. Hash checks
matched against the CI artifact every time — the file on disk was always
the correct version. But GW2 silently never loaded the addon.

Symptoms while the bug was live:

- "I deploy a fix but the bug is still there" — nothing fixed because
  nothing new was running. Survived versions 1.1.14 → 1.1.17.
- Mystic AI didn't appear in the Nexus addon list.
- `grep mystic-ai /proc/$(pgrep Gw2-64.exe)/maps` came back empty.
- An older 1.1.13 DLL that *was* at `addons/mystic-ai.dll` from a much
  earlier session was the only one ever loaded. We mistook it for a
  "stray" and moved it aside, leaving the subdir-only deploys with
  nothing to override and silencing the addon completely.

## Detection recipe

If "the deploy didn't take" / "the addon vanished":

```bash
# 1. Is the addon DLL even in the addon's process map?
ssh host 'grep -i <addon> /proc/$(pgrep -x Gw2-64.exe)/maps | head -1'
# Empty -> Nexus never loaded it.

# 2. Confirm the DLL is at addons/ root (not in a subdir):
ssh host 'ls -la "<addons-path>/<name>.dll"'
# No such file -> the DLL is in the wrong place. Move/copy to root.

# 3. Sanity check against a working sibling:
ssh host 'ls "<addons-path>/" | grep \.dll$ | head'
# Every one of these is at root — same convention applies.
```

## Fixed pattern

`scripts/deploy-mystic-ai-dll.sh` now writes to `addons/mystic-ai.dll`
(root) — identical to `deploy-mystic-clicker-dll.sh`. Per-resolution
config files still live in `addons/MysticAI/mystic-ai-<W>x<H>.cfg` (the
addon writes them via `Paths_GetAddonDirectory`) — that subdir is
useful, just not for the DLL itself.

## Fossil cleanup (2026-06-13)

Root-only deploys do **not** remove a pre-existing subdir copy. Deploying
v1.1.23 to `addons/mystic-ai.dll` (root) left the old `addons/MysticAI/mystic-ai.dll`
(`51d4f270…`, 2026-05-21, 268800 B) sitting there — a ~3-week-old fossil from the
pre-consolidation era. Harmless (Nexus doesn't load it), but a 3-agent adversarial
verify flagged it as a shadow risk, so it was renamed to
`mystic-ai.dll.shadow-removed-<ts>`. **Lesson: after any DLL deploy, run a `find`
shadow check, not just the root hash check:**

```bash
ssh host 'find "<GW2>" -iname "mystic-ai.dll"'   # must return exactly ONE path (the root)
```

The Deck likely carries the same 05-21 fossil — check + clean it when deploying there.

## Related

- [[mystic-clicker-build-and-deploy]] — Mystic Clicker has always
  deployed to `addons/mystic-clicker.dll` (root). That's the convention
  to mirror.
- [[feedback_repo_first_then_deploy]] — same family of "the live state
  doesn't match the repo intent". This one was harder to spot because
  the repo file and the deployed file did match each other; what didn't
  match was "where Nexus looks for it".
