# Project Memory

Index of memory entries for this repo. Each line points at one file in `memory/`.

- [VDF editing rules](vdf-editing-rules.md) — never bulk-rewrite controller VDFs; always validate; full rules in `docs/vdf-editing-golden-rules.md`
- [Steam touch/radial menus cap at 16 buttons](steam-touch-menu-16-button-cap.md) — a 17th slot makes Steam silently reject the whole layout; replace, don't expand
- [Steam Input sync ≠ Cloud toggle](steam-input-sync-not-cloud.md) — Valve bug #7801; signout/signin is the only fix for ghost layouts
- [Chord emission quirks](chord-emission-quirks.md) — uinput modifier-hold + Full_Press double-emit; when to use DLL-press-`I` pattern
- [GW2 GameBinds.xml chord collisions](gw2-keybind-collisions.md) — Home/End/PgUp/PgDn = Skill Profession 2-5; check before picking chord keys
- [configset_controller_neptune.vdf is the appname→layout pointer](configset-controller-pointer.md) — renaming a non-Steam shortcut breaks SSH deploys until you update this file
- [gamescope nested xwayland is hardlocked at EDID max](gamescope-nested-xwayland-constraint.md) — `-w/-h`, `--force-windows-fullscreen`, post-init xrandr all fail; don't try them again
- [GW2 dpiScaling=true + per-mode Wine LogPixels is the UI compensation](gw2-windowed-fullscreen-dpiscaling-trick.md) — never force dpiScaling=false or screenMode=fullscreen; let GW2 scale via Wine DPI
- [Multi GW2 installs — Local + Apple TV + Steam Deck profiles](multi-gw2-installs.md) — **ARCHIVED 2026-05-12**, consolidated to single main install; file kept as historical wiring recipe
- [Nexus / addon multi-deploy rules](nexus-multi-deploy-rules.md) — addons/ is COPIED not symlinked, so changes must deploy to all THREE profile dirs manually; default-to-all unless explicitly per-profile (UI position / screen size)
- [Steam grid art cache bust](steam-grid-cache-bust.md) — when grid art changes don't show after killing steamwebhelper, also delete `userdata/<id>/config/librarycache/<appid>.json` AND `htmlcache/{Cache,Code Cache,GPUCache}` to bust the customimage version stamp
- [Streaming input host vs client](streaming-input-host-vs-client.md) — Deck does Steam Input locally before Moonlight forwards keys; Apple TV sends raw HID. Deck VDF deploys go to deck@172.16.100.95, Apple TV VDFs go to bazzite
- [Nexus InputBinds drift across profiles](nexus-inputbinds-per-profile-drift.md) — each profile has its own InputBinds.json; Nexus silently clears/moves bindings on collision. Diff vs known-good profile, surgical restore only, GW2 must be closed during edit
- [GW2 has /leave + /squadleave slash commands](gw2-leave-slash-commands.md) — native chat slash commands let macros leave party/squad without UI position dependency; replaces brittle right-click-bar macros
- [Steam Input multi-binding fires sequentially](steam-input-multi-binding-fires-sequentially.md) — modifier+key chains in a Full_Press bindings block don't hold the modifier; use single-key alternatives like KEYPAD_PLUS for `+`
- [Moonlight stream stuck at GW2 character select — diagnostic](moonlight-stream-stuck-diagnostic.md) — system cursor visible + Nexus icons unclickable = GW2 lost focus, not crashed; check dual sessions, GFXSettings drift, `sunshine-gaming.service` unit name
- [Controller VDF deploy must enumerate ALL stale-pattern files](controller-vdf-deploy-checklist.md) — patching only configset-referenced files leaves stale historical layouts + autosave files (`moonlight - gw2 steamos/`) Steam Input may still load
- [Mystic Clicker build + auto-deploy pipeline](mystic-clicker-build-and-deploy.md) — GitHub Actions builds DLL on push; Mac launchd watcher (`com.itinyk.gw2-mystic-clicker-watcher`) auto-deploys to all 4 profiles every 5 min when GW2 closed; LAN-private IPs prevent direct cloud deploy
- [AI must always watch and deploy the DLL](feedback_always_watch_and_deploy_dll.md) — after any mystic-clicker push, verify CI green + watcher logged `deploy OK: <sha>` + all 4 profile DLL hashes match before reporting done
- [GDI BitBlt is dead under DXVK + gamescope; use D3D11 swap-chain readback](d3d11-back-buffer-capture-vs-gdi.md) — `BitBlt(GetDC(NULL))` returns all-black on bazzite; capture `APIDefs->SwapChain` back buffer from a Nexus `RT_PostRender` hook instead
- [GW2 Claude-vision daemon](gw2-claude-vision.md) — capture frame → Claude API (vision) → spoken answer; Piper or ElevenLabs TTS (live voice "Lily"); cursor-anchored + headless read
- [OCR line filter tuning for GW2 destroy-popup item names](ocr-line-filter-tuning.md) — trim leading/trailing junk, then whitelist+heuristics (min word length, avg letters/word, character-name blacklist via MumbleLink) to separate real item names from inventory/overlay noise
- [NVIDIA + gamescope display state → 4K@165 flicker](nvidia-gamescope-state-flicker.md) — purple/red colour-cycle flicker after many mode switches / EDID swaps / vkms toggles in one uptime is stale NVIDIA driver state, not the cable or mode; cold reboot is the reliable reset (or full PSU-off capacitor drain if soft reboot doesn't clear); never change gamescope-mode mid-stream
- [GW2 Wine DPI — historical per-profile + current main-install state](per-profile-fixed-dpi.md) — **ARCHIVED 2026-05-12** for the multi-profile mappings; main install now at 0x90 (150%) + MouseSensitivity 20 (reverted from 200% due to login flicker). Rule still applies: never wire dynamic per-mode DPI swap into Sunshine prep-cmd.
- [Bazzite repurpose plan (~2028)](project-bazzite-repurpose-plan.md) — when new TV PC purchased, repurpose current i9-9900K as combined Docker server (replacing thing-pi) + headless Sunshine streaming server
- [Moonlight client-side app cache](moonlight-client-app-cache.md) — Moonlight caches Sunshine app list locally; `moonlight list` reads cache not server; clear `1\apps\*` entries in config or open GUI to refresh after Sunshine app changes
- [CEF overlay black screen on TV input switch](cef-overlay-black-screen-after-stream.md) — any TV input switch back to bazzite can trigger CEF overlay covering BPM; may self-recover in ~15 min, or restart gamescope-session
- [Controller wake TV script](cec-controller-wake-script.md) — WebOS network API replaces CEC for TV input switching; direct HDMI has no CEC adapter; silence-detection on hidraw0 unchanged
- [Bazzite performance tuning](bazzite-performance-tuning.md) — custom TuneD `gaming-performance` profile, 5GHz all-core, GPU high, all sleep masked; PPD override needed to survive reboot
