# Release Notes

Per-version changelog for TumatraPDF — new functions + bug fixes, newest first.
Feature details live in [README.md](README.md); full per-session development
history in [LOG.md](LOG.md).

## Unreleased

- **Toolbar tooltips follow the buttons from launch.** The startup width
  enforcement that makes room for the timer trio and the page-number box
  shifted buttons after their tooltip rectangles had been snapshotted, so
  for the first moments a tooltip could pop up over a neighbouring button
  until the next layout pass re-synced it. Tooltip rectangles are now
  re-synced after each width-enforcement and toolbar-rebuild pass.

## v1.0.15 (2026-10-26)

- **No more crashes when opening the app.** The Arch Tools measurement
  bookkeeping freed its file-path strings while the list still held them,
  so restoring a session could hit an access violation during startup —
  bookkeeping now keeps its strings alive, and the crash report dialog no
  longer opens anything in the browser: on a crash the local report file is
  offered, nothing else.
- **Subtler flashcard highlights.** New cloze cards save into the PDF with
  a 10% highlight opacity instead of 40% — a quiet card marker rather than
  reading emphasis, since the study overlay paints its own per-state colors
  on top. Import follows the same rule.
- **Saving keeps your tools exactly as they are.** Saving annotations into the
  open document reloads it behind the scenes, and that reload used to reset
  per-book state that lives outside the document: the auto-scroll speed, the
  trim margins, Arch Tools scale and measurements, and the contrast/invert
  settings all fell back to defaults on every save. The reload now packages
  the full current state, so a save is transparent to every view setting.
- **Arch Tools toolbar opens at the right height.** The Arch Tools bar used to
  open as a ~90 px tall stacked band (it was laid out at zero width before the
  window was sized, so every button wrapped onto its own row) and stayed that
  way; it now sizes to the window first, like the flashcard toolbar, and opens
  as a single 24 px row.
- **Home hides the secondary toolbars.** While the Home overlay is up (focused
  mode), the Flashcards, Arch Tools and Edit toolbars hide so the home screen
  owns the whole window; they come back exactly as they were the moment Home
  is closed. Previously they stayed on screen below the main toolbar, cutting
  into the Home content. (The same pass now also restores the toolbars when
  switching a markdown document back to a PDF, which the old code missed.)
