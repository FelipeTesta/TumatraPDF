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

> **Note for Krzysztof Kowalczyk** (creator of SumatraPDF): these features were implemented as **direct patches in `sumatrapdf-src/src/`** — the upstream code is preserved in `sumatrapdf-src/`. The `src/features/` and `src/hooks/` directories **do not exist**. Feel free to incorporate any of them into the official SumatraPDF.

---

### Reading experience

#### AutoScroll with timer & ETA

- Continuous auto-scroll with F9 start/stop and F7/F8 speed (round steps), plus a session timer that auto-stops after N minutes.
- ETA to finish the book shown in the toolbar; scroll speed remembered per document, so a book reopens at its last speed.
- Pauses when you click the page or lose focus; works on PDF and Markdown.

#### Scan Mode & Two Columns

- Read two-column books one column at a time (Scan Mode) or as stacked narrow columns (Two Columns), maximizing screen space.
- Margins and inter-column gap adjustable in pixels; a quick toggle shows the full page, then returns to crop view without losing position.
- Bookmarks and links land on the **left column** of the target page; both modes remembered per document, new files start off.

#### Trim (top/bottom margin elimination)

- Cut top and bottom margins from rendered pages to fit more content on screen.
- Draggable red lines per page (context-aware: shows neighbor pages' lines too); applied on first layout so there is no flicker.
- Remembered per document; new files start untrimmed.

#### Markdown full-width reading

- Markdown opens full-width with word-wrap that reflows on resize; auto-scroll and contrast work in WebView2 mode.

### Page tools

#### Image editing (right-click on an image)

- Replace Image, Edit With Paint, Crop/Resize, and Copy Image to clipboard — shown only when an image is under the cursor.
- Crop/Resize edits are written back into the PDF on Save; aspect preserved with Shift-drag.

#### Edit mode

- Paste a clipboard image into the current page and place it with a click; drag handles to move/crop it; Apply writes edits back into the PDF.
- Toggle on/off from the toolbar; mutually exclusive with Arch Tools and Flashcards.

#### Capture tool (Shift+S)

- Draw a rectangle over the page to capture it at the document's DPI and copy it to the clipboard.
- Works on the page the drag started on; auto-exits after capture; Esc cancels.

#### Contrast filter & dark mode

- Semi-transparent black overlay (Ctrl+Shift+C) for comfortable reading; opacity 0–100%, remembered per document.
- Dark theme (default) plus page-color invert (Shift+I); both survive document reload.

#### Arch Tools (scale & measure)

- For architecture PDFs: draw a line, enter its real length, then measure x/y/length in real-world units.
- Radius (3-click arc) and Ref Line (full-page guide line); erase individual lines with E.
- Conflict gates keep Arch Tools, Flashcards, AutoScroll, Scan Mode, and Trim from overlapping.

### Bookmarks & flashcards

#### Editable bookmarks sidebar

- Add bookmarks for the current page; right-click to Rename, Delete, or Update; drag to reorder and indent with a white guide line.
- Changes persist into the PDF on save and survive reopen.

#### Flashcards (cloze SRS)

- Turn text selections into cloze cards — a gray highlight mask over the PDF's own text, nothing duplicated.
- SM-2 spaced repetition per book; rate 1–4 on reveal; Alt+drag and cross-page selections make one grouped card.
- Study timer, per-day stats, Order/Filter/List windows, cross-document sessions, export/import, and local backups.

### Tabs & sessions

#### Tab groups (named sessions)

- Save the open documents of a window as a named group; restore any group on startup or on demand; groups auto-sync as tabs are added, removed, or closed.
- Dropdown lists groups with Open here / Open in new window / Rename / Delete / New session; Ctrl+N starts an unsynced window.

#### Tab drag guide line

- A white vertical line shows the drop position while dragging a tab; dropping reorders tabs in place.

#### Interface defaults

- Sidebar starts hidden on every document open (enable via the bookmarks button, F9/F12, or the View menu).
- Bare digit keys type into edit fields; only Ctrl+digit zoom presets remain global shortcuts.
- Toolbar wraps to a second row on narrow windows instead of cutting off buttons.

### Stats & profile

#### Reading stats

- Time spent reading with auto-scroll tracked per document and per day, shown on a Home dashboard band.
- Most-read ranking, Today/Total totals, 365-day heatmap, and annual graph; master total survives renaming or moving a book.

#### Profile memory & sync

- All reading time, flashcard data, and per-book state in a single `tumatraPDF-data_1.json` that can be exported, imported, and synced (e.g. via Google Drive).
- Home "🔁 Sync" button opens the Profile Sync window; 9 rolling local backups; `profileDataDir` setting points at a synced folder; portable→install import.

### Update & identity

#### Own update system

- The app checks its own GitHub Releases (`update.txt`) daily instead of upstream SumatraPDF checks; downloads, validates (MZ header + size), and self-replaces (portable) or runs the installer (installed).
- Fully isolated from upstream SumatraPDF: own directory, registry keys, settings file, and exe name.

#### Own visual identity

- `TumatraPDF.exe`, "TumatraPDF" window title, `TumatraPDF-settings.txt`, inverted-color logos, and own registry keys — installs side-by-side with SumatraPDF.

#### Repair Document File

- Right-click a damaged PDF → "Repair Document File": engine-corrected in-place save that clears structural errors without touching page content; the original is left untouched on failure.

---

## Modularity Strategy

The fork uses **direct patches in `sumatrapdf-src/src/`** — not a separate `src/features/` or `src/hooks/` tree. Those directories do not exist.

- Upstream code in `sumatrapdf-src/` is the baseline; patches applied directly (~810 insertions across 34 files plus new modules such as AutoScroll, ContrastOverlay, TrimConfigDialog, Flashcard, ReadingStats, ProfileData).
- `MERGE.md` documents the upstream update process (fork point ~912ecf2, Aug 2026).
- Informativos (`README.md`, `LOG.md`, `TODO.md`, `BUILD.md`, `MERGE.md`, `FLOW/*.dot`): English + caveman-compressed, always.

## Download + Update System

- **Releases:** built by GitHub Actions from `v*` tags — grab `TumatraPDF-portable-win64.exe` (just run) or `TumatraPDF-install-win64.exe` (installer) from [Releases](https://github.com/FelipeTesta/TumatraPDF/releases)
- **In-app auto-update:** daily check of the repo's `update.txt`; when a newer version exists, the app downloads it, validates it, and self-replaces (portable) or runs the installer (installed). No installer, no admin needed for portable.
- **Isolation:** installing TumatraPDF does NOT touch an existing upstream SumatraPDF install (separate directories, registry keys, exe and settings names).
- **Privacy:** the only network call the app makes on its own is the update-check GET to this repo. All upstream SumatraPDF data collection (crash upload, symbol download, translation links) was removed — crash dumps stay local.
- Upstream merge tracking documented in `MERGE.md` (manual process).

## Architecture

Based on SumatraPDF:

- **Language:** C, C++
- **Build:** Premake5 → Visual Studio 2022
- **UI:** Native Win32 API
- **PDF engine:** MuPDF
- **Formats:** PDF, EPUB, MOBI, CBZ/CBR, FB2, CHM, XPS, DjVu, Markdown (.md/.markdown)

## Status

🟢 Core features complete. History + fixes: see [CHANGELOG.md](CHANGELOG.md) and [LOG.md](LOG.md).
