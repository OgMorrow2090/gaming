# Bazzite Display Backup — 2026-04-13

Snapshot taken before migrating away from the Evanlak dummy plug / broken EDID-override setup toward a pure software virtual monitor.

## What was captured

| File | What it is |
|---|---|
| `edid-firmware.bin` | `/var/usrlocal/lib/firmware/edid.bin` — the "custom" override. NOT loaded by kernel (firmware path unavailable at early boot). |
| `edid-home.bin` | `~/edid.bin` — older Evanlak EDID backup. Differs from firmware by one Standard Timing (`1152x864@75` vs `1280x800@90`). |
| `edid-gamescope.bin` | `~/.config/gamescope/edid.bin` — same hash as firmware. |
| `edid-firmware-decoded.txt` | `edid-decode` output of the firmware EDID. |
| `edid-home-decoded.txt` | `edid-decode` output of the home EDID. |
| `gamescope-refresh.conf` | `~/.config/environment.d/gamescope-refresh.conf` — added this session; sets `CUSTOM_REFRESH_RATES=60,90,120,144`. |
| `gamescope-cmdline.txt` | live `/usr/bin/gamescope` process cmdline at snapshot time. |
| `sunshine.conf` | `~/.config/sunshine/sunshine.conf` — empty (defaults). |
| `sunshine-apps.json` | `~/.config/sunshine/apps.json`. |
| `kernel-args.txt` | `rpm-ostree kargs` output. |
| `proc-cmdline.txt` | `/proc/cmdline`. |
| `xrandr-current.txt` | `DISPLAY=:1 xrandr` output showing modes gamescope exposes. |
| `dmesg-display.txt` | EDID/HDMI/NVIDIA kernel log lines. |

## What's actually happening right now

1. Kernel cmdline has `drm.edid_firmware=HDMI-A-2:edid.bin` with `firmware_class.path=/usr/local/lib/firmware`.
2. At DRM init (~9s into boot), `/usr/local` is not yet mounted on this ostree system, so the EDID override file cannot be read. Kernel logs `Direct firmware load for edid.bin failed with error -2`.
3. The kernel falls back to the **real** EDID from the physical Evanlak 8K V2 plug on `HDMI-A-2`.
4. The Evanlak advertises: `3840x2160@60` (DTD 1), `1920x1080@144` (DTD 2), `2560x1440@60` (DTD 3), plus `1280x800@90` via GTF Standard Timing, plus CTA VICs including **VIC 118 = 3840x2160@120**.
5. `xrandr` only shows 60Hz for 4K — the kernel isn't translating CTA VICs into modes for the xrandr mode list (known quirk with firmware-override EDIDs, but same behavior is observed with the fallback EDID here).
6. `HDMI FRL link training failed` in dmesg — the Evanlak physically can't negotiate 4K@120 over its HDMI 2.1 FRL link (its hard limit).

## Goal

Get 4K@120Hz available to Sunshine/Moonlight for Apple TV streaming, while keeping Steam Deck native `1280x800` for Deck streaming.

## Rollback

To return to current state after any change:

```bash
# Restore kernel args
sudo rpm-ostree kargs --replace "drm.edid_firmware=HDMI-A-2:edid.bin" --replace "firmware_class.path=/usr/local/lib/firmware"

# Restore EDID files
scp edid-firmware.bin shaun-bazzite:/tmp/edid.bin
ssh shaun-bazzite "sudo cp /tmp/edid.bin /var/usrlocal/lib/firmware/edid.bin && cp /tmp/edid.bin ~/.config/gamescope/edid.bin"

# Restore gamescope env
scp gamescope-refresh.conf shaun-bazzite:~/.config/environment.d/gamescope-refresh.conf
ssh shaun-bazzite "systemctl --user restart gamescope-session-plus@steam.service"

# Reboot to re-apply kernel args
ssh shaun-bazzite "systemctl reboot"
```
