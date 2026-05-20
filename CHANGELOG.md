# Changelog

<!-- markdownlint-disable MD024 -->

All notable changes to Guild Wars 2 Addons will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2026-05-20] — Mystic Clicker 3.6.29 — bigger capture thumbnails

### Changed

- **Capture thumbnails doubled from 128×128 to 256×256.** The display scale
  in the hover tooltip stays at 2×, so the visible image is now 512 px
  instead of 256 px — easier to identify the dialog at a glance. Disk cost
  per thumbnail rises from ~50 KB to ~192 KB; per-slot, per-resolution. Old
  128 px thumbnails keep working but display at their original 256 px until
  re-captured.

## [2026-05-20] — Mystic Clicker 3.6.28 — capture-slot thumbnails on hover

### Added

- **Each capture also grabs a 128×128 thumbnail of the click point**, saved as
  `addons/MysticClicker/thumb-<slot>-<W>x<H>.bmp` per resolution. Hovering a
  captured slot in the capture window now shows the thumbnail in the tooltip
  with a small red `+` at the centre, marking where the click lands inside the
  grab. Helps remember which "Accept N" is which when 12+ slots share the same
  name. Slots without a thumbnail (existing configs, never captured) fall back
  to the plain text tooltip — re-capture once to populate.

## [2026-05-19] — Mystic AI 1.1.13 — remove the standalone TP-region feature

### Removed

- **The standalone "TP region" feature is gone.** The drag-capture panel no
  longer shows the "Save TP region" button, the Settings block no longer shows
  the saved TP region, and the `MYSTIC_AI_TP_REGION` keybind is no longer
  registered. The dedicated TP-region read was superseded by R1+Menu drag-
  capture, which reads whatever item the cursor is on with the full action
  panel — so the cursor-anchored region and its keybind were redundant. The
  Book region (drag-select once, re-read with the Read Book keybind) is
  unchanged. Internally this also drops `MODE_BOOKPANEL`, `OpenPanelNotice`,
  the on-screen notice, the open-guard, and the `keepCrop` / `TakeRegionCrop`
  path in the ClaudeVision client.

## [2026-05-19] — Mystic AI 1.1.12 — Research no longer times out or auto-reads

### Fixed

- **Research no longer times out.** The Research action runs a web-search-
  backed write-up that legitimately takes far longer than a plain screen read,
  but the addon gave every request the same 45-second ceiling — so a Research
  call still in progress showed "timed out - is gw2-claude-daemon.service
  running" instead of its result. Research now has its own 180-second ceiling;
  every other action keeps the short 45-second one, so a genuinely-dead daemon
  is still reported quickly.

### Changed

- **Research is silent now — it no longer reads itself aloud.** Like the
  overview, a Research write-up shows on the panel and is voiced only when you
  press Read. (daemon-side `gw2-claude-daemon.py`.)
- **The overview reliably builds the structured item panel** (daemon-side).
  Long, descriptive item tooltips — containers, trophies, currencies — were
  sometimes returned as a plain transcription, with the stack count and vendor
  value mashed into the prose, instead of the name + "Used for" + Buy/Sell/
  Vendor coin panel. The overview instruction now treats any inventory item as
  an ITEM by default and ignores the stack count / "in Bank" line / coin value
  shown on the tooltip.

## [2026-05-19] — Mystic AI 1.1.11 — full action panel on the TP-region read

### Changed

- **The TP Region keybind opens the full action panel now.** Pressing it used
  to show the overview — item name, "Used for" line, Buy/Sell/Vendor coin rows
  — but with no action buttons, so there was no way to Research or Wiki-
  favourite the item without re-capturing it by hand. The TP-region read now
  keeps its captured region as the crop (`ClaudeVision::RequestRegion` gained a
  `keepCrop` option that hands the pixels back via `TakeRegionCrop`), and the
  panel shows the same **Read / Wiki / Research / Copy / Legendary** buttons as
  a drag-select review. Book and Save TP region stay drag-select-only — they
  set a region from a fresh selection, which a region re-read is not.

## [2026-05-19] — Mystic AI 1.1.10 — cursor-anchored TP region, global Esc fix

### Changed

- **The saved TP region is cursor-anchored now.** It used to capture a fixed
  screen rectangle, so the TP-region keybind only worked when an item happened
  to sit in that exact spot — useless for inventory, where items are never in
  the same place twice. The TP region is now stored as an offset from the
  cursor: the Save TP region button records where the drag box sits relative to
  where the cursor was pointing when the frame froze, and the TP-region keybind
  adds the live cursor position — so it reads whatever item the cursor is on
  now. Re-save your TP region after updating (point at an item, Capture,
  drag-select its tooltip, then Save TP region). The book region is unchanged —
  GW2's book panel is genuinely fixed, so it stays an absolute rectangle.

### Fixed

