# Release Notes

Per-version changelog for TumatraPDF — new functions + bug fixes, newest first.
Feature details live in [README.md](README.md); full per-session development
history in [LOG.md](LOG.md).

## v1.0.7 (pending)

- **Keyboard shortcuts sheet** (`?` / Shift+/): new **Flashcards** and
  **Auto-Scroll & Contrast** sections (fork commands were missing from the
  sheet); the sheet window is now **resizable** — content size is the floor,
  columns center when the window is wider, footer pins to the bottom edge.
- **Trim Config temporarily disables trim** while the dialog is open: the FULL
  page is visible for placing the margin lines; trim comes back on Save/Cancel.
  Cancel now also frees the render cache (tiles cached mid-dialog were rendered
  untrimmed and would go stale after the restore).

## v1.0.6 (2026-10-01)

- **Flashcard sync markers** in the Settings list: green dot = history linked to
  a known PDF location, red = the PDF moved/was renamed (resync needed), grey =
  legacy file without a stored path; **Re-check** button re-validates every
  marker (e.g. after a Drive folder finishes loading and the book reappears).
  Study JSONs now persist the full PDF path.
- **Trim Config context-aware red lines:** near a page top you get the current
  page's top line + the previous page's bottom line; near a page bottom the
  current bottom + the next page's top; a fully visible page shows its own two
  lines. Every visible line is draggable and adjusts its own page.
- **Scan Mode always starts from fit-width** — toggling it from any zoom level
  is safe (no "half of half" division of an already-zoomed page).
- Flashcard "Lista" button renamed to **List**.

## v1.0.5 (2026-10-01)

- Release asset renamed to `TumatraPDF-win64.exe` (platform-explicit name).

## v1.0.4 (2026-10-01) — first public release

Everything below was in the first public build:

- **Own update system:** upstream SumatraPDF update checks + all data collection
  (crash upload, symbol download, translation links) removed; the app checks
  THIS repo's `update.txt` daily, downloads the new portable exe from GitHub
  Releases, validates and replaces itself (no installer, no admin).
- **Flashcard data tools** (Settings window): study-folder change **migrates**
  existing JSONs (cross-volume safe); **RESYNC** — automatic adopt-by-name when
  a book moved (opening it re-links the orphaned history), manual **Link to
  PDF...** for renames; rolling **local backups** `backup\1d|3d|7d` refreshed on
  save + **Restore Backup**; **Import from PDF...** — merge a study partner's
  NEW flashcards from another copy of the same book (duplicates skipped, study
  history NOT imported).
- **UI language:** all fork-added labels in English (public release); the
  column tools were renamed — **Scan Mode** (ex Viewport Crop / Two Column) and
  **Two Columns** (ex Two Column 2).
- **Flashcards core** (built up over the pre-release cycle): cloze cards from
  text selections (`S`, masks = gray highlight over the PDF's own text), SM-2
  scheduling per book, study flow (Space/Enter reveal, 1–4 rate, Next/Back,
  auto-center, TC2-aware), Alt+drag multi-select → one grouped card, Order
  dialog (sequential/random + new-cards position), Filter window (page-set
  expression + bookmark mirror + ON/OFF), cross-document sessions (study all
  open PDFs of the window, auto tab-switch), List panel (4 columns with live
  countdown + colored status), Settings window (folder/list/holds).

## Notable bug fixes (no version claim — see LOG.md for dates)

- **Study-state parser:** JSON read side missed the key's closing quote → every
  StudyLoad returned all-zero states → studied cards came back as "new" on each
  relaunch (latent since the first flashcard build; the Settings window's
  per-book stats exposed it). Save side was always correct.
- **Trim Config under Two Columns:** dialog used the engine mediabox with a
  VIRTUAL page number → wrong box + `ReportIf` storm on even pages; now routed
  through `DisplayModel::PageMediaBox` (same class of fix as the earlier
  `GetTileRes` freeze).
- **TC2 render freeze:** `GetTileRes` missed the virtual→physical routing →
  whole-app stall; render cache re-enabled afterwards without jank.
- **Trim black band:** double top-shift in `RenderCache::GetTileRectDevice`
  removed (single correct shift in `GetTileRectUser`).
- **Small-tab freeze:** upstream tab-width freeze mechanism + stale `TabWidth`
  setting kept tabs tiny; freeze code deleted, Alt+Click close added.
- **Contrast overlay leaks:** cross-document layered-window taint cleared on
  document change; `EnsureContrastOverlayState` invariant re-creates/destroys
  the overlay at every flag-write site (trim save/cancel no longer orphans it).
- **ETA free-fall:** countdown feedback loop (ETA shrinking its own input)
  replaced by 3-point recalibration on real events.
- **Markdown contrast:** OFF now removes the injected overlay (was
  `display:none`), per-doc persistence restored on reopen.

## Pre-release development (2026-08 → v1.0.4)

Feature build-up checklist (moved from README):

- [x] Project setup, base build, binary rename + inverted logos
- [x] AutoScroll + ETA + round-step speed table (25–1600 px/min) + toolbar
- [x] Contrast filter + toolbar; themes Light+Dark
- [x] "New Tools" menu with TumatraPDF commands
- [x] Scan Mode (ex Viewport Crop) + margin trim (zoom bug fixed)
- [x] Arch Tools (Scale + Measure) — 2nd toolbar, scale/measure/erase
- [x] Modularization: design tokens (16A), structs (16B), command dispatch
      C2–C5 (16C), window lifecycle extraction, ToolbarLayout, toolbar wrapping
- [x] Flashcards: MVP → cloze → study flow → multi-select → Order → Clean →
      Filter + cross-doc → Config window → Lista/List panel → data tools
- [x] UI language EN; Scan Mode / Two Columns renames; in-app shortcuts sheet
- [x] Sync markers + Re-check; Scan Mode fit-width start; "Lista" → "List"
- [ ] Automated tests for new features (ad-hoc repros exist; suite pending)

Historical fix notes (2026-08-17):

- **AutoScrollShowEta visibility:** field was generated after the `internalRest`
  marker in `cmd/gen-settings.ts` (4th arg true = hidden); moved before the
  marker, regenerated — now visible in Advanced Options.
- **Trim + cache:** trim=on no longer forces the non-cached render path;
  `RenderCache` tile math takes `trimEnabled`, cache freed on toggle + save —
  autoscroll with trim uses cached tiles, no per-frame re-render lag.
- **Per-document invert/contrast persistence:** `InvertColors`,
  `ContrastEnabled`, `ContrastOpacity` added to `FileState`; save/load wired —
  reopen restores the state.
