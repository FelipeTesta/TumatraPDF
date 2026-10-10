# TumatraPDF

PDF reader based on [SumatraPDF](https://github.com/sumatrapdfreader/sumatrapdf), an open source project (GPLv3) developed by [Krzysztof Kowalczyk](https://github.com/kjk) and contributors.

## Goal

Create an enhanced version of SumatraPDF with additional features focused on reading books and long documents, while maintaining compatibility with upstream updates.

## Credits

This project is a fork of **SumatraPDF** — a lightweight, fast, open source PDF reader for Windows.

- **Original repository:** [github.com/sumatrapdfreader/sumatrapdf](https://github.com/sumatrapdfreader/sumatrapdf)
- **Main author:** [Krzysztof Kowalczyk (kjk)](https://github.com/kjk)
- **License:** [GNU General Public License v3.0](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/COPYING)
- **Official site:** [sumatrapdfreader.org](https://www.sumatrapdfreader.org)
- **Base version used:** Pre-release `3.7.20958`

We thank Krzysztof and all contributors for the excellent work on SumatraPDF.

## 🆕 TumatraPDF Exclusive Features

The features below were developed on top of SumatraPDF and **do not exist in the original version**. They follow the same project philosophy — native C/C++ code, Win32 interface, no external dependencies.

> **Note for Krzysztof Kowalczyk** (creator of SumatraPDF): these features were implemented as **direct patches in `sumatrapdf-src/src/`** — the upstream code is preserved in `sumatrapdf-src/`. The `src/features/` and `src/hooks/` directories **do not exist** (README diagram was aspirational). Feel free to incorporate any of them into the official SumatraPDF.

---

### 1. ⏱️ AutoScroll with Timer and ETA

**What it does:** Continuous automatic document scrolling with speed control, scheduled stop timer, and estimated time remaining (ETA) to finish the book.

**How it works:**

- A high-precision timer (`SetTimer`) fires `MoveDocBy(0, dy)` on each tick, moving the document vertically
- Sub-pixel accumulator ensures smooth scrolling even at low speeds
- ETA calculation uses: `remaining pages × page height / current scroll speed`
- ETA is computed once on start / speed change / page change and then counts down by wall clock — no per-second recalculation, and only the label (not the whole toolbar) is repainted, so the timer stays smooth
- Scrolling pauses automatically when clicking on the page (for text selection)
- Stops at the end of the document (`IsAtDocumentEnd()`)

**Controls:**

| Shortcut | Action                |
| -------- | --------------------- |
| `F9`     | Start/Stop AutoScroll |
| `F7`     | Increase speed (+20%) |
| `F8`     | Decrease speed (-20%) |

**Toolbar interface:**

- Play/Pause toggle button (▶/⏸ icon)
- - and - buttons for speed adjustment
- Speed label `%d px/min` in dedicated toolbar slot via fake placeholder button `SpeedInfoId` + `TbSetButtonDx` (cloned page-counter pattern) — occupies space like a native control, no overlap
- Numeric field for timer (minutes, 0 = no limit) — `[checkbox][Timer:][input]` anchored to the Autoscroll button: moves with it on resize/wrap, hidden as a whole when the wrapped toolbar leaves no room
- Label showing ETA (e.g.: "⏱ 12min remaining") — works on both PDF (fixed pages: remaining height / speed) and Markdown (webview `autoscrollProgress` ~100ms → `DocController::OnAutoScrollProgress` → `UpdateToolbarEtaText`); the label stays hidden until autoscroll is actually on (no empty black box at document open)

**Persistence:** Speed and timer saved in `TumatraPDF-settings.txt` (`AutoScrollSpeed`, `AutoScrollTimerMinutes`). Fresh docs start at min speed `0.008×` (`kMinSpeedMultiplier = 0.008f` ≈ 100 px/min — never-used default). Persistence unchanged; any persisted value below min clamps to min.

**Advanced Options:** `AutoScrollShowEta` (default `true`) — set to `false` to hide the ETA label in the toolbar entirely.

**Markdown (.md):** Works in WebView2 mode (default) via `window.scrollBy(0,dy)` JS injection; stop-at-bottom via JS `autoscrollBottom` → native `DocController::OnAutoScrollBottom`; ETA via JS `autoscrollProgress` → `OnAutoScrollProgress` → ETA `remainingPx / AutoScrollPxPerSec`. MuPDF fallback (`useFixedPageUI=true`) unchanged. F7/F8 multiplier shared. Toolbar buttons (toggle/±) now visible/enabled for .md — fixed 2026-08-25 (was `AsChm()||AsMarkdown()` CHM blacklist → `AsChm()` only + `removeIfMarkdown[]` for fixed-page cmds). PDF ETA path untouched — no regression.

> **Timer UI (Phase 15):** `[checkbox][Timer:][input]` control added to toolbar before Autoscroll button — **BUILD OK** (awaiting user practical test — see `LOG.md` Phase 15).

---

### 2. 🎨 Contrast Filter (Overlay)

**What it does:** Applies a semi-transparent black overlay over the document, simulating brightness/contrast reduction without modifying the original PDF.

**How it works:**

- An overlay window (`WS_EX_LAYERED | WS_EX_TRANSPARENT`) is created as a sibling of `hwndCanvas`
- The `WS_EX_TRANSPARENT` attribute ensures mouse clicks pass through the overlay — text selection and navigation continue to work normally
- Opacity is controlled via `SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA)`, where alpha = 255 × (transparency / 100)
- The overlay automatically resizes with the window (`WM_SIZE`)

**Controls:**

| Shortcut       | Action                         |
| -------------- | ------------------------------ |
| `Ctrl+Shift+C` | Enable/Disable contrast filter |

**Toolbar interface:**

- Toggle button (🌓/☀ icon)
- Slider/numeric input from 0% to 100% (0% = no overlay, 100% = full black)

**Persistence:** State and value saved (`ContrastEnabled`, `ContrastOpacity`). Fresh docs (no `FileState`) reset `contrastEnabled=false` / `contrastOpacity=50` / `autoScrollSpeedMultiplier=0.1f` — no cross-tab leak (`LoadDocument` else-branch). **Win32 overlay cross-doc leak fix:** `DestroyContrastOverlay(win)` called unconditionally on document change (`LoadDocument`) and in OFF toggle branch for all modes (clears tainted cross-doc Win32 layers).

**Markdown (.md):** Works in WebView2 mode (default) via injected `div#__sumatra_contrast` CSS overlay (`Eval`, `pointer-events:none`, max `z-index`); opacity `contrastOpacity*255/100/255.0f` — same math as Win32 overlay; OFF now `d.remove()` not `display:none`. Persisted via `RestoreContrastOverlay` on `OnDocumentComplete`. MuPDF fallback unchanged. Per-doc persistence applies to .md too — reopen restores saved `ContrastEnabled`/`ContrastOpacity`; black veil on reopen = persisted ON → `Ctrl+Shift+C` toggles off (by design, matches PDF).

---

### 3. 📖 Scan Mode & Two Columns (ex Viewport Crop)

**Status:** 🟢 Complete

**What it does:** Displays two-column pages one column at a time (Scan Mode) or as stacked narrow columns (Two Columns), maximizing screen space and optimizing auto-scroll reading flow.

**Names (2026-09-30):** the column-by-column toggle is **Scan Mode** (ex Viewport Crop / "Two Column"); the stacked-columns variant is **Two Columns** (ex "Two Column 2"). Toolbar + `New Tools` menu labels follow the new names. Turning Scan Mode on always first fits the page width, so toggling it from any zoom level is safe (no "half of half" division).

**How it works:**

- The viewport is cropped by user-defined margins (left, right, top, bottom, inter-column gap) in pixels
- In two-column mode, each column is displayed sequentially: left → right → next page left
- The zoom level adjusts like "fit-width" but constrained within the trimmed area
- A quick toggle (double-click or shortcut) temporarily shows the full page — useful for tables or images — and returns to crop view without losing settings or page position

**Configuration dialog:**

| Input            | Unit | Description                  |
| ---------------- | ---- | ---------------------------- |
| Left margin      | px   | Crop from left edge          |
| Right margin     | px   | Crop from right edge         |
| Top margin       | px   | Crop from top edge           |
| Bottom margin    | px   | Crop from bottom edge        |
| Inter-column gap | px   | Gap between two text columns |

**Quick toggle:** Temporarily switch to full-page "fit width" view and back without resetting crop settings or losing the current page position.

**Interface:**

| Access                      | Action                      |
| --------------------------- | --------------------------- |
| Menu: New Tools → Scan Mode | Enable/disable              |
| Toolbar button              | Toggle on/off               |
| Config button (gear icon)   | Open margin settings dialog |
| Quick toggle                | Temporarily show full page  |

---

### 3.5 ✂️ Trim (Top/Bottom Margin Elimination)

**Status:** 🟢 Complete (2026-08-15)

**What it does:** Eliminates top/bottom margins from the rendered pages in continuous reader mode, optimizing screen usage.

**How it works:**

- Clip-based render elimination (`RenderPageArgs.pageRect`) — NOT a zoom hack
- Trim Config dialog with "margin-top" / "margin-bottom" buttons
- Draggable red 2px horizontal line to set the trim distance
- Context-aware lines: every margin line visible in the viewport is shown (near a page top you also get the previous page's bottom line; near a page bottom the next page's top line; a fully visible page shows its own two lines) — drag any of them
- While the Trim Config dialog is open, trim is temporarily disabled so the FULL page is visible for placing the lines; it comes back on Save/Cancel
- ✅ button saves the distances

**Controls:**

| Access             | Action             |
| ------------------ | ------------------ |
| Toolbar button     | Trim on/off        |
| Trim Config button | Open config dialog |

---

### 4. 🔄 Own Update System (replaces upstream checks)

**What it does:** The app checks for its OWN new versions (this repo's GitHub Releases) — upstream SumatraPDF update checks and all upstream data collection were removed.

- Daily check of the repo's `update.txt`; a newer version prompts and downloads the update, validates it (MZ header + size)
- **Portable copy:** replaces its own exe via a detached helper script (no installer, no admin)
- **Installed copy:** runs the downloaded installer (`-install`) — it reuses the install dir remembered in the registry and upgrades files in place
- Releases are built by GitHub Actions from `v*` tags; every release ships TWO assets: `TumatraPDF-portable-win64.exe` and `TumatraPDF-install-win64.exe` (same exe — the `install` in the name flips it into installer mode)
- Installed build is fully isolated from upstream SumatraPDF: own install dir (`%LOCALAPPDATA%\TumatraPDF` / Program Files), own registry keys (`TumatraPDF*` progIDs, Uninstall, App Paths, RegisteredApplications), own settings dir (`%LOCALAPPDATA%\TumatraPDF`), installed exe named `TumatraPDF.exe`
- Tracking/merging UPSTREAM SumatraPDF changes is a manual process documented in `MERGE.md`

---

### 5. 📐 Arch Tools (Scale + Measure)

**Status:** 🟢 Complete + E2E verified (s33, 2026-10-09)

**What it does:** For architecture PDFs (vector drawings). Define a scale by drawing/selecting a line and entering its real length; then measure any line to get real-world x / y / length.

**How it works:**

- A second toolbar (below the main one) opens via the "Arch Tools" toggle button on the main toolbar. Lines (scale + measurements) are visible only when Arch Tools = ON.
- **Scale:** draw a line on the page (snaps to vector endpoints), open the floating Scale dialog, type the real length + Enter/✅ → computes the scale factor (page units / real unit). Drawing a new line replaces the previous one.
- **Calculator input:** the length field accepts simple expressions — `100-20`, `2*50`, `25/2` (+ - * /, left-to-right). Enter (main or numpad) accepts the computed value.
- **Pan between clicks:** after the first click pins point 1, drag with the RIGHT mouse button to pan the page (zoom also works) and click point 2 far away — the pinned point is stored in page coordinates and stays put.
- **Measure:** draw lines; each shows its real x / y / length (in the chosen unit) as a label on the page.
- **Clean lines** (`CmdArchClear`): clears all measurements + the scale line/calibration of the page. **Reset Scale** (`CmdArchResetScale`): clears scale calibration only.
- **Erase:** hold `E` while Arch Tools is ON → red "+" cursor; click a drawn line to delete it individually.
- **Esc:** cancels the current draw / closes the Scale dialog.
- **Conflict gates:** while Arch Tools = ON the Two Columns, Scan View, Autoscroll, Trim and Flashcards buttons gray out (and their shortcuts/menus are blocked — turning a still-active one OFF stays possible). While Flashcards = ON, Arch Tools, Autoscroll and Scan View are disabled the same way. The toolbars never auto-close each other.

**Controls:**

| Access                           | Action                          |
| -------------------------------- | ------------------------------- |
| Main toolbar "Arch Tools" toggle | Show/hide 2nd toolbar + lines   |
| 2nd toolbar "Scale"              | Enter scale mode (opens dialog) |
| 2nd toolbar "Measure"            | Toggle measure mode             |
| 2nd toolbar "Clean lines"        | Clear measurements + scale line |
| 2nd toolbar "Reset Scale"        | Clear scale calibration         |
| Right-drag (while drawing)       | Pan the page between clicks     |
| Hold `E` + click                 | Erase individual line           |
| `Esc`                            | Cancel draw / close dialog      |

**Technical:** `sumatrapdf-src/src/ArchScaleDialog.{h,cpp}` (floating dialog + expression evaluator), `sumatrapdf-src/src/ArchVector.{h,cpp}` (PDF vector segment extraction + snap + hit-test), `sumatrapdf-src/src/Canvas.cpp` (draw overlay + erase + right-drag pan), `sumatrapdf-src/src/Toolbar.cpp` (2nd toolbar + conflict gates in `IsCmdEnabled`), `sumatrapdf-src/src/SumatraPDF.cpp` (command handlers + FrameOnCommand gates + RelayoutFrame), `src/Commands_ArchTools.cpp`. Draw points stored in PAGE coords (pan/zoom-proof). Modularity gate: global pref `archToolsEnabled` (default on; off = zero cost).

---

### 6. 🎨 Own Visual Identity

- **Binary name:** `TumatraPDF.exe` (allows side-by-side installation with original SumatraPDF)
- **Window:** Title "TumatraPDF" instead of "SumatraPDF"
- **Settings file:** `TumatraPDF-settings.txt` (does not conflict with `SumatraPDF-settings.txt`)
- **Icons:** Logos with inverted colors for visual distinction
- **Registry:** Own keys in Windows Registry (do not interfere with SumatraPDF installation)

---

### 7. 🃏 Flashcards — Cloze SRS over PDF text

**Status:** 🟢 Complete (study flow, filter window, cross-doc sessions deployed 2026-09-25; user live tests pending)

**What it does:** Turns text selections into cloze flashcards — a gray highlight annotation acts as an occlusion mask over the PDF's own text (no content duplicated). Spaced repetition (SM-2) schedules reviews per book.

**How it works:**

- **Create:** select text → `S` (or `Shift+S`). `Alt+drag` accumulates disjoint selections → ONE grouped card (N masks).
- **Study:** toolbar Study button. Card arrives masked, vertically centered (TC2 column-aware). `Space`/`Enter` reveals → `1`–`4` rate (Again/Hard/Good/Easy, numpad too) → auto-advance. Again reinserts the card (relearn). Back = previous card, arrives revealed, keeps scroll. Next = skip without rating.
- **SM-2 state:** per-book JSON in the study-history folder (default: portable app-data dir next to the exe, configurable via the Settings window), keyed by MD5(filePath) — per-book isolation; files carry the book's name for the Config list. All flashcard windows are theme-aware (dark mode included).
- **Order dialog** (Order button): Sequential/Random (shuffle within new/due groups) + new-cards position — before due / after due / mixed (proportional interleave, default). Instant apply.
- **Settings window** (Settings button; ex-Clean, ex-Config): (1) **study-history folder** — shows the current dir, `📂` opens a folder picker; point it at a Google Drive folder to keep every book's history backed up/synced (empty = default app-data dir; existing JSONs are MIGRATED to the new folder — files already there are kept); (2) **history list** — every book in the study dir with `|PDF | cards | due | last reviewed|` (~10 rows + scroll, column header strip aligned with the rows); (3) **Clear Selected** (2s orange hold) — clears the SELECTED book's history; (4) **Clear All** (5s red hold); (5) **Link to PDF...** — manual resync: point an orphaned history at a renamed/moved book; (6) **Restore Backup** (5s red hold) — restores the newest snapshot; (7) **sync markers** — a dot before each name: green = history linked to a PDF at a known location, red = PDF moved/renamed (use Link to PDF... to fix), grey = legacy file; the **Re-check** button re-validates all markers (e.g. after a Drive folder finishes loading). Actions run inside the window; the list reloads live. X/Esc closes.
- **Resync:** history files are keyed by MD5(file path) — renaming/moving a PDF would orphan its history. Automatic: opening a moved book (same base name) ADOPTS its old history file. Manual: Settings → Link to PDF... for renames.
- **Local backups:** rolling snapshots of the whole study dir in `backup\1d`, `backup\3d`, `backup\7d` (refreshed after each save when older than the slot's age). Restore via Settings → Restore Backup.
- **Filter window** (Filter button): page-set expression `1-15;20-25;-22-23;` (ranges add, `-` prefix removes, sequential eval) + bookmark mirror (checkbox per TOC item injects the chapter's range; chapter = first bookmark page → page before next flat bookmark, sublevels included) + Filters ON/OFF (keeps expression) + Clear Filters (2s hold). Filter is per-book; Apply/Enter applies. Fully themed (dark mode included).
- **Cross-document sessions:** checkbox "Study all open PDFs (session)" — study queue over ALL PDF tabs in the SAME window; advancing auto-switches tab and centers the card; SM-2 saved to each book's own JSON. Two distinct study modes: current document × global session.
- **Import window** (toolbar Import button): compare another copy of the same book with yours — `| Document | Cards |` table (current doc + picked source), dry-run `New cards: N` count, `[Import new cards to this document]` creates only the DIFFERENT flashcard-tagged highlights (duplicates skipped, study history NOT imported); cards reload immediately.
- **Session window** (toolbar Session button): search every book in your study history, multi-select via checkboxes (`|PDF | cards | due | last reviewed | tags|`, sync dots included) and **Open Session** launches a new TumatraPDF window with all selected books — combine with the session-wide study/filter for a multi-book review.
- **Per-tab state:** cards, study history and filter live on the tab (`WindowTab::flashcard`) — switching tabs reloads each book's cards; session/queue lives on the window.
- **List panel** (List button, ex-"Lista"): docked right-side panel (resizable via splitter) listing the book's cards in 4 columns — Flashcard (the masked text, extracted live; nothing stored in the PDF), Page, Due (countdown to next review, "0min" when due), Status with colors (new = blue, due = orange, learn = purple, ok = green). Click a row → navigate to the card; refreshed after rating and history wipes.

**Controls:** press `?` (Shift+/) for the in-app keyboard shortcuts sheet — it lists the Flashcards and Auto-Scroll & Contrast sections too, and the sheet window is resizable. The Command Palette (`Ctrl+K`) also exposes every fork command.

**Controls:**

| Shortcut             | Action                          |
| -------------------- | ------------------------------- |
| `S` / `Shift+S`      | Create card from selection      |
| `Space` / `Enter`    | Reveal answer                   |
| `1`–`4` (numpad too) | Rate Again / Hard / Good / Easy |

`1`–`4` are rating keys only while a study card is on screen; otherwise they type digits into edit fields (page box, timer field).

**Technical:** `src/Flashcard.{h,cpp}` (loader, SM-2, study JSON incl. docName + folder setting, history list reader, filter parser), `src/Commands_Flashcard.cpp` (session queue `{tab, cardIdx}`, navigation, rating, xorshift shuffle), `src/FlashcardToolbar.cpp` (2nd toolbar + shared dialog scaffolding + Order/Filter/Config windows, theme-aware), `src/FlashcardSidebar.cpp`, `src/Canvas.cpp` (`PaintFlashcardMasks`), `src/WindowTab.h` (per-tab `FlashcardTabState`). Rich `[fc]` logging for bug diagnosis.

---

### 8. 🖼️ Image Editing (right-click on an image)

- **Copy Image To Clipboard** is a top-level context-menu entry (was nested in Image >).
- **Replace Image...** — pick another image from the PC, confirm, and the embedded image is swapped in the PDF (same page position). Save the PDF afterwards (File > Save) to keep the change.
- **Edit With Paint** — the image opens in mspaint; every time you save there, the PDF image is replaced automatically. Save the PDF afterwards to persist.

**Technical:** `EngineMupdf::ReplaceImageWithFile`/`ReplaceImageByIdx` (pageNo + imageIdx identity from the clicked element; mupdf image-store pointer match → XObject dict `pdf_add_image` + `pdf_dict_put`), 1.2s mtime watch timer for the external-edit flow, GDI+ temp-PNG writer.

Also: **Home (`⌂`) toolbar button** before Open — the Home tab no longer auto-opens at startup; it opens only on request.

### 9. 🧰 Responsive Toolbar + Sidebar/Input Defaults

- **Bookmarks toolbar button** (icon-only, Tabler "bookmarks", between Open and Print): toggles the sidebar; checked state mirrors sidebar visibility (single sync point in `SetSidebarVisibility` — F9/F12/menu/toolbar all converge there).
- **Two-row wrap:** when the window gets narrow enough to clip a button, the toolbar doubles its height and the buttons that don't fit move down to a second row (rightmost first). Works via a debounced rebuild (200 ms after the last resize step) — comctl only computes wrap points when buttons are added, so the toolbar deletes and re-adds its buttons at the new width, then re-applies every per-button state (hidden/enabled, checked toggles, tooltips, page box). ArchTools and Flashcard toolbars get their band heights synced through the same subclass.
- **Sidebar starts hidden:** every document opens with favorites/TOC OFF (previous "show TOC by default" heuristics removed); enable manually via the bookmarks button, F9/F12, or the View menu.
- **Digit keys stay digits:** `1`–`4` (and all bare digits) type into edit fields; only Ctrl+digit combos (zoom presets) remain global shortcuts.

**Technical:** `src/Toolbar.cpp` (`AddAllToolbarButtons`, `RebuildToolbarButtonsForWidth`, `SetToolbarBandToRowHeight`, `WndProcToolbar` WM_SIZE/WM_TIMER), `src/ToolbarLayout.cpp` (timer trio anchored to the Autoscroll button rect), `src/Accelerators.cpp` (`isSafeAccel` bare-digit rule), `src/SvgIcons.cpp` (`gIconBookmarks`).

### 📋 Technical Summary for Developers

| Feature                    | Main files                                                                                                                       | Hook point                                                                                              |
| -------------------------- | -------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| AutoScroll                 | `src/AutoScroll.cpp`, `src/AutoScroll.h`, `src/Canvas.cpp`                                                                       | `OnTimer()`, `FrameOnCommand()`                                                                         |
| Contrast Filter            | `src/ContrastOverlay.cpp`, `src/Canvas.cpp`                                                                                      | `WM_CREATE` (overlay window)                                                                            |
| Viewport Crop              | `src/DisplayModel.cpp`, `src/SumatraPDF.cpp`                                                                                     | `GetViewPort()`, `ScrollYBy()`                                                                          |
| Update                     | `scripts/check-updates.ps1`, `MERGE.md`                                                                                          | N/A (external tool)                                                                                     |
| Arch Tools (Scale+Measure) | `src/ArchScaleDialog.{h,cpp}`, `src/ArchVector.{h,cpp}`, `src/Canvas.cpp`, `src/Toolbar.cpp`, `src/SumatraPDF.cpp`               | `CmdArchToolsToggle`/`CmdArchScale`/`CmdArchMeasure`, `OnPaintDocument`, `UpdateToolbar2State`          |
| Flashcards (Cloze SRS)     | `src/Flashcard.{h,cpp}`, `src/Commands_Flashcard.cpp`, `src/FlashcardToolbar.cpp`, `src/FlashcardSidebar.cpp`, `src/WindowTab.h` | `FrameOnCommand()` flashcard branch, `LoadModelIntoTab` (per-tab state), `OnPaintDocument` (mask paint) |

**How to build:** See "Build" section below.

---

### 🗺️ What's new

The per-version changelog (features + bug fixes, newest first) lives in **[CHANGELOG.md](CHANGELOG.md)**.

---

## Modularity Strategy

**Reality (audit 2026-08-24):** The fork uses **direct patches in `sumatrapdf-src/src/`** — not a separate `src/features/` or `src/hooks/` tree. Those directories do not exist.

```
TumatraPDF/
├── sumatrapdf-src/        # Original SumatraPDF code (preserved)
│   └── src/               # ~810 insertions across 34 files + new modules
│       ├── AutoScroll.{h,cpp}        # NEW module
│       ├── ContrastOverlay.cpp       # NEW module
│       ├── TrimConfigDialog.{h,cpp}  # NEW module
│       └── ... (direct patches to existing files)
├── docs/                  # Documentation
├── FLOW/                  # Planning diagrams
├── README.md
├── LOG.md
└── MERGE.md               # Upstream merge instructions
```

**Principles:**

- Upstream code in `sumatrapdf-src/` is the baseline; patches applied directly
- New features added as standalone modules in `sumatrapdf-src/src/` (AutoScroll, ContrastOverlay, TrimConfigDialog)
- `MERGE.md` documents the upstream update process (fork point ~912ecf2, Aug 2026)
- Informativos (`README.md`, `LOG.md`, `TODO.md`, `BUILD.md`, `MERGE.md`, `FLOW/*.dot`): English + caveman, always (AGENTS.md §13)

## Download + Update System (s26, installer since s32)

- **Releases:** built by GitHub Actions from `v*` tags — grab `TumatraPDF-portable-win64.exe` (just run) or `TumatraPDF-install-win64.exe` (installer) from [Releases](https://github.com/FelipeTesta/TumatraPDF/releases)
- **In-app auto-update:** daily check of the repo's `update.txt`; when a newer version exists, the app downloads it, validates it (MZ header + size). Portable copies self-replace via a detached helper script; installed copies run the downloaded installer, which upgrades the install dir in place
- **Isolation:** installing TumatraPDF does NOT touch an existing upstream SumatraPDF install (separate dirs, registry keys, exe and settings names)
- **Privacy:** the only network call the app makes on its own is the update-check GET to this repo. All upstream SumatraPDF data collection (crash upload, symbol download, apptranslator links) was removed — crash dumps stay local
- Upstream merge tracking documented in `MERGE.md` (manual process)

## Architecture

Based on SumatraPDF:

- **Language:** C, C++
- **Build:** Premake5 → Visual Studio 2022
- **UI:** Native Win32 API
- **PDF engine:** MuPDF
- **Formats:** PDF, EPUB, MOBI, CBZ/CBR, FB2, CHM, XPS, DjVu, Markdown (.md/.markdown)
- **Native auto-update:** `src/UpdateCheck.cpp` (SumatraPDF)

## Status

🟢 Core features complete. Modularization in progress: 16A-16C ✅, 16C-F6 ✅, Design system ✅, Autoscroll ✅, Toolbar wrapping ✅, ToolbarLayout ✅, Flashcards ✅ (full stack: cloze SRS + study flow + Order/Filter/Settings/List windows + cross-doc sessions + data tools). History + fixes: see [CHANGELOG.md](CHANGELOG.md).
