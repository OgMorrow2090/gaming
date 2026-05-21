---
name: Nexus loads mystic-ai.dll from addons/ root if a stray sits there — shadows the addons/MysticAI/ copy
description: Nexus scans both addons/ root and one-level subdirs for DLLs. If a stale mystic-ai.dll sits directly in addons/, Nexus loads THAT copy and ignores addons/MysticAI/mystic-ai.dll silently. Every "deploy this fixes it" then has no effect because the live DLL in GW2's memory is the stray, not the deployed version.
metadata:
  type: project
---

<!-- markdownlint-disable MD041 MD032 -->

## The trap

Mystic AI lives in `addons/MysticAI/mystic-ai.dll`, and the deploy script
puts the DLL there. But Nexus also scans `addons/` root for DLLs. If a
stray `addons/mystic-ai.dll` exists (from a legacy deploy convention,
manual copy, or accidental drag), **Nexus loads the stray** and the
subdir copy is silently ignored.

There is no warning. The Nexus addon list shows one entry. The version
string reports whatever the stray is. Every subsequent deploy looks like
it succeeded (file hash on disk matches the artifact) but GW2 is running
the stray in memory.

Confirmed 2026-05-21: a 1.1.13-era stray (mtime May 20 13:57) was loaded
by Nexus while the deploy script faithfully wrote 1.1.14 → 1.1.15 → 1.1.16
→ 1.1.17 into `addons/MysticAI/`. None of the fixes ever took effect.
The smoking gun:

```bash
PID=$(pgrep -x Gw2-64.exe)
loaded_inode=$(grep -i mystic-ai /proc/$PID/maps | head -1 | awk '{print $5}')
ondisk_inode=$(stat -c %i "$ADDONS/MysticAI/mystic-ai.dll")
[ "$loaded_inode" = "$ondisk_inode" ] || echo "GW2 has an OLDER DLL loaded than what is on disk"
find $ADDONS -inum $loaded_inode    # tells you WHERE the stray lives
```

## Detection

If "I deployed the fix but the bug is still there", check inode parity
between the on-disk DLL and the one GW2 has mmapped. If they differ,
something else is loading. `find -inum <loaded>` finds it.

## Prevention

The deploy script (`scripts/deploy-mystic-ai-dll.sh`, 2026-05-21+) now
moves any `addons/mystic-ai.dll` aside as `.stray-<TS>` before writing
the new DLL into `addons/MysticAI/`. Self-healing on every deploy.

The Mystic Clicker deploy already only writes to `addons/mystic-clicker.dll`
(no subdir), so this trap doesn't apply there. But if Mystic Clicker ever
gets its own subdir, mirror the stray-check.

## Why Nexus does this

Nexus's load policy is "any `*.dll` in `addons/` or `addons/<name>/` is a
candidate". When two DLLs export the same Nexus signature (-54323 for
Mystic AI), the first one loaded wins. Load order depends on
`readdir()` scan order — non-deterministic across filesystems / kernel
versions. So even moving the stray temporarily can produce different
behaviour across reboots; the fix is to actually delete (or rename out
of `.dll` extension) the stray.

## Related

- [[mystic-clicker-build-and-deploy]] — the auto-deploy watcher pattern
  that pre-dated the MysticAI/ subdir convention; was the original
  source of the stray at `addons/mystic-ai.dll`.
- [[feedback_repo_first_then_deploy]] — same family ("deploy says
  success but the host doesn't reflect the change"); this one was
  invisible to the repo-vs-live diff because the file on disk did match.
