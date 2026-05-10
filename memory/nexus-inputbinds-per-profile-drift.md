---
name: Nexus InputBinds.json drifts per profile and silently clears conflicting bindings on load
description: Each GW2 profile has its own addons/Nexus/InputBinds.json that drifts independently. When two bindings end up on identical key+modifiers, Nexus silently clears or moves one — recurring "binding stopped working" symptom.
type: project
---

<!-- markdownlint-disable MD041 -->

After the multi-GW2 split (Local + Apple TV + Steam Deck profiles, see [multi-gw2-installs.md](multi-gw2-installs.md)), every profile now has its **own** `addons/Nexus/InputBinds.json`. They diverge over time because:

- Each profile is launched from a different Wine prefix → Nexus reads/writes its own file
- Per-profile UI tweaks (different rebinds for Deck-controller-friendly vs Apple-TV-mouse-friendly) drift further with each in-game change
- **When Nexus loads InputBinds.json and finds two bindings sharing identical Code+Ctrl+Shift+Alt, it silently reconciles** — usually clears one (`Code=0`) or shifts the colliding one to its second-choice key

This shows up to the user as "binding stopped working — used to fire X". The earlier single-profile era had the same root cause but was easy to fix (one file).

## Symptom snapshot — how it presented 2026-05-01

User reported: "press and hold right DPad to auto-sort inventory — instead opens Community LFG." Diagnosis showed the Steam Deck profile (`~/Games/gw2-deck/addons/Nexus/InputBinds.json`) had **6 bindings cleared or moved** vs the Apple TV profile (which was still correct):

| Identifier | Apple TV (correct) | Deck (broken) | Cause |
|---|---|---|---|
| `DEPOSIT_AND_SORT` (Organizer auto-sort) | Ctrl+Q (Code 16) | **Code 0** (cleared) | likely manual clear in Nexus options |
| `0xFC56DD9F_ToggleLFG` (Community LFG addon) | Ctrl+F7 (Code 65) | **Code 0** (cleared) | user explicitly removed it |
| `BANK_COMBO` | F8 (Code 66) | **Code 0** (cleared) | conflicts with Deck `FASTSWAP_TOGGLE` which was on F8 |
| `DEPOSIT_MATERIALS` | Ctrl+F (Code 33) | Ctrl+D (Code 32) | conflicts with Deck `MYSTIC_FORGE` on Ctrl+F |
| `MT_RESET_WINDOWS` / `RESET_WINDOWS` | Ctrl+Shift+Home (Code 45) | **Code 0** | unknown |
| `TRADING_POST` | Ctrl+O (Code 24) | **Code 0** | unknown |

23 of ~200 entries differed total — many were intentional per-profile tweaks (e.g. Mystic Clicker MT_TOGGLE_FLIPLIST/DELIVERY are deliberately swapped Alt+F ↔ Alt+D between profiles). The 6 above were unintentional.

## How to apply

When the user reports "controller binding X stopped working" or "this addon's keybind isn't firing":

### 1. Confirm the chain — controller → key → addon

The chain is **VDF (Steam Input) emits keystroke** → **Wine receives** → **Nexus addon catches via Identifier**. For example DPad-Right hold = `Ctrl+Q` from VDF group 7 dpad_east Long_Press, expected to land on Nexus `DEPOSIT_AND_SORT`. Don't assume the VDF changed — diff the **InputBinds.json** first.

### 2. Diff the broken profile against a known-good one

Use this from the host (bazzite Og@172.16.100.212):

```bash
python3 -c "
import json
A = json.load(open('$HOME/Games/gw2-deck/addons/Nexus/InputBinds.json'))
B = json.load(open('$HOME/Games/gw2-appletv/addons/Nexus/InputBinds.json'))
a = {x['Identifier']: x for x in A}
b = {x['Identifier']: x for x in B}
for ident in sorted(b):
    if ident not in a or (a[ident].get('Code'),a[ident].get('Ctrl'),a[ident].get('Shift'),a[ident].get('Alt')) != (b[ident].get('Code'),b[ident].get('Ctrl'),b[ident].get('Shift'),b[ident].get('Alt')):
        print(ident, '|', 'Deck:', a.get(ident,{}).get('Code',0), 'AppleTV:', b[ident]['Code'])
"
```

### 3. Choose surgical OR wholesale based on damage scope

