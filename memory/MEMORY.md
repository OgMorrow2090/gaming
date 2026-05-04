# Project Memory

Index of memory entries for this repo. Each line points at one file in `memory/`.

- [VDF editing rules](vdf-editing-rules.md) — never bulk-rewrite controller VDFs; always validate; full rules in `docs/vdf-editing-golden-rules.md`
- [Steam Input sync ≠ Cloud toggle](steam-input-sync-not-cloud.md) — Valve bug #7801; signout/signin is the only fix for ghost layouts
- [Chord emission quirks](chord-emission-quirks.md) — uinput modifier-hold + Full_Press double-emit; when to use DLL-press-`I` pattern
- [GW2 GameBinds.xml chord collisions](gw2-keybind-collisions.md) — Home/End/PgUp/PgDn = Skill Profession 2-5; check before picking chord keys
- [configset_controller_neptune.vdf is the appname→layout pointer](configset-controller-pointer.md) — renaming a non-Steam shortcut breaks SSH deploys until you update this file
- [gamescope nested xwayland is hardlocked at EDID max](gamescope-nested-xwayland-constraint.md) — `-w/-h`, `--force-windows-fullscreen`, post-init xrandr all fail; don't try them again
- [GW2 dpiScaling=true + per-mode Wine LogPixels is the UI compensation](gw2-windowed-fullscreen-dpiscaling-trick.md) — never force dpiScaling=false or screenMode=fullscreen; let GW2 scale via Wine DPI
- [Multi GW2 installs — Local + Apple TV + Steam Deck profiles](multi-gw2-installs.md) — Steam install (local play) + ~/Games/gw2-appletv/ + ~/Games/gw2-deck/; symlinked Gw2.dat/bin64/d3d11.dll, copied exe + addons; separate Wine prefix per profile; sync-gw2-exe.sh re-copies launcher after Steam patches
- [Nexus / addon multi-deploy rules](nexus-multi-deploy-rules.md) — addons/ is COPIED not symlinked, so changes must deploy to all THREE profile dirs manually; default-to-all unless explicitly per-profile (UI position / screen size)
- [Steam grid art cache bust](steam-grid-cache-bust.md) — when grid art changes don't show after killing steamwebhelper, also delete `userdata/<id>/config/librarycache/<appid>.json` AND `htmlcache/{Cache,Code Cache,GPUCache}` to bust the customimage version stamp
- [Streaming input host vs client](streaming-input-host-vs-client.md) — Deck does Steam Input locally before Moonlight forwards keys; Apple TV sends raw HID. Deck VDF deploys go to deck@172.16.100.95, Apple TV VDFs go to bazzite
- [Nexus InputBinds drift across profiles](nexus-inputbinds-per-profile-drift.md) — each profile has its own InputBinds.json; Nexus silently clears/moves bindings on collision. Diff vs known-good profile, surgical restore only, GW2 must be closed during edit
- [GW2 has /leave + /squadleave slash commands](gw2-leave-slash-commands.md) — native chat slash commands let macros leave party/squad without UI position dependency; replaces brittle right-click-bar macros
- [Steam Input multi-binding fires sequentially](steam-input-multi-binding-fires-sequentially.md) — modifier+key chains in a Full_Press bindings block don't hold the modifier; use single-key alternatives like KEYPAD_PLUS for `+`
- [Moonlight stream stuck at GW2 character select — diagnostic](moonlight-stream-stuck-diagnostic.md) — system cursor visible + Nexus icons unclickable = GW2 lost focus, not crashed; check dual sessions, GFXSettings drift, `sunshine-gaming.service` unit name
