# Steam Deck - SteamOS

## Host Details

| Field | Value |
| ----- | ----- |
| **Hostname** | steam-deck |
| **OS** | SteamOS (Arch-based, immutable) |
| **IP Address** | 172.16.100.95 |
| **VLAN** | 100 (IoT) |
| **SSH User** | deck |
| **SSH Key** | 1Password (same key as Pi hosts) |

## SSH Access

```bash
ssh steam-deck
# or
ssh deck@172.16.100.95
```

Uses the 1Password SSH agent — same key used for all Pi hosts and Bazzite. Biometric approval required on connection.

**Requires**: SSH config entry in `dev-toolkit/configs/ssh_config` (already added).

## Initial SSH Setup

These steps are performed once on the Steam Deck in Desktop Mode (Konsole):

### 1. Set deck user password

```bash
passwd
```

### 2. Enable SSH

```bash
sudo systemctl enable sshd
sudo systemctl start sshd
```

### 3. Add 1Password public key

From Mac (after password is set):

```bash
ssh-copy-id deck@172.16.100.95
```

Or manually on the Steam Deck:

```bash
mkdir -p ~/.ssh && chmod 700 ~/.ssh
nano ~/.ssh/authorized_keys
# Paste public key from 1Password
chmod 600 ~/.ssh/authorized_keys
```

### 4. Harden SSH (disable password auth)

```bash
sudo nano /etc/ssh/sshd_config.d/hardening.conf
```

```
PasswordAuthentication no
PermitRootLogin no
```

```bash
sudo systemctl restart sshd
```

Use `sshd_config.d/` drop-in files — SteamOS updates can overwrite the main `sshd_config`, but drop-in files survive.

### 5. Passwordless sudo

```bash
sudo steamos-readonly disable
echo 'deck ALL=(ALL) NOPASSWD: ALL' | sudo tee /etc/sudoers.d/zz-nopasswd-deck
sudo chmod 440 /etc/sudoers.d/zz-nopasswd-deck
sudo steamos-readonly enable
```

File named `zz-` prefix so it loads after other sudoers.d files (last match wins).

### 6. SSH ACL (firewalld)

```bash
sudo firewall-cmd --permanent --remove-service=ssh
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="172.16.10.1" port port="22" protocol="tcp" accept'
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="172.16.10.2" port port="22" protocol="tcp" accept'
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="172.16.10.5" port port="22" protocol="tcp" accept'
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="172.16.10.6" port port="22" protocol="tcp" accept'
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="172.16.10.7" port port="22" protocol="tcp" accept'
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="172.16.0.0/24" port port="22" protocol="tcp" accept'
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="172.16.1.0/24" port port="22" protocol="tcp" accept'
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="172.16.2.0/24" port port="22" protocol="tcp" accept'
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="172.16.100.0/24" port port="22" protocol="tcp" accept'
sudo firewall-cmd --reload
```

No Cloudflare WARP rule — local network access only (unlike Pi's which have WARP for remote SSH).

## SteamOS Notes

- Immutable root filesystem (`steamos-readonly disable` to modify, re-enable after)
- System packages may be reset on OS updates — use Flatpak for persistent app installs
- `pacman` available but packages don't survive updates unless you use `steamos-readonly disable`
- DHCP reservation set: 172.16.100.95 on VLAN 100
