# Migration: NVIDIA RTX 3080 Ti → AMD RX 9070 XT (PULSE)

Planned execution: **2026-05-12** (Tuesday) — GPU arrives via Scan delivery.

## Final scope (locked 2026-05-11 evening)

**GPU swap only. CPU/mobo/RAM/case/PSU/NVMe stays.** All upgrade discussion
beyond the GPU (5700X3D AM4 bridge, 9800X3D AM5 platform refresh, Terra SFF
case, etc.) is deferred. Reasoning:

1. The session's actual pain points (Steam Link black screen, Sunshine HDR
   capture, vkms incompatibility, multi-sink HDMI FRL flicker) are ALL
   GPU-driver-side and resolve with the AMD swap alone.
2. GW2 framerate in WvW/meta is a separate problem the user has not flagged
   as actively bothering them. CPU upgrade can be revisited any time in 2026-2028
   if needed.
3. AM4 5700X3D bridge would cost ~£400 for ~50-55% GW2 perf gain but adds
   end-of-life-socket sunk cost. AM5 9800X3D refresh in 2028 has potential
   £100-150 savings from DDR5 price drops by then.
4. Total spend today: £630 (GPU already ordered from Scan).

If GW2 stutters in WvW/zergs become genuinely annoying post-GPU-swap, the AM4
5700X3D bridge spec is documented below as a "future option" — not part of
this migration.

**Keeping (no spend):** NZXT H400 case, 1200W ATX PSU, 64GB DDR4-3200 RAM,
NVMe SSDs, Intel X550 10GbE optical NIC, current i9-9900K + Z390 mobo.

**Swapping:** NVIDIA RTX 3080 Ti → Sapphire PULSE RX 9070 XT 16GB.
Plus bazzite OS rebase from `bazzite-deck-nvidia:stable` to `bazzite-deck:stable`.

## Context

Today's (2026-05-11) Bazzite-in-lounge session uncovered hard architectural
constraints on the NVIDIA proprietary driver that block the user's goal of
having 1280×800@90 visible in the SteamOS resolution dropdown for Deck Moonlight
streaming while keeping the LG G5 at native 4K@165 for couch play:

