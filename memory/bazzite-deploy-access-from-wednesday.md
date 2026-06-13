---
name: bazzite-deploy-access-from-wednesday
description: wednesday can now SSH/deploy to bazzite directly (Og@172.16.100.212) — a Meraki switch ACL exception + authorized key were added 2026-06-08. Previously blocked by the "Deny Node → IoT" VLAN rule.
metadata:
  type: reference
---

# Deploying to bazzite from the wednesday Pi

As of **2026-06-08**, the wednesday host (`172.16.2.1`, "Node" VLAN) can SSH and
deploy directly to **bazzite** (`Og@172.16.100.212`, "IoT" VLAN). Before this it
could not — see why below.

## Why it was blocked (and the fix)

The Core switch (Meraki C9300, `172.16.1.253`) ACL has a deliberate **"Deny Node →
IoT"** rule (`172.16.2.0/24 → 172.16.100.0/24`). bazzite sits on the IoT VLAN with
no per-host exception, unlike Thing (`.100.2`) / Uncle Fester (`.100.1`) which have
explicit allows *above* the deny. So wednesday→bazzite:22 was dropped at the switch.

Two changes removed the block (both approved):

1. **Meraki switch ACL** (`op://wednesday-pi/meraki_apikey_edit`): two rules added
   *above* "Deny Node → IoT" — `Wednesday ↔ Bazzite SSH`
   (`172.16.2.1 ↔ 172.16.100.212:22`, SSH-only; IoT isolation otherwise intact).
   The ACL API is a full-array `PUT` (strip the trailing auto "Default rule", splice,
   PUT). Pre-change 114-rule backup: `~/meraki-acl-backup-20260608-221323.json` on
   wednesday.
2. **Authorized key**: wednesday's `~/.ssh/id_ed25519.pub` (`wednesday-host-dev`) was
   added to `Og@bazzite:~/.ssh/authorized_keys`.

The rule is pinned to bazzite's IP `.212` — relies on that DHCP reservation staying put.

**Steam Deck too (2026-06-09/10):** identical SSH-only exception for
`deck@172.16.100.95` (backup `~/meraki-acl-backup-20260609-191340.json`) + wednesday
key authorized — wednesday can now deploy to **both** GW2 hosts directly.

## How to deploy the GW2 Claude daemon from wednesday

```bash
SSH="ssh -o BatchMode=yes Og@172.16.100.212"
# 1. push daemon (back up first)
$SSH 'cp ~/scripts/gw2-claude-daemon.py ~/scripts/gw2-claude-daemon.py.bak-$(date +%F-%H%M%S)'
scp /srv/docker-data/repos/gaming/scripts/gw2-claude-daemon.py Og@172.16.100.212:scripts/
# 2. restart + verify (NO TTS while user is gaming — check log, don't run --analyze)
$SSH 'systemctl --user restart gw2-claude-daemon.service; sleep 2; tail -3 ~/.local/state/gw2-claude-daemon.log'
```

- Daemon: `~/scripts/gw2-claude-daemon.py`; venv `~/.local/share/gw2-claude/venv`;
  config `~/.config/gw2-claude/config.env`; service `gw2-claude-daemon.service` (`--user`).
- Auth is now the subscription OAuth token — see [[gw2-claude-vision]].
- bazzite still **can't reach the wednesday LAN** (IoT→Node deny stands); cross-VLAN
  alerts from bazzite go out via the public `alerts.itinyk.app` endpoint.

## DLL deploys from wednesday need GitHub *API* auth (not the SSH key)

The daemon deploy above is just `scp` and needs no GitHub auth. But the addon **DLL**
deploy (`deploy-mystic-ai-dll.sh` / `deploy-mystic-clicker-dll.sh`) runs
`gh run download` to fetch the CI-built artifact — that needs a gh OAuth/PAT token,
**which the `github-gaming` SSH deploy key does NOT provide**. Anonymous artifact
download = HTTP 401 (even though the repo is public).

State as of **2026-06-13**:
- `gh 2.94.0` installed at `~/.local/bin/gh` (linux_arm64, no sudo) — **not yet authenticated**.
- `op` (1Password CLI) is installed (`/usr/bin/op` v2.34.0) but **not signed in** on
  wednesday — `~/.config/op/config` has `accounts: null`, no `OP_SERVICE_ACCOUNT_TOKEN`.
  So `op read` can't pull the PAT from the vault yet.
- Plan: Shaun sets up a local 1Password **service account**; then
  `export OP_SERVICE_ACCOUNT_TOKEN=…` →
  `gh auth login --git-protocol ssh --with-token <<< "$(op read 'op://<vault>/<github-pat>/credential')"`
  → DLL deploys run from wednesday. PAT scope needed: Actions read + Contents read on `OgMorrow2090/gaming`.
