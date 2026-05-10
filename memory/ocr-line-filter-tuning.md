---
name: OCR line filter tuning for GW2 destroy-popup item names
description: After yellow-filter + tesseract on the destroy popup, real item-name lines are mixed with noise from inventory icons + character-name overlays. Heuristics that distinguish them.
type: reference
---

Tesseract output for the destroy-confirm popup contains:

- The real item name (often split across 2 lines because GW2 word-wraps): `"Pact Fleet Axe"` + `"Skin"`, `"Holographic"` + `"Display Unit"`, `": Golem Right Arm"`
- Inventory-icon noise: `"fall cal call oa"`, `"ay of"`, `"el Ao Woe"`, `"a fro"`
- Player's character name from ArcDPS overlay: `"Morrowmage"`
- Stray decoration characters that decorate real item lines: `". "`, `": "`, `"* "`

## Filter pipeline (item-name.cpp)

Pre-process every line first: **strip leading/trailing non-letter characters**. Without this, real item lines like `": Golem Right Arm"` are rejected by the strict whitelist below.

Then accept the line iff ALL of:

1. Strict whitelist: only `[A-Za-z space ' -]` chars (after trim)
2. ≥3 letters total
3. Min word length ≥ 2 (rejects single-letter words like "a fro")
4. Single-word lines must be ≥4 letters (rejects "Woe" but accepts "Skin")
5. Multi-word lines: avg letters/word ≥ 3.0 (rejects "ay of", "fowl owl all ay po rae")
6. Lines containing a 2-letter word: avg ≥ 3.5 (catches "fall cal call oa" while keeping "Eye of Janthir" avg 4.0 and "Bag of Holding" avg 3.67)
7. **Doesn't contain the player's own character name** (canonicalised to lowercase letters-only and substring-matched). Read via `MumbleLinkedMem.name` from `APIDefs->DataLink_Get("DL_MUMBLE_LINK")`.

Rare loss: items with avg-word-length < 3.0 like hypothetical "Cup of Joe" (avg 2.67) get rejected. Practically nonexistent in GW2's item DB.

## Concatenation

Surviving clean lines are space-joined. GW2 wraps item names across two lines in the destroy dialog, so concatenation is necessary — `"Pact Fleet Axe" + "Skin"` → `"Pact Fleet Axe Skin"`.

## Why no auto-paste

Empirically `SendInput(Ctrl+V)` (whether `KEYEVENTF_SCANCODE` or VK-only) does **not** reach GW2's destroy-confirm textbox under the Apple TV stream + Steam Input chain. Manual Ctrl+V works fine. The macro writes the clipboard + raises a Nexus alert with the captured name; user pastes manually.