**Rule of thumb (revised after 2026-05-01 catastrophic-drift session):**

- **≤3 bindings differ** → surgical restore (preserves intentional divergence)
- **>3 bindings differ OR multiple chord groups broken at once** → **wholesale copy from Apple TV profile** (the canonical clean state — matches `configs/gw2-keybinds/nexus-inputbinds.json` repo source-of-truth)

Surgical pattern:

```bash
F="$HOME/Games/gw2-deck/addons/Nexus/InputBinds.json"
cp "$F" "$F.bak-$(date +%Y%m%d-%H%M%S)"
python3 -c "
import json
P = '$F'
binds = json.load(open(P))
for b in binds:
    if b['Identifier'] == 'DEPOSIT_AND_SORT':
        b.update({'Code': 16, 'Ctrl': True, 'Shift': False, 'Alt': False})
    elif b['Identifier'] == '0xFC56DD9F_ToggleLFG':
        b.update({'Code': 65, 'Ctrl': True, 'Shift': False, 'Alt': False})
json.dump(binds, open(P,'w'), indent=2)
"
```

Wholesale pattern (when many bindings drifted — verified canonical fix on 2026-05-01):

```bash
F="$HOME/Games/gw2-deck/addons/Nexus/InputBinds.json"
cp "$F" "$F.bak-pre-applewipe-$(date +%Y%m%d-%H%M%S)"
cp "$HOME/Games/gw2-appletv/addons/Nexus/InputBinds.json" "$F"
```

**Trade-off of wholesale:** loses intentional Deck-only customizations like the swapped `MT_TOGGLE_FLIPLIST`/`MT_TOGGLE_DELIVERY` Alt+D ↔ Alt+F mapping. Re-apply those after if needed. Worth it because user can re-edit 2-3 bindings in 30 seconds vs another multi-hour debug session.

### 4. Critical timing — close GW2 first

Nexus reads `InputBinds.json` on addon load and **writes it on game shutdown**. If GW2 is running while you edit the file, Nexus overwrites your edits on close. Always:

1. Close GW2 on the target profile
2. Edit the file
3. Launch GW2 — verify binding works
4. Close GW2 — re-read the file to confirm Nexus didn't clear/move it again

If Nexus re-clears after step 4, there's a **load-time conflict** still active. Search the file for any other entry with the same Code+modifiers and resolve.

### 5. Reference scancodes (Nexus uses scancodes, NOT VK codes)

Common ones for chord debugging:

| Scancode | Key | Scancode | Key |
|---|---|---|---|
| 16 | Q | 30 | A |
| 17 | W | 31 | S |
| 18 | E | 32 | D |
| 19 | R | 33 | F |
| 20 | T | 34 | G |
| 21 | Y | 35 | H |
| 22 | U | 36 | J |
| 23 | I | 37 | K |
| 24 | O | 38 | L |
| 25 | P | 50 | M |
| 59 | F1 | 64 | F6 |
| 60 | F2 | 65 | F7 |
| 61 | F3 | 66 | F8 |
| 62 | F4 | 67 | F9 |
| 63 | F5 | 68 | F10 |
| 45 | Home | | |

Note: GW2's `gamebinds.xml` uses **VK codes** instead — different table. E.g. VK 89 = Y, VK 73 = I, VK 81 = Q.

## Dual-failure mode (catastrophic case — observed 2026-05-01)

When **controller layout AND Nexus bindings drift simultaneously**, surgical fixes don't work because each fix only addresses one of two stacked problems. Symptoms feel "random" — depending on which binding you test, you hit either layer's broken side:

- **Controller side broken** (e.g., Steam Input UI auto-saved a local-edit overriding the CLOUD template, with one chord changed): controller emits the wrong key. Even if Nexus has the right binding waiting, no match.
- **Nexus side drifted** (multiple bindings cleared / moved): controller emits the right key, but no Nexus binding catches it.

If only one is broken, surgical fixes work. If BOTH are broken, you need to:

1. **Restore controller layout** to canonical from `configs/steam-controller/moonlight-gw2-og-template.vdf` (line-anchored copy, NOT regex)
2. **Wholesale-copy InputBinds** from Apple TV profile (the canonical clean state, matches `configs/gw2-keybinds/nexus-inputbinds.json`)
3. Restart Steam so it reloads layout from disk

