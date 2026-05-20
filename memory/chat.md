# Session Log

<!-- markdownlint-disable MD024 -->

Append-only log of agent sessions on this repo. Read at session start for continuity. Entries are summaries — not full transcripts.

## 2026-05-12 — AMD RX 9070 XT swap day + consolidation to single GW2 install

Day-long session across the GPU physical swap, post-rebase clean-up, original 1280×800@90 Deck-streaming goal achieved then re-evaluated, and final consolidation down to a single GW2 install + native Deck install.

### GPU swap + Bazzite rebase

Sapphire PULSE RX 9070 XT installed, NVIDIA modules blacklisted as part of `rpm-ostree rebase ostree-image-signed:docker://ghcr.io/ublue-os/bazzite-deck:stable`. amdgpu loaded clean, gamescope-session healthy, Sunshine VAAPI streaming working out of the box. No more HDMI FRL training failures, no driver-state flicker.

### Original Deck-stream goal solved — then deprecated

1. Got `1280×800@90` into SteamOS BPM dropdown via virtual HDMI-A-2 + custom EDID (`configs/bazzite/edid-virtual-display.bin`). Commit `2f00fbf`. **AMD's open driver exposes virtual connectors cleanly; vkms-on-NVIDIA-proprietary dead-end from 2026-05-11 confirmed as NVIDIA-specific.**
1. Tried per-client gamescope mode auto-switch via Sunshine prep-cmd (`0e10dfe`). **Caused Sunshine crashes mid-stream on AMD** — reverted in `b3a020f`. Cemented rule: never change gamescope-mode mid-stream.
1. Profile DPI memo updated from "Deck = 0x60 (96 DPI)" to "Deck = 0x120 (288 DPI = 300%)" to compensate for the 4K-source → 720p-Deck-downscale architecture (commit `9d4b0cd`).
1. **User then pivoted entirely**: play GW2 native on the actual Steam Deck (no Moonlight), keep Apple TV streaming via existing GW2 - SteamOS Sunshine app. The "1280×800@90 in BPM" hunt that drove the whole 5/11 + 5/12 push was solved AND then made irrelevant by the architectural change.

### Consolidation (commit `feb5225`)

- **Deleted** non-Steam shortcuts `Guild Wars 2 (Apple TV)` (appid 2879321470) and `Guild Wars 2 (Steam Deck)` (appid 3111887265) from `shortcuts.vdf`, their Wine prefixes, their addon dirs, and their per-profile Sunshine entries. ~1.2 GB freed.
- **Removed** virtual HDMI-A-2 kernel args + custom EDID firmware tracking via `rpm-ostree initramfs-etc --untrack`. Boot kargs cleaned with `--delete-if-present` for each variant (had previously double-prefixed itself: `firmware_class.path=firmware_class.path=/etc/firmware`).
- Mystic Clicker deck-tuned config (`mystic-clicker-1280x800.cfg`, MD5 `66eebbf646aa653c82f9c0b48674b796`) deployed to the actual Steam Deck at `/home/deck/.local/share/Steam/steamapps/common/Guild Wars 2/addons/MysticClicker/` so the user doesn't have to re-record captures.
- Memory: `per-profile-fixed-dpi.md` and `multi-gw2-installs.md` marked **ARCHIVED** at top. `MEMORY.md` index entries updated to reflect archive status.

### Wrapper-script bypass for Steam compat-tool save bug (`b91afb5`)

During the time the non-Steam profiles still existed, Steam's `CompatToolMapping` in `config.vdf` kept reverting empty for the new shortcuts (user-set via BPM → not persisted). Workaround: changed shortcut `Exe` to point at bash wrappers (`~/scripts/launch-gw2-{appletv,deck}.sh`) that invoke Proton directly. Steam preserves the Exe field. Wrappers deleted as part of consolidation.

### HDMI-CEC prep for Steam Controller 2 (`7ae768e`)

Ordered UGREEN DP→HDMI 2.1 adapter for arrival 2026-05-13/14. The HDMI on RDNA 4 lacks the kernel patches to do 4K@120/144 with HDR + VRR over direct HDMI — DP→HDMI active adapter unlocks the full pipe. UGREEN exposes CEC pin (confirmed via Reddit thread). Created `scripts/switch-lg-to-bazzite.sh`:

```bash
cec-ctl --to 0 --image-view-on
cec-ctl --to 0 --active-source phys-addr=0.0.0.0
```

To be wired to Steam Controller 2's Steam button (arriving similarly) for one-press LG input switching, mirroring how Apple TV remote already does it. Not yet testable — adapter not arrived.

### Today's late-session tuning (post-consolidation)

GW2 native on bazzite on LG at couch distance, captures from old Apple TV profile no longer align because DPI/UI scale doesn't match. Wine LogPixels iteration on the main Steam prefix (`compatdata/1284210/pfx/user.reg`):

| Step | Value | Result |
| --- | --- | --- |
| Start | `0x90` (144 DPI / 150%) | UI too small for couch viewing |
| Bump 1 | `0xA8` (168 DPI / 175%) | User wanted to test like-for-like with old Apple TV captures first |
| Revert | `0x90` (150%) | Confirmed captures matched old Apple TV scale at 150% |
| Final | **`0xC0` (192 DPI / 200%)** | User picked bigger UI over capture compat — captures need re-record |

Also bumped `MouseSensitivity` from default `"10"` to **`"16"`** (Wine range 1–20, +60% over default) for faster cursor tracking from couch distance. Both changes applied while GW2 was running (PID 42472), force-killed via `pkill -KILL` and confirmed `pgrep Gw2-64.exe` empty — Wine reads these at process start so next launch picks them up.

### Other small bits

- DP→HDMI active adapters (UGREEN PS186, Club3D CAC-1085 MCDP2900) pass **audio** natively via TMDS; CEC is a separate dedicated pin. Both routes work independently.
- AMD-specific Proton launch options: assessed and **nothing meaningful to add**. Current `WINEDLLOVERRIDES="d3d11=n,b" DXVK_ASYNC=1 %command% -provider Portal` is fine. `DXVK_ASYNC=1` is a silent no-op on DXVK 2.x (GPL replaces it) but harmless. Optional: `MESA_SHADER_CACHE_MAX_SIZE=4G` if cache ceiling becomes an issue.
- GW2's single-threaded engine is the real perf bottleneck on AMD; no env var fixes that.

### Open / next session

