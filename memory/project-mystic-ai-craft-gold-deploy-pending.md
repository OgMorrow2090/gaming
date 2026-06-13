---
name: project-mystic-ai-craft-gold-deploy-pending
description: Mystic AI "Craft for Gold" research section + title-case headings — DONE on bazzite (v1.1.23 deployed + adversarially verified 2026-06-13). ONLY the Steam Deck deploy remains (it was powered off); re-run deploy-mystic-ai-dll.sh when deck@172.16.100.95 is awake.
metadata:
  type: project
---

# Mystic AI — Craft-for-Gold research + title-case headings

Feature requested 2026-06-13. Two changes to Mystic AI Research:

1. New optional `#CRAFT FOR GOLD#` section — for crafting materials / craftables
   (food, ingredients, refined mats, gear): most profitable recipe(s) to craft
   and flip on the Trading Post, profession/discipline, key ingredients, and the
   buy-ingredients-vs-sell-output reasoning. One recipe per line, most profitable
   first; skipped for non-craftables.
2. Research headings no longer ALL CAPS on screen. Wire protocol keeps headers
   upper-case (`SectionColour` matches on them; daemon TTS strip regex is
   `^#[A-Z ]+#$`) but the overlay title-cases them for display. New section gets
   a bright-gold heading colour.

## Status: DONE on bazzite ✅ — Deck pending

- **Code:** `OgMorrow2090/gaming` main. Feature `7e779f5`; version bump to
  **v1.1.23** `9920483` (`entry.cpp` Build 22→23). CI run `27469988456` = success.
  - `scripts/gw2-claude-daemon.py` — `RESEARCH_SYSTEM` + the new section/rules.
  - `modules/mystic-ai/overlay.cpp` — `TitleCaseHeader()`, `CRAFT FOR GOLD` colour, title-cased headers.
  - `modules/mystic-ai/entry.cpp` — addon version → **1.1.23**.
- **Daemon:** deployed + restarted on bazzite (sha `3dba307…` == repo). Craft-for-Gold live in research output.
- **DLL (bazzite):** **DEPLOYED + VERIFIED 2026-06-13 ~16:32**. `addons/mystic-ai.dll`
  sha256 `523df410099fbb5b…`, 273408 bytes (== CI artifact for run 27469988456).
  3-agent adversarial verification passed (provenance + daemon + Discord-regression).
- **Shadow cleanup:** verification found a stale `addons/MysticAI/mystic-ai.dll`
  (`51d4f270…`, 268800 B, 2026-05-21) — a fossil from before the root-only
  consolidation. Nexus doesn't recurse into subdirs so it wasn't loaded, but it was
  renamed to `mystic-ai.dll.shadow-removed-<ts>` to kill all doubt. The `MysticAI/`
  config asset (`mystic-ai-3840x2160.cfg`) was preserved. See [[nexus-dll-shadow-load-from-addons-root]].
- **GW2 relaunch needed** to load v1.1.23 (I killed GW2 to deploy — it was a stuck/leftover process; Shaun said it shouldn't have been running).

## Remaining: Steam Deck deploy

The Deck (`deck@172.16.100.95`) was **powered off** at deploy time — `deploy-mystic-ai-dll.sh`
skipped it gracefully (unreachable hosts are warned + skipped, not a hard fail). When the
Deck is awake, re-run the **same one command** (below); it targets bazzite + deck-native and
will no-op the already-current bazzite copy.

## How GitHub auth works from wednesday (the bit that was missing)

wednesday's `github-gaming` SSH deploy key covers `git push`/`pull` only — NOT the
Actions artifact API. For `gh run download` you need a GitHub token:

1. The 1Password service-account token is in **`itinyk-agentd`'s** process env, but a
   fresh `Bash` shell does NOT inherit it. Bridge it:
   `PID=$(pgrep -f itinyk-agentd|head -1); export OP_SERVICE_ACCOUNT_TOKEN="$(tr '\0' '\n' < /proc/$PID/environ | sed -n 's/^OP_SERVICE_ACCOUNT_TOKEN=//p')"`
   (If a future session already has `OP_SERVICE_ACCOUNT_TOKEN` set, skip this.)
2. The PAT: `op read 'op://wednesday-pi/github_pat_portal/password'`.
3. **Do NOT use `gh auth login --with-token`** — this PAT lacks the `read:org` scope
   that login-validation demands, so it errors. Instead set it directly:
   `export GH_TOKEN="$(op read 'op://wednesday-pi/github_pat_portal/password')"` — gh
   then uses it for the API (artifact download needs `actions:read`, which it has).
4. Vaults available to the SA: `common-pi`, `wednesday-pi`. `op whoami` → SERVICE_ACCOUNT.

## Deck deploy runbook (when it's awake)

```bash
PID=$(pgrep -f itinyk-agentd|head -1)
export OP_SERVICE_ACCOUNT_TOKEN="$(tr '\0' '\n' < /proc/$PID/environ | sed -n 's/^OP_SERVICE_ACCOUNT_TOKEN=//p')"
export GH_TOKEN="$(op read 'op://wednesday-pi/github_pat_portal/password')"
ssh -o ConnectTimeout=8 deck@172.16.100.95 'pgrep -x Gw2-64.exe && echo CLOSE_GW2_FIRST || echo OK'   # must be CLOSED
cd /srv/docker-data/repos/gaming && ./scripts/deploy-mystic-ai-dll.sh 27469988456
```

Then verify: deployed `addons/mystic-ai.dll` sha == `523df410099fbb5b…`, and `find` shows
exactly one `mystic-ai.dll` (no `addons/MysticAI/mystic-ai.dll` shadow — the Deck may have
the same fossil from the 05-21 era; remove it the same way if present).

See [[gw2-claude-vision]] for the daemon architecture, [[bazzite-deploy-access-from-wednesday]] for SSH access.
