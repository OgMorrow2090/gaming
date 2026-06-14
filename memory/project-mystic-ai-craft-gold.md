---
name: project-mystic-ai-craft-gold
description: COMPLETE 2026-06-14 — Mystic AI "Craft for Gold" research section + title-case headings, shipped as v1.1.23 to BOTH GW2 hosts (bazzite + Steam Deck), daemon + DLL, adversarially verified, shadow DLLs cleaned. Historical record / wednesday gh-auth recipe.
metadata:
  type: project
---

# Mystic AI — Craft-for-Gold research + title-case headings (COMPLETE)

Feature requested 2026-06-13, fully shipped 2026-06-14. Two changes to Mystic AI Research:

1. New optional `#CRAFT FOR GOLD#` section — for crafting materials / craftables
   (food, ingredients, refined mats, gear): most profitable recipe(s) to craft and
   flip on the Trading Post, profession/discipline, ingredients, craft-vs-sell-raw
   reasoning. One recipe per line; skipped for non-craftables. (daemon prompt)
2. Research headings no longer ALL CAPS — wire protocol stays upper-case
   (`SectionColour` match + TTS strip regex `^#[A-Z ]+#$`); overlay title-cases for
   display. New section gets a bright-gold heading colour. (overlay DLL)

## Shipped — v1.1.23 on BOTH hosts ✅

- **Code:** `OgMorrow2090/gaming` main — feature `7e779f5`, version bump `9920483`
  (entry.cpp Build→23). CI run `27469988456` = success, artifact hash `523df410…`.
  Files: `scripts/gw2-claude-daemon.py` (RESEARCH_SYSTEM), `modules/mystic-ai/overlay.cpp`
  (`TitleCaseHeader()` + CRAFT FOR GOLD colour), `modules/mystic-ai/entry.cpp` (v1.1.23).
- **bazzite** (`Og@172.16.100.212`, 2026-06-13): DLL `523df410…` + daemon `3dba307…`,
  3-agent adversarial verify passed, shadow `addons/MysticAI/mystic-ai.dll` cleaned.
  Confirmed working in-game by operator. Daemon auto-recovers on reboot
  (`Linger=yes`) — see [[gw2-claude-vision]].
- **Steam Deck** (`deck@172.16.100.95`, 2026-06-14): had been powered off on 06-13.
  Deployed DLL `523df410…` (273408 B) + updated daemon `3dba307…` (was stale `432c6bf9…`
  with NO Craft-for-Gold — DLL alone isn't enough, the daemon prompt must also be
  current), service active, shadow DLL cleaned. Both backed up first.

## Lessons captured (in their own memory files)

- The Deck runs its OWN host-side `gw2-claude-daemon` — daemon changes must deploy to
  BOTH hosts, not just bazzite. The DLL only does overlay rendering; the section CONTENT
  is the daemon prompt.
- Root-only DLL deploys leave a stale `addons/MysticAI/mystic-ai.dll` subdir fossil
  (both hosts had the 05-21 one). Nexus doesn't load subdirs so it's inert, but run a
  `find … -iname mystic-ai.dll` shadow check after every deploy. See
  [[nexus-dll-shadow-load-from-addons-root]].
- wednesday gh auth for `gh run download`: bridge `OP_SERVICE_ACCOUNT_TOKEN` from
  `itinyk-agentd`'s `/proc/<pid>/environ`, then `export GH_TOKEN="$(op read
  'op://wednesday-pi/github_pat_portal/password')"` — NOT `gh auth login` (PAT lacks
  `read:org`). See [[bazzite-deploy-access-from-wednesday]].

## Redeploy recipe (either/both hosts)

```bash
PID=$(pgrep -f itinyk-agentd|head -1)
export OP_SERVICE_ACCOUNT_TOKEN="$(tr '\0' '\n' < /proc/$PID/environ | sed -n 's/^OP_SERVICE_ACCOUNT_TOKEN=//p')"
export GH_TOKEN="$(op read 'op://wednesday-pi/github_pat_portal/password')"
cd /srv/docker-data/repos/gaming && ./scripts/deploy-mystic-ai-dll.sh <run-id>   # GW2 must be CLOSED on all reachable targets
# daemon changes: rsync scripts/gw2-claude-daemon.py to each host's ~/scripts/ + systemctl --user restart gw2-claude-daemon.service
```
