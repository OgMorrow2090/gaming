---
name: Steam Input touch/radial menus cap at 16 buttons
description: A radial_menu group with more than 16 touch_menu_button entries makes Steam Input silently reject the ENTIRE controller layout — no error, it falls back to the previous config. Keep every wheel at <=16 slots.
type: reference
---

<!-- markdownlint-disable MD041 -->

Steam Input `radial_menu` / touch-menu groups have a hard ceiling of **16
buttons** (`touch_menu_button_1` .. `touch_menu_button_16`). Add a 17th and
Steam's loader **silently rejects the whole layout** — no log line, no toast —
and falls back to whatever config was loaded before. Same silent-drop failure
mode as a structurally broken VDF (see `docs/vdf-editing-golden-rules.md`).

## How it bit us (controller v20, 2026-05-18)

v20 expanded the "Utility Wheel" to 17 slots to add Home Instance Portal. The
VDF was structurally perfect — brackets balanced, 56 groups, deployed to the
right file, and Steam even logged `Loaded Config ... controller_neptune.vdf`.
But the layout never appeared in-game: the user kept seeing the old "Og v19.9".
Steam read the file, hit the 17-button wheel, and dropped the whole layout.

Confirming evidence: every other radial menu in the user's own config tops out
at exactly 16 ("Commands" wheel = 16). Nothing was ever above 16. The fix was
to keep the wheel at 16 — replace an item (Community LFG → Home Instance
Portal), not append a 17th slot.

## Rule

- A wheel that's "full" at 16 cannot grow. To add an item you must **replace**
  an existing one, not append a 17th.
- After any wheel edit, count `touch_menu_button_*` per `radial_menu` group and
  assert <= 16 before deploying.
- "Layout loads per the Steam log but the change isn't visible in-game" =
  suspect a silent semantic rejection (16-button cap, bad group ref, url
  scheme mismatch) — not a deploy-path problem.

Related: [[vdf-editing-rules]], [[controller-vdf-deploy-checklist]],
`docs/vdf-editing-golden-rules.md`.
