---
name: bazzite-performance-tuning
description: Bazzite is tuned for full gaming performance — custom TuneD profile, 5GHz all-core, GPU high, all sleep masked. Config backups in configs/bazzite/.
metadata:
  type: project
---

Bazzite (i9-9900K, 64GB, RX 9070 XT) is configured for maximum gaming performance as of 2026-05-13.

**What was done:**

1. **Custom TuneD profile** (`gaming-performance`) at `/etc/tuned/profiles/gaming-performance/tuned.conf` — inherits `throughput-performance`, forces governor=performance, EPP=performance, min_perf_pct=100
2. **PPD override** (`/etc/tuned/ppd.conf`) — maps PPD `performance` → `gaming-performance`; default set to `performance`; `ppd_base_profile` set to `performance`
3. **Backup systemd unit** (`cpu-performance.service`) — runs after TuneD as a belt-and-suspenders fallback, sets intel_pstate min_perf_pct=100, GPU to `high`
4. **All sleep/suspend/hibernate targets masked** (sleep, suspend, hibernate, hybrid-sleep, suspend-then-hibernate)

**Why:** CPU was running at ~4.1GHz under `powersave` governor + `balanced-bazzite` TuneD profile despite BIOS supporting 5GHz all-core. Bazzite's `tuned-ppd` daemon was overriding manual TuneD profile changes on every boot.

**How to apply:**

- Config backups are in `configs/bazzite/`: `tuned-gaming-performance.conf`, `tuned-ppd.conf`, `cpu-performance.service`
- To restore after a Bazzite rebase/update that wipes `/etc`:
  ```bash
  sudo mkdir -p /etc/tuned/profiles/gaming-performance
  sudo cp tuned-gaming-performance.conf /etc/tuned/profiles/gaming-performance/tuned.conf
  sudo cp tuned-ppd.conf /etc/tuned/ppd.conf
  echo "performance" | sudo tee /etc/tuned/ppd_base_profile
  echo "gaming-performance" | sudo tee /etc/tuned/active_profile
  sudo cp cpu-performance.service /etc/systemd/system/
  sudo systemctl daemon-reload && sudo systemctl enable cpu-performance.service
  sudo systemctl restart tuned && sudo systemctl restart tuned-ppd
  ```
- After any Bazzite OS update (`rpm-ostree upgrade`), verify with `tuned-adm active` — if it reverts to `balanced-bazzite`, re-apply the above
- Thermals at idle: ~37C package (water cooled). Under GW2 zerg load: ~73-78C. Well within limits (high=86C, crit=100C)

Related: [[nvidia-gamescope-state-flicker]], [[project-bazzite-repurpose-plan]]
