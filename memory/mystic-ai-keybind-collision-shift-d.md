---
name: Mystic AI keybinds collided with GW2 movement (Shift+D / Alt+D); fix is the F13–F24 range
description: MYSTIC_AI_READ_BOOK was bound to Shift+D and MYSTIC_AI_CAPTURE to Alt+D. In GW2 the Shift modifier slows the bound movement key — so Shift+D = "walk slowly right" and Alt+D collides with the MT Fliplist chord. Every normal step the user took re-fired the addon. The clean keyspace for addon binds on this setup is F13–F24 (chord-only — no physical keyboard key exists).
metadata:
  type: project
---

<!-- markdownlint-disable MD041 MD032 -->

## Symptom

Mystic AI fires "random" book reads. User presses Read-Book on the book page
(correct), then walks away from the book using the WASD keys, and ~10–60 s
later a fresh Book read fires against the now-bookless screen. The daemon's
Claude reply describes whatever is on the screen instead ("I don't see any
book page text — this is a gameplay screenshot of a dungeon …") and the
TTS speaks it aloud. Closing GW2 doesn't stop the TTS that's already in
flight.

Identical pattern with MYSTIC_AI_CAPTURE — accidentally fires whenever the
user presses Alt+D for the in-game MT Fliplist combo.

## Root cause

The Nexus InputBinds for this addon were bound to GW2 movement-key
combinations:

| Nexus identifier | Bound to | Collides with |
|---|---|---|
| `MYSTIC_AI_READ_BOOK` | Shift + D (`Code: 68 = VK_D, Shift: true`) | GW2 "Move Right" (D) with the Shift modifier that slows movement → fires every slow step |
| `MYSTIC_AI_CAPTURE` | Alt + D (`Code: 68, Alt: true`) | Steam Input chord "MT Fliplist" which emits `LEFT_ALT, D` |

GW2 treats Shift as a held-modifier on the movement keys (slow walk).
Nexus's InputBinds dispatch sees "Shift + D pressed" and posts the
keybind callback. The addon's debounce (300 ms) doesn't help — each step
is well outside the window.

There is **no code bug** in Mystic AI for this — the auto-advance watch
that was removed in 1.1.14 was a separate (also-removed) issue. This is
purely a Nexus binding choice that collided with normal gameplay input.

## The clean key range on this setup

`scripts/gw2-claude-daemon.py` is irrelevant here; the question is just
"which keyboard scancodes are free across **Nexus binds + GW2 GameBinds
+ Steam Input controller VDF**". On the active GW2 v18.4 controller
layout:

| Range | Status | Use |
|---|---|---|
| F1–F12 (VK 112–123) | All claimed by Steam Input chords in the VDF | DO NOT use |
| **F13–F24 (VK 124–135)** | Free everywhere | **Best choice — physical keyboards don't have these keys, so impossible to press by accident; can only be triggered by an explicit Steam Input chord** |
| Numpad 0–9 (VK 96–105) | Free everywhere | Safe alternate, but a numpad-equipped keyboard *can* press them |
| Pause/Break (13), PrintScreen (44), NumLock (144), ScrollLock (145) | Free everywhere | Safe alternate, single-key |
| Any letter/digit + Shift | DANGEROUS | GW2 reads Shift as movement-modifier; will retrigger on every slow step |
| Any letter/digit + Alt | DANGEROUS on this setup | Steam Input chord modifier — collides with addon chord emissions |
| Any letter/digit + Ctrl | RISKY | Several GW2 actions and Nexus addons use Ctrl-modified letters |

## Applied fix (2026-05-21)

Recommended in-game rebind in Nexus → Keybinds:

| Identifier | Old | New |
|---|---|---|
| `MYSTIC_AI_READ_BOOK` | Shift + D | F13 |
| `MYSTIC_AI_CAPTURE` | Alt + D | F14 |
| `MYSTIC_AI_READ` | unbound | F15 |

Then on bazzite, Steam Input chords get added/edited to emit F13/F14/F15
to whichever controller button the user wants for those actions.

## How to detect this class of bug fast next time

If an addon fires "spuriously" during gameplay:

1. Look up the addon's `Code` + modifier in
   `~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/Nexus/InputBinds.json`.
2. Decode the `Code` as a Windows VK (`68 = VK_D`, `112–135 = F1–F24`,
   `96–105 = Numpad0–9`).
3. Check whether that key (or that key with the user's reported modifier
   removed) is bound in `GameBinds.xml` to a movement / common action.
4. Check whether the same key appears as a `key_press` target in the
   active controller VDF.

Two of those three matching = collision. Move the bind to F13–F24.

## Related

- [[gw2-keybind-collisions]] — the older Home/End/PgUp/PgDn vs. Skill
  Profession 2–5 trap. Same class of bug; this file is the Mystic-AI-
  specific recurrence with the cleaner "F13–F24 always-safe" answer.
- [[steam-input-multi-binding-fires-sequentially]] — adjacent: how
  multiple key bindings inside a single chord-trigger emit *all* the keys
  on one press.
- [[steam-input-invalid-key-tokens]] — adjacent: how to debug a chord
  that fires nothing (vs. this file: a chord that fires by accident).
