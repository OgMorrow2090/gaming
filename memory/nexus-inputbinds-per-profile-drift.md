---
name: Nexus InputBinds.json drifts per profile and silently clears conflicting bindings on load
description: Each GW2 profile has its own addons/Nexus/InputBinds.json that drifts independently. When two bindings end up on identical key+modifiers, Nexus silently clears or moves one — recurring "binding stopped working" symptom.
type: project
---

<!-- markdownlint-disable MD041 -->

After the multi-GW2 split (Local + Apple TV + Steam Deck profiles, see [multi-gw2-installs.md](multi-gw2-installs.md)), every profile now has its **own** `addons/Nexus/InputBinds.json`. They diverge over time because:

- Each profile is launched from a different Wine prefix → Nexus reads/writes its own file
- Per-profile UI tweaks (different rebinds for Deck-controller-friendly vs Apple-TV-mouse-friendly) drift further with each in-game change
- **When Nexus loads InputBinds.json and finds two bindings sharing identical Code+Ctrl+Shift+Alt, it silently reconciles** — usually clears one (`Code=0`) or shifts the colliding one to its second-choice key

This shows up to the user as "binding stopped working — used to fire X". The earlier single-profile era had the same root cause but was easy to fix (one file).

## Symptom snapshot — how it presented 2026-05-01

User reported: "press and hold right DPad to auto-sort inventory — instead opens Community LFG." Diagnosis showed the Steam Deck profile (`~/Games/gw2-deck/addons/Nexus/InputBinds.json`) had **6 bindings cleared or moved** vs the Apple TV profile (which was still correct):

| Identifier | Apple TV (correct) | Deck (broken) | Cause |
|---|---|---|---|
| `DEPOSIT_AND_SORT` (Organizer auto-sort) | Ctrl+Q (Code 16) | **Code 0** (cleared) | likely manual clear in Nexus options |
| `0xFC56DD9F_ToggleLFG` (Community LFG addon) | Ctrl+F7 (Code 65) | **Code 0** (cleared) | user explicitly removed it |
| `BANK_COMBO` | F8 (Code 66) | **Code 0** (cleared) | conflicts with Deck `FASTSWAP_TOGGLE` which was on F8 |
| `DEPOSIT_MATERIALS` | Ctrl+F (Code 33) | Ctrl+D (Code 32) | conflicts with Deck `MYSTIC_FORGE` on Ctrl+F |
| `MT_RESET_WINDOWS` / `RESET_WINDOWS` | Ctrl+Shift+Home (Code 45) | **Code 0** | unknown |
| `TRADING_POST` | Ctrl+O (Code 24) | **Code 0** | unknown |

23 of ~200 entries differed total — many were intentional per-profile tweaks (e.g. Mystic Clicker MT_TOGGLE_FLIPLIST/DELIVERY are deliberately swapped Alt+F ↔ Alt+D between profiles). The 6 above were unintentional.

## How to apply

When the user reports "controller binding X stopped working" or "this addon's keybind isn't firing":

### 1. Confirm the chain — controller → key → addon

The chain is **VDF (Steam Input) emits keystroke** → **Wine receives** → **Nexus addon catches via Identifier**. For example DPad-Right hold = `Ctrl+Q` from VDF group 7 dpad_east Long_Press, expected to land on Nexus `DEPOSIT_AND_SORT`. Don't assume the VDF changed — diff the **InputBinds.json** first.

### 2. Diff the broken profile against a known-good one

Use this from the host (bazzite Og@172.16.100.212):

```bash
python3 -c "
import json
A = json.load(open('$HOME/Games/gw2-deck/addons/Nexus/InputBinds.json'))
B = json.load(open('$HOME/Games/gw2-appletv/addons/Nexus/InputBinds.json'))
a = {x['Identifier']: x for x in A}
b = {x['Identifier']: x for x in B}
for ident in sorted(b):
    if ident not in a or (a[ident].get('Code'),a[ident].get('Ctrl'),a[ident].get('Shift'),a[ident].get('Alt')) != (b[ident].get('Code'),b[ident].get('Ctrl'),b[ident].get('Shift'),b[ident].get('Alt')):
        print(ident, '|', 'Deck:', a.get(ident,{}).get('Code',0), 'AppleTV:', b[ident]['Code'])
"
```

### 3. Surgical restore — DON'T full-sync

**Important**: do not blindly copy one profile's bindings to another — some divergence is intentional. Instead, restore **only the cleared (Code=0) or visibly-wrong** entries.

Pattern (run on bazzite, GW2 must be CLOSED on that profile):

```bash
F="$HOME/Games/gw2-deck/addons/Nexus/InputBinds.json"
cp "$F" "$F.bak-$(date +%Y%m%d-%H%M%S)"
python3 -c "
import json
P = '$F'
binds = json.load(open(P))
for b in binds:
    if b['Identifier'] == 'DEPOSIT_AND_SORT':
        b.update({'Code': 16, 'Ctrl': True, 'Shift': False, 'Alt': False})
    elif b['Identifier'] == '0xFC56DD9F_ToggleLFG':
        b.update({'Code': 65, 'Ctrl': True, 'Shift': False, 'Alt': False})
json.dump(binds, open(P,'w'), indent=2)
"
```

### 4. Critical timing — close GW2 first

Nexus reads `InputBinds.json` on addon load and **writes it on game shutdown**. If GW2 is running while you edit the file, Nexus overwrites your edits on close. Always:

1. Close GW2 on the target profile
2. Edit the file
3. Launch GW2 — verify binding works
4. Close GW2 — re-read the file to confirm Nexus didn't clear/move it again

If Nexus re-clears after step 4, there's a **load-time conflict** still active. Search the file for any other entry with the same Code+modifiers and resolve.

### 5. Reference scancodes (Nexus uses scancodes, NOT VK codes)

Common ones for chord debugging:

| Scancode | Key | Scancode | Key |
|---|---|---|---|
| 16 | Q | 30 | A |
| 17 | W | 31 | S |
| 18 | E | 32 | D |
| 19 | R | 33 | F |
| 20 | T | 34 | G |
| 21 | Y | 35 | H |
| 22 | U | 36 | J |
| 23 | I | 37 | K |
| 24 | O | 38 | L |
| 25 | P | 50 | M |
| 59 | F1 | 64 | F6 |
| 60 | F2 | 65 | F7 |
| 61 | F3 | 66 | F8 |
| 62 | F4 | 67 | F9 |
| 63 | F5 | 68 | F10 |
| 45 | Home | | |

Note: GW2's `gamebinds.xml` uses **VK codes** instead — different table. E.g. VK 89 = Y, VK 73 = I, VK 81 = Q.

## Long-term mitigation

User chose option (A) "surgical restore on demand" rather than a periodic profile sync. If this recurs frequently, options:

- **Sync script** that copies a known-canonical profile's InputBinds.json to others on a cron (loses intentional divergence)
- **Diff script** that warns when bindings drift between profiles without auto-fixing
- **Treat one profile as source-of-truth** and document non-shared per-profile overrides separately

For now, the recipe in step 3 above is the standing fix.
