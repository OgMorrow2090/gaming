# Project Memory

Index of memory entries for this repo. Each line points at one file in `memory/`.

- [VDF editing rules](vdf-editing-rules.md) — never bulk-rewrite controller VDFs; always validate; full rules in `docs/vdf-editing-golden-rules.md`
- [Steam Input sync ≠ Cloud toggle](steam-input-sync-not-cloud.md) — Valve bug #7801; signout/signin is the only fix for ghost layouts
- [Chord emission quirks](chord-emission-quirks.md) — uinput modifier-hold + Full_Press double-emit; when to use DLL-press-`I` pattern
- [GW2 GameBinds.xml chord collisions](gw2-keybind-collisions.md) — Home/End/PgUp/PgDn = Skill Profession 2-5; check before picking chord keys
- [configset_controller_neptune.vdf is the appname→layout pointer](configset-controller-pointer.md) — renaming a non-Steam shortcut breaks SSH deploys until you update this file
- [gamescope nested xwayland is hardlocked at EDID max](gamescope-nested-xwayland-constraint.md) — `-w/-h`, `--force-windows-fullscreen`, post-init xrandr all fail; don't try them again
- [GW2 dpiScaling=true + per-mode Wine LogPixels is the UI compensation](gw2-windowed-fullscreen-dpiscaling-trick.md) — never force dpiScaling=false or screenMode=fullscreen; let GW2 scale via Wine DPI
- [Dual GW2 installs — Deck profile + Apple TV profile](dual-gw2-installs.md) — Steam install + ~/Games/gw2-appletv/ (symlinked Gw2.dat/bin64, copied exe + addons); separate Wine prefix per profile; sync-gw2-exe.sh re-copies launcher after Steam patches
- [Nexus / addon dual-deploy rules](nexus-dual-deploy-rules.md) — addons/ is COPIED not symlinked, so changes must deploy to both profile dirs manually; default-to-both unless explicitly per-profile (UI position / screen size)
