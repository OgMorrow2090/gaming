---
name: Multiple key_press bindings under one Full_Press fire sequentially, not as a chord
description: Steam Input fires each key_press in a Full_Press bindings block as a discrete keypress (down + up), not as a held chord. Patterns that need a held modifier (Shift+Equals → +) break silently and produce only the unmodified key.
type: project
---

Steam Input's controller VDF lets you stack multiple `binding` entries inside a single `Full_Press > bindings` block:

```vdf
"Full_Press"
{
    "bindings"
    {
        "binding"  "key_press T, ty, , "
        "binding"  "key_press Y, ty, , "
        "binding"  "key_press RETURN, Send, , "
    }
}
```

For typing patterns like the chat-wheel "ty" slot above, this works as expected: T fires, then Y fires, then RETURN fires. Each is a discrete `down + up` event in sequence, producing `t y \n` → "ty\n" in chat.

**Why:** Steam Input treats each `key_press` action as an atomic press-and-release. There is no "hold these all simultaneously" semantic in this binding form — modifiers don't stay asserted across the next key.

**How to apply:** When you need a held modifier (e.g. `Shift+Equals` to type a `+`), this pattern breaks silently. Shift fires (does nothing visible), then Equals fires (Shift already released, so produces `=`), then RETURN. The slot types `=\n` not `+\n`.

Workarounds:
- **Pick a single-key alternative**: e.g. `KEYPAD_PLUS` produces `+` directly without modifier. We hit this on Commands wheel slot 6 in controller v19.4 — replaced `LEFT_SHIFT + EQUALS` with `KEYPAD_PLUS`.
- **For chord-as-keybind targets** (where you're not typing into chat but emitting a chord that the *game* binds, e.g. Ctrl+Shift+F7 → some addon): Steam Input *does* hold the modifiers for chord patterns when the receiving binding is a single hotkey lookup rather than text input. The `button_a` Full_Press in our layout fires `LEFT_CONTROL + LEFT_SHIFT + F7` and addons pick up the chord correctly. The difference appears to be whether the receiver is parsing keystroke-by-keystroke (chat input) versus matching a complete hotkey state (Nexus binding lookup, GW2 keybinds). The text-input path is the one that loses modifiers.

If you're not sure which path applies, test before shipping: use a chat box to verify modifier+key combos type the expected character, or check Nexus.log for the dispatch.

## Reference incidents

- 2026-05-03 — Commands wheel slot 6 ("+") was actually typing `=`. Fixed by using `KEYPAD_PLUS` as a single binding (controller v19.4).