- **vkms virtual output** — incompatible (gamescope binds card0 only; Sunshine
  can't dmabuf-import vkms framebuffers on NVIDIA proprietary).
- **`video=DP-1:e drm.edid_firmware=DP-1:edid.bin` virtual DP connector** —
  mode list works, but Sunshine KMS capture fails on virtual DP framebuffers
  (NVIDIA closed-source limitation; documented in commit 56e28e2 from Apr 29).
- **`drm.edid_firmware` injection on the live HDMI connector** — ostree
  early-boot mount-order issue: `firmware_class.path=/usr/local/lib/firmware`
  isn't mounted at DRM init, so kernel logs "Direct firmware load failed -2"
  and falls back to the real EDID.
- **Physical HDMI dummy (Evanlak) as a 2nd sink** — works EDID-wise but causes
  NVIDIA `HDMI FRL link training failed` on the LG side (multi-sink HDMI 2.1
  FRL contention). Confirmed in dmesg today.
- **Per-mode `--prefer-output` switching between HDMI-A-1 and HDMI-A-2** —
  triggers NVIDIA driver state corruption + purple/red colour-cycle flicker
  that survives soft reboots.

Hardware decision: order Sapphire PULSE RX 9070 XT 16GB (Scan code LN154765,
mfr 11348-03-20G, £629.99 inc VAT). RDNA 4, full HDMI 2.1 FRL 48 Gbps,
amdgpu open-source kernel module on Bazzite.

PSU verified: 1200W with 2× free 8-pin PCIe connectors. Case clearance fine
(3080 Ti already 3-slot, 320mm — same envelope as 9070 XT PULSE).

## Why AMD fixes everything

| NVIDIA issue today | AMD equivalent | Status |
| --- | --- | --- |
| vkms + Sunshine dmabuf fails | vkms works natively on amdgpu | ✅ |
| Virtual DP capture fails | Virtual connector capture works | ✅ |
| `drm.edid_firmware` ostree mount order | Same ostree issue, but workaround paths work + sysfs runtime EDID injection available | ✅ |
| Multi-sink HDMI FRL conflict | amdgpu handles multi-sink HDMI 2.1 cleanly | ✅ |
| Per-mode `--prefer-output` causes flicker | Clean output rebind on amdgpu | ✅ |
| HDR + Sunshine NVENC 0x502 capture bug | VAAPI/AMF encoder has no equivalent bug | ✅ |
| HDR adds visible HDMI flicker on LG renegotiation | amdgpu HDR negotiation is more stable | ✅ |
| AV1 encode hardware patchy on NVENC Linux | RDNA 4 has improved AV1 encoder | ✅ |

## Pre-swap checklist (before powering down 2026-05-12)

- [ ] `rpm-ostree status` — confirm current image is `bazzite-deck-nvidia:stable`
- [ ] `git -C ~/Developer/GitHub/ogmorrow2090/gaming status` — clean tree
- [ ] Snapshot current state for rollback:

  ```bash
  ssh Og@172.16.100.212 'rpm-ostree status > ~/pre-amd-rpm-ostree-status.txt
  ps -ef | grep gamescope > ~/pre-amd-gamescope-procs.txt
  cat ~/bin/gamescope-wrapper > ~/pre-amd-wrapper.sh
  cat ~/.config/gamescope-mode > ~/pre-amd-mode.txt'
  ```

- [ ] Confirm Sapphire 9070 XT physically in hand, verify HDMI 2.1 certified
      cable available
- [ ] Confirm 1200W PSU has 2× free 8-pin PCIe cables (or 3× if current 3080 Ti
      uses three; the 9070 XT only needs two)

## Physical swap (PC powered down)

1. Power off, wait for fans to fully stop
2. Switch PSU off at the back (rocker switch)
3. Hold case power button 10s to drain capacitors
4. Open case
5. Unscrew + remove 3080 Ti, disconnect its PCIe power cables
6. Install 9070 XT in same PCIe x16 slot, screw down
7. Connect 2× 8-pin PCIe to the 9070 XT
8. Close case
9. Switch PSU back on
10. Boot

## Post-boot: first power-on (still on NVIDIA image)

Expected: system boots to Bazzite. The `bazzite-deck-nvidia` image will try
to load `nvidia.ko` against an AMD GPU. It will fail — but kernel falls back
to amdgpu and you'll get a basic desktop (probably janky, possibly stuck on
SteamOS Game Mode at 1080p60).

This is **expected** — you're booting an NVIDIA-baked image on an AMD GPU
temporarily.

- [ ] SSH in: `ssh Og@172.16.100.212 uptime`
- [ ] Confirm GPU detected: `lspci -nn -d ::0300` — should show AMD/Navi
- [ ] Confirm amdgpu loaded: `lsmod | grep amdgpu`
- [ ] Confirm HDMI-A-1 connected: `cat /sys/class/drm/card0-HDMI-A-1/status`

## Rebase to AMD image

```bash
# On bazzite (over SSH):
sudo rpm-ostree rebase ostree-image-signed:docker://ghcr.io/ublue-os/bazzite-deck:stable
sudo systemctl reboot
```

Download is ~3-5 GB. Takes 5-10 min.

After reboot:

- [ ] `rpm-ostree status` — confirm new image is `bazzite-deck:stable`
- [ ] Re-layer if missing: `sudo rpm-ostree install fail2ban tesseract`
  - (These are currently layered; verify they survived rebase)
- [ ] Steam Game Mode launches, Big Picture comes up at LG native 4K
- [ ] Audio (HDMI to LG) works — `pactl list sinks short`
- [ ] No NVIDIA-related kernel errors: `sudo dmesg | grep -iE "fail|error" | head -20`

## Restore baseline configs

Today's emergency-mode wrapper is at `~/bin/gamescope-wrapper` and is intentionally degraded for tonight's NVIDIA flicker:

- HDR stripped from upstream args
- `--prefer-output HDMI-A-1` hardcoded
- Modes capped at 1080p120 / 4k60 / 4k120 / 4k165 (no 2k165)

