---
name: GW2 has /leave and /squadleave slash commands
description: Native chat slash commands let macros leave party/squad without depending on captured UI positions. Use these instead of right-click-party-bar UI macros, which break when GW2 panels move.
type: reference
---

GW2 supports two native chat slash commands for leaving groups:

| Command | Effect |
| --- | --- |
| `/leave` | Leave current **party** |
| `/squadleave` (alias `/sqleave`) | Leave current **squad** |

These fire as console commands when typed into chat — they do **not** post anything to the channel. No UI position dependency. Reliable across all resolutions and instance states.

## When to use

Any time a controller chord, Mystic Clicker macro, or chat-wheel slot needs to leave a party or squad. Far more reliable than the right-click-party-bar → click-Leave UI macro pattern, which depends on two captured screen positions that move per-resolution and per-instance.

We had a `LEAVE_PARTY_COMBO` MC macro (right-click bar → wait → click Leave) that captured these positions. It broke whenever the party panel moved — which was often. **Removed in v3.6.16 (2026-05-03)** in favour of slash-command typing on the chat (Commands) wheel slots 15 and 16.

## How to wire as a chat-wheel typing slot

Steam Input chat-wheel `key_press` bindings fire as discrete sequential keypresses, so spelling out the command works directly:

```vdf
"touch_menu_button_15"
{
    "activators"
    {
        "Full_Press"
        {
            "bindings"
            {
                "binding"  "key_press FORWARD_SLASH, /leave, , "
                "binding"  "key_press L, /leave, , "
                "binding"  "key_press E, /leave, , "
                "binding"  "key_press A, /leave, , "
                "binding"  "key_press V, /leave, , "
                "binding"  "key_press E, /leave, , "
                "binding"  "key_press RETURN, Send, , "
            }
        }
    }
    ...
}
```

Don't forget the trailing `RETURN` — without it the command sits in the chat input box unsent.

## Source

- [Chat command — Guild Wars 2 Wiki](https://wiki.guildwars2.com/wiki/Chat_command)