Heuristic: if user reports "many things are broken simultaneously" or "everything is wrong", suspect dual failure. Don't waste hours on surgical fixes — restore both layers in one shot. Took 4+ hours on 2026-05-01 to converge on this; should be 5 minutes next time.

## Why both profiles can appear broken at once

Per [streaming-input-host-vs-client.md](streaming-input-host-vs-client.md), the Apple TV stream uses the **Deck as its physical controller** (over Moonlight, despite the older note that bazzite-side Steam Input governs Apple TV — that note is wrong for this setup). So:

- **Deck stream broken**: Deck Steam Input → wrong keystroke
- **Apple TV stream broken**: Apple TV uses Deck-as-controller → same Deck Steam Input → same wrong keystroke

If the user reports "both Apple TV and Deck are broken the same way", that's the signal. Don't waste time investigating the bazzite-side Apple TV configset — fix the Deck-side controller layout (single source for both routes).

## Recurrence — 2026-05-05 (Alt+F4 hypothesis falsified)

Same 23-binding drift recurred on the Deck profile **3 days after** the v19.2 GRACEFUL_QUIT mitigation deployed (Esc → click Exit Game replacing Alt+F4). User reported it after a ~4-minute Deck Moonlight session ending at 07:18:39. Crucially, the Nexus.log captured a **fully clean shutdown sequence** for that session: every addon's `Unload`/`Stopping` logged in order, ending with `[Core] SHUTDOWN END`. **No force-quit signature.** The user used Logout, not Alt+F4.

Yet the same 23 binds drifted. Apple TV profile (last touched same day) stayed clean. So **Alt+F4 was not the cause** — at least, not the dominant cause. The v19.2 GRACEFUL_QUIT mitigation can't fix this on its own.

Forensic snapshots (broken state, addon backup list, Nexus shutdown log) saved to `forensics/inputbinds-drift/*-20260505.*` for analysis.

What the diff looks like in this incident — patterns that may point at the real cause:

- Several entries flipped `Type` 1↔0 in **both directions** (cleared AND newly bound), so it isn't a one-way "everything cleared" wipe
- Entries that drifted match specific **addon-owned identifiers** — `CraftyLegend.KB_CRAFTY_TOGGLE`, `Hoard__Seek.KB_HOARD_TOGGLE`, `Community_LFG.0xFC56DD9F_ToggleLFG`, `Organizer.DEPOSIT_AND_SORT`, multiple Mystic Clicker entries (`BANK_COMBO`, `LOUNGE_PASS_COMBO`, `TRADING_POST`), `NexusGameWiki.Toggle`, `pathing-render-toggle`, `Toggle Event Timers`
- Several modifier-only changes (e.g. `KB_CRAFTY_TOGGLE` Code 87 Ctrl → Code 38 Ctrl+Shift) that look like **addon defaults**, not collision-resolution

**Strongest remaining theory**: an addon (or several) is rewriting its own keybind entry to its current default on first load when the addon DLL version differs from the one that wrote the bindings the previous session. This would explain why the entries cluster around specific addons and why the changes look like "default preset" patterns. Would require version-tracking each addon DLL across sessions to confirm.

**Next-incident diagnostic to add**: capture each addon DLL's mtime + sha256 on the Deck profile vs Apple TV profile, compare against the prior session's snapshot. If a DLL changed between sessions and that addon's identifier drifted, causation.

## Open bug — root cause of the 23-binding drift remains unknown

Honest assessment after the 2026-05-01 catastrophic-drift session: 23 entries differing between the Deck profile and Apple TV/canonical does **NOT** fit any single mechanism we identified. The wholesale-copy recovery worked, but we never proved the cause. Theories tested and ruled out:

- ❌ **Passive Nexus collision-resolution** — only affects 1-3 bindings per load, not 23
- ❌ **OneDrive sync** — bazzite has no OneDrive (the `users/shaun/OneDrive/Documents/` path inside the Wine prefix is just a folder name, no actual cloud sync)
- ❌ **Steam Cloud sync** — does not apply to non-Steam Nexus configs
- ❌ **Addon_Config_Backup_Tool restore** — the most recent backup zip on disk (`backup-2026-04-30-20-27.zip`) contains the canonical bindings (Ctrl+Q, Ctrl+O, etc.), so a restore from it would have FIXED not broken
- ❌ **User direct edit** — user confidently denies making 23 manual binding changes; this fits