After AMD swap, restore the full pre-session wrapper (or take this opportunity
to write a clean v2):

- [ ] HDR flags can be left intact (no more LG renegotiation flicker)
- [ ] `--immediate-flips` can be left on all modes (no more bandwidth issues)
- [ ] Per-mode `--prefer-output` switching between HDMI-A-1 / HDMI-A-2 / virtual
      connectors NOW WORKS — design the wrapper around this

## Solve the original 1280×800@90 problem (the whole reason for today's session)

With AMD, two solid paths to choose from. Try Path A first (simpler):

### Path A — vkms virtual display

```bash
# Load vkms module on boot
sudo tee /etc/modules-load.d/vkms.conf << 'EOF'
vkms
EOF

# (Optional) Module parameters for vkms
sudo tee /etc/modprobe.d/vkms.conf << 'EOF'
options vkms enable_writeback=1
EOF

sudo systemctl reboot
```

After reboot:

- vkms appears as `/dev/dri/card1` with `Virtual-1` connector
- Add wrapper case for `deck` mode: `--prefer-output Virtual-1 -W 1280 -H 800 -r 90 -o 90`
- Steam BPM resolution dropdown shows 1280×800@90 when in `deck` mode
- Sunshine apps.json: streaming app `"output": "Virtual-1"` (or use the
  card1 KMS connector name as displayed by `drm_info`)

### Path B — Virtual HDMI-A-2 via kernel args (no physical dummy needed)

```bash
sudo rpm-ostree kargs --append-if-missing=drm.edid_firmware=HDMI-A-2:edid.bin \
                     --append-if-missing=video=HDMI-A-2:e

# Deploy the existing edid-virtual-display.bin from the repo
scp configs/bazzite/edid-virtual-display.bin Og@172.16.100.212:/tmp/edid.bin
ssh Og@172.16.100.212 'sudo cp /tmp/edid.bin /var/usrlocal/lib/firmware/edid.bin
sudo systemctl reboot'
```

After reboot:

- HDMI-A-2 force-enabled as virtual connector with custom EDID
- Same wrapper approach as Path A but using `HDMI-A-2` instead of `Virtual-1`

## End-to-end verification

- [ ] `gamescope-mode deck` → Steam BPM dropdown lists 1280×800@90
- [ ] Stream from Steam Deck via Moonlight or Steam Link → see Steam BPM at
      Deck-native resolution, UI sized correctly for 7" screen
- [ ] Stream from Apple TV → 4K resolution works without HDR breaking capture
- [ ] Couch play on LG via `gamescope-mode 4k165` → 4K@165 + HDR + VRR clean
- [ ] `dmesg | grep -i "link training"` → empty (no FRL failures)
- [ ] AV1 encode works in Sunshine (`/var/log/sunshine.log` should mention AV1
      encoder available)

## Rollback path (if AMD GPU has unexpected issues)

The old NVIDIA deployment stays pinned in `rpm-ostree status` as the second
deployment until you next `rpm-ostree cleanup -p`. To roll back:

1. Power down
2. Swap 9070 XT back out, RTX 3080 Ti back in
3. Boot — pick the older `bazzite-deck-nvidia:stable` deployment at the GRUB
   menu (or `rpm-ostree rollback` from a working AMD session before swapping
   GPU back)
4. You're back to today's state

## Order placed

- **Sapphire AMD Radeon RX 9070 XT PULSE 16GB GDDR6** (Scan code: LN154765,
  Mfr code: 11348-03-20G)
- **£629.99** inc VAT, free next-day delivery
- Ordered: 2026-05-11, ETA 2026-05-12

## Post-rebase follow-up tasks (do AFTER AMD is stable, not blocking the swap)

### 1. Block-level nightly clone from nvme0 (OS) to nvme1 (spare 4TB)

The spare 4TB SN850X (serial 24127W4A0Q10, currently unmounted) is earmarked
for disaster-recovery cloning. Bazzite is btrfs (`subvol=root`), so btrfs
send/receive is the right tool. Use **btrbk** for automation.

