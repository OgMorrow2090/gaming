---
name: KEYPAD_MINUS is not a valid Steam Input key token
description: Some keyboard key tokens silently fail in a Steam Input VDF bindings block — the file parses fine but Steam logs "Failed to create digital binding" and the chord does nothing in-game. Confirmed bad: KEYPAD_MINUS. Diagnose via ~/.local/share/Steam/logs/controller.txt.
type: project
---

<!-- markdownlint-disable MD041 -->

Not every key name is a valid Steam Input `key_press` token. An invalid token in a VDF bindings block is **accepted by the file parser but silently rejected at bind time** — the chord produces no key event and the in-game action never fires. There is no on-screen error; the only signal is in Steam's controller log.

## The 2026-05-19 incident

A controller chord (tried as R1+R2, then L1+Menu) bound to `key_press KEYPAD_MINUS` "did nothing" in GW2. Several rounds of blaming the chord type and the trigger-vs-button distinction were all wrong. The real cause was one line in `~/.local/share/Steam/logs/controller.txt`:

```text
Failed to create digital binding 'key_press KEYPAD_MINUS'
```

Reverting the binding to `LEFT_ALT` + `F10` (Alt+F10) fixed it immediately.

## Token validity (confirmed)

| Valid | Invalid |
|---|---|
| `KEYPAD_0`–`KEYPAD_9`, `KEYPAD_PLUS` | `KEYPAD_MINUS` |
| `LEFT_ALT`, `LEFT_SHIFT`, `F10`, letter keys | — |

## How to apply

- When a controller chord silently does nothing, **read `~/.local/share/Steam/logs/controller.txt` first** — before touching the chord type or trigger config. Grep for `Failed to create digital binding`; the log records config loads and every bind failure.
- Prefer proven tokens (`LEFT_ALT`+`F10`, `KEYPAD_PLUS`) over `KEYPAD_MINUS` and other untested ones.
- Lesson from this incident: stop iterating on chord/trigger theories — read the Steam log and let it tell you. Related: [[steam-input-multi-binding-fires-sequentially]], [[chord-emission-quirks]].