Possible causes still on the table (not verified):

- ⚠️ **Alt+F4 hard-quit losing in-session fixes** — *most plausible candidate.* The Menu button's Long_Press is bound to `Alt+F4 (Quit Game)` in the controller layout (button_escape Long_Press). User uses this to quit GW2. While Alt+F4 normally goes through `WM_CLOSE` → graceful Nexus unload → InputBinds.json save, in some cases (mid-frame, hung process, repeat-firing during Long_Press, Wine "not responding" handling) it can become a force-terminate before Nexus's `Unload` runs. Pattern over weeks:
    1. Bindings drift slightly via passive Nexus collision-resolution (1-3 cleared per session)
    2. User notices in-game, re-binds via Nexus options menu (change is in memory only)
    3. User Alt+F4-quits → in-memory fix never saved → disk still has cleared bindings
    4. Repeat across sessions — cumulative cleared-binding count grows because fixes don't persist
    5. After weeks: 23 drifted bindings on disk despite user "fixing" them several times

   This fits the evidence: user firmly denies making 23 changes (they made *fixes* that never persisted), magnitude exceeds pure passive drift alone, Apple TV profile less affected (probably different usage pattern — quit cleanly more often, or Alt+F4 less often used). **Mitigation: avoid Alt+F4 to quit; use in-game Logout instead. Or document that any in-game binding fixes need a clean exit afterward.**

  **Mitigation deployed 2026-05-02** — Mystic Clicker v3.6.11 added a `GRACEFUL_QUIT` macro (Esc → 200 ms → click captured Exit button) bound to `ALT+SHIFT+Q`. Controller v19.2 replaced the `button_escape` Long_Press from `Alt+F4` to that chord, so the Menu-hold quit path now goes through the in-game menu's Exit button → normal shutdown → Nexus `Unload` → InputBinds.json saves cleanly. If drift stops recurring after a few weeks of normal play with v19.2+, that confirms Alt+F4 was the cause. If drift keeps happening, the cause is something else and we should investigate the other candidates below.
- ⚠️ **Nexus addon update bulk-resetting its own identifiers** on first launch in a new version. Each addon owns specific identifiers (e.g. Mystic Clicker owns `DEPOSIT_AND_SORT`, `BANK_COMBO`, etc.); an update could plausibly clear them. Multiple addons updating in one window could explain widespread drift.
- ⚠️ **Stale file overwrite during gw2-deck profile setup on Apr 30** — the addons folder was *copied* (per [multi-gw2-installs.md](multi-gw2-installs.md) and [nexus-multi-deploy-rules.md](nexus-multi-deploy-rules.md)) at profile creation. If the Steam install's InputBinds.json was already drifted at copy time, the gw2-deck profile inherited it. But this doesn't explain why Apple TV profile (also copied on Apr 30) was clean.
- ⚠️ **Cumulative passive drift over weeks** — each load that hits a collision clears 1-3 entries; over many sessions this could add up. User says they didn't make the changes; if the mechanism is the silent reconciliation we already documented, it qualifies as "they didn't make changes" because Nexus did them silently.

**Action item for next time the drift recurs:** capture diagnostic data BEFORE wholesale-restoring:

```bash
# Snapshot the broken state
TS=$(date +%Y%m%d-%H%M%S)
cp "$HOME/Games/gw2-deck/addons/Nexus/InputBinds.json" "/tmp/inputbinds-broken-$TS.json"
ls -la "$HOME/Documents/Guild Wars 2/nexus-configs/" > "/tmp/backup-list-$TS.txt"
grep -E "Loaded addon|Updater|Backup" "$HOME/Games/gw2-deck/addons/Nexus/Nexus.log" | tail -200 > "/tmp/nexus-load-$TS.log"
```

Then commit those snapshots to the repo for forensic analysis. The current memory is "we know the recovery, we don't know the cause."

## Recurrence — 2026-05-10 (incident #3, on Apple TV — every-session pattern confirmed)

