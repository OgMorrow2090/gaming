---
name: Controller VDF deploy must enumerate ALL stale-pattern files on BOTH machines
description: When a controller VDF chord change is deployed, Steam Input may load any of multiple historical or autosave VDF files in the per-app config dir — not just the configset-referenced one. Patching only the canonical `og v18.4.9_0.vdf` per profile leaves users hitting the old behavior via stale historical snapshots or the streaming-flow autosave file.
type: feedback
---

<!-- markdownlint-disable MD041 -->

## The trap (2026-05-04 → 2026-05-05 incident)

Controller v19.5 fixed the TP/Bank double-`I` flicker by removing `key_press I, Open Inventory` from two chord blocks. Initial deploy on 2026-05-04 hit the four files Steam Input "should" load per the configset:

- bazzite: `moonlight/og v18.4.9_0.vdf`, `guild wars 2 (apple tv)/og v18.4.9_0.vdf`, `guild wars 2 (steam deck)/og v18.4.9_0.vdf`
- Deck native: `1284210/controller_neptune.vdf`

Apple TV streaming worked. **Deck-stream-via-Moonlight still flickered TP** the next day. Investigation found:

1. **Each profile dir contained 5–10 historical layout files** (`og v19.0_0.vdf`, `og v19.1_0.vdf`, … `og v19.4_0.vdf`) — Steam Input's Big Picture UI lets the user pick any of them by display name; once picked, the chosen file is what loads, not the configset default.
2. **The "GW2 - SteamOS" Sunshine flow uses an *autosave* VDF on Deck**: `moonlight - gw2 steamos/controller_neptune.vdf`. The configset entry says `"autosave" "1"`, not a CLOUD_template reference. This file was completely missed by the initial deploy and still had the v19.4-era stale `key_press I` lines.
3. The result: Apple TV stream landed on the cleanly-patched `og v18.4.9_0.vdf` while Deck-stream-via-Moonlight landed on the stale autosave file → flicker recurred only on Deck.

## Rule

**When deploying a controller VDF chord behavior change, patch every file matching the change's stale pattern on every machine — not just the configset-referenced file.**

The minimal command: `grep -rl "<stale pattern>" "<Steam Controller Configs root>"` and patch every result. Do this on BOTH bazzite (`Og@172.16.100.212`) AND Deck native (`deck@172.16.100.95`).

**Verify deployed version by content hash, not the embedded `"title"` string.** The title lags the real version when a commit changes bindings without bumping it (e.g. the v24.9 commit `3b82229` still says "Og v24.8" inside). Compare `sha256sum` of the deployed file against the repo's `configs/gw2-keybinds/controller_neptune.vdf`.

## Required deploy targets when changing controller VDF

For bazzite at `~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/`:

| Subdir | Why it matters |
|---|---|
| `moonlight/` | Moonlight non-Steam shortcut layout. Many historical `og v19.*_0.vdf` files; Steam Input's UI may have any of them selected. |
| `guild wars 2 (apple tv)/` | Apple TV stream profile. Same historical fan-out. |
| `guild wars 2 (steam deck)/` | Deck stream profile (bazzite side). Same historical fan-out. |
| `1284210/`, `2959110740/` | Steam-app-id layouts (1284210 = GW2 Steam, 2959110740 = local non-Steam shortcut). Less likely active, still patch for completeness. |

For Deck native at the same relative path under `/home/deck/`:

| Subdir | Why it matters |
|---|---|
| `1284210/controller_neptune.vdf` | Direct GW2-on-Deck launch. Configset-referenced. |
| `moonlight/controller_neptune.vdf` + historical `og v19.*_0.vdf` | Moonlight non-Steam shortcut on Deck — used when streaming from bazzite. |
| **`moonlight - gw2 steamos/controller_neptune.vdf`** | **Autosave file used when launching via "GW2 - SteamOS" Sunshine app. Easy to miss; must include.** |

## Deploy pattern (Python via SSH)

Use the same template for every match. Patch URL field per-location to match the file's actual path so Steam Input doesn't see a URL/location mismatch:

```python
import os, re, glob, subprocess
with open("/tmp/template.vdf") as f: template = f.read()
base = "/path/to/Steam Controller Configs/<userid>/config"

stale_files = subprocess.check_output(
    ["grep","-rl","--include=*.vdf","<stale pattern>", base]
).decode().splitlines()

for f in stale_files:
    rel = os.path.relpath(f, base)
    parent = os.path.dirname(rel)
    fname = os.path.basename(f).replace(".vdf","")
    # Autosave dir uses autosave:// URL with absolute path; everything else is usercloud://<dir>/<basename>
    if parent == "moonlight - gw2 steamos":
        url = f"autosave://{f}"
    else:
        url = f"usercloud://{parent}/{fname}"
    bak = f + ".bak-pre-deploy-<TS>"
    if not os.path.exists(bak):
        with open(f) as r, open(bak,"w") as b: b.write(r.read())
    new = re.sub(r"\"url\"\s+\"[^\"]*\"", f"\"url\"\t\t\"{url}\"", template, count=1)
    with open(f,"w") as w: w.write(new)
```

After deploy, verify zero matches: `grep -rl "<stale pattern>" "$base"` should be empty.

## Final reload step

Steam Input keeps the active layout in memory. Even after on-disk replacement, the running session uses cached bindings. Force reload by:

1. Quit GW2
2. (Aggressive) `pkill steamwebhelper && pkill steam` on the relevant machine — Steam respawns with fresh layout cache
3. Relaunch GW2 stream

**On bazzite (Game Mode):** `pkill steam` races the gamescope session's
auto-respawn, and Steam writes its stale in-memory layout back to the autosave
file on shutdown — clobbering a freshly-deployed VDF. Reliable sequence:
`systemctl --user stop gamescope-session-plus@steam.service` → re-deploy the VDF
→ `sudo systemctl reboot`. Deploy the VDF *after* Steam has stopped so its
shutdown-writeback cannot overwrite it; Steam then reads it fresh on boot.

## Cross-reference

- [streaming-input-host-vs-client.md](streaming-input-host-vs-client.md) — explains why Deck-as-client uses Deck native's Steam Input layout (not bazzite's), so Deck VDF deploys must hit `deck@172.16.100.95`
- [configset-controller-pointer.md](configset-controller-pointer.md) — the configset_controller_neptune.vdf maps app names → templates, but historical files in the same dir may also be loaded
