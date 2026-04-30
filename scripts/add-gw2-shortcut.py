#!/usr/bin/env python3
"""Add a non-Steam GW2 shortcut to bazzite's binary shortcuts.vdf.

Usage:
    add-gw2-shortcut.py --name "Guild Wars 2 (Steam Deck)" \
                        --exe /var/home/Og/Games/gw2-deck/Gw2-64.exe \
                        --start-dir /var/home/Og/Games/gw2-deck/ \
                        --proton proton_11

Computes a stable appid via crc32(exe + name) | 0x80000000 (deterministic, no
collision risk with real Steam app IDs which are < 0x80000000), appends to
shortcuts.vdf, updates config.vdf's CompatToolMapping for that appid.

REQUIREMENTS:
- Steam MUST be killed before running (writes shortcuts.vdf and config.vdf on
  shutdown, would clobber edits).
- Backups written next to originals with .bak-<timestamp> suffix.
"""

import argparse
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


# ---------- Binary VDF parser ----------

def parse_vdf_binary(data: bytes):
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
            elif t == 0x07:
                val = struct.unpack("<Q", data[pos[0]:pos[0]+8])[0]
                pos[0] += 8
                result[key] = val
            else:
                raise ValueError(f"Unknown VDF type byte {hex(t)} at offset {pos[0]-1}")
        return result

    return read_section()


def emit_vdf_binary(d: dict) -> bytes:
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
                val = val & 0xFFFFFFFF
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


def compute_stable_appid(exe_quoted: str, name: str) -> int:
    return zlib.crc32((exe_quoted + name).encode("utf-8")) | 0x80000000


def update_config_vdf_compat(content: str, appid: int, tool: str) -> str:
    appid_str = str(appid)
    pattern = re.compile(
        r'("' + re.escape(appid_str) + r'"\s*\{[^}]*"name"\s*"[^"]*")',
        re.DOTALL,
    )
    if pattern.search(content):
        return pattern.sub(
            f'"{appid_str}"\n\t\t\t\t\t{{\n\t\t\t\t\t\t"name"\t\t"{tool}"',
            content,
        )

    new_block = (
        f'\n\t\t\t\t\t"{appid_str}"\n'
        f'\t\t\t\t\t{{\n'
        f'\t\t\t\t\t\t"name"\t\t"{tool}"\n'
        f'\t\t\t\t\t\t"config"\t\t""\n'
        f'\t\t\t\t\t\t"priority"\t\t"250"\n'
        f'\t\t\t\t\t}}'
    )
    ct_pattern = re.compile(r'("CompatToolMapping"\s*\{)((?:[^{}]|\{[^{}]*\})*?)(\n\s*\})')
    m = ct_pattern.search(content)
    if not m:
        raise ValueError("Could not find CompatToolMapping section in config.vdf")
    return content[:m.end(2)] + new_block + content[m.end(2):]


# ---------- Main ----------

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", required=True, help='Display name, e.g. "Guild Wars 2 (Steam Deck)"')
    parser.add_argument("--exe", required=True, help="Absolute exe path (will be wrapped in quotes)")
    parser.add_argument("--start-dir", required=True, help="Working directory")
    parser.add_argument("--proton", default="proton_11", help='Proton tool name in config.vdf (default: proton_11)')
    parser.add_argument("--launch-options", default='WINEDLLOVERRIDES="d3d11=n,b" DXVK_ASYNC=1 %command%',
                        help="Launch options (default matches Steam GW2 minus -provider Portal)")
    args = parser.parse_args()

    exe_quoted = f'"{args.exe}"'

    print(f"Reading {SHORTCUTS_VDF}")
    raw = SHORTCUTS_VDF.read_bytes()
    parsed = parse_vdf_binary(raw)
    sc_section = parsed.get("shortcuts", {})
    existing = list(sc_section.items())
    print(f"  {len(existing)} existing shortcuts:")
    for k, v in existing:
        print(f"    [{k}] {v.get('AppName', '?')} (appid={v.get('appid', 0)})")

    new_appid = compute_stable_appid(exe_quoted, args.name)
    print(f"\nNew shortcut: {args.name}")
    print(f"  exe:    {exe_quoted}")
    print(f"  appid:  {new_appid} (0x{new_appid:08x})")
    print(f"  proton: {args.proton}")

    for k, v in existing:
        if v.get("AppName") == args.name:
            print(f"ERROR: shortcut already exists at index [{k}] — refusing to add duplicate")
            sys.exit(1)

    new_entry = {
        "appid": new_appid,
        "AppName": args.name,
        "Exe": exe_quoted,
        "StartDir": args.start_dir,
        "icon": "",
        "ShortcutPath": "",
        "LaunchOptions": args.launch_options,
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

    next_idx = str(len(existing))
    sc_section[next_idx] = new_entry
    parsed["shortcuts"] = sc_section

    bak = backup(SHORTCUTS_VDF)
    print(f"\nBackup: {bak}")
    new_bytes = emit_vdf_binary(parsed) + b"\x08"  # trailing 0x08 marker for Steam
    SHORTCUTS_VDF.write_bytes(new_bytes)
    print(f"Wrote {len(new_bytes)} bytes to {SHORTCUTS_VDF}")

    verify = parse_vdf_binary(SHORTCUTS_VDF.read_bytes())
    v_sc = verify["shortcuts"]
    print(f"  verify: {len(v_sc)} shortcuts after edit")
    for k, v in v_sc.items():
        marker = " ← NEW" if v.get("AppName") == args.name else ""
        print(f"    [{k}] {v.get('AppName', '?')} (appid={v.get('appid', 0)}){marker}")

    print(f"\nReading {CONFIG_VDF}")
    cfg_text = CONFIG_VDF.read_text()
    bak_cfg = backup(CONFIG_VDF)
    print(f"Backup: {bak_cfg}")
    new_cfg = update_config_vdf_compat(cfg_text, new_appid, args.proton)
    CONFIG_VDF.write_text(new_cfg)
    print(f"Wrote {len(new_cfg)} chars to {CONFIG_VDF}")

    if f'"{new_appid}"' in new_cfg and f'"{args.proton}"' in new_cfg:
        print(f"  verify: CompatToolMapping entry present for appid {new_appid} → {args.proton}")
    else:
        print(f"  WARN: could not verify CompatToolMapping in config.vdf")

    print("\nDONE. Now restart Steam to pick up the new shortcut.")


if __name__ == "__main__":
    main()
