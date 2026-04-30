#!/usr/bin/env python3
"""Add 'Guild Wars 2 (Apple TV)' as a non-Steam shortcut on bazzite.

- Parses the existing binary shortcuts.vdf
- Adds a new entry as the next-numbered section
- Computes a stable appid via crc32(exe+name) | 0x80000000 (deterministic, no collision risk
  with real Steam app IDs which are < 0x80000000)
- Updates config.vdf's CompatToolMapping to map the new appid to proton_11

REQUIREMENTS:
- Steam MUST be killed before running this — Steam writes shortcuts.vdf and config.vdf on
  shutdown and would clobber our edits.
- Backups written next to originals with .bak-<timestamp> suffix.
"""

import os
import re
import struct
import sys
import time
import zlib
from pathlib import Path

USER_ID = "64793831"
HOME = Path("/home/Og")
SHORTCUTS_VDF = HOME / f".local/share/Steam/userdata/{USER_ID}/config/shortcuts.vdf"
CONFIG_VDF = HOME / ".local/share/Steam/config/config.vdf"

NEW_NAME = "Guild Wars 2 (Apple TV)"
NEW_EXE = '"/var/home/Og/Games/gw2-appletv/Gw2-64.exe"'  # quoted as Steam stores
NEW_START_DIR = "/var/home/Og/Games/gw2-appletv/"
NEW_LAUNCH_OPTIONS = "%command%"  # plain — let Proton/gamescope handle env
NEW_PROTON_TOOL = "proton_11"


# ---------- Binary VDF parser ----------

def parse_vdf_binary(data: bytes):
    """Parse a binary VDF file into nested dicts. Sections are dicts; values are strings or ints."""
    pos = [0]

    def read_string():
        end = data.index(b"\x00", pos[0])
        s = data[pos[0]:end].decode("utf-8", errors="replace")
        pos[0] = end + 1
        return s

    def read_section():
        result = {}
        while pos[0] < len(data):
            t = data[pos[0]]
            if t == 0x08:
                pos[0] += 1
                return result
            pos[0] += 1
            key = read_string()
            if t == 0x00:
                result[key] = read_section()
            elif t == 0x01:
                result[key] = read_string()
            elif t == 0x02:
                val = struct.unpack("<I", data[pos[0]:pos[0]+4])[0]
                pos[0] += 4
                result[key] = val
            elif t == 0x07:  # uint64
                val = struct.unpack("<Q", data[pos[0]:pos[0]+8])[0]
                pos[0] += 8
                result[key] = val
            else:
                raise ValueError(f"Unknown VDF type byte {hex(t)} at offset {pos[0]-1}")
        return result

    return read_section()


def emit_vdf_binary(d: dict) -> bytes:
    """Emit a nested dict back as binary VDF."""
    out = bytearray()
    for key, val in d.items():
        if isinstance(val, dict):
            out.append(0x00)
            out += key.encode("utf-8") + b"\x00"
            out += emit_vdf_binary(val)
            out.append(0x08)
        elif isinstance(val, str):
            out.append(0x01)
            out += key.encode("utf-8") + b"\x00"
            out += val.encode("utf-8") + b"\x00"
        elif isinstance(val, int):
            if val < 0:
                val = val & 0xFFFFFFFF  # signed → unsigned
            if val >= (1 << 32):
                out.append(0x07)
                out += key.encode("utf-8") + b"\x00"
                out += struct.pack("<Q", val)
            else:
                out.append(0x02)
                out += key.encode("utf-8") + b"\x00"
                out += struct.pack("<I", val)
        else:
            raise ValueError(f"Cannot emit value of type {type(val)} for key {key!r}")
    return bytes(out)


# ---------- Operations ----------

def backup(path: Path) -> Path:
    ts = time.strftime("%Y%m%d-%H%M%S")
    dst = path.with_suffix(path.suffix + f".bak-{ts}")
    dst.write_bytes(path.read_bytes())
    return dst


def compute_stable_appid(exe: str, name: str) -> int:
    """Deterministic 32-bit unsigned, high bit set."""
    return zlib.crc32((exe + name).encode("utf-8")) | 0x80000000


