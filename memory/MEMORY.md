# Project Memory

Index of memory entries for this repo. Each line points at one file in `memory/`.

- [VDF editing rules](vdf-editing-rules.md) — never bulk-rewrite controller VDFs; always validate; full rules in `docs/vdf-editing-golden-rules.md`
- [Steam Input sync ≠ Cloud toggle](steam-input-sync-not-cloud.md) — Valve bug #7801; signout/signin is the only fix for ghost layouts
- [Chord emission quirks](chord-emission-quirks.md) — uinput modifier-hold + Full_Press double-emit; when to use DLL-press-`I` pattern
- [GW2 GameBinds.xml chord collisions](gw2-keybind-collisions.md) — Home/End/PgUp/PgDn = Skill Profession 2-5; check before picking chord keys
- [configset_controller_neptune.vdf is the appname→layout pointer](configset-controller-pointer.md) — renaming a non-Steam shortcut breaks SSH deploys until you update this file
