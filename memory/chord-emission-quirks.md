---
name: Steam Input chord emission quirks (uinput modifier hold + Full_Press double-emit)
description: Two Steam Input behaviors that bite Mystic Clicker inventory combos — chord modifier kept asserted via uinput for the entire physical hold, and Full_Press chord output double-emitted at the OS level. Both seen on Bazzite + Sunshine + Moonlight + Steam Deck.
type: project
---

<!-- markdownlint-disable MD041 -->

Two distinct Steam Input behaviors that broke Mystic Clicker inventory combos in v18.4 and required deliberate DLL design to work around. Both confirmed on the Linux Sunshine → Moonlight → Steam Deck pipeline (Bazzite host); may not reproduce on native Windows.

## 1. uinput keeps chord modifiers asserted for the entire physical hold

When a chord activator (`Full_Press` / `Long_Press` / `Double_Press`) emits a `key_press LEFT_SHIFT` (or `LEFT_ALT`) alongside the chord output key, Steam Input keeps the modifier physically asserted via uinput for the **entire duration of the physical button hold**. On a `Long_Press` that is typically 500–2000 ms; on a `Double_Press` it is the duration of the second tap.

If the DLL synthesizes a clean `I` via `SendInput` while uinput is still asserting Shift, GW2 sees `Shift+I` (Wizard Vault toggle) instead of plain `I`. Same with Alt: GW2 sees `Alt+I` (unbound, so inventory does not open at all).

**Workaround:** `WaitForChordModifiersRelease()` in `input-sim.cpp` polls `GetAsyncKeyState(VK_LSHIFT/RSHIFT/LMENU/RMENU/LCONTROL/RCONTROL)` every 50 ms (3 s timeout) and returns the moment the OS sees the modifier release. Replaced fixed-time `Sleep(500)` / `Sleep(800)` defers that were either too short for long holds or too long for quick taps. Each combo logs its initial modifier mask and wait duration to `Nexus.log` for verification.

**Why:** introduced in DLL v3.6.6 after v3.6.5's `Sleep(800)` was still too short for the user's typical Long_Press hold. Polling adapts to actual hold time.

**How to apply:** any combo whose VDF chord carries a modifier (`Shift`, `Alt`, or `Ctrl`) and whose DLL function presses `I` itself must use the `WaitForChordModifiersRelease()` + `OpenInventoryDllAndDoubleClick()` pattern on a detached `std::thread`. Combos with bare-key chords (no modifier) can still use the simpler `OpenInventoryAndDoubleClick()` — the helper still polls but exits immediately when no modifier is held.

## 2. Full_Press chord output double-emitted at OS level

In `dpad_south` of the `R1` action layer specifically (and possibly other dpad blocks where `Long_Press` and `Double_Press` are also defined), Steam Input emits the **entire VDF chord output twice** at the OS level, 50–80 ms apart. Visible in `Nexus.log` as two identical `Teleport Friend Combo: open inventory + double-click` entries at the same millisecond timestamp.

Per-combo Nexus debounce in `keybinds.cpp` (300 ms window) catches the second dispatch and prevents duplicate DLL execution. **But** that does not help for the keys themselves — if the VDF chord includes `key_press I`, GW2 still sees `I` pressed twice independently of Nexus, which toggles inventory open then closed, leaving the captured-slot double-click to land on a closed panel.

**Workaround:** any inventory-open chord on a Full_Press activator that sits alongside a Long_Press / Double_Press in the same dpad block must NOT emit `key_press I` from the VDF. The DLL function must press `I` itself via `OpenInventoryDllAndDoubleClick()`. Nexus debounce ensures only one DLL dispatch per chord press, so `I` is pressed exactly once and the inventory opens cleanly.

**Why:** Friend combo broke in v18.4.5 when moved from R1+DPad Right Full_Press to R1+DPad Down Full_Press. R1+Right has the same chord pattern (`I + F<n>`) and apparently does not exhibit the double-emit (or does it within the same frame and the user does not notice). R1+Down does. Fixed in v3.6.8 by converting `SimulateTeleportFriendCombo` to the DLL-press-`I` pattern.

**How to apply:** prefer the `OpenInventoryDllAndDoubleClick()` pattern (VDF emits chord key only, DLL synthesizes `I`) for every Full_Press inventory combo on dpad blocks that also define Long_Press or Double_Press. Trading Post and Bank in v18.4.x still use the older `OpenInventoryAndDoubleClick()` (relies on VDF `I` press) because they are reported working — but if they ever exhibit the open-then-close symptom, convert them too.

## Diagnostic: how to tell which one bit you

| Symptom | Likely cause |
| ------- | ------------ |
| Inventory opens, captured slot click lands on Wizard Vault | Modifier hold (issue 1) — DLL pressed `I` while Shift still asserted, GW2 saw `Shift+I` |
| Inventory opens then immediately closes; double-click misses | Chord double-emit (issue 2) — VDF `key_press I` fired twice, second toggles closed |
| Inventory does not open at all on `Alt+...` chord | Modifier hold (issue 1) — DLL pressed `I` while Alt still asserted, GW2 saw `Alt+I` (unbound) |

Both leave clear evidence in `Nexus.log`: the `*: modifiers held at start=0xN, released after Nms` line shows hold time, and the `Debounced duplicate dispatch of *` line shows whether Nexus saw the chord twice.
