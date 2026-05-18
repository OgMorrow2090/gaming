---
name: GW2 Claude-vision daemon — read/understand the game screen via the Claude API
description: gw2-claude-daemon.py sends captured GW2 frames to the Claude API (vision) for OCR that both reads and understands the screen. Phase 1 of Claude/GW2 integration; companion to the tesseract OCR daemon.
type: reference
---

# GW2 Claude-vision integration

Goal: let Claude read and *understand* on-screen GW2 text — item lists, tooltips,
trading-post listings, dialog state — not just transcribe it. Phase 1 of a larger
"Claude in GW2" effort. Interaction (Claude → action) is explicitly later phases.

## Why this is cheap to build

The hard problem — getting a GW2 frame out of the DXVK+gamescope pipeline and out
of the Steam pressure-vessel sandbox — was already solved for tesseract OCR. See
`d3d11-back-buffer-capture-vs-gdi.md` (D3D11 swap-chain readback via Nexus) and
`gw2-ocr-daemon.sh` (the `/tmp` file bridge). Phase 1 reuses 100% of that.

## Architecture (Phase 1 — read-only)

```
GW2 (Nexus addon)                bazzite host (outside sandbox)
  capture frame  ──BMP/PNG──▶  /tmp/gw2-claude-input-<TS>.*
                                       │  (bind-mounted into sandbox)
                                       ▼
                            gw2-claude-daemon.py  ──▶ Claude API (vision)
                                       │
  read answer  ◀──txt────  /tmp/gw2-claude-output-<TS>.txt
```

- **`scripts/gw2-claude-daemon.py`** — host-side worker. Polls `/tmp` for
  `gw2-claude-input-*.ready` markers, sends the frame to the Claude API with
  vision, writes the answer back. Also `--analyze <image> [question]` for
  standalone testing with no addon involved.
- **`scripts/gw2-claude-setup.sh`** — one-time setup. Creates a per-user Python
  venv (`~/.local/share/gw2-claude/venv`) and pip-installs `anthropic` + `pillow`.
  bazzite immutability blocks system packages, not user venvs — venv survives OS
  updates; re-run the script to refresh the SDK.
- **`configs/bazzite/gw2-claude-daemon.service`** — `systemd --user` unit, mirrors
  `gw2-ocr-daemon.service`. Lives at `~/.config/systemd/user/` on bazzite.
- **Config**: `~/.config/gw2-claude/config.env` (mode 600) — `ANTHROPIC_API_KEY`
  (required) + `GW2_CLAUDE_MODEL` (optional). Not in the repo — secret.

## Protocol (separate `gw2-claude-*` namespace; coexists with tesseract daemon)

| File | Written by | Meaning |
|---|---|---|
| `/tmp/gw2-claude-input-<TS>.png` (or jpg/bmp/webp) | addon | captured frame |
| `/tmp/gw2-claude-input-<TS>.prompt` | addon (optional) | what to ask Claude |
| `/tmp/gw2-claude-input-<TS>.ready` | addon | atomic "frame complete" trigger |
| `/tmp/gw2-claude-output-<TS>.txt` | daemon | Claude's answer |
| `/tmp/gw2-claude-done-<TS>` | daemon | completion marker (written last) |

The daemon processes `.ready` (never the image directly) to avoid OCR-ing a
half-written frame — same reasoning as the tesseract daemon.

## tesseract vs Claude — hybrid, not replacement

Keep `gw2-ocr-daemon.sh` (tesseract): free, offline, fast, fine for high-frequency
cheap reads. Use the Claude daemon for "understand this screen" and hard reads
(stylised fonts, item lists, tooltip comprehension, low tesseract confidence).

## Image notes

Claude vision accepts JPEG/PNG/GIF/WebP — **not BMP**. The addon's D3D11 readback
currently saves BMP; the daemon converts BMP→PNG via Pillow. Opus 4.7 high-res
caps at 2576px long edge — larger frames are downscaled.

## Status / next phases

- **Phase 1 (done)**: daemon + setup + service in the repo and on bazzite. Works
  standalone via `--analyze` today. Needs `ANTHROPIC_API_KEY` in config.env, then
  `systemctl --user enable --now gw2-claude-daemon.service`.
- **Phase 2 (needs addon recompile)**: Mystic Clicker writes `gw2-claude-*`
  requests + surfaces a "read this / ask Claude" action in-window. C++ change,
  built via the GitHub Actions pipeline.
- **Phase 3 (later, opt-in, ToS-gated)**: closed-loop — Claude returns a
  structured action, the addon executes it. ArenaNet ToS risk lives here; keep it
  a separate explicit toggle, off by default.

## ArenaNet ToS

Read-only/advisory (Phases 1–2) is low risk. A closed see→decide→act loop
(Phase 3) is what anti-bot policy targets — account-ban risk is real. Keep the
tiers separate and opt-in.