User played GW2 on Apple TV stream for **2 minutes** (12:12:16 launched → 12:14:32 shutdown), shutdown was a fully clean `WM_DESTROY` (no Alt+F4, full unload chain in Nexus.log). InputBinds.json on Apple TV was rewritten **1 second later** at 12:14:33 with 24 binds drifted. Other 3 profiles (Local, Deck-bazzite, Deck-native) stayed clean (canonical hash `e131e8aa92faf805`).

This **falsifies the GRACEFUL_QUIT/Alt+F4 hypothesis again** — drift happens on every normal shutdown, not just force-quits. The pattern is now: **any GW2 session on a profile = drift on that profile's InputBinds.json on next save**.

### Per-addon DLL snapshot captured (incident #2 action item complete)

Saved at `forensics/inputbinds-drift/addon-snapshot-appletv-20260510-121529.txt`. Notable: `mystic-clicker.dll` changed hash from earlier today (new CI build deployed by launchd watcher 4 minutes before GW2 launch — `edfcfe67d80f76fa`). Other DLLs unchanged.

Checking the "DLL version change triggers drift" theory: Mystic Clicker DID change before this session, and 12 of 24 drifted entries belong to Mystic Clicker (`DEPOSIT_MATERIALS`, `DEPOSIT_AND_SORT`, `MYSTIC_FORGE`, `MYSTIC_FORGE_COMBO`, `BANK_COMBO`, `COPY_ITEM_NAME`, `RESET_WINDOWS`, `TRADING_POST`, `WAYPOINT_COMBO`, `WIZARD_GOBBLER_COMBO`, `WIZARD_PORTAL_SCROLL_COMBO`, `LOUNGE_PASS_COMBO`). **But the other 12 drifted entries belong to addons whose DLLs did NOT change today** — Hoard, CraftyLegend, NexusGameWiki, Organizer, Pathing, Mystic Trading, Fast Swap, Community LFG, Event Timers, Nexus core. So the DLL-change hypothesis is **partially supported but not the whole story**.

### Drift pattern in this incident

Most drifted entries match the addon's registration default (per the addon's `InputBinds_RegisterWithString` call) — e.g. `DEPOSIT_MATERIALS` Code 33 (Ctrl+F user-customized) → Code 32 (Ctrl+D = MysticClicker default), `MYSTIC_FORGE` Code 0 (user unbound) → Code 33+Ctrl (= MysticClicker default `CTRL+F`). Some entries went to 0 (matching `(null)` defaults). Some have unexplained shifts (e.g. `TRADING_POST` Code 24+Ctrl → 0 even though MysticClicker's default is `CTRL+O`).

The strongest fitting hypothesis: **Nexus's `InputBinds_RegisterWithString` on addon load sometimes overwrites the user's stored value with the addon's default, then writes back the new (default) value on shutdown.** Whether this is a Nexus bug, a load-order race, or a "first-time-after-update" code path is unclear without instrumenting Nexus itself.

### No on-disk Nexus state file

Only `InputBinds.json`, `AddonConfig.json`, `Settings.json` in the Nexus dir — no binary cache that could hold stale state. So Nexus reads InputBinds.json from disk every load. Whatever's overwriting must be in-memory between load and save.

### Operational workaround

Until a Nexus-level fix exists, **InputBinds.json must be re-deployed from canonical after every GW2 session on the affected profile**. Two options:

1. Manual: run `./scripts/deploy-nexus-config.sh` after every play session
2. Automatic: extend the Mac launchd watcher (`scripts/watch-mystic-clicker.sh`) to also check InputBinds.json hash per profile and auto-restore canonical when drift is detected and GW2 is not running

Option 2 is the practical fix — drift becomes self-healing within 5 minutes.

### Forensics committed

- `forensics/inputbinds-drift/inputbinds-broken-appletv-20260510-121529.json` — drifted state at incident
- `forensics/inputbinds-drift/addon-snapshot-appletv-20260510-121529.txt` — all addon DLL hashes + mtimes for cross-incident comparison

## Long-term mitigation

If this recurs more than once or twice, mitigation options:

- **Sync script** that copies the canonical profile to others on a cron (loses intentional divergence — accepted because the practical cost of drift far exceeds the cost of re-applying 2-3 deliberate Deck-only overrides)
- **Diff script** that warns when bindings drift between profiles without auto-fixing
- **Per-launch validation** — small wrapper around game launch that checks key bindings against canonical and aborts/warns if drifted

For now, the recipe in the surgical-or-wholesale section above is the standing fix.
