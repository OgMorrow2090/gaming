#!/usr/bin/env python3
"""gw2-favorites-cleanup.py

Reconcile two GW2 Nexus addon favorite lists against the official GW2 account
API, dropping entries that are already "done":

  NexusGameWiki/library.json  ->  favorites where the wiki page's default
                                  skin is unlocked in /v2/account/skins
  CraftyLegend/favourites.json ->  item IDs whose default skin is unlocked OR
                                  whose item is in /v2/account/legendaryarmory

Refuses to mutate either file while GW2 is running — Nexus rewrites
library.json from in-memory state on addon shutdown, so any edit during a
session is clobbered. Configured to be safe to run on any cadence (skips
quietly when GW2 is up).

Trigger:
  systemctl --user start gw2-favorites-cleanup.service
  (and on a recurring timer — see gw2-favorites-cleanup.timer)

Logs to syslog under tag 'gw2-favorites-cleanup' — view with
  journalctl -t gw2-favorites-cleanup --since '1 hour ago'

Author: OgMorrow2090
"""
import json
import os
import re
import subprocess
import sys
import syslog
import time
import urllib.error
import urllib.parse
import urllib.request

GW2_ADDONS_DIR = os.path.expanduser(
    "~/.local/share/Steam/steamapps/common/Guild Wars 2/addons"
)
WIKI_LIBRARY = os.path.join(GW2_ADDONS_DIR, "NexusGameWiki", "library.json")
CRAFTY_FAVS  = os.path.join(GW2_ADDONS_DIR, "CraftyLegend",  "favourites.json")

CONFIG_ENV = os.path.expanduser("~/.config/gw2-claude/config.env")

WIKI_API = "https://wiki.guildwars2.com/api.php"
GW2_API  = "https://api.guildwars2.com"

# Match `id = NNNNN` inside any of the GW2 wiki's item-style infobox templates.
# Covers Weapon, Armor, Item, Equipment, Trinket, Backpiece, Skin — anything
# that exposes a numeric item id. DOTALL because templates span many lines.
INFOBOX_ID_RE = re.compile(
    r"\{\{\s*(?:Weapon|Armor|Item|Equipment|Trinket|Backpiece|Skin)\s+infobox\b"
    r".*?\|\s*id\s*=\s*(\d+)",
    re.DOTALL | re.IGNORECASE,
)


def log(level, msg):
    syslog.syslog(level, msg)


def is_gw2_running():
    """True if Guild Wars 2 is alive. Cheap pgrep; ~5 ms."""
    r = subprocess.run(["pgrep", "-x", "Gw2-64.exe"], capture_output=True)
    return r.returncode == 0


def load_api_key():
    """Read GW2_API_KEY=... out of the existing gw2-claude config.env."""
    if not os.path.exists(CONFIG_ENV):
        return None
    with open(CONFIG_ENV) as f:
        for line in f:
            line = line.strip()
            if line.startswith("GW2_API_KEY="):
                return line.split("=", 1)[1].strip("'\"")
    return None


def http_get_json(url, timeout=15):
    req = urllib.request.Request(
        url, headers={"User-Agent": "gw2-favorites-cleanup/1.0"}
    )
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read())


def gw2_account_skins(key):
    return set(http_get_json(
        f"{GW2_API}/v2/account/skins?access_token={key}"))


def gw2_legendary_armory(key):
    data = http_get_json(
        f"{GW2_API}/v2/account/legendaryarmory?access_token={key}")
    return {item["id"] for item in data}


def gw2_item(item_id):
    """Return item JSON, or None if the API errors / doesn't know it."""
    try:
        return http_get_json(f"{GW2_API}/v2/items/{int(item_id)}")
    except (urllib.error.HTTPError, urllib.error.URLError, ValueError):
        return None


def wiki_page_item_id(page_id):
    """Parse the wiki page's wikitext for the first `id = NNNNN` inside any
    item-style infobox. Returns int or None."""
    try:
        d = http_get_json(
            f"{WIKI_API}?action=parse&pageid={int(page_id)}"
            "&format=json&prop=wikitext"
        )
    except (urllib.error.HTTPError, urllib.error.URLError, ValueError):
        return None
    text = d.get("parse", {}).get("wikitext", {}).get("*", "")
    m = INFOBOX_ID_RE.search(text)
    if not m:
        return None
    try:
        return int(m.group(1))
    except ValueError:
        return None


def _backup(path):
    """Move the existing file aside with a timestamp suffix."""
    bak = path + f".bak-{int(time.time())}"
    os.replace(path, bak)
    return bak


