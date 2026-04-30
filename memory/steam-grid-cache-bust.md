---
name: Steam grid art cache bust — when changes don't show after restart
description: When you change grid art for a non-Steam shortcut and Steam still shows the old version after killing steamwebhelper, the deeper bust is to delete userdata/<id>/config/librarycache/<appid>.json AND clear htmlcache/{Cache,Code Cache,GPUCache}. Killing the helper process alone isn't enough — Steam tracks "customimage version" stamps in those JSON files.
type: feedback
---

<!-- markdownlint-disable MD041 -->

## When to apply

You changed `~/.local/share/Steam/userdata/<userid>/config/grid/<appid>(p|_hero|_logo)?.png` for a non-Steam shortcut, killed steamwebhelper, Steam respawned, **but the library tile / detail page still shows the old art**. The file mtime updated, the file content is correct, Steam just hasn't re-read it.

## Root cause

Steam caches `customimage` version stamps per-appid in:

`~/.local/share/Steam/userdata/<userid>/config/librarycache/<appid>.json`

The file content looks like:

```json
[["achievements",{"version":2,"data":{...}}],["customimage",{"version":1}]]
```

The `customimage.version` is bumped only when Steam itself sees the user pick "Set Custom Artwork" via the UI. Editing files on disk doesn't bump the version, and Steam's image renderer trusts the cached version stamp over the file content.

Additionally, Steam's webhelper (CEF) caches decoded image bytes in:

- `~/.local/share/Steam/config/htmlcache/Cache/`
- `~/.local/share/Steam/config/htmlcache/Code Cache/`
- `~/.local/share/Steam/config/htmlcache/GPUCache/`

These survive a kill-9 of `steamwebhelper` (they're on disk, not in memory).

## How to apply

Full bust sequence:

```bash
# 1. Kill all Steam helpers (don't let them save state on graceful shutdown)
pkill -9 -f 'steamwebhelper|/Steam/ubuntu12|steam-runtime|pressure-vessel|srt-logger'
sleep 2

# 2. Delete the customimage version-stamp JSONs for affected appids
USERID=64793831  # adjust per host
for appid in <list-of-appids>; do
    rm -f ~/.local/share/Steam/userdata/$USERID/config/librarycache/$appid.json
done

# 3. Clear webhelper image cache
rm -rf ~/.local/share/Steam/config/htmlcache/Cache \
       ~/.local/share/Steam/config/htmlcache/Code\ Cache \
       ~/.local/share/Steam/config/htmlcache/GPUCache

# 4. Wait for Steam to respawn (gamescope-session-plus auto-restarts it on bazzite)
# Steam will rebuild librarycache JSONs and decode fresh image bytes from grid/
```

Steam's UI shows the new art on next library scan (~10 seconds after respawn).

## What does NOT work

- Killing `steamwebhelper` alone — leaves the cached version stamps in place
- `touch`-ing the grid art file — doesn't change content hash, doesn't bump version
- Right-click → Manage → Set Custom Artwork → re-pick — works occasionally but inconsistent (Steam sometimes uses the cached version for the picker preview too)
- `rm -rf ~/.local/share/Steam/appcache/librarycache/` — that's for Steam-published games; non-Steam shortcuts don't have entries there
- Logging out + in — Steam preserves htmlcache across login sessions

## Why this is painful

The `customimage` version-stamp design assumes art only changes via the Steam UI's "Set Custom Artwork" flow. Tools that edit grid/ on disk (this repo's `brand-shortcut-art.sh`, BoilR, SteamGridDB Manager, Decky, etc.) all run into this. The bust is the universal workaround.

## How to apply (real-world example, 2026-04-30)

`brand-shortcut-art.sh` rebranded the Apple TV + Steam Deck non-Steam shortcuts' grid art on bazzite. Killing steamwebhelper showed no change. Diagnosis: `cat ~/.local/share/Steam/userdata/64793831/config/librarycache/2879321470.json` showed `"customimage":{"version":1}`. Deleting both JSONs + `rm -rf htmlcache/{Cache,Code Cache,GPUCache}` + Steam respawn = new art rendered correctly on next library load.