- UGREEN DP→HDMI adapter arrival (1–2 days): test `/dev/cec0` appearance + `switch-lg-to-bazzite.sh` from SSH while watching Apple TV, then validate 4K@120-144 + HDR + VRR over the adapter pipe.
- Steam Controller 2 arrival (1–2 days): bind Steam button → CEC switch script via Steam Input.
- DPI fine-tune: user may iterate above 0xC0 if 200% still feels small at couch distance. Higher hex options: `0xD8` (225%), `0xF0` (250%), `0x108` (275%). Mouse can max at `"20"`.
- Possible Mystic Clicker capture re-record at new 200% scale (only needed if user wants the addon working again — it's not blocking).
- Staged rpm-ostree reboot not urgent; the kargs changes are already in effect post-untrack.

### Files modified this session block (across 2026-05-12)

- `docs/migration-to-amd.md` — full AMD runbook iterated throughout day
- `configs/bazzite/gamescope-wrapper.sh` — modes consolidated; `deck` mode added then removed
- `configs/bazzite/sunshine-apps.json` — per-profile entries removed; prep-cmd removed
- `configs/bazzite/edid-virtual-display.bin` — custom EDID for virtual HDMI-A-2 (added, then removed during consolidation)
- `scripts/sunshine-set-resolution.sh` — DPI swap logic dropped; left as mode-switch only
- `scripts/switch-lg-to-bazzite.sh` (NEW) — HDMI-CEC LG input switch, ready for UGREEN+SC2 arrival
- `memory/nvidia-gamescope-state-flicker.md` — power-drain workflow + "never change mode mid-stream" rule
- `memory/per-profile-fixed-dpi.md` — **ARCHIVED** notice + main-install state
- `memory/multi-gw2-installs.md` — **ARCHIVED** notice
- `memory/MEMORY.md` — index entries updated to reflect archived files
- Live Wine prefix `compatdata/1284210/pfx/user.reg`: `LogPixels=0xC0`, `MouseSensitivity="16"` (not in repo — per-prefix state)

### 4K@60 ceiling caveat (honest)

User correctly noted "you assured this would solve my problems, now capped at 60Hz" after first AMD boot — AMD's HDMI direct out can't hit 4K@120/144 with current kernel without DSC patches that haven't landed in stable. Acknowledged having oversold the swap. The DP→HDMI active adapter is the route around it, and the practical streaming flow (Apple TV stream + bazzite local at 4K@60-120) is fine either way.

## 2026-05-11 — Bazzite-in-lounge LG migration; NVIDIA Linux dead-end; AMD GPU ordered

Long session continuing from the PC physical move into the lounge plugged directly to the LG G5 OLED via HDMI 2.1. Goal restated multiple times: **get 1280×800@90 to appear in the SteamOS resolution dropdown for Deck Moonlight streaming, while keeping LG 4K@165 native for couch play.** Hours of attempts; ultimately decided the only realistic fix is GPU swap.

### Topology now (vs pre-move state)

- Pre-move: NVIDIA RTX 3080 Ti, no physical monitor — `video=DP-1:e drm.edid_firmware=DP-1:edid.bin` virtual DP connector with a generated `edid-virtual-display.bin` providing 1280×800@90 + 4K@120. (User originally thought "nothing plugged in"; this is what was happening.) Worked for Steam BPM dropdown but **commit `56e28e2` (Apr 29) reverted because Sunshine KMS capture from DP-1 fails on NVIDIA proprietary** — virtual framebuffers aren't dmabuf-exposed the same as HDMI.
- Post-move: LG G5 plugged into HDMI-A-1, no virtual outputs, no dummy. EDID is the LG's. LG EDID does not advertise 1280×800.

### What we tried this session, in order, all failed on NVIDIA

1. `drm.edid_firmware=HDMI-A-1:edid.bin` to inject 1280×800@90 DTD into LG's EDID — kernel logged "Direct firmware load failed -2" (ostree firmware_class.path /usr/local mount-order issue at early DRM init). Even when EDID does load, Steam BPM filters out 16:10 modes when preferred mode (DTD1) is 16:9. Confirmed not viable for our purpose.
2. vkms virtual card1 — gamescope only binds card0 on NVIDIA proprietary; `WLR_DRM_DEVICES`, `--prefer-output Virtual-1` both ignored. Sunshine's KMS capture also can't dmabuf-import vkms CPU framebuffers on NVIDIA.
3. Physical HDMI Evanlak dummy on HDMI-A-2 alongside LG on HDMI-A-1 — **caused `nvidia-modeset: HDMI FRL link training failed` warnings in dmesg every 20-30s.** Multi-sink HDMI 2.1 contention. LG signal flickered (purple/red colour cycle). Unplugging the Evanlak restored stability.
4. Per-mode `--prefer-output HDMI-A-1` (LG) vs `HDMI-A-2` (Evanlak) switching in `gamescope-wrapper` — same FRL contention class. Even when only one sink is "active", NVIDIA pre-allocates resources for both during driver init.
5. After Evanlak unplug + cold reboot + static `--prefer-output HDMI-A-1` wrapper + 4K@60 + HDR stripped + no `--immediate-flips` — **still flickering**. Indicates the issue is now NVIDIA driver state corruption from a day of churn, not the live config. Full power-off (drain capacitors) was the next escalation; user did this and report continued to evening play.

### Architectural decision: AMD GPU

Concluded the underlying conflict is **NVIDIA proprietary on Bazzite + multi-output + virtual displays + Sunshine capture is structurally broken**. Every workaround hit the same wall class. AMD's open-source amdgpu driver doesn't have these limitations (vkms + Sunshine works, virtual connector capture works, multi-sink HDMI 2.1 clean).

User ordered **Sapphire PULSE RX 9070 XT 16GB** from Scan (£629.99 inc VAT, Scan code LN154765, mfr 11348-03-20G), delivery 2026-05-12. PSU verified at 1200W with spare 8-pin PCIe cables. Z390 board PCIe 3.0 x16 backward-compatible (small 2-3% perf loss vs PCIe 5.0).

### Today's emergency wrapper state on bazzite (replace tomorrow)

`~/bin/gamescope-wrapper` rewritten as "safe mode": HDR flags stripped, no `--immediate-flips` on lower modes, static `--prefer-output HDMI-A-1`, modes capped at 1080p60/1080p120/4k60/4k120/4k165. To survive tonight's GW2 session.

### Pre-move "no dummy, just kernel args" history (corrected understanding)

The April 13 backup (`configs/bazzite/display-backup-2026-04-13/`) shows kernel args `drm.edid_firmware=HDMI-A-2:edid.bin video=HDMI-A-2:e` were enabled, with EDID identifying as "Evanlak8K V2" — so an Evanlak WAS physically plugged in. User remembers it differently (says nothing was plugged in). Combination of both is possible: the Evanlak was on HDMI-A-2 AND the kernel was attempting a custom EDID override on the same connector (which silently failed per the README, falling back to the dummy's EDID). Either way the working state involved EITHER physical Evanlak EDID OR (later, commit `b7ff66e`) a virtual DP-1 with custom EDID.

### Open items for 2026-05-12 (post-GPU-swap session)

- Power down, swap RTX 3080 Ti → RX 9070 XT
- `rpm-ostree rebase ostree-image-signed:docker://ghcr.io/ublue-os/bazzite-deck:stable`
- Verify amdgpu loads, gamescope-session clean, Sunshine VAAPI works
- Re-enable original goal: 1280×800@90 in SteamOS dropdown via vkms or `video=HDMI-A-2:e` virtual connector — should actually work this time
- Re-write `gamescope-wrapper` from scratch on the clean AMD stack
- Full plan in `docs/migration-to-amd.md`

### Files modified this session

- `memory/nvidia-gamescope-state-flicker.md` (NEW) — driver state churn → HDMI flicker
- `memory/MEMORY.md` — added index entry for above
- `configs/bazzite/sunshine-apps.json` — drop GW2-SteamOS prep-cmd (was already committed in 49f221e but live state had diverged again)
- `docs/migration-to-amd.md` (NEW) — full 2026-05-12 runbook

### Hardware ordered/identified

- Sapphire PULSE RX 9070 XT 16GB — £629.99 inc VAT from Scan (LN154765)
- AM4 + 5800X3D path discussed as future CPU upgrade (~£400 incl mobo+cooler, keeps existing 64GB DDR4-3200)
- 90° HDMI 2.1 certified angled connectors recommended for cable strain relief (Cable Matters or Lindy)

## 2026-04-29 (PM) — Per-mode resolution switching, gamescope xwayland constraint, GW2 - SteamOS hybrid app

Long second-half session continuing morning Sunshine work. User wanted per-stream-client resolution switching (Deck → 1280x800, Apple TV → higher) with GW2 settings auto-adapted, plus a clean way to access Big Picture mid-session to change res.

### Major outcome — gamescope's nested xwayland resists size-locking

Hours of investigation into why GW2 always rendered at 2K on the Deck stream regardless of gamescope `-W -H`:

- **`-W/-H` controls the gamescope output canvas; `-w/-h` is supposed to set the nested xwayland init size.** Without `-w/-h` xwayland defaults to the EDID's largest mode.
- **Even with `-w 1280 -h 800` the xwayland gets bumped up to 2560x1440 once Steam/Wine start.** Steam logs literally say `Desktop state changed: desktop: size: 2560,1440`. Steam ignores gamescope's nested-size flags.
- **`xrandr --output gamescope --mode 1280x800` from outside returns `BadMatch`** — gamescope's nested xwayland refuses post-init RRSetScreenSize. Even attempting `--newmode/--addmode` fails on the nested compositor.
- **`--force-windows-fullscreen` only forces window draw size, not xwayland reported size** — Steam keeps redrawing at 2K, conflicting with gamescope's clamping → visible flicker. Useless for our case.
- **`--generate-drm-mode fixed` doesn't force the connector to deck mode either** — gamescope still picks the highest CVT-derivable mode (2560x1440@144) over EDID DTDs.

Net result: with the EVanlak HDMI dongle EDID + NVIDIA proprietary driver, **gamescope's nested xwayland is hardlocked at 2K**. GW2 in `windowed_fullscreen` always renders at 2K canvas. Reverted all these flags (added then removed in same session).

### Why morning auto-detect "worked fine" — `dpiScaling=true` was the secret sauce

User said the AM session worked but PM testing didn't. Diff showed the PM script forced `dpiScaling=false`. With dpiScaling=true (GW2 default), GW2 honours Wine LogPixels (96/144/192 per gamescope mode) and **scales UI proportionally** to the Wine-reported DPI. After Sunshine downscales the 2K-rendered stream to 1280x800 for the Deck, the per-mode UI scaling lands correctly — UI on Deck looks Deck-sized despite the underlying 2K render. Forcing dpiScaling=false killed that compensation. Reverted.

### Final Sunshine app layout

Stripped per-launch GW2 mode flips because they fought Steam Big Picture and GW2 in unstable ways. New design:

- **SteamOS Game Mode** — auto-detect prep-cmd; lands in Big Picture; default controller layout. Use this to set the gamescope mode for the session.
- **GW2 - SteamOS** (NEW) — auto-detect prep-cmd + `sunshine-wait-gw2.sh` cmd that waits up to 30 min for user to launch GW2 from Big Picture, then waits for GW2 to exit, then returns. Steam Input maps the AppName "GW2 - SteamOS" to the GW2 controller layout. Workflow: connect → Big Picture (change res if needed) → launch GW2 → play → Alt+F4 → stream auto-ends.
- **Guild Wars 2** — direct-launch via `sunshine-launch-gw2.sh`. No prep-cmd. Use when you already have the right gamescope mode and want straight-to-game.
- **Active Session** — no prep-cmd, no cmd, just attaches.
- **Kill Game** / **Reboot** — utility apps unchanged.

The script `sunshine-set-resolution.sh` still does the auto-detect + Wine LogPixels swap + GFXSettings RESOLUTION swap + verticalSync=false + SIGKILL pre-edit, but is only wired up to SteamOS Game Mode and GW2 - SteamOS now.

### Steam Deck side: matched shortcut + controller layout for the new app

- Added `Moonlight - GW2 SteamOS` to `shortcuts.vdf` via custom binary VDF appender (no `vdf` Python module on SteamOS) — appid 3161606423
- Removed redundant `Moonlight - GW2` shortcut (was the direct-launch path before this session's split)
- Force-killed Steam (`pkill -9`) before editing so it didn't overwrite shortcuts.vdf with its in-memory copy on shutdown — Steam writes this file on clean shutdown, will clobber external edits if running. **Critical pattern for any future shortcut edits.**
- Added `"moonlight - gw2 steamos"` entry in `configset_controller_neptune.vdf` pointing at `template CLOUD_moonlight/og v18.4.8_0` so the new shortcut applies the GW2 controller layout
- Copied GW2 VoE artwork (4 grid PNG variants) from old appid `2959110740` to new appid `3161606423`. Removed the orphaned old artwork files.
- Removed the now-orphaned `"moonlight - gw2"` configset entry

### Bazzite side cleanup

- Removed unused `gw2-2k.png` / `gw2-4k.png` covers (per-mode app icons that were briefly added then removed)
- Live `~/.config/sunshine/apps.json` matches `configs/bazzite/sunshine-apps.json` ✓

### Files changed this session

- `configs/bazzite/sunshine-apps.json` — final 6-app layout
- `configs/bazzite/gamescope-wrapper.sh` — back to simple `-W -H -r -o` (added then removed `-w/-h`, `--force-windows-fullscreen`, `--generate-drm-mode fixed`)
- `scripts/sunshine-set-resolution.sh` — Apple TV → 2k120 (was 4k120), SIGKILL GW2 before edits, optional MODE arg via `$1` (kept), per-mode Wine LogPixels (kept), per-mode RESOLUTION (kept), verticalSync=false (kept). Removed: forced screenMode, forced dpiScaling.
- `scripts/sunshine-wait-gw2.sh` (NEW) — Sunshine cmd that waits for GW2 to start (30m grace), then waits for it to exit, then returns
- `configs/steam-deck/configset_controller_neptune.vdf` (NEW backup) — current state of the layout pointer table
- `configs/steam-deck/moonlight-shortcuts-summary.txt` (NEW backup) — human-readable summary of the 5 Moonlight shortcuts and their Sunshine app targets

### Apple TV side — pending

User mentioned wanting the same dual-app pattern on Apple TV (Big Picture + GW2 - SteamOS hybrid). Apple TV uses different controller hardware so the layout pointer file is `configset_controller_xbox.vdf` (or whichever matches the connected MFi controller). The Sunshine apps are already there; just need the controller pointer mapping next session.

### New memory entries (PM session)

- `gamescope-nested-xwayland-constraint.md` — gamescope nested xwayland defaults to EDID max and resists post-init resize
- `gw2-windowed-fullscreen-dpiscaling-trick.md` — keep `dpiScaling=true` so per-mode Wine LogPixels does the UI scaling

## 2026-04-29 — Moonlight shortcut split + configset pointer fix

Cross-repo session driven from `itinyk-app` working dir. User rebuilt the Steam Deck library to have multiple Moonlight non-Steam shortcuts (Moonlight - GW2, Moonlight - SteamOS, Moonlight - Reboot Bazzite, Moonlight - Kill Game) so each could carry its own Steam Input layout. The rename of the original "Moonlight" shortcut to "Moonlight - GW2" broke v18.4.8 controller bindings — Steam appeared to revert to v18.4.2 even though `og v18.4.8_0.vdf` was deployed correctly.

### Major outcome — configset_controller_neptune.vdf is the actual pointer

After hours of editing autosave files, Personal cloud entries, ugcmsgcache, and the per-appid dirs, the bug was finally traced to `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/configset_controller_neptune.vdf`. This file maps each app/shortcut (by AppName lowercased) to its layout source — `template` mode points at a local SSH-editable VDF; `workshop` mode points at an immutable Workshop UGC item.

When the rename happened, Steam auto-created a new `"moonlight - gw2"` entry pointing at a default Workshop UGC (which happened to be the v18.4.2 (v18.3 base) item). The old `"moonlight"` entry still correctly pointed at `CLOUD_moonlight/og v18.4.8_0`. Fix: rewrote the new entry to use the same `template` pointer.

Documented in `memory/configset-controller-pointer.md`. Future controller deploys must verify (and fix) configset entries after any shortcut rename.

### Library / Sunshine work that DID succeed

- **Steam Deck shortcuts.vdf rewritten**: programmatically renamed slot 0 ("Moonlight" → "Moonlight - GW2") preserving its appid; added 3 new shortcuts (SteamOS, Reboot Bazzite, Kill Game) each with stable md5-derived appids
- **Cover artwork** placed in `userdata/<id>/config/grid/` for all 4 Moonlight entries (reused Sunshine's existing covers from Bazzite)
- **Each shortcut** uses `flatpak run com.moonlight_stream.Moonlight stream 172.16.100.212 "<Sunshine app name>" --quit-after` so they auto-launch the right Sunshine app
- **Bazzite Flatpak system updates** applied (Mesa 25.3.5, Chrome 147, Firefox 150, runtime updates)

### Outstanding

- **EVanlak EDID dummy plug** purchased to fix Bazzite's 1280×800 virtual display (no real monitor connected; GW2 renders into a tiny canvas during Moonlight stream). Will plug in next session. Custom Deck/Apple TV resolutions still work because Sunshine renders at client-requested res; the EDID just provides the "valid resolutions" envelope.
- **Resolution-based GW2 profile swap** (Deck vs Apple TV → different GFXSettings + Nexus DPI) deferred until EDID plug installed.
- **Bazzite-deck-nvidia is on F43 stable**; F44 deck images lag desktop by a few days. Auto-update will pick up F44 once Universal Blue ships it.

### Files heavily edited (Deck only this session)

- `~/.local/share/Steam/userdata/64793831/config/shortcuts.vdf` — added 3 shortcuts, renamed 1 (preserved appid)
- `~/.local/share/Steam/userdata/64793831/config/grid/` — 8 PNG cover files placed
- `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/configset_controller_neptune.vdf` — added `"moonlight - gw2"` `template` entry (the actual fix)
- `Steam Controller Configs/.../config/2959110740/`, `.../1284210/`, `.../moonlight/` controller_neptune.vdf — overwrites that turned out to be no-ops (configset was the real pointer)

### New memory entries

- `configset-controller-pointer.md` — the appname→layout pointer file and why renaming shortcuts breaks SSH deploys

## 2026-04-26 — Mystic Clicker v3.6.0 → v3.6.8 + Controller v18.4 → v18.4.8

Long session driven by user-reported bugs after v18.4 was promoted from broken Python-regex VDF rewrite to working layout. Eight DLL releases (3.6.0 → 3.6.8) and eight controller revisions (v18.4.0 → v18.4.8) in a single afternoon. Each fix surfaced the next deeper bug.

### Major outcomes

- **v18.4 outage recovery**: a prior session used Python regex to rewrite `dpad_south`/`dpad_east` blocks in the template VDF; the regex consumed Long_Press/Double_Press activator subgroups across multiple groups and the file went 63 121 → 37 170 bytes. Steam Deck silently dropped the layout (Personal tab empty). Recovered by starting from the v18.3 base and applying chord changes via line-anchored Edit calls only. Documented in `docs/vdf-editing-golden-rules.md` (10 rules + recovery playbook).
- **Renamed** `configs/steam-controller/moonlight-gw2-og-v16.1.vdf` → `moonlight-gw2-og-template.vdf`. The `v16.1` in the filename had been misleading since v17.x.
- **New chord collisions discovered and fixed:**
  - `Ctrl+F11` for Merchant collided with `KB_CRAFTY_TOGGLE` in Nexus InputBinds → moved to `Alt+F11`.
  - `Ctrl+Shift+Home` for Reset Windows fired GW2's `SkillProfession5` (user has Home bound to that skill in `GameBinds.xml`; GW2 ignores chord modifiers) → moved to `Ctrl+Shift+Insert`. Insert is unbound across all of the user's GW2 keybinds.
- **Two Steam Input quirks identified and worked around in DLL:**
  - Modifier-hold via uinput (Shift/Alt asserted for the entire physical chord hold) → replaced fixed `Sleep(500)` defers with `WaitForChordModifiersRelease()` poll on `GetAsyncKeyState`. Adapts to actual hold time.
  - Full_Press chord output double-emitted at OS level on `dpad_south` (50–80 ms apart) → Friend chord (and similar Full_Press inventory combos sharing a dpad block with Long_Press / Double_Press) must omit `key_press I` from VDF and let DLL synthesize it via SendInput exactly once per dispatch.
- **Per-combo debounce in `keybinds.cpp`** (300 ms window) catches the second Nexus dispatch when Steam Input double-emits a chord. Each suppressed call logs `Debounced duplicate dispatch of <combo> (<delta>ms since last)` at DEBUG.
- **Window rescue updated**: clamps to 70% of viewport (was display-size only) so oversized windows actually shrink to a usable size on the Deck. Also logs every press regardless of whether anything was rescued.

### Files heavily edited

- `configs/steam-controller/moonlight-gw2-og-template.vdf` — chord reorganization, version bumps, double_tap_time settings, multiple chord-key changes
- `configs/gw2-keybinds/nexus-inputbinds.json` — `MERCHANT_COMBO` Code 87 + Alt; `RESET_WINDOWS` / `MT_RESET_WINDOWS` Code 71 → 45
- `modules/mystic-clicker/input-sim.cpp` — `WaitForChordModifiersRelease` helper; converted Friend / Wizard combos / Merchant to detached-thread + DLL-press-`I` pattern
- `modules/mystic-clicker/keybinds.cpp` — per-identifier 300 ms debounce
- `modules/mystic-clicker/rescue.cpp` — 70% viewport clamp + always-log
- `modules/mystic-clicker/entry.cpp` — version bumps to 3.6.8, RESET_WINDOWS default → CTRL+SHIFT+INSERT
- `docs/vdf-editing-golden-rules.md` — 10 + 1 rules; new Rule #11 covers chord-collision check via Python snippet
- `CHANGELOG.md` — v3.6.2 through v3.6.8 entries
- New memory entries: `chord-emission-quirks.md`, `gw2-keybind-collisions.md`

### Infrastructure changes

- Pushed `nexus-inputbinds.json` updates live to Bazzite via SSH (Mystic Clicker's `MERCHANT_COMBO` and `RESET_WINDOWS` bindings). User must do a full GW2 restart (not just Nexus reload) for InputBinds.json changes to take effect — Nexus reads the file only on startup.
- Deployed VDFs v18.4.3 → v18.4.8 to Steam Deck via SSH overwrite of the Personal save (`url=usercloud://moonlight/og v18.4.N_0`). Each version under a new filename so the previous Personal stays available as a rollback.
- Pulled Mystic Clicker DLL v3.6.5 → v3.6.8 from GitHub Actions artifact and `scp`'d to Bazzite addons dir.

### Open items / next session

- Trading Post and Bank chords on `dpad_east` (`I+F7` / `I+F8`) likely have the same Steam Input Full_Press double-emit bug as Friend did, but the user reports they work — possibly because the second `I` toggle happens within the same frame. If they ever exhibit the open-then-close symptom, convert to the same DLL-press-`I` pattern as Friend.
- The user must do a full GW2 restart on Bazzite to pick up the `Ctrl+Shift+Insert` binding for Reset Windows. Until then, wheel slot 2 emits the new chord but Nexus's in-memory binding is still the old `Ctrl+Shift+Home`.
- Window rescue 70% viewport clamp + always-log are in v3.6.8 but were never logged because the chord wasn't dispatching (per above). Verify after user's restart.
- The `<button>"36"`/`35`/`34`/`33` skill bindings in user's GameBinds.xml are documented in memory but worth re-checking if user re-imports a fresh template.

## 2026-05-01 — Controller v18.4.9 + ESC-close on MC capture & MT delivery

Driven by user reports: "I don't see new controller build on steamdeck" and "add ESC to close capture window".

### Major outcomes

- **Controller v18.4.8 → v18.4.9**: L1 + D-Pad Up Tap = MT Fliplist (Alt+D, "Flip"). MT Delivery dropped from this slot. Long-Press = MT Dashboard unchanged. Double_Press removed entirely. Validation: 298 → 296 bindings, brackets balanced 924/924.
- **Architecture lesson recorded** at `memory/streaming-input-host-vs-client.md`: for the Deck stream, Steam Input runs on the **Deck** (translates physical input to keys *before* Moonlight transmits) — bazzite never sees a gamepad. Apple TV stream is the opposite (raw HID over Moonlight tvOS app → bazzite-side Steam Input applies). My initial v18.4.9 deploy went only to bazzite per-appname CLOUD subdirs, which the Deck-side stream never reads. Fix: also SCP'd to `deck@172.16.100.95:.../moonlight/og v18.4.9_0.vdf` and bumped Deck's `configset_controller_neptune.vdf` "moonlight" + "moonlight - gw2 steamos" entries.
- **MC v3.6.9 → v3.6.10**: capture window now closes on ESC via `GUI_RegisterCloseOnEscape("Mystic Clicker - Capture", &g_ShowCaptureWindow)`; symmetric Deregister in unload.
- **MT 20260420 → 20260501**: Delivery Box now closes on ESC. Previously Dashboard + FlipColumn registered but Delivery Box was an explicit `// (but NOT delivery box)` gap. All three MT windows now consistent.
- **GW2 UI persistence model corrected**: chat box, minimap layout, hero panel are **server-side / account level**, NOT in `Local.dat` as I initially claimed. What stays per-profile: `GFXSettings.Gw2-64.exe.xml`, Nexus addon configs, Wine reg (DPI + time format). Inventory state (compact toggle, sort order) is also per-install. Documented in conversation only — no memory file because the test was empirical.

### Files

- `configs/steam-controller/moonlight-gw2-og-template.vdf` — title v18.4.9, url, drop Double_Press, swap Full_Press F→D (line-anchored Edits, no regex)
- `configs/steam-controller/moonlight-gw2-og-v18.4.9.vdf` — new snapshot
- `configs/steam-deck/configset_controller_neptune.vdf` — bumped to v18.4.9, added `guild wars 2 (apple tv)` and `(steam deck)` entries
- `modules/mystic-clicker/entry.cpp` — Build 9 → 10, Register/Deregister CloseOnEscape
- `modules/mystic-trading/entry.cpp` — Build 20260420 → 20260501, Delivery Box ESC register
- `CHANGELOG.md` — v3.6.10 entry
- `memory/streaming-input-host-vs-client.md` (new) + `memory/MEMORY.md` (index entry)

### Infrastructure changes

- **Bazzite (Og@172.16.100.212)**: configset rewritten with per-appname `template` entries (moonlight, guild wars 2 (apple tv), guild wars 2 (steam deck)); new CLOUD subdirs created for both new GW2 profiles; v18.4.9 VDF deployed in each; Steam fully restarted (kill steamwebhelper + parent + bust htmlcache); both DLLs deployed to all three profile addons dirs (`Steam install`, `gw2-appletv`, `gw2-deck`) with `.bak-20260501-092609` rotation.
- **Steam Deck (deck@172.16.100.95)**: v18.4.9 SCP'd to `moonlight/og v18.4.9_0.vdf`; configset bumped (`moonlight` + `moonlight - gw2 steamos`); Steam respawned via steamwebhelper + parent kill + htmlcache bust.

### Open items / next session

- Mystic Clicker Bouncy Meta Progress capture position (added in v3.6.9 prior session) still needs to be set in-game per profile.
- 12-hour clock fix on Apple TV + Steam Deck profiles: applied via Wine reg `iTime`/`sShortTime`/`sTimeFormat` patch earlier today; user said "will need test after 12 as AM moment so will work either way". Verify after noon.
- Build "Bouncy Meta Progress" capture coordinates per profile or copy `.cfg` files between profiles (positions are resolution-dependent).

## 2026-05-01 (evening) — Controller v19.0/v19.1 + Deck profile InputBinds catastrophic-drift recovery (4+ hour debug)

Session started with user reporting auto-sort inventory (DPad-Right hold) was firing Community LFG instead. What followed was a **dual-failure-mode** debugging session that took ~4 hours to resolve. Documented in detail at [nexus-inputbinds-per-profile-drift.md](nexus-inputbinds-per-profile-drift.md) Dual-failure section.

### Root cause (of the symptom)

Two independent failures stacked:

1. **Controller layout side**: Steam Input UI auto-saved a local copy of `controller_neptune.vdf` with `Ctrl+C` on `dpad_east` Long_Press instead of `Ctrl+Q`. The file path was `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight - gw2 steamos/controller_neptune.vdf` with `description: "#SettingsController_AutosaveDescription"` (Steam Input's autosave marker). User firmly denies making this edit; mechanism likely a Steam Input UI side-effect of opening the layout editor or a sync/merge event. **This broke both Apple TV and Deck streams** because Apple TV uses Deck-as-controller per [streaming-input-host-vs-client.md](streaming-input-host-vs-client.md) (older note about bazzite-side Steam Input governing Apple TV is wrong for this setup).

2. **Nexus side**: Deck profile's `InputBinds.json` had **23 entries** drifted vs canonical (Apple TV / repo source-of-truth). Many bindings cleared (`Code=0`), several mismapped. Cause of the magnitude unknown — see open-bug section in nexus-inputbinds-per-profile-drift.md for ruled-out theories.

Surgical fixes (only `DEPOSIT_AND_SORT` + `0xFC56DD9F_ToggleLFG`) didn't converge because each only addressed one of the two failures. The breakthrough was recognizing the dual-failure mode and:

1. Replacing controller layout with clean canonical (deployed v19.0, then v19.1 with `Double_Press` removed from `dpad_east`)
2. Wholesale-copying Apple TV profile's clean `InputBinds.json` over the Deck profile's drifted state

After both layers fixed: auto-sort works, Trading Post works, Bank works, Mystic Forge works, all chord bindings restored.

### Major outcomes

- **Controller v18.4.9 → v19.0 → v19.1**: title bumps to make active layout visible in Steam Input UI; `Double_Press` `S+U+M` removed from `dpad_east` (was dead code — three keys aren't a usable GW2 chord). Validation: 296 → 293 bindings.
- **bazzite Sunshine recovery**: a separate Sunshine-can't-find-display issue surfaced when SDDM was restarted. Root cause: bazzite's immutable `/usr` (composefs) doesn't allow `setcap` on `/usr/bin/sunshine`, AND `Og` was not in `video`/`render` groups (system groups live in read-only `/usr/lib/group`; user-side memberships need to be added to `/etc/group`). Fix: appended `video:x:39:Og` and `render:x:105:Og` to `/etc/group`, then `sudo systemctl restart sddm` to refresh the user session's group membership. Sunshine can now access `/dev/dri/card0`.
- **gw2-deck native install identified on the Deck** at `/home/deck/.local/share/Steam/steamapps/common/Guild Wars 2/` (78 GB, appid 1284210). Used as a streaming-isolation test environment by copying the Apple TV InputBinds + v19.1 controller layout there too.
- **Memory honest correction**: my initial memory entry attributing the 23-binding drift to "passive collision-resolution" was over-reach. Updated to acknowledge cause-unknown with theories tested.

### Files

- `configs/steam-controller/moonlight-gw2-og-template.vdf` — title v19.1, Double_Press removed from dpad_east, line-anchored Edits per VDF rules
- `configs/steam-controller/moonlight-gw2-og-v19.0.vdf` — snapshot
- `configs/steam-controller/moonlight-gw2-og-v19.1.vdf` — snapshot (current canonical)
- `memory/nexus-inputbinds-per-profile-drift.md` — added Dual-failure mode section, Open bug section (cause unknown), wholesale-vs-surgical decision rule, capture-diagnostics recipe for next time

### Infrastructure changes

- **bazzite (Og@172.16.100.212)**:
  - `/etc/group`: appended `video:x:39:Og` and `render:x:105:Og`
  - `sudo systemctl restart sddm` (to refresh user session group membership for Sunshine)
  - `Documents/Guild Wars 2/nexus-configs/backup-2026-04-30-19-45.zip` was deleted by Addon_Config_Backup_Tool's auto-rotation (kept `backup-2026-04-30-20-27.zip`)
  - `~/Games/gw2-deck/addons/Nexus/InputBinds.json` ← wholesale-copied from `~/Games/gw2-appletv/addons/Nexus/InputBinds.json` (canonical state); backup at `InputBinds.json.bak-pre-applewipe-20260501-215910`
  - `~/Games/gw2-deck/addons/Nexus/InputBinds.json.bak-20260501-193659` — earlier surgical-restore backup (before wholesale)
  - `~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/Nexus/InputBinds.json` ← also wholesale-copied from Apple TV
  - `~/.local/share/Steam/userdata/64793831/config/configset_controller_neptune.vdf` — restored from repo (was missing entirely on bazzite; cause unknown)
  - `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight/og v18.4.9_0.vdf` — added (was missing); also v19.0 + v19.1 deployed to all per-appname CLOUD subdirs
- **Steam Deck (deck@172.16.100.95)**:
  - `~/.local/share/Steam/userdata/64793831/config/configset_controller_neptune.vdf` — restored from repo (was missing)
  - `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight - gw2 steamos/controller_neptune.vdf` — overwritten with v19.0 then v19.1; backup at `.bak-locally-edited-20260501-202007` (the Ctrl+C autosave that started this whole mess)
  - `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/1284210/controller_neptune.vdf` — overwritten with v19.1 for native Deck GW2 testing; backup at `.bak-pre-test-*`
  - `~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/Nexus/InputBinds.json` ← wholesale-copied from Apple TV (Deck native install)
  - Multiple `pkill steam.sh` cycles to force config reloads

### Open bugs / unresolved

- **🐛 Cause of 23-binding drift on Deck profile InputBinds is unknown.** Theories ruled out: passive Nexus drift, OneDrive sync, Steam Cloud sync, Addon_Config_Backup_Tool restore. Theories on the table: addon update bulk-resetting identifiers, stale file overwrite during multi-profile setup on Apr 30, accumulated silent collision resolutions over weeks. **If recurs**, capture diagnostic snapshots BEFORE wholesale-restoring (recipe in [nexus-inputbinds-per-profile-drift.md](nexus-inputbinds-per-profile-drift.md) open-bug section).
- **🐛 Cause of `controller_neptune.vdf` autosave with `Ctrl+C` instead of `Ctrl+Q` is unknown.** User firmly denies editing the controller layout. The file had `description: "#SettingsController_AutosaveDescription"` and `progenitor: usercloud://moonlight - gw2 steamos/controller_neptune` indicating Steam Input UI autosaved it. Mechanism unclear. **If recurs**, capture the file's progenitor + url + description fields and any Steam Input UI events from the day it changed.
- **🐛 Cause of `configset_controller_neptune.vdf` going missing on both bazzite and Deck simultaneously is unknown.** This is the appname→layout pointer file. Was missing entirely on both machines at session start. Restored from repo backup. Mechanism for both being deleted simultaneously unclear.
- **dpad_east Long_Press not firing** — initially suspected Steam Input regression from today's SteamOS update (`/etc/os-release` modified May 1 01:28). Ultimately resolved to be the dual-failure mode (controller emitting wrong key + Nexus binding cleared). Dual fix worked. Whether SteamOS update contributed is unverified — left as ambient possibility.
- **Long-term mitigation for InputBinds drift** — user hasn't decided between sync script / diff script / per-launch validation. For now, the manual recovery recipe is the standing fix.

### What worked (for next-time triage)

- **Smoke test using `Q` typed in chat** — temporarily change Long_Press binding to plain `Q` and check if the keystroke shows up in GW2 chat. If `Q` doesn't appear, the Long_Press isn't firing at the controller layer (rules out Nexus). If `Q` appears, Long_Press fires fine and the issue is downstream (binding misalignment).
- **Test Device Inputs on the Deck** for hardware verification (Settings → Controller → Test Device Inputs). Rules out physical button issues.
- **Tail Nexus.log live with grep filter** during user testing to capture exactly which addon dispatch fires:
  ```bash
  ssh Og@172.16.100.212 'timeout 60 tail -F "$HOME/Games/gw2-deck/addons/Nexus/Nexus.log" 2>&1 | grep --line-buffered -E "MysticClicker|DEPOSIT|Combo|LFG"'
  ```
- **Wholesale-copy from Apple TV profile** is the canonical recovery — Apple TV's InputBinds.json reliably matches `configs/gw2-keybinds/nexus-inputbinds.json` repo source-of-truth.

### Late addition — Alt+F4 hard-quit theory

User raised this after the main session: the Menu button's Long_Press is bound to `Alt+F4 (Quit Game)` ([template:2908-2914](configs/steam-controller/moonlight-gw2-og-template.vdf#L2908-L2914)). User uses this to quit GW2. While Alt+F4 normally goes through `WM_CLOSE → Nexus Unload → InputBinds.json save → exit`, in some cases (mid-frame, hung process, repeat-firing during Long_Press, Wine "not responding") it can become a force-terminate before Nexus's `Unload` runs.

The mechanism that fits the drift evidence:

1. Bindings drift slightly via passive Nexus collision-resolution (1-2 cleared per session)
2. User notices in-game, re-binds via Nexus options menu (in memory only)
3. Alt+F4 quit → in-memory fix never saved → disk still has the old cleared state
4. Repeat across sessions; cumulative cleared count grows because fixes don't persist
5. Disk state diverges from what user expects, despite "fixing" several times

**This is now the most plausible candidate** in the open-bug section of [nexus-inputbinds-per-profile-drift.md](nexus-inputbinds-per-profile-drift.md). Mitigation: avoid Alt+F4 to quit (use in-game Logout / Exit menu); or do a clean GW2 exit any time you change a binding in-game.

Apple TV profile being less drift-affected is consistent with this — different usage pattern, possibly more clean quits.

### Planned for next session — Mystic Clicker GRACEFUL_QUIT macro

Replace controller `button_escape` Long_Press = `Alt+F4` (Quit Game) with a graceful Mystic Clicker macro to eliminate the force-quit drift hypothesis.

**Flow** (no confirm dialog click — clicking Exit closes the game directly):

1. Press `Esc` (open in-game menu)
2. Sleep 200ms
3. SendInput click at captured `g_ExitGameX/Y` coordinate
4. Done — GW2 closes via normal shutdown path → Nexus `Unload` runs → InputBinds.json saves cleanly

**Files to change:**

| File | Change |
| --- | --- |
| `modules/mystic-clicker/shared.h` | Add `GRACEFUL_QUIT` constant; extern `g_ExitGameX/Y` |
| `modules/mystic-clicker/entry.cpp` | `InputBinds_RegisterWithString(GRACEFUL_QUIT, ProcessKeybind, "ALT+SHIFT+Q")`; matching Deregister; bump version to v3.6.11 |
| `modules/mystic-clicker/keybinds.cpp` | Dispatch `GRACEFUL_QUIT → SimulateGracefulQuit()` |
| `modules/mystic-clicker/input-sim.cpp` | `SimulateGracefulQuit()`: `SendInput(VK_ESCAPE)` → `Sleep(200)` → `SimulateClickAt(g_ExitGameX, g_ExitGameY)` with log line |
| `modules/mystic-clicker/capture-ui.cpp` | New capture entry "Exit Game" or "Exit to Character Select" with description |
| `modules/mystic-clicker/config.cpp` | Read/write `ExitGameX=`, `ExitGameY=` per-resolution `.cfg` |
| `configs/gw2-keybinds/nexus-inputbinds.json` | Add `GRACEFUL_QUIT` entry: scancode for Q (16), Ctrl=False, Shift=True, Alt=True |
| `configs/steam-controller/moonlight-gw2-og-template.vdf` | `button_escape` Long_Press: replace `LEFT_ALT + F4` with `LEFT_ALT + LEFT_SHIFT + Q` (label `Graceful Quit`); bump title to v19.2 |
| `configs/steam-controller/moonlight-gw2-og-v19.2.vdf` | New snapshot |
| `CHANGELOG.md` | v3.6.11 entry |
| `modules/mystic-clicker/CMakeLists.txt` (if version-string lives there) | Bump |

**Default chord** (TBD with user tomorrow): `ALT+SHIFT+Q` works because Q is unused with that modifier combo, mnemonic matches "Quit". Alternatives: `CTRL+SHIFT+Q`, `ALT+Q`. Verify against existing nexus-inputbinds.json before picking.

**Captures needed in-game** (after deploy):

- Steam Deck profile (1280×800) — capture once
- Apple TV profile (2560×1440) — capture once
- Local play profile (3840×1600) — capture once

User uses Capture Mode (Ctrl+Shift+C) → "Exit Game" → 5s countdown → click the actual Exit button position.

**Deploy steps:**

1. Code + commit → trigger GitHub Actions build → pull `mystic-clicker.dll` artifact
2. Deploy DLL to all 3 profiles (`addons/mystic-clicker.dll`) per [nexus-multi-deploy-rules.md](nexus-multi-deploy-rules.md)
3. Deploy v19.2 controller layout to Deck (`config/moonlight - gw2 steamos/controller_neptune.vdf` + `moonlight/og v19.2_0.vdf` + `1284210/controller_neptune.vdf`)
4. Deploy updated `nexus-inputbinds.json` to all 3 profiles
5. Restart Steam on Deck so layout reloads
6. Capture Exit Game position in each profile
7. Verify by holding Menu button — GW2 should close gracefully (no Alt+F4 force-quit)

**Estimated effort:** ~1.5 hours coding + 30 min deploy + capture per profile


## 2026-05-02 (AM) — GRACEFUL_QUIT shipped + Utility Wheel center cleared (v3.6.11 + v19.2 + v19.3)

Implemented yesterday's plan and shipped a follow-up wheel cleanup. End-to-end deploys verified; user reports working in-game ("working a treat!").

### Mystic Clicker v3.6.11 + controller v19.2 — GRACEFUL_QUIT replaces Alt+F4

- **New keybind `GRACEFUL_QUIT`** (default `ALT+SHIFT+Q`, scancode 16+Alt+Shift). Verified no chord collision against `gamebinds.xml` (Q-no-modifier = MoveTurnLeft only) and against the 16 wheel slots in the controller VDF. Q with Ctrl is taken by `DEPOSIT_AND_SORT` — Alt+Shift was the cleanest free combo.
- **Flow**: `SimulateKeyPress(VK_ESCAPE)` → `Sleep(200)` → `SimulateClickAt(g_ExitGameX, g_ExitGameY)`. Used `SimulateClickAt` not `SimulateRealClickAt` because the in-game menu is a GW2 UI surface, not a Nexus overlay.
- **Per-resolution coords**: added `g_ExitGameX/Y` globals + `ExitGameX/ExitGameY` keys to the `.cfg` reader/writer/reset paths in `config.cpp`. Capture entry "Exit Game" added at the end of `s_Targets[]` in `capture-ui.cpp`. New `CAPTURE_EXIT_GAME` Nexus identifier registered (no default key) so it surfaces in Nexus settings.
- **Controller v19.2**: `button_escape` Long_Press changed from `LEFT_ALT + F4` to `LEFT_ALT + LEFT_SHIFT + Q` (label "Graceful Quit"). Validation: brackets 921=921, groups 56, bindings 293→294 (replaced 2 with 3, net +1).

### Controller v19.3 — Utility Wheel center cleared

- User reported the wheel center fires every release-without-direction, triggering Open Settings unintentionally on every wheel use.
- Removed `touch_menu_button_0` (was `F11, Open Settings`); added `touch_menu_button_16` with the same F11 binding so Settings is on the ring instead of the center. Wheel now has 16 ring slots, no center button. Validation: brackets 921=921, groups 56, bindings unchanged (294, removed 1 added 1).
- Other 15 slots unchanged (Toggle Paths, Reset Windows, ArcDPS, Crafty Legend, Waypoint, Fishing, Skiff, Rift, Capture Mode, Community LFG, Hoard & Seek, Event Timers, Organizer, Game Wiki, Nexus) — muscle memory preserved.

### Deploy

- DLL + nexus-inputbinds.json deployed to all 4 GW2 profile dirs: bazzite Local play / Apple TV / Steam Deck + Deck native. SHA-256 matches across all 4 + source repo.
- Controller VDFs deployed to:
  - **Deck (active)**: `1284210/controller_neptune.vdf`, `moonlight - gw2 steamos/controller_neptune.vdf` (autosave URL preserved via local sed), `moonlight/og v19.X_0.vdf`
  - **bazzite (parity, snapshots only — configset still points at v18.4.9)**: `moonlight/og v19.X_0.vdf`, `guild wars 2 (apple tv)/og v19.X_0.vdf`, `guild wars 2 (steam deck)/og v19.X_0.vdf`
- All backups tagged `*-bak-pre-3611-*` / `*-bak-pre-v19[23]-*`. Bazzite configset update deferred — would require killing Steam mid-session.

### Critical gotcha — JSON CRLF preservation

When rewriting `nexus-inputbinds.json` via Python `json.dump`, the file got rewritten with LF endings even though the original used CRLF. This made the diff show 3802 lines changed (whole-file rewrite). Fix: `f.write(text.replace('\n', '\r\n').encode('utf-8'))`. Resulting diff: clean +18 lines (just the two new entries).

### Open: capture step is per-resolution and user-driven

User needs to capture "Exit Game" position in each profile before the macro fires (otherwise alerts "Exit Game position not set"). Capture flow: `Ctrl+Shift+C` → "Exit Game" → 5s countdown → `Esc` → hover Exit to Character Select → countdown clicks. Once per resolution: 1280×800 / 2560×1440 / 3840×1600.

### Steam-on-Deck reload caveat

Steam keeps controller layouts in memory and writes-through on shutdown. After my deploys to `moonlight - gw2 steamos/controller_neptune.vdf`, if user shuts Steam down without first reloading, the in-memory pre-deploy state could clobber my changes. Mitigation: have user restart Steam (or open Steam Input UI and save) before next stream session. Same pattern as the `pkill -9 steam` rule in `vdf-editing-golden-rules.md` for shortcuts.vdf.

## 2026-05-02 (PM) — Sunshine first-connect crashes fixed by removing prep-cmd

User reported: first Moonlight connect from Deck/Apple TV often shows "session error" then works on retry. Investigation found 16 Sunshine crashes in the past week, all `Error reading events from display: Broken pipe` triggered by `sunshine-set-resolution.sh`'s `gamescope-mode` call. Sequence: prep-cmd runs → gamescope tears down to switch mode → Sunshine's wayland connection breaks → exit status 1 → ~10s gap (RestartSec=5 + ExecStartPre=sleep 5) before Sunshine accepts again. User's first-connect window times out during that gap.

### Why the script became obsolete

The split into 3 GW2 profiles (Local / Apple TV / Deck) gave each its own Wine prefix with permanent `LogPixels` baked into `compatdata/<appid>/pfx/user.reg`. The script used to flip Wine DPI per stream — now that's redundant because each profile has the right DPI saved per-prefix. And user now sets gamescope resolution manually in SteamOS Big Picture, so the `gamescope-mode` call was also redundant. The script was actively *worse* than nothing because it overwrote both stream profiles' DPI to match the connecting client (e.g. Apple TV connecting set Deck's prefix to 144 too, undoing the per-profile config).

### Changes

- **`~/.config/sunshine/apps.json`** (and `configs/bazzite/sunshine-apps.json` repo backup): removed `prep-cmd` block from `SteamOS Game Mode` and `GW2 - SteamOS` apps. No more script invocation.
- **Deck prefix** (`compatdata/3111887265/pfx/user.reg`, `[Control Panel\Desktop]`): LogPixels `dword:00000090` (144) → `dword:00000060` (96). Reset to correct Deck-native DPI; this had been wrong since the 14:11:39 Apple TV connect that bumped it.
- **Restarted `sunshine-gaming.service`** to pick up the new apps.json. Confirmed clean start, no crashes.
- Backups: `apps.json.bak-pre-strip-*`, `user.reg.bak-pre-dpi96-*`.

### Result

Verified DPI state across all 3 prefixes:

| Profile | LogPixels | Status |
|---|---|---|
| Apple TV (2879321470) | 0x90 = 144 | ✓ correct, unchanged |
| Deck (3111887265) | 0x60 = 96 | ✓ corrected |
| Local (1284210) | 0x60 = 96 | ✓ correct, unchanged |

Each prefix now keeps its own DPI permanently — no more cross-profile bleed on connect. Sunshine no longer touches gamescope on connect → no more broken-pipe crashes → first-connect should be reliable from both Deck and Apple TV.

`/var/home/Og/scripts/sunshine-set-resolution.sh` is now dead code on bazzite. Left in place for now (could be deleted in a future cleanup if confirmed unused).

### Systemd unit speedup deferred

Was about to drop `ExecStartPre=/bin/sleep 5` and reduce `RestartSec=5s` to `1s` to shrink the post-crash gap from ~10s to ~1s. Not needed anymore — with prep-cmd gone, there's nothing to crash from.

## 2026-05-03 — Mystic Clicker v3.6.12 → v3.6.16 + controller v19.4

Long session shipping five MC bumps + a major controller revision. End-state: 5 commits pushed, all 4 GW2 profiles deployed (DLL `0daaa396…`, JSON `8dcd5362…`), Deck VDF deployed to all 3 paths.

### v3.6.12 — TP + Bank combos use DLL-press-`I` (intermittency fix)

User reported TP combo "open then close inventory, sometimes open" — landed on the captured-slot double-click hitting a closed panel. Found that `SimulateTradingPostCombo` and `SimulateBankCombo` were the only two combos still calling the old `OpenInventoryAndDoubleClick` helper, which assumes the VDF chord pressed `I`. But the chords are bare F7 and F8 with no `I` — so inventory was at the mercy of Steam Input's chord double-emit. Migrated both to the proven `WaitForChordModifiersRelease` + `OpenInventoryDllAndDoubleClick` pattern (same as Teleport Friend, Wizard Gobbler, Lounge Pass, Merchant). DLL now synthesizes `I` itself, exactly once per dispatch.

### v3.6.13 — Capture window grouped by category + clearer names

Capture target list grew too big to scan at a glance. Added a `category` field to `CaptureTarget`, populated `s_Categories[]` with 12 group names, and refactored the render loop to emit one `ImGui::CollapsingHeader` per group (default collapsed). Each header shows `(N unset)` or `(all set)` so it's obvious which categories still need attention. Within-group sort still "uncaptured first then alphabetic". Active 5-second capture countdown auto-expands its category via `ImGui::SetNextItemOpen(true)` so the highlighted button stays visible. Uses `###cat_%d` ID suffix on the header label so the visible-text change (unset count) doesn't reset open/closed state.

19 display-label renames (TP Collect, TP Cancel Listing, Single Craft, Wizard Vault Collect, Wizard Vault Confirm, etc.). **Saved coordinates are NOT affected** because `mystic-clicker.cfg` keys (e.g. `TradingPostX=`) are tied to the internal `g_*` variables, not display names — verified by spot-checking 12 keys in `config.cpp`.

### v3.6.14 — Shorten "Generic Accept Combo" header

The `(N unset)` suffix wrapped the header onto a second line. Renamed to "Generic Accept" — "Combo" was implied since these slots are only used by the Accept-Combo sequence. Single `replace_all` of 22 occurrences in capture-ui.cpp.

### v3.6.15 — Pathing toggle-all macro + bind world render layer

User asked why the Utility Wheel's "Toggle Paths" slot wasn't triggering the render. Found the chord (Ctrl+F3) was hitting nothing — Pathing addon's `pathing-render-toggle` had `Code=0` (unbound) in `nexus-inputbinds.json`. Bound it to `Alt+Shift+F3` (parity with the F1/F2 family that already worked for `pathing-render-minimap-toggle` and `pathing-render-map-toggle`). Added a new `PATHING_TOGGLE_ALL` macro (default `CTRL+F3` = the wheel chord) that fires Alt+Shift+F1, Alt+Shift+F2, Alt+Shift+F3 in sequence with 50 ms gaps so a single bind toggles all three render layers at once. Detached thread + `WaitForChordModifiersRelease` so the wheel-held Ctrl is released before the Alt+Shift sends start (otherwise GW2 sees Ctrl+Alt+Shift+Fn and Pathing's exact-modifier match would miss).

`SendInput` with KEYEVENTF_SCANCODE for the modifier+Fn key sequence — same approach as `OpenInventoryDllAndDoubleClick` for reliability under Wine/Sunshine.

### v3.6.16 + controller v19.4 — Remove brittle Leave Party Combo, slash-command alternative on chat wheel, Exit Instance moved

User: "I can't get leave party work as there too many clicks UI move too much". The right-click-party-bar → click-Leave macro depended on two captured UI positions which moved per-resolution and per-instance — fundamentally brittle. Asked the GW2 wiki: GW2 has `/leave` (party only) and `/squadleave` / `/sqleave` (squad only) slash commands. Moved party-leaving to the chat (Commands) wheel as native slash-command-typing slots — no UI dependency.

Removed entirely from MC: `LEAVE_PARTY_COMBO`, `SimulateLeavePartyCombo()`, two capture entries ("Party Bar (right-click)" + "Leave Party (in menu)"), four globals (`g_PartySquadBarX/Y` + `g_LeavePartyX/Y`), and three Nexus binding entries (`LEAVE_PARTY_COMBO`, `CAPTURE_PARTY_SQUAD_BAR`, `CAPTURE_LEAVE_PARTY`). The "Party" category disappeared from the capture window. Persisted `PartySquadBarX/Y`/`LeavePartyX/Y` keys in users' `mystic-clicker.cfg` files linger as unused; harmless.

Controller v19.4 bundled changes:

- Commands wheel slot 6 ("+"): replaced `LEFT_SHIFT + EQUALS + RETURN` with single `KEYPAD_PLUS + RETURN`. The `Shift+Equals` form was actually broken — Steam Input fires each `key_press` as a discrete keypress, not a chord, so Shift released before Equals fired and the slot was typing `=` not `+`.
- Commands wheel slot 15 (NEW): `/leave` + Enter → leaves party.
- Commands wheel slot 16 (NEW): `/squadleave` + Enter → leaves squad.
- `touch_menu_button_count` 14 → 16.
- R1 + DPad-Down Double_Press: `Shift+F10` (Leave Party Combo) → `Ctrl+E` (Exit Instance). A-Long-Press Exit Instance kept as redundant binding.

VDF validation: brackets 931=931, groups 56, bindings 312 (up from 294: +18 from slot 15/16 additions and slot 6 simplification).

### Sunshine first-connect crashes — fixed earlier in the day session

User reported "first Moonlight connect from Deck/Apple TV often shows session error then works on retry". Investigation found 16 Sunshine crashes in the past week, all `Error reading events from display: Broken pipe` triggered by `sunshine-set-resolution.sh`'s `gamescope-mode` call tearing down gamescope, which broke Sunshine's wayland connection. Sunshine then exited status 1 → systemd `RestartSec=5s` + `ExecStartPre=sleep 5` = ~10 s gap before next Moonlight connect could land.

Script became obsolete with the 3-profile split: each GW2 profile carries its own permanent Wine `LogPixels` in `compatdata/<appid>/pfx/user.reg`, and the user now sets gamescope mode manually in SteamOS Big Picture. The script was actively *worse* than nothing because it overwrote both stream profiles' DPI to match the connecting client (e.g. Apple TV connecting set Deck profile to 144 too). Stripped `prep-cmd` from `SteamOS Game Mode` and `GW2 - SteamOS` apps in `~/.config/sunshine/apps.json` (live + repo backup). One-time fix to Deck prefix LogPixels 144 → 96 (had drifted from a prior Apple TV connect). Sunshine restarted cleanly — no crashes since.

### Important Steam Input gotcha worth remembering

Multiple `binding` entries within a single `Full_Press > bindings` block fire as **discrete sequential keypresses**, NOT a held chord. This is fine for typing patterns like `T, Y, RETURN` → "ty\n", but breaks when you need a held modifier (e.g. `Shift+Equals` to type `+`). Workaround: pick a single-key alternative (`KEYPAD_PLUS` for `+`, etc.) or use a different binding mode. Worth a memory entry — likely to recur.

## 2026-05-04 (PM) — Moonlight stream-stuck diagnostic + TP/Bank chord double-`I` fix

### Moonlight "stuck on GW2 character select" investigation

User reported intermittent failure: Moonlight stream from bazzite shows GW2 frozen at character select with bazzite cursor visible and Nexus icons rendered but unclickable. GW2 itself was alive (478% CPU, addons cycling normally). Three contributing factors found:

- **Dual Sunshine sessions** (`active sessions: 2`): Apple TV streaming to TV + Deck connected as controller → encoder forks, focus stalls. Will be moot once dedicated Steam Controller arrives ~2026-05-09 and Deck stops being used as a controller.
- **Apple TV profile GFXSettings drift**: `RESOLUTION` was Deck-sized 1280×800 in compatdata `2879321470`. Fixed to 2560×1440 to match gamescope output and Wine LogPixels 144.
- **Wrong Sunshine unit name**: `sunshine.service` is masked on bazzite; correct unit is `sunshine-gaming.service`. First restart attempt failed before I figured this out.

Recovery played: SIGTERM Gw2-64.exe, restart sunshine-gaming, fix GFXSettings, single-client reconnect. User confirmed working but flagged it's intermittent and hard to fully verify — wants to revisit if it recurs after Steam Controller arrives.

Created `memory/moonlight-stream-stuck-diagnostic.md` (project type) capturing the full playbook.

### TP + Bank chord double-`I` flicker — fixed

User reported Trading Post combo regressed to "rapidly opens and closes inventory" on Apple TV; other chords fine. Hashed `mystic-clicker.dll` on all 4 profiles (Steam install, Apple TV, Deck-bazzite, Deck-native) — all v3.6.16, same hash. So it wasn't a deployment gap.

Root cause was in the controller VDF, not the DLL. The v3.6.12 migration moved `I` press inside the DLL (`OpenInventoryDllAndDoubleClick`), but the VDF chord bindings for Trading Post Combo (R1 + DPad-East Full_Press) and Bank Combo (R1 + DPad-East Long_Press) still emitted `key_press I, Open Inventory` alongside their `F7`/`F8` macro keys. Sequence:

1. VDF presses `I` → inventory opens
2. F7 fires `TRADING_POST_COMBO` → DLL `OpenInventoryDllAndDoubleClick`
3. DLL releases modifiers, presses `I` → inventory closes (!)
4. DLL sleep 600ms, double-click captured TP icon → panel is closed → flicker

Removed both stale `key_press I, Open Inventory` lines (template lines 2142, 2150). TP and Bank chords now bare `F7`/`F8` matching post-migration pattern of Teleport Friend (bare F6).

Apple TV exposed it more reliably than Deck because Apple TV's raw-HID path has higher input latency, making the DLL's `I` press more likely to register as distinct from the VDF's `I` press. On Deck (Steam Input local), the timing sometimes coalesced into a single `I` event by chance.

### Live deploy structure clarified

While deploying v19.5, mapped out the actual VDF deploy targets (previously fuzzy). Active layout files Steam Input reads:

- bazzite Apple TV stream: `/home/Og/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/guild wars 2 (apple tv)/og v18.4.9_0.vdf`
- bazzite Deck stream: same parent, `guild wars 2 (steam deck)/og v18.4.9_0.vdf`
- bazzite moonlight: same parent, `moonlight/og v18.4.9_0.vdf`
- Deck native: `/home/deck/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/1284210/controller_neptune.vdf`

The configset `configset_controller_neptune.vdf` references `og v18.4.9_0` (CLOUD prefix) for all three bazzite profiles. The repo backup has the same. So the active filename is `og v18.4.9_0.vdf` even though it now contains v19.5 content — Steam Input reads by configset reference path, not by title parsing. URL field inside each file patched to match its actual location (`usercloud://<dir>/og v18.4.9_0`).

Deployed v19.5 by overwriting `og v18.4.9_0.vdf` in place across the 3 bazzite paths + Deck native's `controller_neptune.vdf`. Backups saved at `*.bak-pre-v195-20260504`.

### Files changed

- `configs/steam-controller/moonlight-gw2-og-template.vdf` — title v19.4 → v19.5; removed 2 stale `I` press lines (TP + Bank chords)
- `configs/steam-controller/moonlight-gw2-og-v19.5.vdf` — new snapshot
- `CHANGELOG.md` — controller v19.5 entry
- `memory/moonlight-stream-stuck-diagnostic.md` — new (project type)
- `memory/MEMORY.md` — index entry for moonlight-stream-stuck-diagnostic

### Live deploys

- bazzite (Og@172.16.100.212): 3 paths overwritten + 3 backups created
- Deck (deck@172.16.100.95): 1 path overwritten + 1 backup created
- Live Wine prefix (Apple TV): GFXSettings `RESOLUTION` 1280×800 → 2560×1440 (one-time fix)

### Commits

- `fcb2bd7` — `docs(memory): Moonlight stuck-at-char-select diagnostic playbook`
- `0843373` — `controller v19.5: fix TP + Bank chord rapid open/close flicker`

### Open / next session

- User to test TP/Bank chords after relaunching GW2 — should be a clean single-open + double-click, no flicker. If flicker persists on either, the active layout file Steam Input picked may differ from the 4 we updated; grep all VDFs for `key_press I, Open Inventory` to find any stragglers.
- Steam Controller arrives ~2026-05-09. Once Deck is no longer needed as a TV controller, the dual-Sunshine-session symptom should stop. If "stuck character select" recurs after that, focus on causes 2 (GFXSettings drift) and 3 (`sunshine-gaming.service` unit-name confusion) per `memory/moonlight-stream-stuck-diagnostic.md`.

## 2026-05-05 — InputBinds drift recurrence + TP fix on Deck-stream + repo-as-source-of-truth tooling

### InputBinds drift on Deck profile (incident #2)

Drift recurred 3 days after the v19.2 GRACEFUL_QUIT mitigation. **Crucial new datapoint**: today's session ended with a fully clean shutdown sequence in Nexus.log (`[Core] SHUTDOWN END` with full addon Unload chain). Not Alt+F4. **Falsifies the Alt+F4 hypothesis** that drove v19.2.

Same 23 binds drifted on Deck profile only; Apple TV stayed clean. Restored from Apple TV (canonical hash `8dcd5362a5830f20`). Forensic snapshots committed to `forensics/inputbinds-drift/*-20260505.*`.

New strongest theory: drift entries cluster around addon-owned identifiers (CraftyLegend, Hoard__Seek, Community_LFG, Organizer, Mystic Clicker, NexusGameWiki) and several look like "default preset" patterns rather than collisions. Possibly an addon rewrites its keybind on first load when DLL version differs from the one that wrote the bindings the previous session. Need per-DLL hash + mtime snapshot for next-incident forensics.

### Nexus version mistake

Initial diagnosis claimed Nexus was 10 months out of date (`v2025.4.22.1050`). User asked why auto-update wasn't working. Investigation found the actual version (UTF-16 PE VersionInfo) is `v2026.2.17.1210` — current latest. The `v2025.4.22.1050` I grepped was a stale UTF-8 string elsewhere in the binary. Methodology fix: when reading a Windows DLL version, decode UTF-16LE first as authoritative; UTF-8 is fallback only.

Auto-update **is** working — `[Updater] Installed Build of Nexus is up-to-date.` runs every game launch. So the InputBinds drift is happening on the latest Nexus version, suggesting an unfixed bug or a non-Nexus cause.

### TP combo regressed on Deck-stream after restore

User reported TP "rapidly opens and closes inventory" on Deck-stream-via-Moonlight after I restored InputBinds. Bank works, TP flickers — same flicker pattern as the original v19.5 fix.

**Root cause**: yesterday's v19.5 deploy hit `1284210/controller_neptune.vdf` on Deck native (used for direct GW2 launch), but **NOT** the autosave file `moonlight - gw2 steamos/controller_neptune.vdf` (used by the "GW2 - SteamOS" Sunshine streaming flow). Plus historical `og v19.x_0.vdf` files in the `moonlight/` dir on Deck remained stale.

Per [streaming-input-host-vs-client.md](streaming-input-host-vs-client.md): Deck-as-client does Steam Input locally, so chord emission comes from Deck native's layout, not bazzite's. We had patched the wrong file on Deck.

Fix: enumerated 17 stale-pattern VDFs across Deck native and patched all. User confirmed TP working after `pkill steamwebhelper` forced Steam Input layout reload.

Captured the lesson in new memory entry [controller-vdf-deploy-checklist.md](controller-vdf-deploy-checklist.md) — when changing controller VDF chord behavior, patch every stale-pattern file on every machine, not just the configset-referenced one. Two specific traps: historical `og v19.x_0.vdf` files in profile dirs (Steam Input UI may have any selected) and autosave files in `moonlight - gw2 steamos/`.

### Repo-as-source-of-truth tooling

User asked: can Nexus configs follow the same version-tracking pattern as controller VDFs, and can controller VDFs become repo-first too?

Built two deploy scripts:

**`scripts/deploy-nexus-config.sh`** — push canonical InputBinds/AddonConfig/GameBinds from `configs/gw2-keybinds/` to all 4 profiles in one shot. Pre-flight refuses if GW2 running anywhere, timestamped backups, SHA256 verified per target, unreachable hosts warned and skipped (not fatal). Verified end-to-end deploy: all reachable profiles match canonical hash.

**`scripts/deploy-controller-vdf.sh`** — push canonical template VDF from `configs/steam-controller/` to every active Steam Input layout file across both machines. Filename filter (`controller_neptune.vdf` or `og v<version>_<N>.vdf`) skips pre-2025 legacy `og_<N>` exports. Per-location URL patching (autosave:// for `moonlight - gw2 steamos/`, usercloud:// elsewhere). Patched 7 newly-discovered bazzite stragglers.

Snapshot pattern matches controller VDFs: `nexus-inputbinds-v1.json` is the immutable golden master of 2026-05-05 working state.

**Workflow going forward**: edit canonical in repo → run deploy script → commit. Never edit on-device first.

### Files changed today

- `configs/steam-controller/moonlight-gw2-og-template.vdf` — title v19.4 → v19.5; removed 2 stale `key_press I` lines
- `configs/steam-controller/moonlight-gw2-og-v19.5.vdf` — new snapshot
- `configs/gw2-keybinds/nexus-inputbinds-v1.json` — golden master snapshot (new)
- `scripts/deploy-nexus-config.sh` — new
- `scripts/deploy-controller-vdf.sh` — new
- `memory/controller-vdf-deploy-checklist.md` — new (project type)
- `memory/nexus-inputbinds-per-profile-drift.md` — appended 2026-05-05 incident + falsified Alt+F4 hypothesis section
- `memory/nexus-multi-deploy-rules.md` — added canonical-source workflow section pointing at script
- `memory/MEMORY.md` — index entries
- `forensics/inputbinds-drift/{inputbinds-broken-deck-20260505.json, backup-list-20260505.txt, nexus-load-deck-20260505.log}` — new
- `CHANGELOG.md` — controller v19.5 entry + tooling 2026-05-05 entry

### Live deploys at session end

- Bazzite Og@172.16.100.212: 3 Nexus profiles canonical (`8dcd5362…f20`); 19 controller VDFs canonical (12 already-matching + 7 newly-patched), 43 historical/legacy skipped intentionally
- Deck native deck@172.16.100.95: was asleep at end-of-session deploy attempt. **Pending**: re-run both scripts when Deck is awake to finish:
  ```
  ./scripts/deploy-nexus-config.sh
  ./scripts/deploy-controller-vdf.sh
  ```
- Live one-time fixes earlier in day: GW2 Apple TV GFXSettings 1280×800 → 2560×1440, Sunshine ghost-session restart

### Commits

- `fcb2bd7` — Moonlight stuck-at-char-select diagnostic memory
- `0843373` — controller v19.5 TP+Bank flicker fix
- `69cb768` — chat log 2026-05-04 PM session
- `75047bd` — InputBinds drift forensics + falsified Alt+F4 hypothesis
- `4abac88` — deploy-nexus-config.sh + controller VDF deploy checklist
- `cd6e22b` — deploy-controller-vdf.sh + nexus-inputbinds-v1 golden master + resilience

### Open / next session

- Re-run both deploy scripts when Deck is awake
- TP fix verified working on Deck-stream after `pkill steamwebhelper` reload — **report any other chord regressions**
- If InputBinds drift recurs (incident #3), capture per-addon DLL hash + mtime snapshot for forensic comparison (per memory action item)
- Steam Controller for bazzite arrives ~2026-05-09; will eliminate the dual-Sunshine-session symptom from Apple-TV-stream + Deck-as-controller pattern

## 2026-05-10 — Mystic Clicker COPY_ITEM_NAME OCR pipeline

Built end-to-end OCR-based "copy hovered item name to clipboard" macro for the destroy-confirm dialog, after a long debugging arc through three different fundamental architectural failures.

### Architecture (final)

```
F10 / L1+DPad-East Long_Press
    │
    ▼
Mystic Clicker DLL (in GW2 Wine process)
    │  capture D3D11 back buffer via Nexus APIDefs->SwapChain
    │  apply yellow-pixel filter (R≥180, G≥150, B≤100)
    │  write BMP to /tmp/gw2-ocr-input-<TS>.bmp
    │  touch /tmp/gw2-ocr-input-<TS>.ready
    ▼
gw2-ocr-daemon.service (host-side, OUTSIDE sandbox)
    │  poll /tmp at 10 Hz for *.ready files
    │  /usr/bin/tesseract <bmp> <out> -l eng --psm 6
    │  touch /tmp/gw2-ocr-done-<TS>
    ▼
DLL polls for done marker, reads .txt, runs:
    │  trim leading/trailing non-letter chars per line
    │  whitelist [A-Za-z space ' -]
    │  reject 1-letter words, single-word <4 letters, low avg word length
    │  reject lines containing player's character name (MumbleLink)
    │  concatenate clean lines (handles wrapped item names)
    ▼
WriteClipboardUtf8(item_name) + GUI_SendAlert
User: manual Ctrl+V into the textbox (auto-paste doesn't reach textbox under Apple TV stream)
```

### Three architectural dead-ends (and what saved each)

1. **`start /unix /wait /usr/bin/tesseract` from Wine** — broken in Proton 11.0 ("Could not translate the specified Unix filename to a DOS filename"). Fixed by introducing the host-side daemon with a /tmp file-rendezvous protocol (BMP + .ready trigger / .txt + .done marker). New script `scripts/gw2-ocr-daemon.sh` + systemd user unit `configs/bazzite/gw2-ocr-daemon.service`.

2. **`BitBlt(GetDC(NULL))` from inside Wine** — returns all-black on bazzite because GW2 renders via DXVK → Vulkan → gamescope, bypassing Wine's GDI desktop surface entirely. Confirmed by inspecting raw forensic captures. Fixed by reading directly from `IDXGISwapChain` (exposed at `APIDefs->SwapChain`) on a Nexus `RT_PostRender` callback. New module `modules/mystic-clicker/screen-capture.{h,cpp}`.

3. **Yellow filter caught inventory icons + overlays** — first failure was 700×500 capture missing the yellow text (it sits ~400px right of cursor in textbox at 2560×1440); bumped to 1800×900. Then tesseract noise from inventory icons (`"fall cal call oa"`, `"a fro"`, `"el Ao Woe"`) was passing the longest-line picker. And ArcDPS character-name overlay (`"Morrowmage"`) was real but wrong. Fixed via the line filter pipeline now documented in `memory/ocr-line-filter-tuning.md` — trim, whitelist, structural heuristics, character-name blacklist via MumbleLink.

### Files added/changed

| File | Purpose |
| --- | --- |
| `modules/mystic-clicker/screen-capture.{h,cpp}` | NEW — D3D11 swap-chain capture on render thread |
| `modules/mystic-clicker/ocr.{h,cpp}` | OCR pipeline: capture → BMP → daemon rendezvous |
| `modules/mystic-clicker/item-name.cpp` | COPY_ITEM_NAME macro: yellow filter + line filter + clipboard |
| `modules/mystic-clicker/entry.cpp` | Register/deregister screen-capture hook |
| `mystic-clicker.vcxproj` | Add screen-capture.cpp to build |
| `scripts/gw2-ocr-daemon.sh` | NEW — host-side tesseract worker (polling, no inotify) |
| `configs/bazzite/gw2-ocr-daemon.service` | NEW — systemd user unit |
| `memory/d3d11-back-buffer-capture-vs-gdi.md` | NEW — why GDI BitBlt is dead |
| `memory/ocr-line-filter-tuning.md` | NEW — line filter heuristics + rationale |
| `memory/mystic-clicker-build-and-deploy.md` | Updated — daemon section added |
| `memory/feedback_always_watch_and_deploy_dll.md` | NEW — verify CI+watcher+4-profile-hash before "done" |

### Infra changes (bazzite)

- `rpm-ostree install tesseract` — done earlier this session (reboot 13:13)
- `~/scripts/gw2-ocr-daemon.sh` installed (committed clean polling version)
- `~/.config/systemd/user/gw2-ocr-daemon.service` enabled + started
- Verified `systemctl --user is-active gw2-ocr-daemon` = `active`

### Open / next session

- Final test: relaunch GW2, open destroy popup, click textbox, F10 — should put correct item name on clipboard with no Morrowmage prefix and no inventory-icon noise. Deployed DLL hash: `b7bc71e2a1412f7f` (commit `7bf668e`).
- Manual Ctrl+V is the documented paste path; auto-paste was removed because `SendInput(Ctrl+V)` doesn't reach GW2's destroy-confirm textbox under Apple TV stream + Steam Input chain.
- ArcDPS font-size reduction is an alternative noise-reducer but no longer needed (character name now blacklisted via MumbleLink).
- If a new noise pattern emerges for an item, the daemon doesn't keep snapshots anymore (clean version restored). Re-add the `cp "$bmp" "$DEBUG/last.bmp"` lines to the daemon for one-off debugging.

## 2026-05-13 — Post-AMD script audit + cleanup

Continuation of 2026-05-12 AMD swap. Audited all scripts and systemd timers on bazzite for NVIDIA-specific or stale multi-profile references.

### Confirmed working (no changes needed)

- `btrfs-nightly-clone.sh` + timer (03:00 daily) — hardware-agnostic
- `bazzite-auto-update.sh` + timer (01:00 daily) — rpm-ostree handles AMD drivers
- `sunshine-set-resolution.sh` — calls gamescope-mode, no GPU references
- `sunshine-launch-gw2.sh` / `sunshine-wait-gw2.sh` — pgrep-based, hardware-agnostic
- `gw2-ocr-daemon.sh` — running, tesseract-based
- `kill-game.sh` / `reboot.sh` / `bazzite-reboot-notify.sh` — generic
- `power-button-reboot.sh` — event2 still correct for power button
- `gw2-backup-sync.sh` — rclone to gdrive, hourly timer, paths valid
- `gamescope-mode` / `switch-lg-to-bazzite.sh` — hardware-agnostic

### Issues fixed

1. **Deleted `sync-gw2-exe.sh`** (bazzite + repo) — referenced deleted `~/Games/gw2-appletv/` and `~/Games/gw2-deck/` dirs. Dead code since multi-profile consolidation.

2. **Updated `gamescope-wrapper`** (bazzite + repo) — removed NVIDIA HDR-strip workaround + safe-mode comments. Passes upstream `$@` through unmodified (HDR works on AMD). Aligned all modes with `gamescope-mode`: added `2k165`, `1080p165`, `1080p144`, `deck`, `deck-vkms`. Default changed from 1080p60 to 4k60.

3. **Created `proton-ge-update.timer`** (bazzite + repo) — weekly Monday 02:00. Service existed but had no trigger. Enabled, first run confirmed GE-Proton10-34 up to date.

4. **Cleaned NVIDIA references** from `bazzite-auto-update.sh` comments (bazzite + repo).

### User confirmations

- OCR F10 test: working (closes item from 2026-05-10)
- DPI locked at 0xC0 (200%) + MouseSensitivity 20 — user happy, no more tuning
- Nexus UI scaling (GlobalScale 1.5, FontSize 16) + Mystic Clicker re-capture — user will test/redo themselves

### Files changed

| File | Change |
| --- | --- |
| `scripts/sync-gw2-exe.sh` | DELETED (repo + bazzite) |
| `configs/bazzite/gamescope-wrapper.sh` | Rewrote: AMD modes, no HDR strip |
| `configs/bazzite/proton-ge-update.timer` | Daily → weekly Monday 02:00 |
| `scripts/bazzite-auto-update.sh` | Comment cleanup (NVIDIA → generic) |

### Open / next session

- **DP→HDMI adapter + Steam Controller 2**: blocked on delivery (~today/tomorrow). Test CEC `/dev/cec0`, `switch-lg-to-bazzite.sh`, 4K@120-144 + HDR + VRR
- **Nexus UI scaling**: user testing GlobalScale 1.5 + FontSize 16 in-game
- **Mystic Clicker re-capture**: user will redo at 200% DPI

## 2026-05-13 (PM) — Bazzite display audit, Sunshine/Moonlight cleanup, Deck non-Steam games

Session focused on post-reboot hygiene: display audit, Sunshine config fixes, Moonlight app list trim, and rebuilding the Deck's non-Steam game library.

### Display audit

Removed `deck-vkms` virtual display mode from `gamescope-wrapper` and `gamescope-mode` on bazzite. Was a holdover from the NVIDIA virtual-display era — no longer needed on AMD with the single-install architecture. Live files + repo synced.

### Sunshine config fixes

Three issues found and fixed in `~/.config/sunshine/sunshine.conf`:

1. **Xbox controller injection**: `controllers = 0` was silently ignored — Sunshine renamed the option to `controller` (singular) in newer versions. Changed to `controller = 0`. Confirmed via Sunshine logs: `Unrecognized configurable option [controllers]` no longer appears.

2. **System default apps leaking**: Sunshine was serving Desktop, Low Res Desktop, and Steam Big Picture alongside user apps. Added `file_apps = /home/Og/.config/sunshine/apps.json` to suppress system defaults from `/usr/share/sunshine/apps.json`.

3. **Correct service name**: `sunshine-gaming.service` (user unit), not `sunshine.service` (masked system unit). Already known from prior session but still tripped me up initially.

### Sunshine apps trimmed to 3

Rewrote `~/.config/sunshine/apps.json` to contain exactly:

- **SteamOS** — lands in Big Picture
- **Reboot Game** — runs `kill-game.sh`
- **Reboot Bazzite** — runs `reboot.sh`

Removed: SteamOS Game Mode, Active Session, GW2 - SteamOS, Guild Wars 2, Kill Game, Reboot (old names).

### Sunshine wrapper scripts removed

Deleted from bazzite (`~/scripts/`) and repo (`scripts/`):

- `sunshine-launch-gw2.sh` — direct GW2 launcher (obsolete since consolidation)
- `sunshine-wait-gw2.sh` — GW2 wait-for-exit wrapper (obsolete)
- `sunshine-set-resolution.sh` — per-client Wine DPI swap (obsolete since 2026-05-02 prep-cmd removal)

### Moonlight client-side app cache discovery

Despite Sunshine correctly serving the new 3-app list (confirmed via `curl -u sunshine:admin123 https://localhost:47990/api/apps`), Moonlight on the Deck kept showing the old 6-app list. Root cause: **Moonlight caches the app list in its config file** at `~/.var/app/com.moonlight_stream.Moonlight/config/Moonlight Game Streaming Project/Moonlight.conf` under the `[hosts]` section (`1\apps\1\name=...` etc). The `moonlight list` CLI command reads this cache, NOT the server.

Fix: removed all `1\apps\*` entries from the Moonlight config. Next GUI connection to bazzite will re-fetch the current 3-app list. Created memory entry `moonlight-client-app-cache.md`.

### Deck non-Steam games rebuilt

Steam Cloud sync made `shortcuts.vdf` manipulation unreliable — entries written to the file got wiped on Steam restart because Cloud had synced the "empty" state after user manually removed stale entries (VS Code, Chrome, GW2 Addon Loader) through the UI.

Used `steamos-add-to-steam` (native SteamOS tool that sends `steam://addnonsteamgame/` URL to running Steam client) instead. Created 3 launcher scripts on Deck:

- `~/bin/moonlight-steamos.sh` → `flatpak run com.moonlight_stream.Moonlight stream 172.16.100.212 "SteamOS"`
- `~/bin/moonlight-reboot-game.sh` → `... "Reboot Game"`
- `~/bin/moonlight-reboot-bazzite.sh` → `... "Reboot Bazzite"`

All 3 successfully added via `steamos-add-to-steam`. User confirmed working.

### Sunshine web UI password

Set Sunshine web UI password to `admin123` during debugging (via `sunshine --creds sunshine admin123`). Used for API verification only. Consider changing to a stronger password in a future session.

### Repo commit

`6e8f4f7` — bazzite cleanup: fix controller injection, trim Sunshine to 3 apps, remove wrappers. 6 files changed: 3 configs modified, 3 scripts deleted.

### Files changed

| File | Change |
| --- | --- |
| `configs/bazzite/sunshine.conf` | `controllers` → `controller`; added `file_apps` |
| `configs/bazzite/sunshine-apps.json` | 6 apps → 3 apps (SteamOS, Reboot Game, Reboot Bazzite) |
| `configs/bazzite/gamescope-wrapper.sh` | Removed `deck-vkms` mode |
| `scripts/sunshine-launch-gw2.sh` | DELETED |
| `scripts/sunshine-wait-gw2.sh` | DELETED |
| `scripts/sunshine-set-resolution.sh` | DELETED |
| `memory/moonlight-client-app-cache.md` | NEW |

### Open / next session

- **DP→HDMI adapter + Steam Controller 2**: still pending delivery. Test CEC, 4K@120-144 + HDR + VRR
- **Rename Deck non-Steam entries**: script filenames show as-is (moonlight-steamos.sh etc); user can rename via Steam UI for cleaner names
- **Sunshine web UI password**: change from `admin123` to something stronger
- **Moonlight app cache on other clients**: Apple TV, Mac mini, MacBook Air may also have stale cached app lists — clear on next connect or when issues arise

## 2026-05-13 PM — GW2 login flicker fix + DPI tuning + performance check

### What was done

- **GW2 login screen flicker investigation**: user reported flickering on login window that started after 2026-05-12 DPI bump to 0xC0 (200%). No flicker on character select or in-game.
- **screenMode fix**: changed GFXSettings.xml `screenMode` from `fullscreen` to `windowed_fullscreen` — fullscreen is documented as problematic under gamescope (GW2 tries to call SetDisplayMode, fails inconsistently). Did not fix the flicker alone.
- **Wine LogPixels reverted**: dropped from 0xC0 (200%) back to 0x90 (150%) in both `[Control Panel\\Desktop]` and `[Software\\Wine\\Fonts]` sections of user.reg. User reported flicker started after this DPI was increased; reverting to test.
- **Nexus GlobalScale bumped**: increased from 1.5 to 2.0 and FontSize from 16 to 20 in Nexus Settings.json to compensate for lower Wine DPI — user was happy with GW2 native UI at 150% but Nexus addons were too small.
- **SteamOS beta / NTSync research**: user asked about kernel improvements for GW2. Key finding: NTSync (Windows NT synchronization primitives in Linux kernel) is already active on bazzite (kernel 6.17, `/dev/ntsync` loaded with 3820 refs). Deck is on SteamOS 3.7.24 / kernel 6.11 — needs 3.8.x beta for NTSync on native Deck games.
- **Bazzite performance profiling**: checked system load during idle and WvW zerg:
  - Idle: GW2 336% CPU, GPU 20%, 78% CPU idle
  - Zerg: GW2 300% CPU, GPU 42%, 79% CPU idle
  - i9-9900K boosting to 4.6 GHz — GW2's single-threaded renderer is the bottleneck, not hardware
  - Sunshine input handling is negligible — local controller won't improve FPS, only input latency (~5-10ms saved)

### Live config changes on bazzite

| File | Change |
| --- | --- |
| GFXSettings.Gw2-64.exe.xml | `screenMode` fullscreen → windowed_fullscreen |
| user.reg (appid 1284210) | `LogPixels` 0xC0 → 0x90 (both Desktop + Wine\\Fonts sections) |
| Nexus Settings.json | `GlobalScale` 1.5 → 2.0, `FontSize` 16 → 20 |

### Repo changes

| File | Change |
| --- | --- |
| `configs/gw2-keybinds/nexus-settings.json` | Backed up with new GlobalScale/FontSize |
| `configs/gw2-keybinds/gfxsettings.xml` | Backed up with windowed_fullscreen |
| `memory/per-profile-fixed-dpi.md` | Updated current value to 0x90/150% |

### Open / next session

- **Login flicker**: user testing whether 0x90 DPI resolves it — confirm next session
- **Nexus scaling**: user said "can go much larger" — may want GlobalScale > 2.0 next session
- **SteamOS 3.8 beta on Deck**: user plays native games on Deck too; 3.8.x would bring NTSync for those
- **DP→HDMI adapter + Steam Controller 2**: still pending delivery

## 2026-05-18 — Controller v20, Claude screen-reader voice (ElevenLabs), headless read

### What was done

- **mystic-clicker v3.6.20**: Accept-slot naming-drift cleanup — renamed drifted
  "Accept" combo labels so the visible name matches the action across the addon
  UI, the controller VDF, and logs (`General Accept 2` → `Accept 4`, etc.).
- **controller v20.0**: added "Home Instance Portal" to the Utility Wheel. First
  attempt expanded the wheel to 17 slots → Steam silently rejected the whole
  layout (16-button cap). Fixed by replacing "Community LFG" — wheel stays at 16.
- **mystic-clicker v3.6.21**: cursor-anchored "Read at Cursor" (captures a
  960×620 box at the mouse cursor) + Esc-to-stop.
- **mystic-clicker v3.6.22**: headless AI read — `CLAUDE_READ_SCREEN` no longer
  opens the capture panel; the answer is spoken. Killed the user hitting the
  position-capture 5-second countdown when triggering the AI from the controller.
- **gw2-claude — ElevenLabs TTS**: daemon gained an engine selector
  (piper / elevenlabs / auto). ElevenLabs is far more natural; live voice "Lily"
  (British female). Playful reading-tone added to the daemon SYSTEM_PROMPT.

### Live config / infra changes on bazzite

| What | Change |
| --- | --- |
| `1284210/controller_neptune.vdf` | controller v20.0 (16-slot wheel, Home Instance Portal, renamed Accept buttons, title "Og v20.0") |
| `addons/mystic-clicker.dll` | v3.6.22 deployed |
| `~/scripts/gw2-claude-daemon.py` | ElevenLabs support deployed |
| `~/.config/gw2-claude/config.env` | added `GW2_CLAUDE_TTS_ENGINE`, `ELEVENLABS_API_KEY`, `ELEVENLABS_VOICE_ID` (Lily), `ELEVENLABS_MODEL` |
| Piper voices | added `en_GB-northern_english_male-medium`, `en_GB-jenny_dioco-medium` |
| bazzite | rebooted twice — Steam caches the controller layout; an on-disk VDF change needs a restart |

### Repo changes

- mystic-clicker: `keybinds.cpp`, `entry.cpp`, `capture-ui.cpp`, `claude-vision.{h,cpp}`, `input-sim.cpp`, `shared.h`
- `configs/gw2-keybinds/controller_neptune.vdf` — v20.0
- `scripts/gw2-claude-daemon.py`, `scripts/gw2-claude-setup.sh` — ElevenLabs + voice
- `memory/steam-touch-menu-16-button-cap.md` (new), `gw2-claude-vision.md`, `controller-vdf-deploy-checklist.md`, `cec-controller-wake-script.md` (updated); CHANGELOG, `.gitignore`

### 1Password

- New item **Home → "ElevenLabs API Key - GW2 Screen Reader"** (API Credential) — holds the ElevenLabs TTS key.

### Open / next session

- **controller-wake-tv**: first controller wake after a bazzite reboot is swallowed by the `ever_seen_data` gate — TV doesn't switch. Optional fix pending.
- **Streaming-profile controller VDFs**: v20 deployed only to the direct-bazzite config (`1284210/`). If streaming to Deck/Apple TV resumes, the Deck-side / streaming-profile VDFs need v20 too.
- **Home Instance Portal** sits in the old Community-LFG wheel slot (10), not 12 o'clock — reposition if wanted.

## 2026-05-19 — Mystic AI 1.1.10 → 1.1.13: cursor-anchored TP, controller R1+Menu, TP-region feature removed

Long session refining the Mystic AI screen reader's capture trigger and output, then removing the standalone TP-region feature entirely.

### What was done

- **Cursor-anchored TP region (1.1.10).** The saved TP region captured a fixed screen spot — useless for inventory. Made it cursor-relative: the box is stored as an offset from the cursor and re-anchored to the live cursor on each read.
- **Global-Esc bug fixed (1.1.10).** Mystic AI's WndProc swallowed Esc whenever book-watch was armed / speech playing — even with no visible UI — so Event Timers' Esc-to-close stopped working. Fixed: only own Esc while a Mystic AI window is on screen (`g_uiActive = (g_mode != MODE_IDLE)`).
- **Full action panel on the TP-region read (1.1.11).**
- **Research no longer times out or auto-reads (1.1.12).** 45s ceiling → 180s for Research (web-search write-ups legitimately take ~60s); Research is now silent (panel-only, voiced on Read). Overview prompt hardened so item tooltips aren't misread as plain text.
- **Quartz Crystals TP bug fixed (daemon).** `bltc_find_item` now retries plural→singular ("Quartz Crystals" → "Quartz Crystal") — gw2bltc lists singular names.
- **Controller → R1+Menu = Capture (v24.1 → v24.5).** Long detour: R1+R2 / L1+Menu chords "did nothing". Root cause found in Steam's `controller.txt` log — `KEYPAD_MINUS` is an INVALID Steam Input key token. Reverted to Alt+F10. Final: R1+Menu Full_Press = Capture (drag), Long_Press = Read Book. Dedicated TP-region hotkey dropped. See `memory/steam-input-invalid-key-tokens.md`.
- **TP-region feature removed entirely (1.1.13).** R1+Menu drag-capture already reads whatever the cursor is on with the full panel, so the cursor-anchored region + its keybind were redundant. Removed the "Save TP region" button, the Settings row, the `MYSTIC_AI_TP_REGION` keybind, and all backing code (`StartTpRead`, `CMD_TPREGION`, `MODE_BOOKPANEL`, `OpenPanelNotice`, `keepCrop`/`TakeRegionCrop`). −251 net lines.

### Deployed (bazzite `Og@172.16.100.212` + Deck `deck@172.16.100.95`)

| What | Version / md5 |
| --- | --- |
| `addons/mystic-ai.dll` | 1.1.13 — md5 `dbbc492d` |
| `~/scripts/gw2-claude-daemon.py` | md5 `e39f1d5a` (overview/research/bltc fixes) |
| `Nexus/InputBinds.json` | MYSTIC_AI_CAPTURE = Alt+F10, TP_REGION unbound |
| `1284210/controller_neptune.vdf` | v24.5 — md5 `b16528be` |

bazzite: deployed via the sddm dance. Deck: GW2 was closed so the DLL copied direct; controller VDF deployed via `systemctl --user stop steam-launcher.service` (the no-sudo Deck equivalent of the sddm dance).

### Repo changes

- `modules/mystic-ai/{overlay,claude-vision,config,entry,keybinds}.cpp` + `.h`, `shared.h`
- `configs/gw2-keybinds/{controller_neptune.vdf, nexus-inputbinds.json}`
- `scripts/gw2-claude-daemon.py`
- `CHANGELOG.md`; `memory/steam-input-invalid-key-tokens.md` (new)

### Open / next session

- **Other GW2 profiles not synced.** Only the main `Guild Wars 2/addons/` on bazzite + the Deck got 1.1.13 — the apple-tv and deck-bazzite profiles on bazzite are still on the old build.
- **mystic-clicker.dll on the Deck** not synced to latest CI (intentional — unchanged this session).


---

## 2026-05-19 — Back-fill: recovered May 13-15 session logs

These seven entries were logged to the itinyk-app project's auto-memory
directory by mistake during May 13-15 sessions and never reached this repo.
Recovered verbatim during itinyk-app session cleanup. The reusable lessons
were already captured in dedicated memory files (`bazzite-performance-tuning.md`,
`cef-overlay-black-screen-after-stream.md`, `cec-controller-wake-script.md`);
this restores the activity log.

## 2026-05-15 (continued) — Mystic modules: UI fixes, VDF deploy fix, in-window settings

### What was done
- **ROOT CAUSE: VDF deployed to wrong path**: R1+dpad_east bindings (Bank hold, Merchant double-tap, TP tap) weren't working because VDF changes were being deployed to the Moonlight path instead of the native GW2 app path (`Steam Controller Configs/64793831/config/1284210/controller_neptune.vdf`). Fixed by deploying to native path.
- **F11 Merchant conflict**: F11 opens GW2 Options menu natively. Changed MERCHANT_COMBO from F11 (scan 87) to F9 (scan 67) in both VDF and Nexus InputBinds.
- **Controller config cleanup**: Removed all legacy VDF directories from bazzite, only native `1284210/` remains.
- **Mystic Clicker UI Scale**: Added `g_UIScale` config variable with slider in Settings collapsing header. Scales font, spacing, padding, button heights, window width.
- **Mystic Clicker close button fix**: Moved Close Panel button to top of window (always visible). BeginChild negative-height approach didn't work reliably with ImGui 1.80 + Nexus GlobalScale 2.0.
- **Mystic Clicker text size fix**: Reduced font to 85% (`g_UIScale * 0.85f`) to fit at Nexus GlobalScale 2.0. Compacted button labels: `"Name (x,y)"` instead of `"Name  (x, y)"`, `"(-)"` for unset.
- **Mystic Clicker min width constraint**: Added `SetNextWindowSizeConstraints` (min 400*scale) to prevent text clipping.
- **Mystic Trading in-window appearance settings**: Added Text Size / Icon Size / Row Height / Opacity sliders to right-click context menus on Flip List, Dashboard, and Delivery Box windows. Previously only accessible via Nexus addon options panel.
- **Mystic Trading opacity persistence**: Added `window_opacity` to SaveConfig/LoadConfig (was missing).
- **GitHub Actions builds**: All changes built and deployed via CI. DLLs deployed to bazzite via SCP.
- **Nexus config backup**: Refreshed nexus-inputbinds.json and nexus-settings.json from bazzite.

### Files changed (guildwars2 repo)
- `modules/mystic-clicker/capture-ui.cpp` — close button top, font 85%, compact labels, min width, UI scale
- `modules/mystic-clicker/config.cpp` — g_UIScale load/save
- `modules/mystic-clicker/shared.h` — extern g_UIScale declaration
- `modules/mystic-trading/render.cpp` — appearance sliders in all context menus
- `modules/mystic-trading/api-client.cpp` — window_opacity persistence
- `configs/steam-controller/moonlight-gw2-og-v19.9.vdf` — R1 dpad_east fixes, F9 merchant, activator order
- `configs/gw2-keybinds/nexus-inputbinds.json` — MERCHANT_COMBO F9, KB_MENU Ctrl+F5
- `configs/gw2-keybinds/nexus-settings.json` — refreshed from bazzite

### Commits (guildwars2 repo)
- `863fdd2` — mystic-clicker UI scale + VDF R1 dpad fixes
- `fdd8b38` — close button scroll fix + trading appearance settings
- `ff5da34` — widen window + min size constraint
- `8e8b31a` — close button moved to top
- `4345d80` — font 85%, compact labels
- `f857c5c` — backup nexus keybinds + settings

### Key facts
- **Native GW2 VDF path**: `Steam Controller Configs/64793831/config/1284210/controller_neptune.vdf` (NOT Moonlight path)
- **ImGui version**: 1.80 (Nexus bundled) — negative BeginChild height works but unreliably with GlobalScale 2.0
- **Nexus GlobalScale**: 2.0, FontSize: 20.0 — all addon UI rendered at 2x
- **F9 scan code**: 67 (Nexus uses scan codes, not VK codes)

### Resolved
- R1+dpad_east all three bindings working (TP tap F7, Bank hold F8, Merchant double-tap F9)
- VDF path confusion — documented correct native path
- F11 Merchant conflict — changed to F9
- Mystic Clicker close button visibility
- Mystic Clicker text overflow at GlobalScale 2.0
- Mystic Trading appearance settings accessible in-window

### Open items
- SUM chord double-fire — removed interruptable=1, hold_repeats=0 still in place, needs user testing
- Community LFG addon broken (`CRITICAL: Cancelled load:`) — community addon, not fixable by us
- Test UI Scale slider at different values (user confirmed 1.0 looks better)

---

## 2026-05-15 (continued) — TV WOL over WiFi fix

### What was done
- **TV not waking from standby**: Wake script WOL worked when TV was already on (Apple TV input), but couldn't wake TV from full standby. Root cause: LG TV "Wake on LAN" (WiFi) setting was disabled — WiFi radio powered down completely in standby, so WOL magic packets never reached it
- **Fix**: Enabled "WoL" in LG TV settings (Settings → General → Devices). TV now responds to WOL over WiFi even from standby
- **Verified**: TV off → controller wake → WOL powers on TV → WebOS switches to bazzite — full flow working end-to-end

### Files changed
- None (TV settings change only)

### Resolved
- Wake script retry on cold-start TV — now working after enabling TV WoL setting

### Open items
- Verify SUM chord no longer double-fires (user testing)
- `scripts/vendors/workspace/workspace_users.py` (itinyk-app) has uncommitted changes from another session

---

## 2026-05-15 — Wake script retry fix, CEF overlay auto-fix, VDF double-fire fix

### What was done
- **Wake script failed overnight**: WebOS error `8:1008` — TV was in deep sleep, 2s WOL wait too short. Added retry logic: 4 attempts with 3/5/8/12s waits, re-sends WOL between attempts
- **CEF overlay black screen recurred**: same stuck steamwebhelper overlay when switching TV inputs. Added `pkill steamwebhelper` to wake script — runs after successful TV switch, Steam auto-restarts it in ~5s, clears the overlay automatically
- **VDF double-fire on L1+right dpad**: "SUM" chord (three `key_press S/U/M` bindings) was typing twice. Added `hold_repeats=0` to the Full_Press settings to prevent re-trigger

### Files changed
- `configs/bazzite/controller-wake-tv.py` (guildwars2) — retry logic + steamwebhelper kill
- `configs/steam-controller/moonlight-gw2-og-v19.9.vdf` (guildwars2) — hold_repeats=0 on SUM chord

### Commits
- `1544800` (guildwars2) — wake script: add retry logic + CEF overlay fix; VDF: hold_repeats=0 on SUM chord

### Open items
- Verify SUM chord no longer double-fires (user testing)
- ~~Verify wake script retry succeeds on cold-start TV (next morning test)~~ — resolved, TV WoL WiFi setting was off
- `scripts/vendors/workspace/workspace_users.py` (itinyk-app) has uncommitted changes from another session

---

## 2026-05-13 PM (continued) — CEF overlay black screen fix

### What was done
- **Diagnosed black screen on bazzite after TV input switch**: after ending Apple TV Moonlight stream and switching TV back to bazzite direct HDMI, user saw cursor + pure black screen. Right-click showed Chromium context menu ("forward", "print") — identified as a steamwebhelper CEF overlay stuck as topmost gamescope surface
- **Recovered without reboot**: `systemctl --user restart gamescope-session-plus@steam.service` brought Big Picture back in ~15 seconds, no full reboot needed
- **Saved diagnostic as memory**: created `memory/cef-overlay-black-screen-after-stream.md` in guildwars2 repo with root cause, recovery command, and diagnostic tell
- **Config backups**: bazzite GW2 configs backed up (no drift detected from last session)

### Files changed
- `memory/cef-overlay-black-screen-after-stream.md` (guildwars2, new) — CEF overlay diagnostic
- `memory/MEMORY.md` (guildwars2) — added index entry for new memory

### Commits
- `d1e0a82` (guildwars2) — memory: add CEF overlay black screen diagnostic after stream end

### Open items
- User testing whether the black screen recurs after regular TV watching (no Moonlight) — if it does, issue is TV input switching, not Sunshine-specific
- Login flicker verification at 150% DPI still pending (from earlier session)
- Steam Controller delivery still pending — will eliminate Deck-as-controller and reduce Moonlight usage

---

## 2026-05-14 (continued) — Direct HDMI migration + WebOS TV switching

### What was done
- **Diagnosed no audio through UGREEN DP-to-HDMI dongle**: extensive troubleshooting confirmed PipeWire, ALSA, ELD, mixer all correct on bazzite side. Direct ALSA writes (S16_LE, S32_LE) reached GPU hardware but TV heard nothing. Proved dongle doesn't pass audio by plugging HDMI direct — instant audio
- **LG TV firmware update**: installed firmware update + cold reboot. Fixed HDMI 2.1 negotiation (4K@120Hz + FreeSync now works on direct HDMI — was stuck at 60Hz before firmware update). Also fixed Apple TV CEC volume control
- **Switched from DP dongle to direct HDMI**: GPU native HDMI 2.1 port delivers 4K@120Hz, FreeSync, audio, all in one cable. Dongle no longer needed
- **Discovered AMD GPU native HDMI has no CEC**: `/dev/cec*` doesn't exist on direct HDMI — the dongle was providing CEC via its own chip
- **Replaced CEC with WebOS network API**: LG TV discovered at 172.16.100.44 via SSDP. Paired using `aiowebostv` library. New script `controller-wake-tv.py` uses same silence-detection on hidraw0 but switches TV via WebOS `set_input("HDMI_2")` + WOL instead of CEC commands
- **Verified end-to-end**: controller power-off → 30s silence → controller power-on → TV switches from Apple TV to bazzite via WebOS
- **Removed old CEC script**: disabled and deleted `cec-controller-wake.sh` and service from bazzite
- **Controller joystick sensitivity**: changed from 200% back to 150% in VDF on bazzite and repo backup

### Files changed
- `configs/bazzite/controller-wake-tv.py` (guildwars2, new) — WebOS-based TV wake/switch script
- `configs/bazzite/controller-wake-tv.service` (guildwars2, new) — systemd user service
- `configs/bazzite/cec-controller-wake.sh` (guildwars2) — kept as historical reference (old CEC version)
- `memory/cec-controller-wake-script.md` (guildwars2) — updated to document WebOS approach
- `memory/MEMORY.md` (guildwars2) — updated index entry

### Key facts
- TV IP: 172.16.100.44, MAC: 6c:15:db:8d:a5:a6, WebOS client key: 2c37f5beb73ad7247415b32263c9501f
- Bazzite on HDMI_2, Apple TV on HDMI_1
- Controller sends 0x42 reports at ~267/sec (was 0x7b at ~2/sec with dongle — different hidraw behavior on direct HDMI)
- Controller takes ~35 seconds to fully power down after holding Steam button

### Resolved
- No audio through dongle — bypassed by switching to direct HDMI
- CEC not available on direct HDMI — replaced with WebOS network control
- Apple TV CEC volume — fixed by TV firmware update

---

## 2026-05-14 — CEC wake script v2 (silence-detection) + bazzite config backup

### What was done
- **CEC wake script rewritten to silence-detection**: Original bash `dd`-based script triggered on gamescope restart (sensor data `0x7b` reports flowed continuously). Investigated Steam's exclusive input grab — all button data invisible through hidraw and evdev while gamescope runs. Three HID report types identified: `0x42` (input, always 00,00), `0x7b` (sensor/IMU), `0x43` (connection/battery). Steam button intercepted by firmware, produces no HID reports.
- **Final approach**: Python script detects controller sleep→wake via hidraw silence (30s no data = asleep). Uses `ever_seen_data` gate to prevent false triggers on boot — only fires after data has been observed then goes silent then resumes. Eliminates boot grace period entirely.
- **Verified all three cases**: gamescope restart (no trigger), system reboot (no trigger — confirmed via journal logs, TV flip on reboot is gamescope display pipeline not CEC script), controller wake from sleep (triggers CEC correctly)
- **Backed up all changed bazzite files to guildwars2 repo**:
  - CEC wake script (Python silence-detection version)
  - Gamescope wrapper (`--immediate-flips` removed from all modes, `4k144` added)
  - Gamescope mode helper (synced mode list)
  - Controller VDF v19.9 (sensitivity 200%, all haptics zeroed)
  - Firewall rules (reordered, same ruleset)
- **GW2 keybinds + Nexus configs**: backed up, no drift detected

### Files changed
- `configs/bazzite/cec-controller-wake.sh` (guildwars2) — complete rewrite from bash to Python
- `configs/bazzite/gamescope-wrapper` (guildwars2) — removed `--immediate-flips`, added `4k144`
- `configs/bazzite/gamescope-mode.sh` (guildwars2) — synced mode list
- `configs/bazzite/firewalld-rules.txt` (guildwars2) — refreshed (reordered)
- `configs/steam-controller/moonlight-gw2-og-v19.9.vdf` (guildwars2, new) — controller VDF backup

### Commits
- `e89b2f8` (guildwars2) — bazzite: CEC silence-detection wake script, gamescope --immediate-flips removed, controller v19.9
- `c32aaab` (guildwars2) — bazzite: refresh firewalld rules backup

### Open items
- TV flips to bazzite on reboot (gamescope display pipeline, not CEC script — user OK with this)

### Resolved from prior sessions
- Login flicker at 150% DPI — confirmed fixed
- Dual GW2 installs — no longer needed, plan cancelled
- Steam Controller delivered and in use

---

## 2026-05-13 PM (continued) — Bazzite full performance tuning

### What was done
- **Audited sleep/suspend/hibernate**: all already masked except `suspend-then-hibernate.target` — now masked
- **Full performance tuning applied**:
  - CPU governor: powersave → performance
  - EPP: balance_performance → performance
  - intel_pstate min_perf_pct: 16 → 100 (never downclock)
  - GPU: auto → high
  - TuneD: balanced-bazzite → custom `gaming-performance` profile
- **Result**: all 16 threads now run at **5.0 GHz** (was ~4.1 GHz idle). Package temp 73C water cooled
- **Persistence battle**: TuneD's `tuned-ppd` daemon was overriding profile on boot. Fixed by creating custom profile at `/etc/tuned/profiles/gaming-performance/`, overriding `/etc/tuned/ppd.conf` to map performance → gaming-performance, and setting `ppd_base_profile` to performance. Also created `cpu-performance.service` as belt-and-suspenders fallback
- **Verified across 3 reboots**: settings now persist correctly

### Files changed (guildwars2 repo)
- `configs/bazzite/tuned-gaming-performance.conf` (new) — custom TuneD profile backup
- `configs/bazzite/tuned-ppd.conf` (new) — PPD override backup
- `configs/bazzite/cpu-performance.service` (new) — systemd unit backup
- `memory/bazzite-performance-tuning.md` (new) — full documentation + restore instructions
- `memory/MEMORY.md` — added index entry

### Open items
- User testing whether the black screen recurs after regular TV watching (no Moonlight)
- ~~Login flicker verification at 150% DPI~~ — confirmed fixed 2026-05-14
- ~~Steam Controller delivery~~ — delivered 2026-05-14, in use

## 2026-05-20 — controller-wake-tv first-wake fix + Mystic Clicker capture thumbnails

Fixed the long-standing "TV doesn't switch on first wake after a bazzite reboot" gap, then built capture-thumbnail-on-hover into Mystic Clicker, iterating size + aspect from user testing, plus a Clear-All-Captures action.

### What was done

- **controller-wake-tv first-wake gap fixed.** `ever_seen_data` and `was_silent` both initialised `False`, so on a fresh boot with the controller asleep, the first Steam-button wake was always swallowed (no prior data → silence-detection never armed → switch gate never fired). Init both `True` so the very first hidraw0 data fires the switch; `last_trigger=0.0` keeps `COOLDOWN` satisfied until then. Memory note `cec-controller-wake-script.md` updated to mark the gap fixed.
- **Mystic Clicker — capture-slot thumbnails on hover.** When a capture button fires, grab a back-buffer region around the click point and save as `addons/MysticClicker/thumb-<slot>-<W>x<H>.bmp`. Hovering the slot shows the image in the tooltip with a red marker at the click point — visual reminder of which "Accept N" is which when many slots share names. Iterated through user testing:
  - **3.6.28**: initial 128×128 square thumbnails on hover
  - **3.6.29**: doubled to 256×256
  - **3.6.30**: 384×192 wide rectangle (2:1) — matches GW2 dialog button geometry
  - **3.6.31**: 768×320 (2.4:1) + adaptive display scale — caps at 2× but shrinks to fit ~70%×60% of game window, Deck-safe
  - **3.6.32**: fat filled red dot (radius 22 with white outline ring) instead of thin `+`, plus "Clear all captures" in Settings with confirm modal; new `Thumbs::DeleteAll()` enumerates and unlinks `thumb-*.bmp`
  - **3.6.33**: confirm buttons relabeled "Yes" / "No" (matched widths)
- **Deploy script cleanup.** `deploy-mystic-clicker-dll.sh` and `deploy-nexus-config.sh` still listed the apple-tv / deck-bazzite addons paths from the 2026-05-12 multi-install consolidation — `set -e` made them hard-fail on the first non-existent target. Both scripts now point only at the live two targets (bazzite + deck-native).
- **Vendored ImGui caveat learned.** `ImGui::GetMainViewport()` doesn't exist in this tree's ImGui copy; CI caught it. Switched to `GetIO().DisplaySize`-based centring. Saved as `memory/vendored-imgui-no-main-viewport.md`.

### Deployed (bazzite `Og@172.16.100.212` + Deck `deck@172.16.100.95`)

| What | Version / hash |
| --- | --- |
| `~/bin/controller-wake-tv.py` (bazzite only — host-side wake script) | md5 `9e9f2646` |
| `addons/mystic-clicker.dll` | 3.6.33 — sha `035caed3` |

### Repo changes

- `modules/mystic-clicker/capture-thumbs.{h,cpp}` (NEW), `capture-ui.cpp`, `entry.cpp`
- `mystic-clicker.vcxproj` — added the new `.cpp` to `ClCompile`
- `configs/bazzite/controller-wake-tv.py`
- `scripts/deploy-{mystic-clicker-dll,nexus-config}.sh`
- `CHANGELOG.md`
- `memory/cec-controller-wake-script.md` (gap marked fixed), `memory/vendored-imgui-no-main-viewport.md` (new)

### Operational gotchas (worth knowing next session)

- **`deploy-mystic-clicker-dll.sh` can race a just-finished CI run.** Its "latest successful" lookup (`gh run list --status=success`) doesn't see `in_progress` runs, so if CI just transitioned in-progress → success the script can pick the PREVIOUS success and deploy a stale DLL. Workaround: pass the run ID explicitly (`./scripts/deploy-mystic-clicker-dll.sh <run-id>`). Proper fix (deferred): look up the success run for HEAD's SHA instead of the workflow's most recent overall success.
- **Nexus has no `Textures` release API.** For texture invalidation (re-capture replacing a file on disk), bump a per-slot version counter and use a fresh identifier — old textures stay cached but orphaned. Pattern is documented in `modules/mystic-clicker/capture-thumbs.cpp`.

### Open / next session

- Possible DPI / cursor-hotspot offset on the capture-click marker — user noted the red dot appears slightly down-left of where they think the cursor tip was. Offered to dump client-rect vs back-buffer dims to diagnose; user said "fine as is" for now.
- Apple-TV / deck-bazzite multi-install profile dirs are gone; if those profiles ever return, re-add their entries to the deploy scripts.
- **Conditional `pkill steamwebhelper` in controller-wake-tv** — user raised mid-cleanup: currently the script kills steamwebhelper on every Steam-button press to clear the CEF black-screen overlay, but only needs it sometimes. Wants a detect-then-conditionally-restart logic. To engage next.
