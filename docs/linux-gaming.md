# Linux Gaming Desktop - Bazzite

## Host Details

| Field | Value |
| ----- | ----- |
| **Hostname** | shaun-bazzite |
| **OS** | Bazzite (Fedora-based Linux gaming distro) |
| **IP Address** | 172.16.100.212 |
| **VLAN** | 100 (IoT) |
| **SSH User** | og |
| **SSH Key** | 1Password (same key as Pi hosts) |

## SSH Access

```bash
ssh shaun-bazzite
# or
ssh og@172.16.100.212
```

Uses the 1Password SSH agent — same key used for all Pi hosts. Biometric approval required on connection.

**Requires**: SSH config entry in `dev-toolkit/configs/ssh_config` (already added).

## Bazzite OS

Bazzite is a Fedora-based immutable Linux distribution built for gaming. Similar to SteamOS but runs on standard desktop hardware. Ships with Steam, Proton, and gaming-optimized drivers out of the box.

## Guild Wars 2 on Linux

GW2 runs on Bazzite via Steam + Proton. The game itself is Windows-only but works well under Proton/Wine.

### Addon Compatibility

Nexus addon loader and DLL-based addons (Mystic Clicker, Mystic Trading) need testing under Proton. Notes:

- Nexus DLL injection may require Proton configuration
- Input simulation (WndProc) behavior may differ under Wine/Proton
- Per-resolution config files should work as normal
- ImGui rendering (Mystic Trading) compatibility TBD

### Testing Checklist

- [ ] GW2 launches and runs via Steam/Proton
- [ ] Nexus addon loader installs and loads
- [ ] Mystic Clicker DLL loads in Nexus
- [ ] Keybind capture works (Ctrl+Shift combos)
- [ ] Input simulation clicks register correctly
- [ ] Per-resolution config saves/loads
- [ ] Mystic Trading DLL loads in Nexus
- [ ] Mystic Trading UI renders correctly (ImGui)
- [ ] GW2 API calls work from within addon