def update_config_vdf_compat(content: str, appid: int, tool: str) -> str:
    """Add or update a CompatToolMapping entry. Returns modified text."""
    appid_str = str(appid)

    # Look for existing entry for this appid
    pattern = re.compile(
        r'("' + re.escape(appid_str) + r'"\s*\{[^}]*"name"\s*"[^"]*")',
        re.DOTALL,
    )
    if pattern.search(content):
        # Update name
        return pattern.sub(
            f'"{appid_str}"\n\t\t\t\t\t{{\n\t\t\t\t\t\t"name"\t\t"{tool}"',
            content,
        )

    # Insert new entry inside CompatToolMapping section
    new_block = (
        f'\n\t\t\t\t\t"{appid_str}"\n'
        f'\t\t\t\t\t{{\n'
        f'\t\t\t\t\t\t"name"\t\t"{tool}"\n'
        f'\t\t\t\t\t\t"config"\t\t""\n'
        f'\t\t\t\t\t\t"priority"\t\t"250"\n'
        f'\t\t\t\t\t}}'
    )

    # Find CompatToolMapping section and insert before its closing brace
    ct_pattern = re.compile(r'("CompatToolMapping"\s*\{)((?:[^{}]|\{[^{}]*\})*?)(\n\s*\})')
    m = ct_pattern.search(content)
    if not m:
        raise ValueError("Could not find CompatToolMapping section in config.vdf")
    return content[:m.end(2)] + new_block + content[m.end(2):]


# ---------- Main ----------

def main():
    print(f"Reading {SHORTCUTS_VDF}")
    raw = SHORTCUTS_VDF.read_bytes()
    print(f"  {len(raw)} bytes")

    parsed = parse_vdf_binary(raw)
    sc_section = parsed.get("shortcuts", {})
    existing = list(sc_section.items())
    print(f"  {len(existing)} existing shortcuts:")
    for k, v in existing:
        print(f"    [{k}] {v.get('AppName', '?')} (appid={v.get('appid', 0)})")

    # Compute new appid
    new_appid = compute_stable_appid(NEW_EXE, NEW_NAME)
    print(f"\nNew shortcut: {NEW_NAME}")
    print(f"  exe:    {NEW_EXE}")
    print(f"  appid:  {new_appid} (0x{new_appid:08x})")

    # Refuse if already present
    for k, v in existing:
        if v.get("AppName") == NEW_NAME:
            print(f"ERROR: shortcut already exists at index [{k}] — refusing to add duplicate")
            sys.exit(1)

    # Build new entry
    new_entry = {
        "appid": new_appid,
        "AppName": NEW_NAME,
        "Exe": NEW_EXE,
        "StartDir": NEW_START_DIR,
        "icon": "",
        "ShortcutPath": "",
        "LaunchOptions": NEW_LAUNCH_OPTIONS,
        "IsHidden": 0,
        "AllowDesktopConfig": 1,
        "AllowOverlay": 1,
        "OpenVR": 0,
        "Devkit": 0,
        "DevkitGameID": "",
        "DevkitOverrideAppID": 0,
        "LastPlayTime": 0,
        "FlatpakAppID": "",
        "tags": {},
    }

    # Append as next-numbered index
    next_idx = str(len(existing))
    sc_section[next_idx] = new_entry
    parsed["shortcuts"] = sc_section

    # Backup + write
    bak = backup(SHORTCUTS_VDF)
    print(f"\nBackup: {bak}")
    # Steam expects a trailing 0x08 marker at file end (close-of-root); our emitter
    # doesn't add it because the top-level call to read_section returns when consuming
    # the trailing 0x08 from below. Append it manually.
    new_bytes = emit_vdf_binary(parsed) + b"\x08"
    SHORTCUTS_VDF.write_bytes(new_bytes)
    print(f"Wrote {len(new_bytes)} bytes to {SHORTCUTS_VDF}")

    # Verify by re-parsing
    verify = parse_vdf_binary(SHORTCUTS_VDF.read_bytes())
    v_sc = verify["shortcuts"]
    print(f"  verify: {len(v_sc)} shortcuts after edit")
    for k, v in v_sc.items():
        marker = " ← NEW" if v.get("AppName") == NEW_NAME else ""
        print(f"    [{k}] {v.get('AppName', '?')} (appid={v.get('appid', 0)}){marker}")

    # Update config.vdf for CompatToolMapping
    print(f"\nReading {CONFIG_VDF}")
    cfg_text = CONFIG_VDF.read_text()
    bak_cfg = backup(CONFIG_VDF)
    print(f"Backup: {bak_cfg}")
    new_cfg = update_config_vdf_compat(cfg_text, new_appid, NEW_PROTON_TOOL)
    CONFIG_VDF.write_text(new_cfg)
    print(f"Wrote {len(new_cfg)} chars to {CONFIG_VDF}")

    # Verify CompatToolMapping
    if f'"{new_appid}"' in new_cfg and f'"{NEW_PROTON_TOOL}"' in new_cfg:
        print(f"  verify: CompatToolMapping entry present for appid {new_appid} → {NEW_PROTON_TOOL}")
    else:
        print(f"  WARN: could not verify CompatToolMapping in config.vdf")

    print("\nDONE. Now restart Steam to pick up the new shortcut.")


if __name__ == "__main__":
    main()
