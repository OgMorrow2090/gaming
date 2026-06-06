---
name: Mystic Clicker build + auto-deploy pipeline
description: How mystic-clicker.dll (and sibling addons) are built on GitHub Actions and auto-pushed to all 4 GW2 profiles via a Mac-side launchd watcher. Replaces "build manually on Windows + scp by hand" with "git push -> auto-deployed within 5 min".
type: reference
---

<!-- markdownlint-disable MD041 -->

## Pipeline

```
git push  ──►  GitHub Actions windows-latest runner builds .vcxproj
                    │
                    ▼
              artifact: mystic-clicker.dll
                    │
                    ▼
       launchd agent on Mac (every 5 min)
                    │
                    ▼
         scripts/deploy-mystic-clicker-dll.sh
              │  pulls latest artifact
              │  pre-flight: GW2 closed everywhere reachable
              │  scp + sha256 verify per target
              ▼
       all 4 GW2 profiles updated
       (3 bazzite + 1 Deck native)
```

## Build (GitHub Actions)

`.github/workflows/build.yml` runs on every push to main + PRs:

- Matrix: `mystic-clicker`, `mystic-trading` (+ any future module)
- Runner: `windows-latest` with `microsoft/setup-msbuild@v2`
- Builds `Release|x64` via msbuild
- Uploads `bin/Release/<module>.dll` as a workflow artifact named after the module

To grab a DLL manually: `gh run download <run-id> --repo OgMorrow2090/gaming --name mystic-clicker`.

## Deploy script

`scripts/deploy-mystic-clicker-dll.sh`:

- No args: pulls the **latest successful** build's artifact from main
- `<run-id>`: deploys a specific run's artifact (for rollback or out-of-band testing)
- `--local <path>`: deploys a hand-built DLL (skip GitHub download)

Same pre-flight pattern as `deploy-nexus-config.sh` and `deploy-controller-vdf.sh`:

- Refuses if GW2 (`pgrep -x Gw2-64.exe`) is running on **any reachable** host
- Skips + warns unreachable hosts (Deck asleep, etc.)
- Per-profile `.bak-<TS>` backup before `scp`
- `sha256` verified per target
- 4 profiles: `local`, `apple-tv`, `deck-bazzite` (all on `Og@172.16.100.212`), `deck-native` on `deck@172.16.100.95`

## Watcher (Mac-side auto-deploy)

`scripts/watch-mystic-clicker.sh` + launchd agent at `configs/launchd/com.itinyk.gw2-mystic-clicker-watcher.plist`:

- Runs every 300 s (5 min) plus once at agent load
- Compares latest CI build SHA against state file `~/.cache/itinyk/gw2-mystic-clicker-last-sha`
- If different AND bazzite reachable AND GW2 not running anywhere reachable → invokes deploy script
- Updates state file on success
- Logs to `/tmp/gw2-mystic-clicker-watcher.log`

### Install on a fresh Mac

```bash
cp /Users/shaunchittenden/Developer/GitHub/ogmorrow2090/gaming/configs/launchd/com.itinyk.gw2-mystic-clicker-watcher.plist \
   ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/com.itinyk.gw2-mystic-clicker-watcher.plist
```

### Manage

```bash
launchctl list | grep gw2-mystic-clicker         # status (PID + last exit)
launchctl unload ~/Library/LaunchAgents/com.itinyk.gw2-mystic-clicker-watcher.plist  # stop
launchctl load   ~/Library/LaunchAgents/com.itinyk.gw2-mystic-clicker-watcher.plist  # start
tail -f /tmp/gw2-mystic-clicker-watcher.log      # observe
echo > ~/.cache/itinyk/gw2-mystic-clicker-last-sha  # force re-deploy on next tick
```

## Why a Mac watcher and not a GitHub Actions deploy step

GitHub Actions cloud runners can't reach `172.16.100.x` private LAN IPs. Three workarounds exist (self-hosted runner on bazzite, Tailscale tunnel, public exposure of bazzite), all involve additional setup or risk. The Mac is already on the LAN and already authenticated to bazzite via SSH key, so polling from there is the lowest-friction path. **Trade-off**: Mac must be online for the auto-deploy. If the Mac is off, the next time it wakes the watcher catches up automatically.

## When to suspect the watcher needs intervention

- New CI build hasn't deployed within ~10 min of push and Mac is online → check `/tmp/gw2-mystic-clicker-watcher.log` for "GW2 running" (close GW2) or "bazzite unreachable" (network)
- DLL deployed but addon doesn't load on relaunch → check `addons/Nexus/Nexus.log` for `[Loader] Loaded addon: mystic-clicker.dll`
- `launchctl list | grep gw2-mystic-clicker` shows non-zero exit code → most recent run failed; check stderr at `/tmp/gw2-mystic-clicker-watcher.err`

## Related

- Build infra also covers `mystic-trading` — same pattern. Could extend the deploy script to handle trading too if/when it iterates fast.
- Companion deploy scripts: `scripts/deploy-nexus-config.sh` (canonical InputBinds JSON), `scripts/deploy-controller-vdf.sh` (Steam Input layout). Same 4-target list, same pre-flight pattern. Don't yet have watchers — those configs change less often.

## OCR side-channel: gw2-ocr-daemon (host-side tesseract worker)

The COPY_ITEM_NAME macro can't spawn `/usr/bin/tesseract` from inside the GW2 sandbox because Wine's `start /unix` is broken in Proton 11.0 ("Could not translate the specified Unix filename to a DOS filename"). Fix is a **host-side daemon** that runs OUTSIDE the sandbox and reads/writes via /tmp (bind-mounted into the sandbox).

- `scripts/gw2-ocr-daemon.sh` — polls /tmp at 10 Hz, runs tesseract on `gw2-ocr-input-<TS>.bmp` files when their `.ready` marker appears, writes `gw2-ocr-output-<TS>.txt` + touches `gw2-ocr-done-<TS>`.
- `configs/bazzite/gw2-ocr-daemon.service` — systemd user unit (target: `default.target`).
- Install: `install -m 755 scripts/gw2-ocr-daemon.sh ~/scripts/`, `install -m 644 configs/bazzite/gw2-ocr-daemon.service ~/.config/systemd/user/`, `systemctl --user daemon-reload && systemctl --user enable --now gw2-ocr-daemon`.
- Logs: `~/.local/state/gw2-ocr-daemon.log`.
- Bazzite-only — Steam Deck native install doesn't have rpm-ostree-installed tesseract; OCR macros simply time out on that profile.

If the macro reports "OCR timed out — gw2-ocr-daemon.service not running on bazzite?", the daemon stopped or was never enabled. `systemctl --user status gw2-ocr-daemon` to check.
