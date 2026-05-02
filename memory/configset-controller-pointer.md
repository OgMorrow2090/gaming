---
name: Steam Input configset_controller_neptune.vdf is the appname→layout pointer
description: Renaming a non-Steam shortcut creates a new configset entry that defaults to a Workshop fallback, breaking SSH-deployed VDFs that target the OLD appname. Always update configset_controller_neptune.vdf when renaming a non-Steam shortcut.
type: project
---

<!-- markdownlint-disable MD041 -->

Steam Input on Steam Deck reads the active layout for each app/shortcut from a single master file:

```text
~/.local/share/Steam/steamapps/common/Steam Controller Configs/<userid>/config/configset_controller_neptune.vdf
```

Format is one block per app keyed by **AppName lowercased** (or app ID for Steam-store games):

```vdf
"controller_config"
{
    "0"          { "workshop"  "3714613267" }                   // global default
    "1284210"    { "workshop"  "2858596725" }                   // GW2 Steam game
    "moonlight"  { "template"  "CLOUD_moonlight/og v18.4.8_0" } // non-Steam shortcut named "Moonlight"
}
```

Two value modes:

| Mode | What it points to | File location |
|------|-------------------|---------------|
| `workshop` | Workshop UGC item ID | Cloud-served from `cdn.steamusercontent.com/ugc/<id>/...` — immutable |
| `template` | `CLOUD_<dir>/<filename without extension>` | Local file at `Steam Controller Configs/<userid>/config/<dir>/<filename>.vdf` — SSH-editable |

The `<dir>` part of the template path is **the AppName lowercased**, NOT the appid.

## What breaks when you rename a non-Steam shortcut

Renaming a shortcut in `shortcuts.vdf` (e.g. "Moonlight" → "Moonlight - GW2") leaves the original `"moonlight"` configset entry intact but **creates a brand new entry for the new appname** that defaults to:

```vdf
"moonlight - gw2"
{
    "workshop"   "<some default Workshop UGC>"
}
```

Steam picks the global default Workshop item for the new appname. Any SSH-deployed `og v<x.y.z>_0.vdf` files in `Steam Controller Configs/.../moonlight/` are now ORPHANED — the original `"moonlight"` entry still points at them but the renamed shortcut now reads from a Workshop UGC instead.

This is the bug we hit on 2026-04-29: renamed `Moonlight` → `Moonlight - GW2` in Steam shortcuts, the new configset entry pointed at the v18.4.2 (v18.3 base) Workshop item instead of `og v18.4.8_0.vdf`. All today's SSH overwrites of `og v18.4.8_0.vdf` were correct on disk, but Steam loaded v18.4.2 from the Workshop fallback.

**Why:** found via 2026-04-29 session — user reported "all SSH edits worked perfectly until today's split". Investigation traced through autosave files, Personal/Templates tabs, ugcmsgcache files, and finally found `configset_controller_neptune.vdf` was the actual app→layout pointer table. The old "moonlight" entry still had the right `template` pointer; the new "moonlight - gw2" entry had a `workshop` fallback.

## How to apply

When renaming a non-Steam shortcut OR when SSH-deployed layouts stop working after a rename:

1. Read `~/.local/share/Steam/steamapps/common/Steam Controller Configs/<userid>/config/configset_controller_neptune.vdf` — find the entry for the new appname
2. If it has `workshop`, swap to `template` and point at the existing CLOUD path:

   ```vdf
   "moonlight - gw2"
   {
       "template"   "CLOUD_moonlight/og v18.4.8_0"
   }
   ```

3. Stop Steam (`systemctl --user stop steam-launcher.service`) before editing — Steam writes this file on shutdown
4. Edit, then restart Steam

The `CLOUD_<dir>/<filename>` path means the file at `Steam Controller Configs/<userid>/config/<dir>/<filename>.vdf` — `.vdf` extension is implicit.

## Future-proofing the SSH deploy workflow

For future Mystic Clicker / Og Morrow controller releases, the deploy steps must include a configset check:

1. SSH-copy `og v<X.Y.Z>_0.vdf` to `Steam Controller Configs/<userid>/config/moonlight/`
2. Read `configset_controller_neptune.vdf` — confirm both `"moonlight"` and `"moonlight - gw2"` entries point at `template` mode with the new filename. If a new shortcut name is introduced, add an entry for it.
3. Stop Steam, edit configset if needed, restart.

If the configset only has `"moonlight"` and no `"moonlight - gw2"` (or vice versa), the SSH-deployed layout will only apply to whichever shortcut name has the matching configset entry.

## Autosave files need a URL patch on deploy

A third value mode exists: `autosave 1` for shortcuts where Steam Input UI saves changes. The active file lives at `<dir>/controller_neptune.vdf` and must contain a self-referential `autosave://` URL:

```vdf
"url"  "autosave:///home/deck/.local/share/Steam/steamapps/common/Steam Controller Configs/<userid>/config/<dir>/controller_neptune.vdf"
```

**Repo VDF templates use `usercloud://...` URLs.** When deploying a repo VDF on top of an autosave file (e.g. `moonlight - gw2 steamos/controller_neptune.vdf` on the Deck), patch the URL line to the autosave path before scp. The sed pattern must contain **literal tabs** (VDF's KeyValues field separator) — the `<TAB><TAB>` markers below stand for two real tab characters:

```bash
cp configs/steam-controller/moonlight-gw2-og-vX.Y.vdf /tmp/vX.Y-autosave.vdf
sed -i '' 's|"url"<TAB><TAB>"usercloud://moonlight/og vX.Y_0"|"url"<TAB><TAB>"autosave:///home/deck/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight - gw2 steamos/controller_neptune.vdf"|' /tmp/vX.Y-autosave.vdf
scp /tmp/vX.Y-autosave.vdf "deck@172.16.100.95:.../moonlight - gw2 steamos/controller_neptune.vdf"
```

For non-autosave paths (`1284210/controller_neptune.vdf`, `moonlight/og vX.Y_0.vdf`) deploy the unmodified repo template — the `usercloud://` URL is fine.

Steam Input UI may rewrite the description to `#SettingsController_AutosaveDescription` on its next save; that's harmless. Steam keeps the layout in memory and writes-through on shutdown, so reload Steam (or open Steam Input UI and save) before next session to lock in the deploy.

## Lookup chain summary

When Steam Input loads a layout for shortcut "X":

1. Look up `"X"` (lowercased) in `configset_controller_neptune.vdf`
2. If `template`, load file at `Steam Controller Configs/.../config/<dir>/<filename>.vdf` — SSH-editable
3. If `workshop`, fetch from CDN URL stored in `userdata/<userid>/ugcmsgcache/*.cachedmsg` manifest (points to `cdn.steamusercontent.com`) — NOT SSH-editable
4. If no entry, fall back to global default (`"0"` block)