def _write_json(path, data):
    with open(path, "w") as f:
        json.dump(data, f, indent=2)


def cleanup_wiki(unlocked_skins):
    """Drop favorites whose wiki page's item.default_skin is in the wardrobe."""
    if not os.path.exists(WIKI_LIBRARY):
        log(syslog.LOG_INFO, "wiki: library.json not found, skipping")
        return 0
    with open(WIKI_LIBRARY) as f:
        data = json.load(f)
    favs = data.get("favorites", [])
    keep, removed = [], []
    for fav in favs:
        page_id = fav.get("pageId")
        title   = fav.get("title", "?")
        item_id = wiki_page_item_id(page_id) if page_id else None
        if item_id is None:
            # Couldn't resolve the wiki page to an item — leave it alone.
            keep.append(fav)
            continue
        item = gw2_item(item_id)
        skin_id = (item or {}).get("default_skin")
        if skin_id and skin_id in unlocked_skins:
            removed.append((title, page_id, item_id, skin_id))
            continue
        keep.append(fav)

    if not removed:
        log(syslog.LOG_INFO,
            f"wiki: nothing to clean (kept {len(keep)} favorite(s))")
        return 0
    bak = _backup(WIKI_LIBRARY)
    data["favorites"] = keep
    _write_json(WIKI_LIBRARY, data)
    log(syslog.LOG_INFO,
        f"wiki: removed {len(removed)} unlocked-skin favorite(s); "
        f"kept {len(keep)}; backup={os.path.basename(bak)}")
    for title, pid, iid, sid in removed:
        log(syslog.LOG_INFO,
            f"  - {title} (pageId={pid}, itemId={iid}, skinId={sid})")
    return len(removed)


def cleanup_craftylegend(unlocked_skins, legendary_armory_ids):
    """Drop item IDs whose skin is unlocked OR which appear in the
    legendary armory."""
    if not os.path.exists(CRAFTY_FAVS):
        log(syslog.LOG_INFO, "CraftyLegend: favourites.json not found, skipping")
        return 0
    with open(CRAFTY_FAVS) as f:
        data = json.load(f)
    favs = data.get("favourites", [])
    keep, removed = [], []
    for item_id in favs:
        item = gw2_item(item_id)
        if not item:
            keep.append(item_id)
            continue
        skin_id  = item.get("default_skin")
        name     = item.get("name", "?")
        complete = False
        reason   = ""
        if skin_id and skin_id in unlocked_skins:
            complete, reason = True, f"skinId={skin_id} in wardrobe"
        elif item_id in legendary_armory_ids:
            complete, reason = True, "in legendary armory"
        if complete:
            removed.append((name, item_id, reason))
        else:
            keep.append(item_id)

    if not removed:
        log(syslog.LOG_INFO,
            f"CraftyLegend: nothing to clean (kept {len(keep)} favourite(s))")
        return 0
    bak = _backup(CRAFTY_FAVS)
    data["favourites"] = keep
    _write_json(CRAFTY_FAVS, data)
    log(syslog.LOG_INFO,
        f"CraftyLegend: removed {len(removed)} completed favourite(s); "
        f"kept {len(keep)}; backup={os.path.basename(bak)}")
    for name, iid, reason in removed:
        log(syslog.LOG_INFO, f"  - {name} (itemId={iid}, {reason})")
    return len(removed)


def main():
    syslog.openlog("gw2-favorites-cleanup")
    if is_gw2_running():
        log(syslog.LOG_INFO,
            "GW2 is running — skipping (will retry on next tick)")
        return 0
    key = load_api_key()
    if not key:
        log(syslog.LOG_ERR,
            "no GW2_API_KEY in ~/.config/gw2-claude/config.env")
        return 1
    try:
        unlocked_skins   = gw2_account_skins(key)
        legendary_armory = gw2_legendary_armory(key)
    except Exception as e:
        log(syslog.LOG_ERR,
            f"GW2 API fetch failed: {type(e).__name__}: {e}")
        return 2
    log(syslog.LOG_INFO,
        f"GW2 API: {len(unlocked_skins)} skins unlocked, "
        f"{len(legendary_armory)} legendary armory item(s)")
    n_wiki   = cleanup_wiki(unlocked_skins)
    n_crafty = cleanup_craftylegend(unlocked_skins, legendary_armory)
    log(syslog.LOG_INFO,
        f"done: wiki={n_wiki} removed, craftylegend={n_crafty} removed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
