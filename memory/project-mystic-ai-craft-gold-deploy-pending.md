---
name: project-mystic-ai-craft-gold-deploy-pending
description: DEFERRED — Mystic AI "Craft for Gold" research section + title-case headings are coded/committed/pushed and the daemon is live on bazzite, but the DLL deploy to bazzite + Deck is PENDING a GitHub auth path on wednesday (user setting up a 1Password service account).
metadata:
  type: project
---

# Mystic AI — Craft-for-Gold research + title-case headings (DLL deploy pending)

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

## State as of 2026-06-13 (end of session)

- **Code:** committed + pushed — `7e779f5` on `OgMorrow2090/gaming` main.
  - `scripts/gw2-claude-daemon.py` — `RESEARCH_SYSTEM` extended with the new section + rules.
  - `modules/mystic-ai/overlay.cpp` — `TitleCaseHeader()` helper, `CRAFT FOR GOLD`
    colour, header rendered title-cased (`<cctype>` added).
- **CI:** build.yml run for `7e779f5` = **success** (artifact `mystic-ai` built).
- **Daemon:** DEPLOYED + restarted on bazzite (`systemctl --user restart
  gw2-claude-daemon.service`), hash-verified. The Craft-for-Gold section is
  **live in research output now** — on the OLD DLL it renders in default accent
  colour + caps (graceful); gold colour + title-case need the new DLL.
- **DLL deploy: NOT DONE** — blocked, see below.

## The blocker — GitHub auth on wednesday

`deploy-mystic-ai-dll.sh` runs `gh run download` to pull the compiled artifact.
wednesday has git via the SSH deploy key (`github-gaming` alias) — enough for
`git push`/`pull` and the daemon scp deploy — but the **Actions artifact API
needs a gh/OAuth or PAT token, NOT the SSH key**. Anonymous download = 401.

- **gh installed this session:** `gh 2.94.0` at `~/.local/bin/gh` (linux_arm64,
  no sudo). NOT yet authenticated.
- **op (1Password CLI):** installed (`/usr/bin/op` v2.34.0) but **not signed in**
  — `~/.config/op/config` has `accounts: null`, no `OP_SERVICE_ACCOUNT_TOKEN` in
  env or `.env`. The PAT lives in a 1Password vault ("common" or "wednesday").
- **User action (next session):** Shaun is setting up a **local 1Password service
  account** so I can read the PAT non-interactively.

## Next-session runbook

1. Confirm op auth: `export OP_SERVICE_ACCOUNT_TOKEN=...` (Shaun's SA) then
   `op whoami`. Get the exact PAT item ref (GitHub PAT, scope: Actions read +
   Contents read on `OgMorrow2090/gaming`).
2. Auth gh: `gh auth login --git-protocol ssh --with-token <<< "$(op read 'op://<vault>/<item>/credential')"`
   then `gh auth status`.
3. **Verify GW2 is CLOSED** on bazzite (`ssh Og@172.16.100.212 pgrep -f Gw2-64.exe`)
   — the deploy script refuses while GW2 runs. It was RUNNING at 14:xx on 06-13.
4. Deploy: `cd /srv/docker-data/repos/gaming && ./scripts/deploy-mystic-ai-dll.sh`
   (pass the run-id for `7e779f5` explicitly to avoid the stale-success race noted
   in chat.md ~line 1296). Writes to `addons/mystic-ai.dll` ROOT (not subdir — see
   [[nexus-dll-shadow-load-from-addons-root]]).
5. **Verify on-disk DLL hash == CI artifact** before claiming done (chat.md lesson:
   a "close GW2 and redeploy" cycle once left the old DLL live).
6. **Deck:** still powered off as of 06-13 — Shaun will turn it on later. Re-run the
   same deploy (it targets bazzite + deck-native) once the Deck is reachable
   (`deck@172.16.100.95`). See [[bazzite-deploy-access-from-wednesday]].

See [[gw2-claude-vision]] for the daemon architecture.
</content>