- **Scanned pages drag like pages.** Clicking the background of a scanned
  PDF and dragging no longer starts an image drag-out (as if pulling the
  page's photo somewhere) — it pans the page like any other drag. The old
  full-page detection (image covering 80% of the page) missed scans with
  margins or tiles; a page with no extractable text is now treated as a
  scan, so its images count as background whatever their exact size. Figures
  in text documents keep their drag-out behavior.
- **Smoother scrolling while studying flashcards.** With flashcard mode on,
  every scroll used to repaint the whole canvas (the fast strip-scroll path
  was conservatively disabled for it); the flashcard masks are
  document-anchored, so the strip path now stays on and scrolling while
  studying feels like normal reading again.
- **Flashcard study time and card counts.** While study mode is on (and no
  answer is revealed), the time spent on each card is credited to the book and
  to a global total — capped at 60 s per card so a forgotten open book does not
  rack up hours. Cards rated are counted too. The main totals live in
  `flashcard-stats.txt` (per-day lines with seconds and cards, survives
  renaming, deleting or moving any book) and each book keeps its own time and
  card count in its file history. The Home screen shows it all: a
  **Flashcards** tab next to the reading ranking (each row: cards rated and
  time), the combined `Flashcards: Today 13 cards / 10m | Total: 100 cards /
8h 20m` line, the calendar hover shows read and flashcard minutes per day,
  and the daily graph gains a second (peru) line for flashcard time.
- **Flashcard study session timer.** The flashcard toolbar's Study button now
  hosts a timer trio in the autoscroll format — `☐ Timer: [30]`. Checking it
  and starting a study session runs a countdown of the configured minutes
  (1-600): a 2 px silver line at the bottom of the screen shrinks toward the
  centre as the session burns down, and when it expires the session ends with
  a beep and the same "∴" overlay the autoscroll timer uses. Unchecking it
  studies without a limit. The toolbar itself was rebuilt to match the main
  one: theme-aware backgrounds in every theme, and every button label
  centred vertically and horizontally.
- **Save toolbar button.** A disk (save) icon right of Print saves annotations
  and edits into the open document — same as `Ctrl + Shift + S`. Greyed out
  while there is nothing unsaved; lights up as soon as a highlight, annotation
  or edit marks the document dirty. The page-number box used to anchor on
  Print's rect and now follows the nearest visible button to its left, so it
  no longer covers the new icon.
- **Toolbar tooltips follow their buttons.** The tooltip control kept the
  position each button had when it was created, so after buttons were hidden,
  the page box widened or the toolbar re-wrapped, a button's tooltip popped up
  over the wrong button (or over nothing at all). Every button's tooltip region
  now tracks the live layout.

## v1.0.14 (2026-10-23)

- **Much lighter scrolling.** Auto-scroll used to invalidate and blit the whole
  canvas on every 64 Hz tick; it now shifts the previous frame and repaints
  only the 1-4 pixel strip the scroll actually exposed (~958x less repainted
  area per tick), falling back to a full repaint whenever the layout changed
  under it (e.g. a lazy page-size measurement) or an overlay that cannot be
  preserved is on screen (flashcard masks, text selection, Arch tools).
- **Lighter flashcard mask overlay.** The mask paint used to loop every card
  with a GDI+ object per rect; it now paints one shared object set and skips
  every card/rect outside the visible area, so books with hundreds of clozes
  no longer pay for off-screen cards.
- **Faster flashcard advance.** The next cards in the study queue prefetch
  their rendered bitmaps, so moving to the next card is a blit instead of a
  page render.
- **Two Columns: favorites and flashcard filters use the column domain.**
  Favorites store the physical page (so the star and menu work for both
  columns of a spread), and a flashcard page filter now matches a physical
  page number when either of its two columns is shown.
- **Flashcard study feedback.** The card being studied is marked in the accent
  colour (orange) instead of grey, and on reveal the rectangle becomes a
  bottom underline so the answer text stays readable.
- **Cross-page flashcards restore fully.** The per-book study JSON no longer
  keeps only the first card of a multi-page cloze, and cards of deleted files
  are pruned from the saved states.
- **Image context menu reports failures.** Crop/Resize, Image > Save / Convert
  to PDF and Copy Image show a notification instead of silently doing nothing
  when the page element has no bitmap.
- **Toolbars follow the theme.** Rebar toolbars painted with the default
  background even on non-default themes; they now use the theme background.
- **Version reporting fixed.** v1.0.12/v1.0.13 builds still reported 1.0.11 in
  About (the version constant was not bumped with the tag), which also made
  the update check loop; the reported version now matches the release tag.

## v1.0.13 (2026-10-21)

- **Trim and Two Columns are per book again.** Both modes are remembered per
  document, and a newly opened file always starts with them OFF instead of
  inheriting the mode the last book was left in. The trim margin distances are
  per document too, and they are applied when the first layout is built, so a
  book opens exactly as it was left (and new files start untrimmed).
- **Bookmarks land on the correct page in Two Columns.** PDF destinations store
  physical page numbers; they are now mapped to the column domain before
  scrolling, so a bookmark opens the LEFT column of the registered page instead
  of the middle of the spread.
- **`Ctrl+Shift+T` reopens the whole window.** Closing a window with several
  documents no longer loses all but the last one: the closed group is restored
  in one go, with the original tab order.
- **Reading time gets a master total.** The Home stats line reads
  `Today <t> | Total: <t>` (the "7d" figure is gone) and the all-time total is
  summed from the daily data, so it survives renaming, deleting or moving a
  book. The times are also drawn in the strong text colour, which fixes them
  being nearly invisible in dark themes.
- **Auto-scroll speed is remembered per book**, so a long read resumes at the
  speed it was last read with.
- **The Trim Config dialog pauses auto-scroll** while it is open and resumes it
  on save or cancel, so the page cannot drift while the margin lines are placed.
- **Auto-scroll timer controls stay inside their button.** They are positioned
  from the Autoscroll button rect looked up by command id (`TB_GETRECT` wants a
  button index, not a command id, which is why they could previously be placed
  over neighbouring buttons), and they hide as a whole when the row is too
  narrow.
- **Finer zoom steps.** Intermediate levels were added so the zoom buttons keep
  a ~1.33 ratio instead of jumping 1.5× (200→300, 400→600, 3200→4800).
- **Flashcards span page breaks.** A cloze whose text selection continues onto
  the next page is now ONE card, not two: every page the selection covers is
  masked at once during study, and the review counter, queue and study history
  treat it as a single card. The List panel shows the page range (e.g.
  `12-13`) and the card label joins the masked text of all its pages. Existing
  single-page cards are unchanged.

## v1.0.12 (2026-10-17)

- **Reading statistics ("Stats").** Time spent reading with auto-scroll
  active is now tracked per document and per day, and shown in a new stats
  band at the TOP of the Home page (its own "Stats" section; recently-opened
  books + search moved below it, separated by a divider line): most-read
  books with proportional silver bars (wheel-scrolls when long),
  "Today / last 7 days" totals, a 365-day calendar heatmap (silver gradient
  by minutes read, black cells for empty days, hover = silver +
  per-day tooltip, TODAY outlined orange, year navigation arrows) and an
  annual days×hours bar chart. The calendar toggles to a **line-graph mode**
  (day × hours curve with month ticks, data points and a TODAY marker).
  Per-document time persists with the file history; the daily data lives in
  a plain `reading-stats.txt` sidecar next to the settings file.
- **Stats heatmap reads like a calendar.** The 365-day grid is now
  month-separated — 12 blocks with gaps, weekday letters (M T W T F S S,
  Monday-first) on the left of every row and 3-letter month names centered
  under the columns they span; the line graph labels each month segment on
  the axis. Year navigation starts at 2026 (the first year with data).
- **Toolbar wraps in coherent groups.** On narrow windows buttons move down
  whole functional blocks instead of spilling half a group: file buttons
  stay together with the page-number box and the Previous/Next page
  buttons, fit/rotate/zoom stay together, and the timer controls
  ("Timer:" box + −/+) now live INSIDE the Autoscroll button, wrapping with
  it. No more half-groups or dead empty rows.
- **Home = focus mode.** While the Home page overlay is on, every toolbar
  button hides except Home (shown active, now an icon-only house button);
  toggling Home off restores the toolbar.
- **Auto-scroll auto-pauses** when the window is minimized or loses focus
  (nobody is reading), and resumes automatically when the app is
  reactivated — with a fresh time base so nothing jumps. A manual toggle
  always overrides.
- **Fixed: toolbar buttons could vanish during window drag-resize.**
  Progressively shrinking or expanding the window could leave the toolbar
  empty until the next resize; the layout now recovers by itself and a
  drag-end forces a rebuild. Toolbar text buttons also no longer truncate
  ("Auto-s...") at 100% DPI.

## v1.0.11 (2026-10-15)

- **Fixed: autoscroll could stall permanently** when Two Columns / Trim /
  Contrast / Invert were toggled mid-scroll — transient layout rebuilds were
  misread as "document end" and killed the timer. Mid-rebuild ticks are now
  skipped safely. (Diagnostic `[autoscroll-dbg]` lines stay in the log for a
  while to confirm the fix in daily use — they're low-volume.)
- **Fixed: page drag felt heavy with Two Columns on** — every scroll tick
  rebuilt the whole page layout (twice under the doubled virtual pages).
  The visible-parts computation now updates pages in place.
- **Fixed: couldn't select OCR text in the RIGHT column** (Two Columns) —
  the text hit-test mixed up virtual and physical page numbers, so dragging
  on the right column panned instead of selecting. Selecting now works on
  both columns.
- **Fixed: Shift-hold quick view (Two Columns)** — while holding Shift to
  peek at the full page, touchpad vertical scrolling did nothing (the wheel
  was misrouted as horizontal scroll because Shift was held). And releasing
  Shift now returns to the column you were reading (it always jumped back to
  the LEFT column before).

## v1.0.10 (2026-10-11)

- **Right-click Image tools fixed + Crop/Resize writeback.** The Image menu
  (Replace Image, Edit With Paint, Crop, Resize, Save) was effectively dead —
  its items showed on every right-click and the edits never landed in the PDF.
  Items now appear only when an image is under the cursor; Replace/Paint swap
  the embedded image correctly (with the page cache invalidated); Crop/Resize
  write the edited image back into the PDF on Save (position/aspect preserved)
  in addition to exporting the file. Shift while dragging a resize edge keeps
  the aspect ratio. "Convert page to PDF" is grayed out pending a revisit.
- **New "Edit" mode** — toolbar toggle (like Flashcards/Arch Tools) with a
  +Text/+Img/Crop/Apply toolbar. While on, Ctrl+V pastes the clipboard image
  INTO the current page: the next left-click places it (clamped to the page) as a
  stamp annotation. With Edit off, Ctrl+V still opens the clipboard image
  as a new document.
- **In-page image crop/resize:** clicking an embedded image in Edit mode (or
  right-click → Image → Crop/Resize) draws an editable bounding box directly
  over the page — drag its border/edges to crop/resize, drag inside to move.
  "Crop" dims the outside area; "Apply" writes all pending image edits back
  into the PDF (image stays an image — a pure data swap, no annotation; File >
  Save persists). Resize handles are round, crop handles square (standard
  editor convention); the preview shows the result live during the drag, and
  edits can be composed iteratively (Apply keeps the selection on the edited
  region).
- **Unsaved-changes marker on every edit:** replacing/pasting/cropping an
  image (and every annotation) now shows the red "document modified" dot in
  the tab and highlights Save, so it's clear the PDF must be saved to persist.
- **Resize can go BEYOND the original image area** — Apply rewrites the
  image's draw rectangle in the page content stream itself, so images really
  grow/move on the page (no annotation, no padding). Pages with rotated or
  nested-drawn images fall back to clamping inside the original area.
- **Flicker-free bbox drag** — repaints only the affected strip during drags.
- **Arch Tools: Radius + Ref Line.** "Radius" (3 clicks: chord start, chord
  end, arc bulge point) draws the arc through them and labels the radius in
  the page scale. "Ref Line" (2 clicks) draws a thin subtle gray guide line
  spanning the whole page through both points.
- **Scale dialog: Enter / numpad Enter applies from the length field** (no
  mouse needed); scale labels are white now (were unreadable on dark pages).
- **Fixed: crash on Ctrl+F** — a rect-remap helper wrote through a null
  pointer on the find worker thread (second search after a match).
- **Fixed: edit/flashcard toolbars broke the layout** (the edit toolbar never
  got positioned — a missing field in the layout-snapshot cache made the
  relayout a no-op) and **toggle ON markers** (Invert, Flashcards, Arch, Edit)
  now survive window resizes and document reloads instead of going dark.
- **Fixed: Invert no longer corrupts images** — inverted pages render a true
  per-pixel negative instead of the theme recolor ramp (which crushed hues
  and repainted "link-colored" pixels inside scanned images).
- **Fixed: Save/Reload keeps Invert + Contrast** — the save-a-copy / reload
  path no longer drops the document's Invert and Contrast settings.
- **Ctrl+click on a flashcard highlight opens the flashcard panel (Lista)**
  instead of the annotation editor; first-click slowness got `[perf]` timing
  instrumentation for follow-up.
- **Fixed: Arch Tools scale was never accepted.** Since the per-page scale
  refactor the drawn line was stored against a legacy flag that nothing read,
  so Scale's OK always beeped "no scale line defined". Lines drawn (click or
  drag) now register correctly.
- **Arch Tools: calculator input** — the scale-length field accepts simple
  expressions (`100-20`, `2*50`, `25/2`; `+ - * /`), showing the computed
  result when accepted. Enter (main or numpad) confirms.
- **Arch Tools: pan between clicks** — after the first click pins point 1,
  drag with the right mouse button to pan (and zoom freely) before placing
  point 2 far away. The pinned point is page-anchored and stays put.
- **Arch Tools: "Clean lines" now clears everything on the page** —
  measurements AND the scale line + calibration (previously the green scale
  line persisted and the button looked broken). "Reset Scale" keeps its
  scale-only meaning.
- **Enter confirms in every numeric field** — the autoscroll timer edit now
  applies on Enter and leaves the field (page box already did; numpad Enter
  works everywhere).
- **Conflict gates:** while Arch Tools is on, the Two Columns, Scan View,
  Autoscroll, Trim and Flashcards buttons gray out and their shortcuts/menus
  are blocked; while Flashcards is on, Arch Tools, Autoscroll and Scan View
  are disabled. The toolbars never auto-close each other anymore — anything
  still active can always be turned off.

## v1.0.9 (2026-10-09)

- **Installable version:** every release now ships `TumatraPDF-install-win64.exe`
  alongside the portable exe. Same binary — the `install` in the name flips it
  into installer mode. Installs to `%LOCALAPPDATA%\TumatraPDF` (or Program Files
  with "all users"), registers file associations/preview/filter, creates
  shortcuts, and lists its own entry in Windows "Installed apps"
  (uninstall included). Fully isolated from an upstream SumatraPDF install —
  separate directories, registry keys and settings.
- **Auto-update on installed copies:** the daily update check now upgrades an
  installed copy by running the downloaded installer (previously it refused
  unsigned updates for installed exes). Portable copies keep the direct
  self-replace flow.
- **Fixed update-check version loop:** released exes reported version `1.0`
  regardless of the release tag, so the update prompt reappeared forever. The
  app version now tracks the release (`CURR_VERSION` is bumped with each tag).
- **Right-click on an image:** new **Replace Image...** (pick another image
  from the PC; swaps the embedded image in the PDF) and **Edit With Paint**
  (opens the image in mspaint — saving it there replaces it in the PDF).
  Both require saving the PDF afterwards (File > Save) to keep the edit.
  **Copy Image To Clipboard** is now a top-level context-menu entry (no longer
  nested inside Image).
- **Home button (`⌂ Home`)** on the toolbar (before Open): the Home tab no
  longer auto-opens on startup — it appears only when requested.
- **Home button opens the home page as an overlay** over the canvas — no extra
  tab is created or consumed; dismiss with 🏠 again, `Esc`, or by opening a file.
  The startup home tab stays disabled.
- **Bookmarks toolbar button** (icon-only, between Open and Print): toggles the
  sidebar; its pressed state always mirrors the sidebar's visibility.
- **Responsive toolbar:** on narrow windows the toolbar doubles its height and
  the buttons that don't fit wrap down to a second row (rightmost first)
  instead of being cut off. The autoscroll timer controls move with the
  Autoscroll button and hide as one unit when there's no room.
- **Sidebar starts hidden:** every document opens with the
  favorites/bookmarks sidebar OFF (the old "show TOC by default" heuristics
  are gone) — enable it manually when wanted.
- **Fixed:** digits (`1`–`4` and all bare number keys) could not be typed into
  the page-number and autoscroll-timer fields — the flashcard rating keys were
  swallowing them outside study mode. Digits now always type into edit
  fields; Ctrl+digit zoom presets are unchanged.
- **Fixed:** an empty black ETA box appeared in the toolbar when opening a
  PDF without autoscroll running. The ETA label now stays hidden until
  autoscroll is actually on.

## v1.0.8 (2026-10-01)

(ships the keyboard-sheet and trim-features below together — their code landed
after the v1.0.7 tag)

## v1.0.7 (2026-10-01)

- Auto-update verified end-to-end; release asset renamed to
  `TumatraPDF-portable-win64.exe`.

- **Flashcard Import window** (new toolbar **Import** button): compare another
  copy of the same book with yours — `| Document | Cards |` table, dry-run
  `New cards: N` count, one click creates only the DIFFERENT flashcard-tagged
  highlights (duplicates skipped, study history NOT imported). Supersedes the
  old Settings → Import path.
- **Flashcard Session window** (new toolbar **Session** button): search all
  books in your study history, multi-select via checkboxes and **Open Session**
  launches a new TumatraPDF window with every selected book — ready for
  session-wide (cross-document) study.
- **Settings window polish:** toolbar button renamed Config → **Settings**;
  document list gained a column-header strip aligned with the rows.
- **Filter window dark-theme fix:** the expression field and the bookmark list
  no longer paint white in dark themes.
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
