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

```text
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
  venv (`~/.local/share/gw2-claude/venv`) and pip-installs `anthropic`,
  `pillow`, `piper-tts`. bazzite immutability blocks system packages, not user
  venvs — venv survives OS updates; re-run the script to refresh the SDK.
- **`configs/bazzite/gw2-claude-daemon.service`** — `systemd --user` unit, mirrors
  `gw2-ocr-daemon.service`. Lives at `~/.config/systemd/user/` on bazzite.
- **Config**: `~/.config/gw2-claude/config.env` (mode 600) — `ANTHROPIC_API_KEY`
  (required), `GW2_CLAUDE_MODEL`, and the TTS keys (`GW2_CLAUDE_TTS`,
  `GW2_CLAUDE_TTS_ENGINE`, `GW2_CLAUDE_TTS_GAIN_DB`, `GW2_CLAUDE_VOICE`,
  `ELEVENLABS_API_KEY`, `ELEVENLABS_VOICE_ID`, `ELEVENLABS_MODEL`). Not in the
  repo — secret. The
  ElevenLabs key lives in 1Password at
  `op://Home/ElevenLabs API Key - GW2 Screen Reader/credential`.

## Protocol (separate `gw2-claude-*` namespace; coexists with tesseract daemon)

| File | Written by | Meaning |
| --- | --- | --- |
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

- **Phase 1 (done)**: daemon + setup + service in the repo and live on bazzite.
  `ANTHROPIC_API_KEY` deployed from `op://wednesday-pi/anthropic/api-key`, model
  `claude-haiku-4-5`, `gw2-claude-daemon.service` enabled. End-to-end verified.
- **Phase 2 (done — mystic-clicker v3.6.17)**: `claude-vision.{h,cpp}` in the
  addon. "Ask Claude" header in the capture window — Read Screen / Read Tooltip /
  List Items / Read Dialog buttons. Async state machine: capture on a detached
  worker thread (avoids deadlocking the render thread), `Poll()` per-frame.
  `CLAUDE_READ_SCREEN` keybind (unbound by default) — bind it to a controller
  long-hold for no-keyboard triggering. Built via GitHub Actions.
- **Phase 2.5 (done)**: text-to-speech — the daemon speaks Claude's answer
  aloud; audio rides the Sunshine stream back to the player.
  `/tmp/gw2-claude-stop` kills playback, `/tmp/gw2-claude-speaking` marks it in
  progress. Two engines: local **Piper** (free, offline) and **ElevenLabs**
  cloud TTS — far more natural; used when `ELEVENLABS_API_KEY` is set,
  auto-falls back to Piper. Live voice: ElevenLabs "Lily" (British female). The
  daemon SYSTEM_PROMPT carries a light/playful reading tone.
- **Phase 2.6 (done — mystic-clicker v3.6.21–22)**: cursor-anchored read
  ("Read at Cursor" captures a 960×620 box at the mouse, not the whole frame);
  Esc-to-stop; read/stop toggle on one control; and a **headless** trigger —
  CLAUDE_READ_SCREEN no longer opens the capture panel, the answer just comes
  back as speech.
- **Phase 4 (done — Mystic AI module, 2026-05-18)**: the addon side spun out of
  Mystic Clicker into its own Nexus addon, **Mystic AI** (`modules/mystic-ai/`,
  `mystic-ai.dll`, signature `-54323`). The trigger is redesigned as a
  freeze-frame **drag-select** capture — a hotkey freezes the frame, the player
  drags a box over the text, the crop is sent to the daemon, the answer is
  shown in a panel anchored to the box and read aloud. A **Book** button saves
  the selection as a static region the `MYSTIC_AI_READ_BOOK` keybind re-reads
  with no drag. Keybinds `MYSTIC_AI_CAPTURE` / `MYSTIC_AI_READ_BOOK` (unbound).
  Mystic Clicker 3.6.25 dropped `claude-vision.{h,cpp}`, the Ask-Claude UI and
  `CLAUDE_READ_SCREEN`.
- **Phase 5 (done — Mystic AI 1.1.0, 2026-05-18)**: side-panel action buttons.
  After a drag capture the box-anchored panel has **Check TP Prices**, **Add to
  Wiki**, **Research**, **Copy Text** and **Read**. The addon prefixes the
  daemon `.prompt` with `@action:<name>`; `gw2-claude-daemon.py` dispatches to
  `trading-post` (GW2 API price; item name→ID via gw2tp's cached bulk list),
  `wiki-fav` (GW2 wiki search API → `NexusGameWiki/library.json` favourites),
  and `research` (Claude + the `web_search` tool, model
  `GW2_CLAUDE_RESEARCH_MODEL`, default `claude-sonnet-4-6`). **Copy Text** is
  local — `copytext.cpp` OCRs the crop via the tesseract daemon and sets the
  Windows clipboard, no Claude call.
- **Phase 3 (later, opt-in, ToS-gated)**: closed-loop — Claude returns a
  structured action, the addon executes it. ArenaNet ToS risk lives here; keep it
  a separate explicit toggle, off by default.

## Controller trigger

Mystic AI registers `MYSTIC_AI_CAPTURE` (freeze-frame drag-select read) and
`MYSTIC_AI_READ_BOOK` (static book-region read), both unbound. Bind them in
Nexus, then in the Steam Input config add activators on spare buttons that send
those keys — `MYSTIC_AI_READ_BOOK` suits a double-press for hands-free book
reading. Pressing the bound key again, or Esc, cancels a capture / stops a read
or playback. (Mystic Clicker's old `CLAUDE_READ_SCREEN` keybind is gone.)

## ArenaNet ToS

Read-only/advisory (Phases 1–2) is low risk. A closed see→decide→act loop
(Phase 3) is what anti-bot policy targets — account-ban risk is real. Keep the
tiers separate and opt-in.
