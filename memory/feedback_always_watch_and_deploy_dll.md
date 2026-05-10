---
name: AI must always watch and deploy the Mystic Clicker DLL
description: After any push that triggers a CI build of mystic-clicker.dll, verify CI succeeded and the watcher deployed to all 4 profiles before reporting "done"
type: feedback
---

After any commit that touches `modules/mystic-clicker/**` or `mystic-clicker.vcxproj`:

1. **Wait for CI** — `gh run list --branch main --limit 1` until the run is `completed success` (or fail loudly if it failed).
2. **Verify watcher deployed** — tail `/tmp/gw2-mystic-clicker-watcher.log` for a `deploy OK: <commit-sha>` line matching the new HEAD.
3. **Confirm hash on all 4 profiles** match the CI artifact hash:
   - `local`     — Og@172.16.100.212:`/home/Og/.local/share/Steam/steamapps/common/Guild Wars 2/addons/mystic-clicker.dll`
   - `apple-tv`  — Og@172.16.100.212:`/home/Og/Games/gw2-appletv/addons/mystic-clicker.dll`
   - `deck-bazzite` — Og@172.16.100.212:`/home/Og/Games/gw2-deck/addons/mystic-clicker.dll`
   - `deck-native` — deck@172.16.100.95:`/home/deck/.local/share/Steam/steamapps/common/Guild Wars 2/addons/mystic-clicker.dll`
4. **Only then** notify the user it's ready to test.

The watcher polls every 300s, so steps 2–3 may need a wait. If urgent, run `~/Developer/GitHub/itinyk/dev-toolkit/scripts/deploy-mystic-clicker-dll.sh` manually instead of waiting for the next tick.

**Why:** User explicitly asked for this guarantee on 2026-05-10 ("AI must always watch and deploy the DLL") — they were burned by reporting "DLL pushed" while the actual deployed DLL on bazzite was still an older hash. The watcher exists precisely to remove that gap; failing to verify it is undermines the whole pipeline.

**How to apply:** Whenever the conversation includes a `git push` (or CI-triggering commit) for mystic-clicker code, do not call the work complete until CI artifact hash == bazzite/Deck deployed hash on all 4 profiles. Cite the matching hashes in the user-facing summary.

**Don't apply when:** The commit only touches scripts, configs, or docs (no DLL rebuild). Use `git diff --name-only` to scope.