```bash
# 1. Format the spare drive
sudo mkfs.btrfs -L bazzite_clone /dev/nvme1n1

# 2. Mount, layer btrbk via rpm-ostree
sudo mkdir -p /mnt/clone
sudo mount /dev/nvme1n1 /mnt/clone
sudo rpm-ostree install btrbk
sudo systemctl reboot

# 3. Configure /etc/btrbk/btrbk.conf — primary subvols to snapshot:
#    /var (rpm-ostree state, layered packages, container images)
#    /var/home (user data — /home is a bind mount to /var/home on bazzite)
#    Skip /usr (ostree-managed, reproducible via rebase anyway)
# 4. Enable the timer
sudo systemctl enable --now btrbk.timer
```

Recovery flow: if nvme0 fails, boot from a Fedora/Bazzite USB, mount nvme1,
btrfs receive the latest snapshot to a new drive, or swap nvme1 → nvme0 slot
and reboot. Add this drive entry to `/etc/fstab` with `noauto,nofail` so a
missing drive doesn't block boot.

### 2. Prevent NVIDIA drivers from ever returning

Mostly automatic on the AMD image rebase, but explicit belt-and-braces:

```bash
# Verify nothing NVIDIA remains post-rebase
rpm -qa | grep -i nvidia    # should be empty
lsmod | grep nvidia          # should be empty
ls /etc/modprobe.d/ | grep -i nvidia  # check stale config

# Optional explicit blacklist (defense in depth — protects against
# accidentally layering a CUDA package or similar)
sudo tee /etc/modprobe.d/blacklist-nvidia.conf <<'EOF'
# Post-AMD-swap 2026-05-12: ensure nvidia modules cannot load.
blacklist nvidia
blacklist nvidia_drm
blacklist nvidia_modeset
blacklist nvidia_uvm
install nvidia /bin/false
install nvidia_drm /bin/false
install nvidia_modeset /bin/false
EOF
```

The bazzite-deck (AMD) image doesn't ship NVIDIA drivers, so this is paranoid
but harmless. Useful if you ever experiment with VM/GPU-passthrough scenarios.

### 3. Re-enable HDR + immediate-flips in gamescope-wrapper

The current wrapper is "safe-mode" with HDR stripped — needed only for NVIDIA's
HDR-renegotiation flicker. On AMD, restore the full HDR + immediate-flips
wrapper. Either cherry-pick from earlier git history or hand-edit to add the
HDR flags back to each mode + uncomment immediate-flips on the high-refresh
modes.

### 5. HDMI-CEC: trigger LG input switch from bazzite (Steam Controller 2 button)

**Goal:** pressing the Steam button on the Steam Controller 2 (launched 2026-05-04)
should auto-switch the LG G5 HDMI input to bazzite's port — mirroring the
Apple TV behaviour (its remote already switches LG to Apple TV via CEC).

**Why this matters more on AMD:**

- NVIDIA stripped HDMI-CEC silicon from consumer cards years ago → guaranteed
  no native CEC on the RTX 3080 Ti
- AMDGPU has CEC support in the kernel (since 4.20). RDNA 4 native HDMI CEC
  status undocumented for consumer reports — test after swap. Worst case
  use a Pulse-Eight USB-CEC adapter (£40-50) for guaranteed CEC.

**Test native AMD CEC first:**

```bash
sudo rpm-ostree install cec-utils libcec
sudo systemctl reboot
cec-ctl --list-devices                       # GPU should expose a CEC adapter
cec-ctl --to 0 --image-view-on               # should switch LG input to bazzite
```

If `cec-ctl --list-devices` shows a `/dev/cec0` adapter → native CEC works.
If empty → buy a Pulse-Eight USB-CEC Pro adapter, plug it in via USB + sit it
in the HDMI chain, repeat.

**Wire it up via Steam Input:**

Bind Steam Controller 2's Steam button to run `cec-ctl --to 0 --image-view-on`
via a Steam Input layered keybind → desktop script. Or use a generic hotkey
daemon if Steam Input layering proves fiddly.

**Bonus CEC ideas once basic switching works:**

