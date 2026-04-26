# Steam Controller VDF Editing — Golden Rules

> Lessons learned from the v18.4 outage (2026-04-26). Read this **before** touching any `.vdf` in `configs/steam-controller/`.

## TL;DR

The Steam Deck VDF parser **silently rejects** structurally damaged files — no error, no log, the layout just doesn't appear in Personal/Templates. A bulk-rewrite of the v18.4 VDF stripped ~25 chord activator subgroups (Long_Press/Double_Press), reducing the file from 63 121 → 37 170 bytes. It looked syntactically valid (brackets balanced, KeyValues legal) but Steam's parser dropped it. Recovery took an entire session of signing in/out, deleting workshop items, and rolling back to v18.3.

**The rule that would have prevented it: never bulk-rewrite a working VDF. Edit it line-by-line.**

---

## Why VDF is fragile

A Steam Input VDF is a deeply nested KeyValues tree. Each chord on a button (e.g. R1+DPad Right) lives under:

```text
"group" -> "inputs" -> "<dpad direction>" -> "activators" ->
  Full_Press -> bindings -> binding
  Long_Press -> bindings -> binding
  Double_Press -> bindings -> binding
```

Even though the file is text and easy to parse, **Steam's loader applies semantic validation** beyond bracket-balance:

- Group `id` references must resolve in `group_source_bindings`
- Activator names are a fixed enum
- The `controller_caps` value pins the layout to a specific controller
- The `url` field follows scheme rules (`autosave://` for working autosave, `usercloud://` for Personal, `workshop://` for Community)

When any of these is wrong, the layout is **silently dropped from the layout list**. There is no error in `~/.steam/steam/logs/`, no toast, nothing — it just doesn't appear.

---

## Golden Rules

### 1. Never bulk-rewrite blocks with regex

A `re.sub(...)` over a 60 KB VDF is the most reliable way to break it. Even when the regex is "right", greedy/non-greedy quirks have eaten Long_Press/Double_Press subgroups before — that was the v18.4 root cause.

**Allowed**: targeted line-anchored edits (`Edit` tool, single `old_string` → `new_string` pair, large enough surrounding context to be unique).

**Forbidden**: any tool that rewrites whole `dpad_*`, `activators`, or `bindings` blocks programmatically.

### 2. Always validate before deploying

After any edit, run all four checks:

```bash
# Bracket balance
awk '{for(i=1;i<=length($0);i++){c=substr($0,i,1); if(c=="{")o++; else if(c=="}")cl++}} \
  END{print "open:",o,"close:",cl,"diff:",o-cl}' <file.vdf>

# Group count (compare to last working version)
grep -c '"group"' <file.vdf>

# Binding count (compare to last working version)
grep -c '"binding"' <file.vdf>

# Line count and bytes (sanity)
wc -lc <file.vdf>
```

A working layout for Moonlight + Deck is ~3550 lines / ~63 KB / 56 groups / ~296 bindings. **A drop of more than ~5 % in any of those is a regression**, even if brackets balance.

### 3. Diff against the last-known-good before saving

```bash
git diff configs/steam-controller/<file.vdf>
```

If the diff is more than ~30 lines for a chord change, you've probably touched too much. Revert and try again with a smaller edit.

### 4. The `url` field controls Steam Deck visibility

Three valid schemes for personal layouts:

| Scheme | Meaning | When |
| ------ | ------- | ---- |
| `autosave:///home/deck/.local/share/Steam/.../controller_neptune.vdf` | The active autosave | Auto-managed by Steam — don't ship this |
| `usercloud://moonlight/<name>_0` | Personal save | What we deploy via SSH |
| `workshop://<id>` | Community/workshop | Set by Steam when you upload |

When deploying via SSH overwrite of a Personal save:

1. Patch `url` to `usercloud://moonlight/<name>_0` matching the **filename** (`<name>_0.vdf`).
2. Keep `export_type = personal_local`.
3. Use the existing `Timestamp` and `controller_caps` — don't fabricate.

### 5. Always deploy alongside, never overwrite-in-place

When testing a new version, scp it under a **new filename** (e.g. `og v18.4.3_0.vdf`) and let Steam offer it in Personal. The previous working version stays available as a fallback. If the new one fails to load, you can re-apply the old one from the UI in seconds — no SSH, no panic.

### 6. Steam Input sync ≠ Steam Cloud toggle