- **Mystic AI no longer swallows the Esc key while idle.** Its WndProc hook
  consumes Esc so the results panel / freeze overlay close without GW2 also
  closing the book or inventory behind them. But the hook also claimed Esc
  whenever a book auto-advance watch was armed or speech was playing — both of
  which run with no Mystic AI window on screen. Once a book had been read the
  watch stayed armed, so Esc was dead for GW2 and every other addon (e.g. the
  Event Timers addon's Esc-to-close) until the watch was toggled off. The hook
  now claims Esc only while a Mystic AI window is actually visible. Stop a book
  auto-advance with a second Read-Book press, as before.

## [2026-05-19] — Mystic AI 1.1.9 — book reads audio-only, TP-region panel fix

### Changed

- **Book reads are audio-only now — no on-screen panel.** The Read-Book keybind
  used to open the results panel and show the transcribed page text. The daemon
  already reads the page aloud, so the panel just repeated it. Pressing
  Read-Book now simply captures the saved book region and sends the read; the
  daemon speaks it, and nothing opens on screen. The keybind toggle (a second
  press stops the watch and speech), Esc, and the page-turn auto-advance all
  still work — the auto-advance is simpler now, with no panel ever over the
  region. With no book region saved the keybind shows a Nexus toast instead of
  a panel. Drag-select Capture and the TP-region read still use the panel.

### Fixed

- **The TP Region keybind panel reliably shows and stays now.** Pressing it sent
  the request and the daemon processed it, but the results panel only flickered
  on screen and never stayed — the player never saw the overview. A chord bound
  to the TP-region keybind can have Steam Input + Nexus deliver the chord's
  *other* keybind a frame or two later; the per-identifier debounce only
  suppresses a repeat of the same identifier, so that stray command drained
  into the generic "any keybind cancels the active mode" rule and closed the
  panel that had just opened. The TP-region read now arms a brief open guard so
  a paired chord press in the frames right after cannot close its panel. The
  panel opens with the overview — item name, "Used for" line, Buy/Sell/Vendor
  coin rows — and keeps it.

## [2026-05-19] — Mystic AI 1.1.8 — TP-region + book auto-advance fixes

### Fixed

- **The TP Region keybind reliably reads now.** Pressing it used to do nothing
  visible — a brief flicker, no Claude request, no panel — whenever a results
  panel was already open or a previous read's speech was still playing: the
  press was swallowed by the generic "any keybind cancels the current mode"
  rule. The TP-region keybind is a dedicated re-read action, so it is now
  handled before that rule and always (re)starts a fresh `@action:overview` of
  the saved rectangle — opening the panel, capturing the region and showing the
  reading progress, mirroring the Read-Book keybind. Any in-flight read or
  speech is stopped first so the new read does not talk over it. The
  no-TP-region on-screen notice is reliable too: the panel notice now wins over
  any stale result left by an earlier read.
- **Book auto-advance no longer re-fires reads in a loop.** A single book read
  could trigger two or three more reads a few seconds apart — speech going
  start / stop / start / stop. The page-turn watch was sampling the book region
  while Mystic AI's own results panel sat over it; the panel's changing content
  (status text → answer text) polluted every checksum and read as a "page
  turn". The watch now samples **only while the panel is closed and Mystic AI
  is fully idle** — no read in flight, no speech — so the back-buffer grab sees
  only the game. It also fires a re-read only on a *changed-and-now-stable*
  page: a sample that differs from the last-read page is held as a candidate
  and confirmed by the next sample before the read fires, so transient cursor /
  animation flicker is rejected. After firing it re-baselines to the new page.
  Closing the panel with its X leaves an armed watch running (so it can sample
  cleanly); Esc, a fresh keybind press or a new drag-select still stop it.

## [2026-05-19] — Mystic AI 1.1.7 — auto-sizing panel

### Changed

- **The results panel now sizes itself.** It auto-grows in height to fit
  whatever it shows — long Research write-ups no longer come up cramped, and
  there is nothing to drag taller. Growth is capped at 80% of the game window;
  past that the panel holds its height and the text area scrolls. The Settings
  header still composes with this — opening it just grows the panel to fit.
- **The redundant "Close Panel" button is gone.** The panel's window close (X)
  and the Esc key already close it, so the extra button has been removed.

### Fixed

- **The TP Region keybind always shows feedback now.** Pressing it with no TP
  region saved used to do nothing visible (only a spoken hint, which may be
  inaudible). It now opens the panel with a clear on-screen instruction —
  "No TP region saved - drag-select an item, then press 'Save TP region'." —
  and with a region saved it opens the panel and shows the overview progress as
  before. The Read-Book keybind got the same treatment for the no-book-region
  case, so neither region keybind can ever silently do nothing.

## [2026-05-19] — Mystic AI 1.1.6 — book auto-advance, favourites, colour research

### Added

- **Book pages auto-advance.** The Read-Book keybind now toggles a watch: once
  a page is read and narrated, Mystic AI watches the saved book region and
  automatically reads the **next page** when you turn it — no extra key press.
  Press Read-Book again, press Esc, or start a new drag-select to stop the
  watch. Page turns are detected by sampling the region every couple of seconds
  on a worker thread and comparing a tiny-thumbnail brightness checksum.
- **A Legendary button.** Sits beside Wiki in the action row and queues the
  selected item for the **CraftyLegend** favourites.
- **Two new keybinds** (unbound by default): **Read** voices the current panel
  content just like the Read button, and **TP Region** re-reads a saved static
  item rectangle as a silent overview — the item, its "Used for" line and its
  Trading Post / vendor coin rows. A new **Save TP region** button stores the
  current selection as that rectangle (per-resolution, like the book region).

### Changed

- **Favourites now survive a relaunch.** NexusGameWiki (and CraftyLegend)
  rewrite their library files from memory when the game closes, so a live
  append while GW2 ran was wiped. The daemon now **queues** wiki and Legendary
  favourites and flushes them into the addon JSON files only while GW2 is shut
  — so the addons read them fresh on the next launch. The Wiki / Legendary
  buttons report a queued confirmation on the panel (not spoken).
- **Research is colour-coded.** The Research write-up now comes back in labelled
  sections — About, How to get, Uses, Recipes, Prices, Tips — and the panel
  renders each section with its own coloured heading so it is easy to scan.
- **The overview now says what an item is used for.** A drag-select (or TP
  Region) overview adds a green "Used for:" line — key recipes, crafting and
  collections — under the item description, and includes it in the spoken read.

## [2026-05-19] — Mystic AI 1.1.5 — overview panel + on-demand reading

### Added

- **Drag-select now shows a silent on-screen overview** instead of reading the
  selection aloud. For an item it lists the item name, a one-line description
  of what it is, its Trading Post **Buy** / **Sell** prices and its **Vendor**
  value — all rendered in the panel with no narration. A selection that is not
  an item falls back to a plain on-screen transcription.
- **Reading aloud is now on-demand** — the **Read** button voices the panel's
  content (the item's spoken description, or the transcription) only when you
  ask, leaving the panel on screen while it speaks.

### Changed

- **Action buttons moved above the text.** The Read / Wiki / Research / Copy /
  Book row is now drawn at the top of the panel, so the mouse lands on the
  buttons — right where the drag ended — rather than on the result text.
- **Copy now extracts just the item name.** Instead of OCR-ing the whole
  selection, the Copy button asks the daemon for the item's name and puts that
  on the clipboard, ready to paste into GW2's item-destroy confirmation box.
- **The standalone TP button is gone** — Trading Post prices are part of the
  overview panel now.
- **Opening Settings expands the panel** so the controls show without
  scrolling, and collapses it back when closed. A normal drag-resize is still
  saved as before.
- **The waiting status no longer shows a counting number** — it read like a
  countdown. It is now a plain "Reading the screen…" with animated dots.
- **Cut-off-text narration removed** — the drag-select prompt no longer asks
  Claude to flag text clipped at an edge.

## [2026-05-19] — gw2-claude — TP lookup via gw2bltc.com + cleaner reading

### Fixed

- **Trading Post lookups failed for every item.** gw2tp's bulk name list
  (`api.gw2tp.com`) is dead — it no longer resolves — so the daemon had no
  name→ID map and reported every item as "not on the Trading Post". Item
  names are now resolved via a **gw2bltc.com** search (the same source the
  itinyk portal uses), whose substring search also handles partial names
  ("Powerful Blood" → "Vial of Powerful Blood"). Prices and vendor value
  still come from the official GW2 API.

### Changed

- **`SYSTEM_PROMPT`** — the screen reader no longer narrates the screenshot
  ("this is a tooltip showing…") or flags cut-off / unclear text; it reads the
  legible text only, plainly, with no commentary or asides.
- **`RESEARCH_SYSTEM`** — the Research action now produces a genuinely in-depth
  write-up (acquisition, uses, builds, economy, tips), not a short blurb.

## [2026-05-19] — Mystic Clicker 3.6.27 — colour-coded capture categories

### Changed

- **The capture panel's category headers are now colour-coded** — each of the
  eleven categories (Mystic Forge, Trading Post, Bank & Inventory, …) gets a
  distinct tint on its collapsing header, and its idle capture buttons carry a
  faint version of the same colour. The long target list is far easier to scan
  and the categories tell apart at a glance. Countdown / confirmation states
  are unchanged.
- Version 3.6.26 → **3.6.27** (`entry.cpp`).

## [2026-05-19] — Mystic AI 1.1.4 — Trading Post coin panel

### Added

- **The TP action now shows a proper price panel** — the item name, then
  **Buy**, **Sell** and **Vendor** rows, each rendered with gold / silver /
  copper coin discs (the look of the portal GW2 pages) instead of one line of
  text. Coins scale with the Text-size slider.
- **Vendor value** — the panel's third row is the item's sell-to-vendor price,
  fetched from the GW2 item API (`/v2/items/{id}`). Items flagged `NoSell`
  show "not sellable"; a side of the Trading Post with no orders shows
  "no buy orders" / "no sell listings".

### Changed

- **`gw2-claude-daemon.py`** — `action_trading_post` now also fetches the item
  (vendor value, canonical name, `NoSell` flag) and returns a machine-readable
  `@TP|buy=..|sell=..|vendor=..|name=..` marker line ahead of the spoken prose.
  `clean_for_speech` strips that marker so TTS only reads the sentence. Lookup
  failures still return plain text (no marker) and display as a message.
- Version 1.1.3 → **1.1.4** (`entry.cpp`).

## [2026-05-19] — Mystic Clicker 3.6.26 — granular Text/Button/Opacity settings

### Changed

- **Settings** — the single "UI scale" slider in the capture window is replaced
  by three sliders: **Text size** and **Button size** (0.6–2.5), plus
  **Opacity** (0.2–1.0), matching the granular style of Mystic AI and Mystic
  Trading. Text size scales the window font; Button size scales the window,
  buttons, spacing and layout; Opacity sets the capture window background.
- Old `UIScale` configs migrate — the saved value seeds both Text size and
  Button size (clamped to 0.6–2.5).
- Version 3.6.25 → **3.6.26** (`entry.cpp`).

## [2026-05-19] — Mystic AI 1.1.3 — resizable panel + granular settings

### Changed

- **The panel is now a freely drag-resizable rectangle** — grab any edge to
  size it; the size is saved per resolution and the panel snaps to it on each
  capture. A long answer scrolls inside the fixed rectangle instead of growing
  the window into a tall narrow strip.
- **Settings** — the single "UI scale" slider is replaced by **Text size** and
  **Button size** sliders (0.6–2.5), matching Mystic Trading's granular style;
  opacity unchanged. Old `UIScale` configs migrate into both.
- Version 1.1.2 → **1.1.3** (`entry.cpp`).

## [2026-05-19] — TTS routing — book reads → ElevenLabs Lily, the rest → Piper

### Changed

- **`gw2-claude-daemon.py`** — under `GW2_CLAUDE_TTS_ENGINE=auto`, only **book
  reads** (the Read-Book keybind) use ElevenLabs "Lily"; drag-select reads,
  tooltips and the TP / Wiki / Research actions now use free Piper "Jenny".
  `run_daemon` flags a book read by the `BOOK_PROMPT` marker; `speak()` gained
  an `is_book` argument. `elevenlabs` / `piper` still force a single engine.
  Spends the ElevenLabs credit budget on long-form book reading and keeps quick
  functional reads free.

## [2026-05-19] — Piper fallback voice → Jenny (British female)

### Changed

- **Piper fallback TTS voice** is now `en_GB-jenny_dioco-medium` ("Jenny", a
  natural British-female voice) in place of `en_GB-northern_english_male-medium`.
  When ElevenLabs "Lily" is unavailable (quota / network), the local free
  fallback now sounds close to Lily rather than dropping to a male voice.
  - `scripts/gw2-claude-setup.sh` — downloads Jenny; `config.env` template
    `GW2_CLAUDE_VOICE` default set to Jenny.
  - `scripts/gw2-claude-daemon.py` — `_speak_piper` hard-coded default updated.
- Deployed to bazzite: Jenny model fetched into `~/.local/share/gw2-claude/
  voices/`, `config.env` switched, `gw2-claude-daemon` restarted.

## [2026-05-19] — Mystic AI 1.1.2 — button grid + Esc no longer leaks to GW2

### Changed

- **`overlay.cpp` — capture-panel action buttons** now lay out as a **3-wide,
  2-row grid** (Read / TP / Wiki, then Research / Copy / Book). The single row
  of six still clipped "Research" and "Copy"; each button now gets a third of
  the panel width, so every label fits.

### Fixed

- **Esc no longer closes the in-game book / inventory behind the panel.** Mystic
  AI polled Esc with `GetAsyncKeyState`, which doesn't consume the key — GW2 saw
  it too. New Nexus **WndProc hook** (`MysticAIWndProc`) swallows Esc while the
  panel/overlay is open (closing Mystic AI + stopping audio) and returns 0 so
  the key never reaches GW2. Idle, Esc passes through unchanged.
- Version 1.1.1 → **1.1.2**.

### Next

- Book auto-continue (read the next page on a page-turn) — deliberately a
  separate change: it needs a long-lived page-watch thread done with care.

## [2026-05-19] — Mystic AI 1.1.1 — capture-panel action buttons

### Changed

- **`modules/mystic-ai/overlay.cpp`** — the six capture-panel action buttons
  (Read / TP / Wiki / Research / Copy / Book) now lay out as one equal-width
  row sized to the panel. Previously the auto-width text buttons overflowed the
  440 px panel — Copy was half-clipped and **Book was entirely off-screen and
  unreachable**.
  - New `ActionButton` helper replaces `IconButton`: per-action tint (Read
    blue, TP gold, Wiki green, Research purple, Copy slate, Book brown, Stop
    red), fixed width, smaller label font (0.82×).
  - Still prefers the embedded GW2 icon when its texture is available.
- Version 1.1.0 → **1.1.1** (`entry.cpp`).

### Known issue

- The action buttons render as text, not icons — the embedded icon textures
  aren't loading (`Icons::Get` returns null). Separate fix.

## [2026-05-18] — Controller layout Og v21.0 — Mystic AI chords + Waypoint→Menu

### Changed

- **`configs/gw2-keybinds/controller_neptune.vdf`** — Triton controller layout
  (GW2 app 1284210 autosave) bumped v20.0 → **v21.0**, four surgical chord edits:
  - **Menu long-press**: `Alt+F10` "Claude Read" → `Shift+F9` **Waypoint Combo**
    (moved from R1+DPad-Down long-press).
  - **R1+Menu tap**: `Insert` "Toggle Language" → `Alt+F10` **Mystic AI Capture**.
  - **R1+Menu long-press**: added — `Shift+F10` **Mystic AI Read Book**.
  - **R1+DPad-Down long-press**: Waypoint Combo removed (relocated to Menu).
  - Toggle Language dropped (user's call); R1+DPad-Down single (Teleport to
    Friend) / double (Exit Instance) untouched.
- Validated per the VDF golden rules: 56 groups, 313 bindings, braces 908=908,
  all radial menus ≤16, 3498 lines (+1 vs v20.0); diff 14 ins / 13 del.

### Notes

- Pairs with the Nexus keybinds: `Alt+F10`=Capture, `Shift+F10`=Read Book,
  `Shift+F9`=Waypoint Combo are all bound in `nexus-inputbinds.json`.
- v21 is a Steam-Input autosave file — deploying it needs Steam stopped on
  bazzite during the file swap, then a Steam restart, or the shutdown
  write-back clobbers it.

## [2026-05-18] — Mystic AI keybinds assigned + deployed to bazzite

### Changed

- **`configs/gw2-keybinds/nexus-inputbinds.json`** — assigned the Mystic AI
  addon keybinds:
  - `MYSTIC_AI_CAPTURE` → **Alt+F10** (freeze-frame drag-select read)
  - `MYSTIC_AI_READ_BOOK` → **Shift+F10** (saved book-region read)
  - Removed the stale `CLAUDE_READ_SCREEN` entry (Mystic Clicker 3.6.25 dropped
    that keybind); Mystic AI Capture reuses its freed Alt+F10 slot.
- Keys collision-checked against `nexus-inputbinds.json` and `gamebinds.xml`:
  F9/F10 *bare* are taken (`MERCHANT_COMBO`, `COPY_ITEM_NAME`), but Alt+F10 and
  Shift+F10 are clear on both the Nexus and GW2 sides.

### Notes

- Deployed with `deploy-nexus-config.sh` to the bazzite `local` profile only —
  the Deck was asleep. The script's `gw2-appletv` / `gw2-deck` targets are the
  archived multi-install profiles (consolidated away 2026-05-12) and now error;
  the script should be trimmed to the live profile + Deck.
- R1+Menu controller routing is not included here: the live layout is a
  Steam Input autosave on the new `controller_triton` pad, edited in the Steam
  UI — outside the repo's neptune-era VDF pipeline.

## [2026-05-18] — Mystic AI Phase 2: side-panel actions (TP / Wiki / Research / Copy)

### Added

- **Mystic AI 1.1.0 — capture-panel action buttons.** After a drag-select
  capture, the box-anchored panel gains a row of GW2-icon buttons that act on
  the selection:
  - **Check TP Prices** — the daemon identifies the item and reports its
    Trading Post buy/sell price (GW2 API; item name → ID via gw2tp.com's
    bulk list, cached a day).
  - **Add to Wiki** — resolves the subject via the GW2 wiki search API and
    appends it to NexusGameWiki's `library.json` favourites (de-duplicated).
  - **Research** — a web-search-backed deep explanation (Claude + the
    server-side `web_search` tool; `GW2_CLAUDE_RESEARCH_MODEL`, default
    `claude-sonnet-4-6`).
  - **Copy Text** — OCRs the selection with the tesseract daemon and puts the
    text on the Windows clipboard, no AI — for pasting an item name into GW2's
    destroy-confirm box. `ocr.cpp` gained `OcrPixels()` (OCR a given crop);
    new `copytext.{h,cpp}`.
  - **Read** re-reads the selection aloud.
- **gw2-claude-daemon.py — `@action:` dispatch.** A request whose `.prompt`
  starts with `@action:<name>` routes to the `trading-post` / `wiki-fav` /
  `research` handler; no prefix is the unchanged plain read.

### Notes

- Web search must be enabled for the Anthropic org; if it isn't, Research falls
  back to a plain knowledge-based answer.
- NexusGameWiki may rewrite `library.json` from memory while running — a daemon
  favourite append is reliably picked up on the next GW2 launch.

## [2026-05-18] — Mystic AI: new addon (Phase 1), spun out of Mystic Clicker

### Added

- **Mystic AI** — a new Nexus addon (`modules/mystic-ai/`, `mystic-ai.dll`,
  signature `-54323`, v1.0.0). The Claude-vision screen reader, spun out of
  Mystic Clicker into its own addon. Phase 1:
  - **Freeze-frame drag-select capture** — the `MYSTIC_AI_CAPTURE` keybind
    freezes the current frame (macOS-screenshot style); drag a box over any
    text and Claude reads it aloud. Freezing *at key-press* keeps a hovered
    tooltip captured even after the mouse moves off it.
  - **Box-anchored panel** — the answer + action buttons appear right at the
    selection box, so the mouse barely moves.
  - **Static book region** — a **Book** button saves the selection as a fixed
    region; the `MYSTIC_AI_READ_BOOK` keybind re-reads it with no drag.
  - GW2-style icons embedded in the DLL as Win32 resources; a Nexus Quick
    Access shortcut; per-resolution settings (`mystic-ai-<W>x<H>.cfg` — UI
    scale, panel opacity, book region).
  - `MYSTIC_AI_CAPTURE` / `MYSTIC_AI_READ_BOOK` keybinds, unbound by default.
  - `mystic-ai.vcxproj` + solution entry + a CI build-matrix entry — GitHub
    Actions now builds all three addon DLLs.

### Changed

- **Mystic Clicker 3.6.25** — the Claude-vision feature is removed
  (`claude-vision.{h,cpp}`, the "Ask Claude" capture-window header, the
  `CLAUDE_READ_SCREEN` keybind). It now lives in Mystic AI; Mystic Clicker
  keeps the one-click hotkeys. `screen-capture` / `ocr` stay (still used by
  Mystic Clicker's own macros).

## [2026-05-18] — controller v20.0, Claude screen-reader voice + headless read

### Added

- **gw2-claude — ElevenLabs cloud TTS**: `gw2-claude-daemon.py` gained an engine
  selector — ElevenLabs (far more natural) when `ELEVENLABS_API_KEY` is set,
  else local Piper. No new Python dependency (stdlib `urllib`); MP3 → ffmpeg
  decode → pw-play. Live voice: ElevenLabs "Lily" (British female). The daemon
  SYSTEM_PROMPT carries a light/playful reading tone.
- **mystic-clicker v3.6.21 — cursor-anchored read**: "Read at Cursor" captures a
  960×620 box at the mouse cursor instead of the whole frame; Esc also stops a
  read/playback in progress.
- **controller v20.0**: "Home Instance Portal" added to the Utility Wheel
  (replacing "Community LFG" — the wheel stays at 16 slots; Steam Input silently
  rejects touch menus over 16 buttons).

### Changed

- **mystic-clicker v3.6.22 — headless AI read**: the `CLAUDE_READ_SCREEN`
  keybind no longer opens the capture panel — the read runs headless and the
  answer is spoken aloud. Stops the user hitting the position-capture 5s
  countdown when triggering the AI from the controller.

### Fixed

- **mystic-clicker v3.6.20 — Accept-slot naming drift**: drifted "Accept" combo
  labels renamed so the visible name matches the action across the addon UI and
  the controller layout (`General Accept 2` → `Accept 4`, etc.).

## [controller v19.7] — Fix v19.6 chord misfires (F12 → logout, Ctrl+Shift+X chord doesn't trigger addon)

### Fixed

- **Long_Press**: F12 was GW2's default Logout key — pressing fired the in-game logout dialog instead of reaching Nexus. Switched to **bare F10** (Code 68 in canonical InputBinds.json), unbound in GW2 native and in our addon stack.
- **Double_Press**: Ctrl+Shift+X via 3 separate `key_press` lines hit the [Steam Input multi-binding fires sequentially](memory/steam-input-multi-binding-fires-sequentially.md) issue — Item Detail Popups' chord detection saw discrete Ctrl, Shift, X presses instead of a held chord, so the popup never fired. Switched to **bare F11**. Bare F11 is unused (only `Alt+F11` = Merchant Combo exists). User must reassign the Item Detail Popups bind to bare F11 in Nexus → Settings → Keybinds.

### Pre-existing (unrelated to this change)

- ArcDPS log warnings observed: `client executable in memory does not match executable on disk` (hash check warning, fires every launch on every profile, benign), and `received incompatible imgui context, using standalone` (Nexus + ArcDPS ImGui version mismatch — addon falls back to standalone ImGui, cosmetic). Not introduced by v19.6/v19.7.

### Snapshot

`configs/steam-controller/moonlight-gw2-og-v19.7.vdf` — frozen working state.

## [controller v19.6] — L1+DPad-East: Copy Item Name (Long) + Item Detail Popup (Double)

### Added

- **L1 + DPad-East Long_Press** → emits `F12`, which is now the COPY_ITEM_NAME hotkey (set in `configs/gw2-keybinds/nexus-inputbinds.json`, Code 88, no modifiers). Triggers the Mystic Clicker chat-link → GW2 API → clipboard macro from the previous entry.
- **L1 + DPad-East Double_Press** → emits `Ctrl+Shift+X`, the default hotkey of the community-made `Item_Detail_Popups.dll` addon ([lorkano/item_detail_popups](https://github.com/lorkanoo/item_detail_popups)). Hover an item, double-tap dpad-east while holding L1, popup appears with wiki/GW2 API details. The addon registers its bind via Nexus default (not via persisted JSON entry), so sending the keystroke is sufficient even if InputBinds.json doesn't list the addon's identifier.

### Changed

- VDF title bumped v19.5 → v19.6.
- Both new activators added to L1 layer dpad group (id 30) — overrides what would otherwise fall through to the base preset's `Long_Press → Deposit & Sort`. Tap (Full_Press) of L1+DPad-East still emits the existing `S, U, M` sequence.
- COPY_ITEM_NAME canonical InputBinds entry: `Code: 0` → `Code: 88` (F12).

### Deploy

After GW2 is closed on every reachable target:

1. `./scripts/deploy-controller-vdf.sh` — pushes v19.6 template to all active layout files
2. `./scripts/deploy-nexus-config.sh` — pushes the F12 bind for COPY_ITEM_NAME to all 4 profiles
3. Restart GW2 (full quit + relaunch is the safe path for the new Mystic Clicker DLL with the COPY_ITEM_NAME bind)

### Validation

VDF brackets 937=937 balanced. JSON valid. Snapshot at `configs/steam-controller/moonlight-gw2-og-v19.6.vdf`.

## [unreleased] — Mystic Clicker: Copy Item Name macro

### Added

- **`COPY_ITEM_NAME` keybind** — hover an item in the GW2 inventory, press the hotkey, the item's display name lands on the Windows clipboard. Designed for the destroy-confirmation dialog ("Type the name of the item to destroy") so the user can paste rather than type long item names on a controller keyboard.
- New module `modules/mystic-clicker/item-name.cpp` (~340 lines). Flow on dispatch (detached thread): wait for chord modifiers to release → drain modifiers → save cursor + clipboard → Esc → Enter (open fresh empty chat input) → restore cursor → SendInput Ctrl+LeftClick at cursor → Ctrl+A → Ctrl+C → Esc (close chat without sending) → parse base64 chat link → 24-bit item ID → cache lookup → on miss, GW2 API GET `/v2/items/<id>?lang=en` and persist → write item name to clipboard. On any failure: restore original clipboard, GUI alert, log entry.
- Item-name cache at `addons/MysticClicker/item-name-cache.cfg` (id=name lines, append-only).
- Default keybind: `(null)` — assign in Nexus options or by setting `Code` in `configs/gw2-keybinds/nexus-inputbinds.json` and deploying.

### Build

- Added `modules\mystic-clicker\item-name.cpp` to `mystic-clicker.vcxproj` ClCompile items.
- Links against `wininet.lib` via `#pragma comment(lib, "wininet.lib")` in the new file (matches mystic-trading's pattern).

### Caveat

- Macro presses Esc once before opening chat (to close any chat-with-text safely). If a GW2 dialog is open at hotkey press, that Esc may close it. Run the macro before opening the destroy confirmation, not while it's already up.

## [tooling 2026-05-05] — Repo as source of truth for Nexus + controller deploys

### Added

- **`scripts/deploy-nexus-config.sh`** — push canonical `configs/gw2-keybinds/nexus-inputbinds.json` (and optionally `nexus-addonconfig.json`, `gamebinds.xml`) to all 4 profiles (3 bazzite + 1 Deck native) in one shot. Pre-flight refuses if GW2 is running on any reachable target. Timestamped backups + SHA256 verification per target. Unreachable hosts warned and skipped (so a sleeping Deck doesn't block bazzite-only deploys).
- **`scripts/deploy-controller-vdf.sh`** — push canonical `configs/steam-controller/moonlight-gw2-og-template.vdf` to every active Steam Input layout file across both machines. Filename filter only patches files matching `controller_neptune.vdf` or `og v<version>_<N>.vdf`, skipping pre-2025 legacy `og_<N>` exports. Same pre-flight, backup, and verification as the Nexus script. Handles autosave URL pattern for `moonlight - gw2 steamos/`.
- **`configs/gw2-keybinds/nexus-inputbinds-v1.json`** — golden master snapshot of the InputBinds canonical state confirmed working 2026-05-05 (TP + Bank combos verified post-v19.5). Mirrors the version-snapshot pattern used for controller VDFs. Future drift recoveries can restore from this if `nexus-inputbinds.json` itself drifts.

### Workflow change

**Going forward, both Nexus configs and controller VDFs are repo-first.**

For Nexus changes:

1. Edit `configs/gw2-keybinds/nexus-inputbinds.json` (or sibling) in repo
2. Run `./scripts/deploy-nexus-config.sh` to push to all 4 profiles
3. Commit the repo change
4. Snapshot if hitting a stable version: `cp nexus-inputbinds.json nexus-inputbinds-vN.json`

For controller VDF changes:

1. Edit `configs/steam-controller/moonlight-gw2-og-template.vdf` in repo (bump title)
2. Snapshot: `cp moonlight-gw2-og-template.vdf moonlight-gw2-og-vN.M.vdf`
3. Run `./scripts/deploy-controller-vdf.sh` to push to every active layout file across both machines
4. Commit

Never edit on-device first — devices drift, repo loses ground truth.

### Live state at end of 2026-05-05 deploy

- All 3 bazzite Nexus profiles: `InputBinds.json` hash `8dcd5362a5830f20…` (matches `nexus-inputbinds-v1.json`)
- All bazzite controller VDFs (12 already-matching + 7 newly-patched, 43 legacy skipped): canonical v19.5 template content, URL per-location patched
- Deck native Nexus + controller: pending (Deck asleep at deploy time — re-run scripts when awake)

## [controller v19.5] - 2026-05-04 — Fix Trading Post + Bank chord double-`I` flicker

### Fixed (controller VDF)

- **Trading Post Combo** (R1 + DPad-East Full_Press) and **Bank Combo** (R1 + DPad-East Long_Press) chord bindings still emitted `key_press I, Open Inventory` alongside their `F7`/`F8` macro keys — leftover from before the v3.6.12 DLL migration to `OpenInventoryDllAndDoubleClick`. The DLL now presses `I` itself, so the duplicate VDF press caused: VDF opens inventory → DLL closes it → 600ms wait → double-click lands on closed panel = "rapid open/close flicker" reported on Apple TV.
- Removed both stale `key_press I, Open Inventory` lines. TP and Bank chords are now bare `F7` and `F8` respectively, matching the post-migration pattern of Teleport Friend (bare F6).
- Symptom may have intermittently affected Deck too — likely exposed more reliably under Apple TV's higher-latency raw-HID path.
- Validation: brackets 931=931, snapshot saved as `configs/steam-controller/moonlight-gw2-og-v19.5.vdf`.

### Deployed

- bazzite: `moonlight/`, `guild wars 2 (apple tv)/`, `guild wars 2 (steam deck)/` → `og v18.4.9_0.vdf` (configset reference filename retained, URL field patched per-location).
- Deck native: `1284210/controller_neptune.vdf`.

## [3.6.16] - 2026-05-03 — Remove brittle Leave Party Combo; chat wheel /leave + /squadleave; controller v19.4

### Removed (Mystic Clicker)

- **`LEAVE_PARTY_COMBO` keybind + `SimulateLeavePartyCombo()`** — the right-click-party-bar → click-Leave macro depended on two captured UI positions and the GW2 party panel layout, which moves between resolutions and instances. Replaced by GW2's native slash commands (`/leave` and `/squadleave`) which work without UI position dependence.
- Capture targets **"Party Bar (right-click)"** and **"Leave Party (in menu)"**; **"Party"** category dropped from the capture window (now empty).
- Globals `g_PartySquadBarX/Y`, `g_LeavePartyX/Y` removed (declarations + read + write + reset paths).
- Nexus bindings `LEAVE_PARTY_COMBO`, `CAPTURE_PARTY_SQUAD_BAR`, `CAPTURE_LEAVE_PARTY` removed from `nexus-inputbinds.json` (210 entries now).
- Persisted `PartySquadBarX/Y`, `LeavePartyX/Y` keys in your `mystic-clicker.cfg` files will linger as unused — harmless, the new code just ignores them.

### Changed (controller v19.4)

- **Commands wheel** (group 120):
  - **Slot 6 ("+")**: replaced `LEFT_SHIFT + EQUALS + RETURN` (broken — Steam Input fires each `key_press` discretely so Shift released before Equals) with single `KEYPAD_PLUS + RETURN`.
  - **Slot 15 (NEW)**: types `/leave` + Enter — leaves party.
  - **Slot 16 (NEW)**: types `/squadleave` + Enter — leaves squad.
  - `touch_menu_button_count` 14 → 16.
- **R1 + DPad-Down Double_Press**: was `Shift+F10` (Leave Party Combo). Now `Ctrl+E` (Exit Instance) — gives Exit Instance a controller binding alongside the existing A-Long-Press one (kept as redundant binding).
- VDF title bumped to v19.4. Validation: brackets 931=931, 56 groups, 312 bindings (up from 294 with the slot 15/16 additions).
- Snapshot saved as `configs/steam-controller/moonlight-gw2-og-v19.4.vdf`.

### Files

- `modules/mystic-clicker/shared.h` — remove `LEAVE_PARTY_COMBO`, `CAPTURE_PARTY_SQUAD_BAR`, `CAPTURE_LEAVE_PARTY` constants + globals + proto
- `modules/mystic-clicker/entry.cpp` — remove register/deregister; version bump 3.6.15 → 3.6.16
- `modules/mystic-clicker/keybinds.cpp` — remove dispatch
- `modules/mystic-clicker/input-sim.cpp` — remove `SimulateLeavePartyCombo()`
- `modules/mystic-clicker/config.cpp` — remove globals + read/write/reset
- `modules/mystic-clicker/capture-ui.cpp` — remove capture entries + Party category
- `configs/gw2-keybinds/nexus-inputbinds.json` — strip 3 entries
- `configs/steam-controller/moonlight-gw2-og-template.vdf` — chat wheel slots 6/15/16 + R1+DDown Double; title v19.4
- `configs/steam-controller/moonlight-gw2-og-v19.4.vdf` — new snapshot

## [3.6.15] - 2026-05-03 — Mystic Clicker: Pathing render-all macro + bind world render layer

### Fixed

- **Pathing addon's world-render toggle was unbound** (`pathing-render-toggle` had `Code=0` in `nexus-inputbinds.json`). The Utility Wheel's "Toggle Paths" slot has been emitting Ctrl+F3 to nothing for who-knows-how-long. Bound it to **Alt+Shift+F3**, matching the F1/F2 family that already works for minimap/map toggles.

### Added

- **`PATHING_TOGGLE_ALL` macro** (default `CTRL+F3`, the Utility Wheel slot 1 chord) — fires Alt+Shift+F1 → Alt+Shift+F2 → Alt+Shift+F3 in sequence with 50 ms gaps. Single keybind toggles all three Pathing render layers (minimap, map, world) at once. The three individual Nexus hotkeys remain available for layer-specific toggling. Detached thread + `WaitForChordModifiersRelease` so the wheel-held Ctrl is released before the Alt+Shift sends start (otherwise GW2 sees Ctrl+Alt+Shift+Fn and Pathing's exact-modifier match misses).

### Files

- `modules/mystic-clicker/shared.h` — `PATHING_TOGGLE_ALL` constant + `SimulatePathingToggleAll()` proto
- `modules/mystic-clicker/entry.cpp` — register/deregister; version bump 3.6.14 → 3.6.15
- `modules/mystic-clicker/keybinds.cpp` — dispatch
- `modules/mystic-clicker/input-sim.cpp` — `SimulatePathingToggleAll()` implementation
- `configs/gw2-keybinds/nexus-inputbinds.json` — `PATHING_TOGGLE_ALL` (Code 61 Ctrl) added; `pathing-render-toggle` unbound → Alt+Shift+F3

## [3.6.14] - 2026-05-02 — Mystic Clicker: shorten "Generic Accept Combo" header to fit on one line

### Changed

- Capture window category renamed: **"Generic Accept Combo" → "Generic Accept"**. Was wrapping to a second line in the header given the `(N unset)` suffix. The "Combo" suffix was implied anyway since these slots are exclusively used by the Accept-Combo sequence.

## [3.6.13] - 2026-05-02 — Mystic Clicker: capture window grouped by category + clearer names

### Changed

- **Capture window** now groups targets under collapsing-header categories: Mystic Forge, Trading Post, Bank & Inventory, Crafting, Mail, Wizard Vault, Wizard Items, Travel, Bouncy Chest, Party, Generic Accept Combo, Misc. Headers default collapsed to reduce visual clutter as the list grew. Each header shows an "(N unset)" or "(all set)" suffix so you can tell at-a-glance which categories still need attention. Within each category the existing "uncaptured first, then alphabetic" sort still applies. The category containing an active 5-second capture countdown auto-expands so the highlighted button stays visible.
- **Renamed display labels** for clarity (saved coordinates **not affected** — the `.cfg` file uses internal `g_*` variable keys, not display names):

  | Before | After |
  | --- | --- |
  | Trading Post | TP Collect |
  | TP Remove | TP Cancel Listing |
  | Trading Post Icon | TP Portable Icon |
  | Bank Icon | Bank Portable Icon |
  | Merchant | Merchant Portable Icon |
  | Craft | Single Craft |
  | Craft Collapse | Craft Collapse All |
  | Craft Close | Close Crafting |
  | Wizard Vault | Wizard Vault Collect |
  | Wizard Vault Complete | Wizard Vault Confirm |
  | Chat Waypoint | Chat Waypoint (1st) |
  | Map Waypoint | Map Waypoint (2nd) |
  | Party/Squad Bar | Party Bar (right-click) |
  | Leave Party Button | Leave Party (in menu) |
  | Accept 1 | Accept 1 (Chest) |
  | Accept 2 | Accept 2 (Yes early) |
  | Char Swap | Character Swap |
  | LFG Search | LFG Search Tab |
  | Guild Hall | Guild Hall (in panel) |

### Files

- `modules/mystic-clicker/capture-ui.cpp` — `CaptureTarget` gains a `category` field; `s_Targets[]` repopulated with renames + categories; render loop iterates `s_Categories[]` and emits one `ImGui::CollapsingHeader` per group with auto-expand for active countdowns
- `modules/mystic-clicker/entry.cpp` — version bump 3.6.12 → 3.6.13

## [3.6.12] - 2026-05-02 — Mystic Clicker: TP + Bank combos use DLL-press-`I` (intermittency fix)

### Fixed

- **`SimulateTradingPostCombo` / `SimulateBankCombo`** — migrated to the same `WaitForChordModifiersRelease` + `OpenInventoryDllAndDoubleClick` pattern Teleport Friend / Wizard Gobbler / Lounge Pass / Merchant already use. Symptom was TP chord opening then closing inventory (sometimes opening), making the captured-slot double-click land on a closed panel. Root cause: the old `OpenInventoryAndDoubleClick` helper assumed the VDF chord pressed `I`, but the chords (bare F7 / F8) don't — and Steam Input's chord double-emit could land conflicting events. Now the DLL releases held modifiers and synthesizes `I` itself, exactly once per dispatch.

### Files

- `modules/mystic-clicker/input-sim.cpp` — TP and Bank combos rewritten
- `modules/mystic-clicker/entry.cpp` — version bump 3.6.11 → 3.6.12

## [controller v19.3] - 2026-05-02 — Utility Wheel: empty center, Settings moved to slot 16

### Changed

- **Utility Wheel (group id 63)** — `touch_menu_button_0` (center) removed; `Open Settings` (`F11`) moved to a new `touch_menu_button_16` on the ring. Center is now a no-op so accidental release in the middle of the wheel doesn't fire Settings — was getting in the way of every wheel trigger.
- Snapshot saved as `configs/steam-controller/moonlight-gw2-og-v19.3.vdf`. Brackets balanced (921=921), groups 56, bindings 294 (unchanged: removed 1, added 1).

### Files

- `configs/steam-controller/moonlight-gw2-og-template.vdf` — title v19.3, Utility Wheel center removed + slot 16 added
- `configs/steam-controller/moonlight-gw2-og-v19.3.vdf` — new snapshot

## [3.6.11] - 2026-05-02 — Mystic Clicker GRACEFUL_QUIT macro + controller v19.2

### Added

- **Mystic Clicker `GRACEFUL_QUIT` keybind** (default `ALT+SHIFT+Q`) — replaces controller Menu-button Long_Press = `Alt+F4` force-quit. Flow: send `Esc` (open in-game menu) → 200 ms wait → click captured "Exit Game" position. GW2 closes via the normal shutdown path so Nexus runs `Unload` and `InputBinds.json` saves cleanly. Mitigates the InputBinds drift hypothesis where Alt+F4 force-quit skipped the Nexus save.
- New capture target **"Exit Game"** in the Mystic Clicker Capture window — captures the in-game menu's "Exit to Character Select" button per-resolution.
- New config keys `ExitGameX` / `ExitGameY` persisted to per-resolution `.cfg` (zeroed on resolution reset).
- New Nexus binding identifier `CAPTURE_EXIT_GAME` (registered, no default key) so the capture entry surfaces in Nexus settings.

### Changed

- **Controller layout v19.2** — `button_escape` Long_Press: `LEFT_ALT + F4` → `LEFT_ALT + LEFT_SHIFT + Q` (label `Graceful Quit`). Snapshot saved as `configs/steam-controller/moonlight-gw2-og-v19.2.vdf`.

### Files

- `modules/mystic-clicker/shared.h` — `GRACEFUL_QUIT` + `CAPTURE_EXIT_GAME` constants; `g_ExitGameX/Y`; `SimulateGracefulQuit()` proto
- `modules/mystic-clicker/entry.cpp` — register/deregister + version bump 3.6.10 → 3.6.11
- `modules/mystic-clicker/keybinds.cpp` — dispatch `GRACEFUL_QUIT → SimulateGracefulQuit()`
- `modules/mystic-clicker/input-sim.cpp` — `SimulateGracefulQuit()` implementation
- `modules/mystic-clicker/capture-ui.cpp` — "Exit Game" capture target row
- `modules/mystic-clicker/config.cpp` — read/write/reset `ExitGameX/Y`
- `configs/gw2-keybinds/nexus-inputbinds.json` — `GRACEFUL_QUIT` (Code 16, Alt+Shift) + `CAPTURE_EXIT_GAME` (Code 0)
- `configs/steam-controller/moonlight-gw2-og-template.vdf` — title v19.2, button_escape Long_Press updated
- `configs/steam-controller/moonlight-gw2-og-v19.2.vdf` — new snapshot

## [3.6.10] - 2026-05-01 — Mystic Clicker capture window + Mystic Trading delivery box close on ESC

### Added

- **Mystic Clicker**: capture window registers `GUI_RegisterCloseOnEscape` so pressing ESC closes the panel just like any other Nexus window. Symmetric `GUI_DeregisterCloseOnEscape` in `AddonUnload`.
- **Mystic Trading**: Delivery Box now also closes on ESC. Previously only Dashboard and FlipColumn responded — Delivery Box was an explicit gap noted in a `// (but NOT delivery box)` comment. Comment updated; all three MT windows now consistent.

### Files

- `modules/mystic-clicker/entry.cpp` — register/deregister + version bump 3.6.9 → 3.6.10
- `modules/mystic-trading/entry.cpp` — register/deregister Delivery Box + version build bump 20260420 → 20260501

## [3.6.9] - 2026-05-01 — Mystic Clicker: Bouncy Meta Progress capture

### Added

- **New capture point: "Bouncy Meta Progress"** — fits between Bouncy Accept and Bouncy Meta Complete in the OPEN_CHEST_COMBO flow. Some bouncy chest meta events show a separate progress dialog mid-flow that needs clicking before the final completion dialog appears; without this capture step, the combo would skip it and stall.
- New config keys `BouncyMetaProgressX`/`BouncyMetaProgressY` persisted to / loaded from `mystic-clicker.cfg` and per-resolution variants.
- Capture-UI row inserted between "Bouncy Accept" and "Bouncy Meta Complete"; existing Meta Complete description updated from "(combo step 3)" → "(combo step 4)".
- Combo sequencing in `SimulateOpenChestCombo`: Right-click chest → 500ms → Accept (optional) → 100ms → **Meta Progress (optional, NEW)** → 100ms → Meta Complete (optional). All clicks remain optional; whichever positions are captured fire.

### Files

- `modules/mystic-clicker/shared.h` — extern declarations
- `modules/mystic-clicker/config.cpp` — globals, primary load, save, reset, secondary load (5 touch-points)
- `modules/mystic-clicker/capture-ui.cpp` — new row in `s_Targets[]`
- `modules/mystic-clicker/input-sim.cpp` — combo flow click between Accept and Meta Complete
- `modules/mystic-clicker/entry.cpp` — version bump 3.6.8 → 3.6.9

### Deploy reminder

Per `memory/nexus-multi-deploy-rules.md`, after GitHub Actions builds the new DLL it must be deployed to **all three** profile install dirs (`~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/`, `~/Games/gw2-appletv/addons/`, `~/Games/gw2-deck/addons/`). User then captures the Meta Progress position once per profile (or copies the `.cfg` files between profiles since click positions are screen-resolution-dependent).

## [3.6.8] - 2026-04-26 — Mystic Clicker + Controller v18.4.8

### Fixed

- **Friend chord still opened then immediately closed inventory** even with v3.6.7's Nexus-side debounce. Log analysis showed Steam Input is double-emitting the **entire VDF chord** (both `I` and `F6`) on Full_Press in this layout, 50–80 ms apart. Nexus debounce caught the second `F6` dispatch (`Debounced duplicate dispatch of TELEPORT_FRIEND_COMBO`), but **GW2 still saw `I` pressed twice** at the OS level — and two `I` presses toggle inventory open then closed.
- Fix: convert Friend to the **DLL-press-`I` pattern** (matches Wizard Gobbler / Portal / Lounge / Merchant). VDF chord now emits `F6` alone — no `I`. The DLL synthesizes `I` exactly once per dispatch via `OpenInventoryDllAndDoubleClick`. The chord has no modifier so `WaitForChordModifiersRelease` returns instantly — no perceptible delay.
- **Mystic Clicker DLL `RegisterWithString` default for `RESET_WINDOWS` updated** from `CTRL+SHIFT+HOME` → `CTRL+SHIFT+INSERT` to match v18.4.7's wheel-slot-2 chord change.

### Notes

After deploying v18.4.7 the user reported: "Screen rescue shows no skill error[,] still does nothing[. No] window change." Translation: chord moved off Home (skill collision gone) but Nexus isn't dispatching the new `Ctrl+Shift+Insert` chord — the on-disk InputBinds.json change to Code 45 hasn't been picked up because **Nexus only reads InputBinds.json on startup**. The user must do a full GW2 restart (not just an addon reload) for the Reset Windows binding to take effect. The DLL default change in this release ensures any future install picks up Insert directly without manual rebinding.

The Steam Input double-emit-Full_Press behavior also affects Trading Post and Bank chords (`I+F7`, `I+F8`) on dpad_east, but the user reports those work — likely because the second `I` toggle happens within the same frame as the first and isn't visually obvious. Leaving those alone for now (don't fix what isn't broken). If they ever exhibit the symptom, convert them to the same DLL-press-`I` pattern.

Versions: VDF v18.4.8 (revision 2408), Mystic Clicker DLL 3.6.8.

## [3.6.7] - 2026-04-26 — Mystic Clicker + Controller v18.4.7

### Fixed

- **R1+DPad Down → Friend "doesn't open inventory" — actually a chord double-fire.** Nexus.log shows two `Teleport Friend Combo: open inventory + double-click` entries at the **same millisecond timestamp** for every press (15:02:26.16 ×2, 15:02:33.847 ×2, 15:02:51.711 ×2). The pattern affects all Full_Press chords in v18.4.x layouts (Friend, Wizard Gobbler, TP, Bank). Pressing `I` twice toggles the inventory **closed** on the same combo that just opened it, so the second double-click lands on a closed panel. Added per-identifier debounce in `ProcessKeybind` — drops any dispatch within 300 ms of the same identifier's last fire. Logs the suppressed dispatch at DEBUG level for verification.
- **Utility Wheel slot 2 (Reset Windows) also fired GW2's Skill Profession 5** ("skill recharging" message). User has `SkillProfession5` bound to `button="36"` (Home key) in `GameBinds.xml`, and GW2 ignores the Ctrl+Shift modifiers — bare Home press triggers the skill. Switched the rescue chord from `Ctrl+Shift+Home` → `Ctrl+Shift+Insert` (Insert is unbound across all of the user's GW2 keybinds) and updated both `RESET_WINDOWS` + `MT_RESET_WINDOWS` Nexus bindings to Code 45 (Insert). The `Ctrl+Shift+Insert` chord is also unique to Mystic Clicker — no other Nexus addon claims it.

### Notes

The user's GW2 GameBinds.xml binds **Home / End / PageUp / PageDown** all to Skill Profession 2-5 — pressing any of those keys (regardless of modifier state) fires the skill. When picking chord keys for Steam Input wheel slots, check `gamebinds.xml` for `button="36"` (Home), `button="35"` (End), `button="33"` (PgUp), `button="34"` (PgDn) and avoid them. F11 / F12 / Insert / Delete / Numpad 0 / 1 / 5 / 7 / 9 / + / - / / are all free in this user's setup.

Versions: VDF v18.4.7 (revision 2407), Mystic Clicker DLL 3.6.7.

## [3.6.6] - 2026-04-26 — Mystic Clicker DLL only

### Fixed

- **Long_Press inventory combos (Wizard Portal Scroll) still opened Wizard Vault** even with v3.6.5's 800 ms defer. Root cause: Steam Input keeps Shift asserted via uinput for the **entire physical hold**, which on a Long_Press routinely exceeds 800 ms (user's natural hold is 500 – 2000 ms). Any fixed `Sleep(N)` is a guess.
- Replaced the fixed-time defer in all four detached-thread combos (Wizard Gobbler, Wizard Portal Scroll, Lounge Pass, Merchant) with `WaitForChordModifiersRelease()` — polls `GetAsyncKeyState(VK_LSHIFT/RSHIFT/LMENU/RMENU/LCONTROL/RCONTROL)` every 50 ms (3 s timeout) and exits as soon as the OS sees the modifier release. Adapts to actual hold time so quick taps don't feel laggy and long holds don't leak `Shift+I`/`Alt+I` to GW2.
- Each combo now logs the modifier state on entry and the wait duration on exit (`Wizard Portal Scroll: modifiers held at start=0x1, released after 1450ms (timeout=no)`), so future debugging can see hold patterns directly in `Nexus.log`.

### Notes

DLL-only release — no VDF or Nexus binding changes. v18.4.6 controller layout stays current.

Versions: Mystic Clicker DLL 3.6.6.

## [3.6.5] - 2026-04-26 — Mystic Clicker + Controller v18.4.6

### Fixed

- **R1+DPad Left Double_Press → Lounge Pass opened Wizard Vault instead.** Same `Shift+I → WV` race we fixed for Long_Press in v3.5.2, but on Double_Press the second physical tap can run longer than 500 ms — uinput is still asserting Shift when the DLL synthesizes `I`. Bumped `SimulateLoungePassCombo`'s detached-thread defer from 500 ms → 800 ms so the held virtual modifier fully drains. Same change applied to `SimulateMerchantCombo` (also Double_Press, also `Alt+F11`).
- **R1+DPad Right Double_Press → Merchant didn't always trigger** because the user's natural double-tap rhythm exceeded Steam Input's default `double_tap_time` window. Added explicit `"double_tap_time" "500"` to all three v18.4 Double_Press activators (Merchant on R1+Right, Leave Party on R1+Down, Lounge Pass on R1+Left), giving 500 ms between taps.
- **Window rescue (Utility Wheel slot 2) didn't change window sizes** — `rescue.cpp` only clamped to display size, so a 1920×1080 window on the Deck became a 1280×800 window filling the whole screen. Now clamps to 70% of viewport (896×560 on Deck) so oversized windows actually shrink to a usable size. Also logs every press now (even when no windows needed rescuing) so the user can confirm the chord reached the addon.

### Notes

The two Double_Press combos that drive inventory items both run the held-modifier-drain pattern with an 800 ms defer (longer than the 500 ms used by Long_Press combos). Pattern: when an inventory-item combo lives on a Double_Press activator and the chord carries `Shift` or `Alt`, the DLL must wait at least 800 ms before synthesizing `I` — uinput key-hold during the second physical tap can outlast a 500 ms drain.

Versions: VDF v18.4.6 (revision 2406), Mystic Clicker DLL 3.6.5.

## [3.6.4] - 2026-04-26 — Mystic Clicker + Controller v18.4.5

### Fixed

- **R1+DPad Down → Friend didn't open inventory before clicking the captured slot.** `SimulateTeleportFriendCombo` does not press `I` itself — it relies on the VDF chord emitting `I` alongside the keycode. v18.3 had Friend on R1+DPad Right Full_Press emitting `I+F6`; v18.4.x moved Friend to R1+DPad Down and only emitted `F6`, so the 600ms wait + double-click landed on a closed inventory and did nothing. Fix: add `key_press I, Open Inventory` back to the dpad_south Full_Press chord. (F6 is bare, so `I` reaches GW2 cleanly with no `Shift+I`→WV bug.)
- **R1+DPad Right Double_Press → Merchant had the same bug**, but unlike Friend the chord carries `Alt`, so adding `key_press I` to the VDF wouldn't help (GW2 sees `Alt+I`, which is unbound). Fix: switch `SimulateMerchantCombo` to use `OpenInventoryDllAndDoubleClick` on a detached `std::thread` with a 500ms pre-delay — the same pattern already used by Wizard Gobbler / Portal Scroll / Lounge Pass to drain the held virtual modifier before the DLL synthesizes a clean `I`.

### Notes

The two inventory-combo patterns in this codebase:

1. **Bare-chord pattern** (Friend, Trading Post, Bank): VDF emits `I + F<n>`. GW2 sees `I` → opens inventory. Nexus catches `F<n>` → DLL waits 600ms → double-click captured slot. DLL function uses `OpenInventoryAndDoubleClick`.
2. **Modifier-chord pattern** (Wizard Gobbler / Portal Scroll / Lounge Pass / Merchant): chord uses `Shift` or `Alt`, so VDF must NOT emit `I` (would fire `Shift+I` = WV or be ignored as `Alt+I`). DLL runs on a detached `std::thread`, sleeps 500ms to let the held virtual modifier drain, then `ReleaseChordModifiers()` and synthesizes `I` via SendInput. DLL function uses `OpenInventoryDllAndDoubleClick`.

Pattern selection is determined by whether the chord carries a modifier — not by which button the chord lives on. When moving a combo across DPad slots, the new chord's modifier set must match the DLL function's pattern, or the inventory-open step silently fails.

Versions: VDF v18.4.5 (revision 2405), Mystic Clicker DLL 3.6.4.

## [3.6.3] - 2026-04-26 — Mystic Clicker + Controller v18.4.4

### Fixed

- **R1+DPad Right Double_Press → Merchant Combo opened Settings + Wizard Vault instead** — Nexus log showed no `Merchant Combo` event because the v18.4.3 chord (`Shift+F11+I`) didn't match what Nexus had stored for `MERCHANT_COMBO` (`F11` alone). Meanwhile GW2 caught `F11` (Game Options) and `Shift+I` (Wizard Vault toggle) — the same `Shift+I → WV` bug that bit Wizard Gobbler in v3.5.1. Fix: strip `key_press I` from the chord (DLL opens inventory itself with the 500 ms detached-thread defer) and move the chord off `Shift+F11` entirely. First tried `Ctrl+F11`, which collided with `KB_CRAFTY_TOGGLE` — settled on **`Alt+F11`** which is unbound. Updated `configs/gw2-keybinds/nexus-inputbinds.json` (deployed live to Bazzite) and the DLL `RegisterWithString` default so first-run users get the same key.

### Added

- **Utility Wheel slot 0 → Open Settings** (`F11`) — bare F11 is GW2's default Game Options keybind and is unbound in Nexus, so a single emit opens Settings cleanly.
- **Golden Rule #11: check Nexus for chord collisions before picking a key** — added to `docs/vdf-editing-golden-rules.md` with a one-line Python snippet that lists everything bound to a given scancode. Would have caught the `Ctrl+F11` / `KB_CRAFTY_TOGGLE` collision before deploy.

### Changed

- **Renamed `configs/steam-controller/moonlight-gw2-og-v16.1.vdf` → `moonlight-gw2-og-template.vdf`.** The `v16.1` in the filename had been misleading for many releases — the actual VDF version is in the `title` field inside the file. The repo template now has no version in its name; per-release deploys still go to the Deck under `og v<N.M.P>_0.vdf`.
- DLL bumped to **3.6.3**, controller VDF bumped to **v18.4.4** (revision 2404).

## [3.6.2] - 2026-04-26 — Mystic Clicker + Controller v18.4.3

### Fixed

- **Controller v18.4 VDF was structurally rejected by the Steam Deck parser** — Personal/Templates tabs went empty, ghost workshop refs kept getting restored on every Steam restart, and the user's saved layout could not be applied. Root cause: an earlier session used Python regex to rewrite `dpad_south` and `dpad_east` blocks in `moonlight-gw2-og-v16.1.vdf`. The greedy regex consumed Long_Press/Double_Press activator subgroups across multiple groups, dropping ~25 of them. The file went from 63 121 → 37 170 bytes (3543 → 1768 lines, 296 → 158 bindings, 56 → 31 effective groups). Brackets still balanced and the file looked legal, but Steam silently dropped it.
- **Recovery (v18.4.3)**: started fresh from the v18.3 base on disk (3543 lines, 56 groups, 296 bindings) and applied the v18.4 chord changes via two surgical line-level edits. Final file: 3551 lines, 63 305 bytes, 56 groups, 298 bindings (+2 for the new Merchant chord), bracket balance 918/918. Deployed to the Deck as `og v18.4.3_0.vdf` with `url=usercloud://moonlight/og v18.4.3_0` and `export_type=personal_local`.

### Added

- **R1+DPad Right Double_Press → Merchant Combo** (`SHIFT+F11`) — opens inventory and double-clicks the captured Merchant slot. New capture `CAPTURE_MERCHANT` already existed in v3.6.0; this commit wires it to a chord.
- **R1+DPad Right reorganized**: Full=TP (F7) / Long=Bank (F8) / **Double=Merchant** (Shift+F11, new).
- **R1+DPad Down reorganized**: Full=Friend (F6) / Long=Waypoint (Shift+F9) / Double=LeaveParty (Shift+F10). Friend was previously bound on R1+DPad Right Full.
- **`docs/vdf-editing-golden-rules.md`** — full reference doc with hard rules, validation checklist, and recovery playbook to prevent this class of outage. Memory entries (`memory/vdf-editing-rules.md`, `memory/steam-input-sync-not-cloud.md`) point at it.

### Notes

- Steam Input sync layer is **separate** from the Steam Cloud toggle in the client UI (Valve bug #7801). Disabling Cloud does not stop Steam Input from re-syncing controller layouts. Confirmed during recovery — only signout/signin combined with local cache wipe (`appworkshop_241100.acf`, `ugcmsgcache/`, `userdata/.../remotecache.vdf`) consistently broke the ghost-layout cycle.
- DLL `MERCHANT_COMBO` default changed from `(null)` to `SHIFT+F11`.
- Mystic Clicker DLL bumped to **3.6.2**. GitHub Actions builds it on push.

## [3.5.2] - 2026-04-21 — Mystic Clicker + Controller v17.7

### Added

- **Five new combos on R1+DPad Left and R1+DPad Down** (v3.5.0/v17.6):
  - R1+DPad Left Full → **Wizard Gobbler** (Shift+F1)
  - R1+DPad Left Long → **Wizard Portal Scroll** (Shift+F2)
  - R1+DPad Left Double → **Lounge Pass** (Shift+F3)
  - R1+DPad Down Full → **Waypoint Combo** (Shift+F4) — single-click captured chat waypoint → map auto-opens → double-click captured map waypoint to travel
  - R1+DPad Down Long → **Leave Party Combo** (Shift+F5) — right-click captured party/squad bar → single-click captured Leave button

### Fixed

- **Wizard Gobbler / Portal Scroll / Lounge Pass opened Wizard Vault instead of inventory** (v3.5.1): the VDF emitted `I + LEFT_SHIFT + F#` simultaneously. GW2 observed `Shift+I` (WV toggle) while Nexus caught `Shift+F#`. Fix: strip `key_press I` from the 3 R1+DPad Left activators; new `OpenInventoryDllAndDoubleClick()` helper releases chord modifiers and synthesizes a clean `I` via SendInput scan code (same pattern WV combo uses for Shift+I).
- **Long_Press / Double_Press variants still opened WV** after v3.5.1 fix (v3.5.2): Steam Input's Long_Press and Double_Press activators keep the chord keys held via uinput for the duration of the physical button press — our DLL's SendInput Shift-KEYUP got overridden by the still-asserting uinput hold. Fix: the 3 inventory combos now run on a detached `std::thread` with a 500ms pre-delay, letting the user's button release drain the virtual Shift before we press `I`.
- **Waypoint Combo map double-click missed the waypoint** — 700ms post-chat-click sleep wasn't enough for the map to settle on the pin. Bumped to 1200ms.

### Notes

Architecture split between the 3 R1+DPad Right portable-item combos (bare-F6/F7/F8 chord, VDF emits `I` directly — no Shift conflict) and the 3 R1+DPad Left portable-item combos (Shift+F1/F2/F3 chord, DLL synthesizes `I` after a 500ms defer). Keep them that way — they use the same `OpenInventoryAndDoubleClick` / `OpenInventoryDllAndDoubleClick` helpers but resolve different Linux/Sunshine uinput quirks.

## [3.4.1] - 2026-04-20 — Mystic Clicker + Controller v17.5

### Added

- **Personal Marker** moved from VDF mouse_button binding into DLL. New `PERSONAL_MARKER` keybind on bare F9 triggers a single SendInput batch (Alt-down → LeftClick-down → LeftClick-up → Alt-up) at the current cursor position. Reliable through Sunshine/uinput where Steam Input's `mouse_button LEFT` wasn't registering the Alt modifier in sync.
- **Three portable-item combos** on R1+DPad Right variants. Each emits `key_press I` natively (opens inventory) + a bare F-key chord that fires the DLL to double-click the captured icon:
  - R1+DPad Right Full → Teleport to Friend (F6)
  - R1+DPad Right Double → Trading Post (F7)
  - R1+DPad Right Hold → Bank (F8)
- **Controller v17.3–v17.5** ships the R1+DPad Right bindings, new Personal Marker chord on R1+Y, and removed a duplicate empty `button_y` block that was shadowing Personal Marker.

### Fixed

- **WV Combo** now triple-clicks Collect per tab (catches multi-collect states), single-clicks Complete, and returns to Daily tab at the end so the next opening starts on Daily.
- **MT lock bypass for rescue/resolution change**: locked Delivery / FlipList windows no longer stay stuck when a resolution change or Ctrl+Shift+Home rescue fires — `NoMove/NoResize/NoTitleBar` flags are temporarily stripped for that frame so the reposition actually takes effect.
- **ClampWindowPos** now caps window size to the viewport, preventing a 4K-sized window from rendering oversized on an 800p display.

### Notes

After multiple misfires on chord choice (Ctrl+Alt+F1/F2/F3 = Linux TTY switch; Ctrl+Shift+Alt+F1/F2/F3 = 4-binding drop; Ctrl+Shift+PAGEUP/DOWN/INSERT and F13/F14/F15 = key names not in Steam Input's VDF parser), settled on **bare F6/F7/F8/F9** with `key_press I` alongside the chord. Steam Input definitely knows F1-F12; Nexus distinguishes bare-F6 from Ctrl+Shift+F6 by modifier bits.

## [3.2.0] - 2026-04-20 — Mystic Clicker + Mystic Trading v0.5.0 + Controller v17.2

### Added

- **Drag-anywhere** (both modules): click-and-hold any empty area to move a panel. No longer limited to the title bar.
- **Per-resolution window positions** (both modules): each window remembers pos + size per resolution. Switch 4K → 1280×800 and it auto-loads the right layout. New state files: extended `mystic-clicker-{WxH}.cfg`, new `mystic-trading-windows-{WxH}.cfg`.
- **Auto-clamp** on resolution change keeps at least 60px of each window on-screen.
- **Rescue hotkey Ctrl+Shift+Home** snaps all addon windows back to (100, 100) at default size. 200ms arming window so all 3 MT panels consume it in one frame.
- **Controller v17.2**: Utility Wheel slot 2 fires Ctrl+Shift+Home (Reset Windows).

### Changed

- Saves for window pos/size are 1-second debounced so drags don't hammer the disk.

## [3.1.3] - 2026-04-20 — Mystic Clicker

### Added

- **Wizard Vault Special Tab** capture + combo step — combo now loops through Daily → Weekly → Special, each with Collect + Complete clicks. All three tabs optional.

## [3.1.2] - 2026-04-20 — Mystic Clicker

### Added

- **Capture panel sort**: uncaptured targets on top, captured below, both A-Z. Re-sorts each frame so newly-captured entries drop to the bottom immediately.

### Fixed

- **LFG Combo not firing**: Nexus keybind store on Bazzite had `LFG_COMBO` with Code=0 (unbound) and `GENERAL_ACCEPT` stealing Ctrl+Shift+F6. Fixed InputBinds.json directly — LFG_COMBO now bound to scan code 64 (F6), GENERAL_ACCEPT cleared. Nexus uses scan codes, not VK codes.

## [3.1.1] - 2026-04-20 — Mystic Clicker

### Added

- **Wizard Vault Daily Tab** capture + combo step — WV remembers the last-selected tab, so the combo now clicks Daily Tab first (when captured) to ensure Daily is always processed before switching to Weekly.

## [3.1.0] - 2026-04-20 — Mystic Clicker + Controller v17.1

### Added

- **Accept Combo** expanded from 10 to 15 slots (Accept 11-15).
- **Wizard Vault Weekly Tab** capture — combo now clicks Daily → Collect/Complete → Weekly Tab → Collect/Complete.
- **LFG Combo** — press Y (opens LFG) → click Search tab. Bound to Ctrl+Shift+F6.
- **Controller v17.1**: DPad Left Full Press now fires LFG Combo (was raw Y press).

### Changed

- **Click intervals** reduced from 300ms to 100ms between all combo clicks for faster flow.

### Migrated

- Accept 10's captured position moved to Accept 15 slot (one-time cfg migration on Bazzite 1280x800).

## [3.0.0] - 2026-04-19 — Mystic Clicker + Controller v17.0

### Added

- **Accept Combo**: 8 → 10 slots with Bouncy Meta Complete (3rd bouncy capture for meta-completion dialogs).
- **Right-click-to-clear** in capture window to reset a single captured position.
- **Bouncy Open + Bouncy Accept** as separate captures from general Accept combo.
- **Mail / Guild Hall / Wizard Vault combos** with keypress + click patterns using SendInput (KEYEVENTF_SCANCODE) to handle Nexus chord modifier timing.

### Fixed

- **Scan code requirement**: WM_KEYDOWN without scan code in lParam is silently dropped by GW2 — affected Guild Hall Combo (G key). Now constructs `(scan << 16) | 0x01` via `MapVirtualKey(vk, MAPVK_VK_TO_VSC)`.
- **Nexus chord modifier timing**: Nexus keybind callback fires while physical Ctrl/Shift still held — combos that send SendInput chords (Shift+I for WV, Shift+0 for Mail) first release VK_LCONTROL/RCONTROL/LSHIFT/RSHIFT via SendInput KEYUP before sending the intended chord.

## [0.3.0] - 2026-03-18 — Mystic Trading

### Changed

- **Dashboard (Alt+T)**: Simplified to single column — Wallet, Bank, Materials only
- **Delivery Box (Alt+D)**: Simple toggle on/off, removed auto-show logic
- **Delivery Box**: Removed duplicate inner heading (window title bar is sufficient)
- **Flip List (Alt+F)**: Removed duplicate "Profitable Flips" inner heading

### Added

- **Window transparency slider** in Options > Appearance (0.3–1.0, default 0.92)
- **Linux gaming docs**: Added `docs/linux-gaming.md` for Bazzite desktop setup

### Removed

- Flips and Delivery Box sections from main dashboard (use Alt+F and Alt+D instead)

## [2.0.0] - 2026-03-15

### Changed

- **Renamed addon to Mystic Clicker** - formerly "Inventory Hotkeys"
- **New addon directory**: `MysticClicker` (was `InventoryHotkeys`)
- **New config filenames**: `mystic-clicker-{width}x{height}.cfg` (was `inventory-hotkeys-*.cfg`)
- **New DLL name**: `mystic-clicker.dll` (was `inventory-hotkeys.dll`)
- **Reorganized source code** into `src/mystic-clicker/` subfolder for multi-module support

### Added

- **Automatic config migration** - On first load, copies all saved position configs from the old `InventoryHotkeys` directory to `MysticClicker` with renamed filenames. No manual action needed.

### Technical

- Solution and project files renamed to `mystic-clicker.sln` / `mystic-clicker.vcxproj`
- Source files moved to `src/mystic-clicker/` to prepare for additional addon modules

## [1.4.0] - 2026-02-05

### Fixed

- **BlishHUD/Nexus GG compatibility** - Fixed keybind conflict that prevented other addons from receiving keyboard input
- **Reserved name conflict** - Renamed "VENDOR" to "VENDOR_BUY" to avoid Nexus internal name collision

### Changed

- **Updated default keybinds** - All actions now use CTRL+key, captures use CTRL+SHIFT+key
  - CTRL+D: Deposit Materials
  - CTRL+S: Sort Inventory
  - CTRL+B: Open Chest
  - CTRL+Q: Deposit & Sort
  - CTRL+J: Sell Junk
  - CTRL+O: Trading Post
  - CTRL+U: Vendor (changed from CTRL+V to avoid paste conflict)
  - CTRL+E: Exit Instance
  - CTRL+Y: Yes Dialog
  - CTRL+F: Mystic Forge
  - CTRL+R: Mystic Refill
  - CTRL+M: Mystic Forge Combo
  - CTRL+T: TP Remove

### Technical

- Discovered "VENDOR" is a reserved/conflicting identifier in Nexus
- All 24 keybinds (13 actions + 11 captures) now work without breaking other addons

## [1.3.0] - 2026-02-04

### Added

- **Yes Dialog Hotkey** (Ctrl+P) - Click the Yes button on confirmation dialogs
- **Mystic Forge Hotkey** (Ctrl+F) - Click the Mystic Forge combine button
- **Mystic Refill Hotkey** (Ctrl+R) - Click the Mystic Forge refill button
- **Mystic Forge Combo** (Ctrl+A) - Forge then Refill with 100ms delay
- **Vendor Hotkey** (Ctrl+V) - Click vendor button
- **Sell Junk Hotkey** (Ctrl+J) - Click sell junk button at vendors
- **Trading Post Hotkey** (Ctrl+O) - Click trading post button
- **TP Remove Hotkey** (Ctrl+T) - Click "Take" button in Trading Post pickup
- **Capture keybinds** for all new hotkeys (Ctrl+Shift+P/F/R/V/J/O/T)

### Changed

- **Cleaner keybind names** - Removed "KB_" prefix from all keybind identifiers
  - e.g., "KB_DEPOSIT_MATERIALS" → "DEPOSIT_MATERIALS"
  - Requires re-binding keys in Nexus after update

### Technical

- Added 14 new position variables for new hotkeys
- All positions saved per-resolution in config files

## [1.2.0] - 2026-02-04

### Added

- **Exit Instance Hotkey** (Ctrl+E) - Click the exit instance button
- **5 Generic Hotkeys** - User-assignable click hotkeys for any UI element
  - Generic 1-5 with capture keys Ctrl+Shift+1-5
  - Unassigned by default - configure in Nexus Options
- **Capture Exit Instance** (Ctrl+Shift+E) - Save exit instance button location

### Technical

- Added 12 new position variables for exit instance and generic hotkeys
- All positions saved per-resolution in config files

## [1.1.0] - 2026-02-04

### Added

- **Per-Resolution Config Files** - Positions now save to resolution-specific files
  - e.g., `inventory-hotkeys-1920x1080.cfg`, `inventory-hotkeys-2560x1440.cfg`
- **Auto Resolution Detection** - Detects current game window resolution on startup
- **Resolution Change Detection** - Automatically switches config when resolution changes
- **Multi-Device Support** - Switch between laptop and desktop without reconfiguring

### Changed

- Config files now include resolution in filename
- Alert messages show current resolution when loading/saving

## [1.0.0] - 2026-02-03

### Added

- **Deposit Materials Hotkey** (Ctrl+D) - Click deposit materials button
- **Sort/Compact Hotkey** (Ctrl+S) - Click sort/compact button
- **Bouncy Chest Hotkey** (Ctrl+B) - Right-click to open bouncy chests
- **Deposit + Sort Combo** (Ctrl+Q) - Performs deposit then sort in sequence
- **Position Capture System** - Keybinds to save button positions at cursor
  - Ctrl+Shift+D for deposit button
  - Ctrl+Shift+S for sort button
  - Ctrl+Shift+B for bouncy chest
- **Config Persistence** - Saves positions to `inventory-hotkeys.cfg`
- **Nexus Integration** - Full keybind customization via Nexus Options UI
- **Visual Studio 2025 Support** - Built with Platform Toolset v145

### Technical

- Nexus API v6 integration
- `WndProc_SendToGameOnly` for input simulation
- `GUI_SendAlert` for user feedback
- Auto-creates config directory if missing

## [0.1.0] - 2026-01-01

### Initial Release

- Initial repository structure
- Project documentation (readme.md, agents.md, roadmap.md)
- Development setup guide
- Research documentation for Nexus addon development

---

## Version History Summary

| Version | Date       | Highlights                                      |
| ------- | ---------- | ----------------------------------------------- |
| 2.0.0   | 2026-03-15 | Renamed to Mystic Clicker, config migration     |
| 1.3.0   | 2026-02-04 | Mystic Forge, Vendor, Trading Post hotkeys      |
| 1.2.0   | 2026-02-04 | Exit instance hotkey, 5 generic hotkeys         |
| 1.1.0   | 2026-02-04 | Per-resolution configs, multi-device support    |
| 1.0.0   | 2026-02-03 | Full release with all core features             |
| 0.1.0   | 2026-01-01 | Initial structure and research                  |

---

Changelog for Guild Wars 2 Addons
