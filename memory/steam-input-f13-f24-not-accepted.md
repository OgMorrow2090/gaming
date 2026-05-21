---
name: Steam Input silently rejects F13–F24 even though Nexus accepts them
description: F13–F24 are valid Windows scancodes that Nexus's keybind UI happily binds, but Steam Input's key_press enum does NOT accept them. The VDF parses without complaint and the chord activator loads, but at controller-attach time Steam logs "Failed to create digital binding" and the chord silently does nothing. Stick to F1–F12 plus Numpad tokens that are already proven to load in the active VDF.
metadata:
  type: project
---

<!-- markdownlint-disable MD041 MD032 -->

## What happened

Tried to give Mystic AI a "physically untypeable" key range by rebinding to
F13 / F14 / F15 — Nexus's `InputBinds.json` accepted those `Code` values
fine, but the Steam Input VDF (`key_press F13, ..., , `) was silently
rejected. Symptom: chord activator looked correct, file passed all four
VDF golden-rule checks (brackets, group count, binding count, diff), but
the chord did absolutely nothing in-game.

The smoking gun is in `~/.local/share/Steam/logs/controller.txt`:

```
[2026-05-21 10:23:34] Failed to create digital binding 'key_press F14, Mystic AI Capture, , ' for 1284210
[2026-05-21 10:23:34] Failed to create digital binding 'key_press F13, Mystic AI Read Book, , ' for 1284210
```

Same class of bug as [[steam-input-invalid-key-tokens]] (the
`KEYPAD_MINUS` case): Steam Input's `key_press` token list is a closed
enum, separate from Windows scancodes / VKs. An invalid token is parsed
and stored but never wired to anything.

## The two namespaces

| Layer | Code field | What it accepts |
|---|---|---|
| **Nexus `InputBinds.json` `Code`** | Windows scancode (e.g. `68` = F10, `30` = A, `124` = F13) | Anything the OS can send through the keyboard hook — includes F13–F24, Numpad, media keys, etc. |
| **Steam Input VDF `key_press TOKEN`** | Closed enum of token strings | `F1`–`F12`, letters, `KEYPAD_0`–`KEYPAD_9`, `KEYPAD_PLUS`, `LEFT_ALT/CONTROL/SHIFT`, `ESCAPE`, `RETURN`, `SPACE`, `HOME`, `INSERT`, `DASH`, `EQUALS`, `PERIOD`, `FORWARD_SLASH`, `BACK_TICK`, `LEFT_BRACKET`, `RIGHT_BRACKET`, and a few others. **`F13`–`F24` are NOT in the enum.** `KEYPAD_MINUS` is NOT in the enum. |

When picking a key for "chord-only, hard-to-press-by-accident": you need
a token that's in **both** namespaces. The intersection of "valid Steam
Input token" + "no other Nexus addon listening on its scancode" is
narrower than I assumed.

## How to verify a candidate before deploying

```bash
# Every key_press token currently in the active VDF — this is the proven-valid set
grep -oE 'key_press [A-Z][A-Z0-9_]*' configs/gw2-keybinds/controller_neptune.vdf \
  | awk '{print $2}' | sort -u
```

Tokens already in the file are guaranteed to load. Anything **not** in
that set is suspect — test in a dummy chord first, watch
`~/.local/share/Steam/logs/controller.txt` for a `Failed to create
digital binding` line.

## Recommended approach for unique chord-only keys

Combine a **proven Steam-Input token** with a **modifier** the user is
unlikely to hold during normal gameplay. E.g. on this controller layout
the existing `LEFT_ALT + F10` / `LEFT_SHIFT + F10` chords work fine and
nothing else in the VDF emits `F10`, so they are *already* chord-only in
practice — no rebind needed. The earlier "F13/F14" attempt was solving a
non-existent collision and broke the chord pipeline.

## Related

- [[steam-input-invalid-key-tokens]] — first known case
  (`KEYPAD_MINUS`). Same diagnostic: check `controller.txt` for
  "Failed to create digital binding".
- [[mystic-ai-keybind-collision-shift-d]] — historic; the analysis in
  that file conflated VK_D (= 68) with the F10 scancode (also 68). The
  real Nexus binding was Alt + F10 scancode 68, not Alt+D. The
  collision theory was wrong; reverted.
- [[gw2-keybind-collisions]] — original GW2 chord collision memory.
- [[vdf-editing-rules]] — golden rules for editing the VDF safely.