If a deleted layout keeps coming back after Steam restart, the cause is the **Steam Input sync layer**, not Steam Cloud. They are separate (Valve bug #7801). The Cloud toggle in the client UI does not stop Steam Input from re-syncing controller configs from Valve's servers.

Recovery sequence when ghost layouts return:

1. Quit Steam fully.
2. Remove the local cache:
   - `~/.steam/steam/userdata/<userid>/241100/remotecache.vdf`
   - `~/.steam/steam/steamapps/workshop/appworkshop_241100.acf`
   - `~/.steam/steam/userdata/<userid>/ugcmsgcache/` (all files)
3. Sign out of Steam. Sign back in.
4. The sync layer rebuilds from scratch. Ghost workshop refs should be gone.

If you see "hidden" errors when trying to delete a workshop item from a browser — that's a server-side flag the user can't clear. The signout/signin is the only practical workaround.

### 7. Personal tab disappearing

If Personal/Templates show empty after applying a new layout:

1. Check the file is still on disk: `ls "/home/deck/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight/"`
2. If the file is there but the tab is empty, signout/signin Steam (forces sync layer rebuild).
3. If it persists, the VDF is structurally invalid — see Rule #2 (validate).

### 8. Always commit + back up before changing

The repo's session-cleanup rule already mandates backing up controller and Nexus configs after every change. Treat the **opposite direction** as equally important: pull a fresh `moonlight-active.vdf` from the deck **before** starting any edit, in case the working state on disk is newer than what's in the repo.

```bash
scp steam-deck:'~/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight/controller_neptune.vdf' \
  configs/steam-controller/moonlight-active.vdf
```

### 9. Bump only what you understand

- `revision` — increment by 1+ on each save. Steam uses it to dedupe.
- `title` — human-visible name. Bump on every release (e.g. v18.4.3).
- `Timestamp` — leave alone unless you know why.
- `controller_caps` — leave alone. Pinning to wrong caps invalidates the layout.

### 10. Mirror DLL + VDF version in the same commit

Mystic Clicker DLL and the controller VDF together form one product. When a chord is added or moved:

1. Update the VDF (chord routing).
2. Update the DLL `RegisterWithString` defaults so first-run users get matching keys.
3. Bump both versions in the same commit, with `(VDF vN.M.P / DLL vA.B.C)` in the title.

The repo's GitHub Actions builds the DLL on push — so the order matters: commit + push **before** asking the user to pull on Bazzite.

### 11. Check Nexus for chord collisions before picking a key

The `key_press` chord emitted by Steam Input is delivered to **everything that's listening** — GW2 itself, Mystic Clicker, and any other Nexus addon. Before picking a chord for a new combo, verify nothing else is bound to the same key+modifier set.

```bash
python3 - <<'PY'
import json
with open('configs/gw2-keybinds/nexus-inputbinds.json') as f:
    binds = json.load(f)
# Replace 87 with the scancode you're considering. F1=58 .. F12=88, F11=87.
target_code = 87
for b in binds:
    if b.get('Code') == target_code:
        mods = [m for m in ('Ctrl','Shift','Alt') if b.get(m)]
        print('+'.join(mods + [f"K{target_code}"]).ljust(25), b['Identifier'])
PY
```

A live example: when wiring `MERCHANT_COMBO` to `Ctrl+F11`, the script revealed `KB_CRAFTY_TOGGLE` was already bound there — so pressing the chord would fire both. Switched Merchant to `Alt+F11`, which was unbound. Cost: 30 seconds. Cost of skipping the check: a chord that silently does the wrong thing.

Also worth double-checking against GW2's own bindings — `F11` alone, `F12` alone, and `Esc` are GW2 defaults (Game Options, Wiki, Menu) regardless of what Nexus does. Choose chord keys whose **bare** form GW2 ignores, or whose **modified** form GW2's strict-modifier-match suppresses. When in doubt, prefer `Alt+Fn` or `Ctrl+Alt+Fn` over plain `Shift+Fn` — Shift is the modifier most likely to collide with a typing key in GW2.

---

## Recovery Playbook

If you've broken a deployed layout:

1. **Don't panic, don't overwrite the file again.** The damage is on the Deck side.
2. Check how big it is:

   ```bash
   ssh steam-deck 'ls -la "/home/deck/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config/moonlight/"'
   ```

3. If your new file is dramatically smaller than the working baseline, it was bulk-rewritten. Revert from the repo:

   ```bash
   git show <last-good-commit>:configs/steam-controller/<file.vdf> > /tmp/recovery.vdf
   # patch url to usercloud://moonlight/<name>_0
   # scp to the deck under a NEW name (don't overwrite the broken one)
   ```

4. In the Deck UI, apply the recovery layout. The broken one stays in the list as evidence — delete it later from the UI once recovery is confirmed.
5. **Do not `git push --force` or rewrite history** — the broken file in the repo is part of the audit trail. Add a `fix(...)` commit on top instead, the way `c278737` did for v18.4.3.

---

## Reference: the v18.4 incident

- **What happened**: a Python regex was used to rewrite `dpad_south` and `dpad_east` blocks inside the template VDF (now `configs/steam-controller/moonlight-gw2-og-template.vdf`, formerly named `moonlight-gw2-og-v16.1.vdf` until 2026-04-26 when the v16.1 in the name became misleading and was dropped) to add the Merchant chord and rearrange Friend / Waypoint / LeaveParty.
- **What broke**: the regex's block boundary was greedy and consumed `Long_Press` / `Double_Press` activator subgroups across multiple groups, dropping ~25 of them. File went 63 121 → 37 170 bytes (3543 → 1768 lines, 296 → 158 bindings, 56 → 31 effective groups).
- **What Steam did**: silently dropped the layout. Personal tab empty. Templates empty. No error.
- **What rescue took**: signout/signin, multiple workshop-cache wipes, fighting "hidden" errors on workshop items, eventually rolling back to v18.3 and SSH-overwriting the Personal save with the v18.3 base.
- **What fixed it for v18.4.3**: started from the v18.3 base on disk (3543 lines, 56 groups, 296 bindings), applied two `Edit`-tool surgical replacements for the chord changes, validated bracket balance + group/binding counts, deployed under a new filename. Total diff: ~70 lines.

The cost of the bulk rewrite was a multi-hour outage that nearly required a Steam Input full reset. The cost of the surgical edit was 5 minutes. **Always pay 5 minutes.**
