---
name: GW2 GameBinds.xml chord-key collisions to avoid
description: User has Home/End/PageUp/PageDown bound to Skill Profession 2-5 in GameBinds.xml. GW2 ignores chord modifiers — bare key match wins. Always grep gamebinds.xml before picking a chord key for a Steam Input wheel slot or DPad activator.
type: project
---

<!-- markdownlint-disable MD041 -->

GW2 reads `<action ... button="N" ... />` entries in `GameBinds.xml` as bare-key bindings: pressing the key triggers the action **regardless of which modifiers are held**. There is no `Ctrl`/`Shift`/`Alt` field in the XML.

That means a Steam Input chord like `Ctrl+Shift+Home` will:

1. Reach Nexus as `Ctrl+Shift+Home` (good — matches `RESET_WINDOWS` in InputBinds.json)
2. Reach GW2 as `Home` (because `Ctrl` and `Shift` are dropped from GW2's match logic) — and trigger whatever action the user has on `Home`

If the user has `Home` bound to a skill, the chord fires both the Nexus addon AND the skill — visible as a "skill recharging" toast on every chord press.

## This user's GameBinds.xml bindings worth knowing

| Key | Code | GW2 action |
| --- | ---- | ---------- |
| Home | 36 | Skill Profession 5 |
| End | 35 | Skill Profession 4 |
| PageDown | 34 | Skill Profession 3 |
| PageUp | 33 | Skill Profession 2 |

These four keys are off-limits for Mystic Clicker chord output. Always check `gamebinds.xml` for `button="N"` before picking a chord key. Verified safe (unbound) on this user's setup as of 2026-04-26: `Insert (45)`, `Delete (46)`, `F11 (122)`, `F12 (123)`, `Numpad 0`, `Numpad Multiply / Plus / Minus / Divide / Decimal`. Numpad 1–9 mostly bound to `((WeaponSwap))` / `((MoveForward))` etc — check before using.

**Why:** v18.4.7 wheel slot 2 (Reset Windows) emitted `Ctrl+Shift+Home`. Worked correctly on the addon side but every press also fired SkillProfession5 in GW2, leaving the "skill recharging" toast. Fixed by switching the chord to `Ctrl+Shift+Insert` (Code 45), which is unbound across all of the user's GW2 bindings.

**How to apply:** before picking any chord key for a new Mystic Clicker combo or wheel slot, run:

```bash
grep -E 'button="<scancode>"' configs/gw2-keybinds/gamebinds.xml
```

If it returns a `<action ...>` entry, pick a different key. The Python helper in `docs/vdf-editing-golden-rules.md` Rule #11 also checks Nexus InputBinds collisions; both checks should pass before deploying a chord.