| Trigger | CEC command | Effect |
| --- | --- | --- |
| Steam button on SC2 | `cec-ctl --to 0 --image-view-on` | LG switches to bazzite |
| Sunshine session start | Same | LG wakes + switches to bazzite for couch viewing of stream |
| Sunshine session end | `cec-ctl --to 0 --standby` (optional) | LG sleeps after stream done |
| GW2 exit (script hook) | Switch to Apple TV input | Reverses current Apple TV → bazzite default flow |

### 4. Restore the deck mode in the wrapper (Moonlight is the chosen path)

**Decision 2026-05-12: stick with Moonlight + Sunshine for ALL streaming
clients (Deck + Apple TV). Steam Link not pursued** because:

- Linux gamescope hosts don't auto-negotiate client resolution
  (Steam-for-Linux issue #6577) — manual mode switching required either way
- Sunshine prep-cmd hooks are more powerful than Steam Remote Play has
- Apple TV Moonlight client significantly more mature than Steam Link tvOS
- Existing infrastructure (apps.json, sunshine-wait-gw2.sh, controller VDFs)
  is already tuned and working

So restore the `deck` mode in the wrapper:

```bash
deck)     exec /usr/bin/gamescope "${ARGS[@]}" --prefer-output HDMI-A-1 \
            -W 1280 -H 800 -r 90 -o 90 --immediate-flips ;;
```

Workflow per stream session:

```bash
~/bin/gamescope-mode deck      # switch BEFORE opening Moonlight on the Deck
# Open Moonlight on Deck → Sunshine GW2 app → play

~/bin/gamescope-mode 4k165     # back to couch play after stream ends
```

Same pattern as before the AMD swap, but on AMD the mode switch is clean
(no NVIDIA driver-state flicker class). The "never change mode mid-stream"
rule from `memory/nvidia-gamescope-state-flicker.md` still applies as a
general best practice though.

## Future considerations (deferred — not in this migration scope)

### CPU upgrade options if GW2 framerate becomes actively bothersome

**AM4 bridge build (~£400, ~50-55% GW2 perf gain over 9900K, keeps DDR4):**

| Item | Choice | Price |
| --- | --- | --- |
| CPU | Ryzen 7 5700X3D (not 5800X3D — overpriced now at £539 new vs 5700X3D £190) | £190 |
| Mobo | MSI MAG B550M Mortar MAX WiFi (WiFi 6E + BT 5.3) | £130 |
| Cooler | Arctic Liquid Freezer III 240 AIO (or Thermalright Peerless Assassin 120 SE air at £35) | £80 |
| **Total** | | **£400** |

Keep existing case, PSU, RAM, NVMe, NIC. 10GbE NIC fits B550 mobo's PCIe 3.0 x4
slot below the 2.5-slot GPU.

**AM5 platform refresh (~£700-750 new RAM, ~£600-650 net after selling DDR4):**

| Item | Choice | Price |
| --- | --- | --- |
| CPU | Ryzen 7 9800X3D | £380 |
| Mobo | B650M microATX (e.g. MSI MAG B650M Mortar) | £170 |
| RAM | 32GB DDR5-6000 (sell existing 64GB DDR4-3200, recover ~£100) | £130 |
| Cooler | Arctic Liquid Freezer III 240 | £80 |
| **Total** | | **£760 (or £660 after DDR4 resale)** |

9800X3D is ~85-95% GW2 perf gain over 9900K vs ~50-55% for 5700X3D. Future
upgrade path on AM5 (Zen 6 X3D in 2027+).

### 5800X3D explicitly NOT recommended

The Ryzen 7 5800X3D's used-market price has surged in 2026 ($600-800 USD,
£400-600 UK) — often *more* expensive than the 9800X3D new at £380. Buy
5700X3D for AM4 or 9800X3D for AM5; skip 5800X3D entirely.

### Other deferred ideas

- 90° HDMI 2.1 angled connectors for cable strain relief (Cable Matters or
  Lindy, FRL-certified) — ~£15-20 each
- Fractal Design Terra SFF cube case rebuild — requires SFX-L PSU swap (+£200)
  and is borderline-tight for the PULSE 9070 XT (320mm vs 322mm clearance);
  not worthwhile vs the current NZXT H400 (~37L, equivalent volume to Fractal
  Define 7 Mini)
