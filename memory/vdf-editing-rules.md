---
name: VDF editing rules
description: Hard rules for editing Steam Controller .vdf files in this repo — never bulk-rewrite, always validate, always deploy under a new filename
type: feedback
---

When editing any `.vdf` in `configs/steam-controller/`:

1. **Never bulk-rewrite blocks with regex / `re.sub` / scripted block replacement.** Use line-anchored `Edit` tool calls only.
2. **Always validate after editing**: bracket balance, group count, binding count, line/byte count. Compare to the last working version — a >5% drop is a regression.
3. **Deploy under a new filename**, never overwrite an in-use Personal save. Patch `url` to `usercloud://moonlight/<name>_0` matching the new filename.
4. **Mirror DLL + VDF version bumps in the same commit**, since GitHub Actions builds the DLL on push.

**Why:** v18.4 outage on 2026-04-26 — a Python regex rewrite of `dpad_south`/`dpad_east` blocks in `moonlight-gw2-og-v16.1.vdf` greedily consumed Long_Press/Double_Press activators across multiple groups, dropping ~25 subgroups (file went 63 121 → 37 170 bytes, 296 → 158 bindings). Steam Deck silently dropped the layout — no error, no log, Personal tab empty. Recovery took an entire session.

**How to apply:** before any VDF edit, read `docs/vdf-editing-golden-rules.md` in this repo. After any VDF edit, run the four validation checks listed there. If unsure, revert and use a smaller edit.
