**English** · [Español](README.es.md)

# TumatraPDF

A PDF reader for long sessions and built-in flashcards motors (in development).

## 📹 See it in action

[![TumatraPDF presentation](https://img.youtube.com/vi/1lx40vES_sk/maxresdefault.jpg)](https://youtu.be/1lx40vES_sk)

**▶ Watch the presentation:** [Tumatra PDF v120 with Flashcards](https://youtu.be/1lx40vES_sk) — a walkthrough of the app for long reading sessions and built-in flashcards.

## Credits

This project is a fork of **SumatraPDF** — a lightweight, fast, open source PDF reader for Windows.

- **Original repository:** [github.com/sumatrapdfreader/sumatrapdf](https://github.com/sumatrapdfreader/sumatrapdf)
- **Main author:** [Krzysztof Kowalczyk (kjk)](https://github.com/kjk)
- **Base version used:** Pre-release `3.7.20958`

## 🆕 Features

The features below were developed on top of SumatraPDF and **do not exist in the original version**. They follow the same project philosophy — native C/C++ code, Win32 interface, no external dependencies.

> **Note for Krzysztof Kowalczyk** (creator of SumatraPDF): these features were implemented as **direct patches in `sumatrapdf-src/src/`** — the upstream code is preserved in `sumatrapdf-src/`. The `src/features/` and `src/hooks/` directories **do not exist**. Feel free to incorporate any of them into the official SumatraPDF.

---
### READING

#### EYE STRESS / Bright controls
Bright and contrast controls for long sessions reading on screens.
* `Invert`: This was already present in original app. Just added a on/off button.
* `Contrast` `+` `-`: **New feature**:
	* **PDF**: adds an adjustable black filter over all pages
	* **Markdown**: change letter-colors to darker tones
___
#### AutoScroll with timer & ETA
- `[] Timer` `Autoscroll` `+` `-` Continuous speed adjustable auto-scroll with a timer.
	- `F9 key`: start/stop
	- `F7 (speed down) / F8 (speed up) key` quick adjustments
- `ETA` estimated time to finish the book based on current speed
- `Ctrl:hold` quick pauses → resume when `Ctrl:release`
- `[x]` 5m `Timer`: Insert the time you want your reading session to last

> `Autoscroll:on` feeds _profile data_ with your reading time, reflecting on `Home`/`Graphs`.
> Last speed used persists for each document.
> Works on PDF and Markdown.

Also works together with other tools: `Two Columns:on` `Trim:on` `Invert` `Contrast` etc.
___
#### Two Columns
Quick split two column documents for single column continuous reading.
* `Two Column`: Divide each page in two virtual Left/Right pages and stack them in vertical order so you don't have to scroll horizontaly

Also works together with other tools: `Autoscroll:on` `Trim:on` `Invert` `Contrast` etc.

> When `Autoscroll:on`
* `Shift key:hold`: Quick stops _autoscroll_ and render page original layout so you can read full page graphs or tables in between the columns (very common layout in educational books)
- `Ctrl:hold` quick pauses → resume when `Ctrl:release`
___
#### Trim
* `Trim` Quick remove empty spaces in begining and ending of pages, optimizing reading when `Autoscroll` `Two Columns` buttons active
* `Trim Config`: Draw draggable _Red Lines_ for quick adjustment of page limits.
- Remembered per document; new files start untrimmed.

Also works together with other tools: `Two Columns:on` `Autoscroll:on` `Invert` `Contrast` etc.
___
#### Hold

- `Hold`: Quick stores your reading position so you can go to another session of the book and quickly return for where you stopped
___
### FLASHCARDS
Anki style cards made inside PDF documents.
#### Creating
Quickly create cards (cloze style) inside PDFs. Renders highlight mask over the PDF text/region.
- `Text Selection`→`Flashcard floating button ('s' key)`: Turn text selections into cards (cloze style)
- `Ctrl:hold`+`Click draw rectangle`→ `s key`: Turn rectangle into card (image oclusion style)
- `Alt key`: allows multiple text/rectangle selections → `s key` → single card with the group
- `Ctrl`+`Click` flashcard highlight: opens _flashcards list_
___
#### flashcards toolbar
Secundary toolbar allows you to study the cards inside this PDF. SM-2 spaced repetition algorithm.
- `Study`: Start rendering masks and evaluating process. Rate cards with keyboard `1-4` numbers.
- `1`: Again | `2`: Hard | `3`: Good | `4`: Easy
	- Evaluating cards change review time for next sessions.
	- Search about `spaced repetition` and `Anki` is recomendable.
- `Delete`: Deletes active _card_
- `Order`: modify order of _revised_ and _new_ cards
- `List`: Cards inside this document
- `Filter`: Review cards from a specific region, chapter (bookmarks) of one document OR to study between multiple documents opened in the same **session** 
- `Import`:
	- Convert normal highlights to flashcards (duplicate them so the original formating is kept)
	- Import cards from **same document** edited from another person. You can split the card making work with your study friends then import their cards into a single shared file.
- `Session`: Shows last documents with cards in you recently studied

> Flashcards use normal highlights mechanism so any card save inside a document stays as a normal PDF and can be open normally in any other software
> You can 'see' the regions marked as flashcards in other PDF software but you will only be able to study it inside `Tumatra PDF`

Study history are stored inside a `Profile Data` folder you can change in `Profile Sync`. You can also root it in a cloud sync folder such as _Google Drive_ and feed it between multiple PCs (only windows for now - multiplatform is a future goal).
___
### Tabs & sessions ⭕🟢

#### SESSIONS
Sync _sessions_ for quick open multiple documents commonly used for different study sessions. Restore any group on startup or on demand; groups auto-sync as tabs are added, removed, or closed.
- `⭕`: Not synced session. Any book open here will not be recovered if you close the window.
- `🟢`: Synced session. Books in this session will be remembered when you reopen the session.
- `New Session`: Create a new synced session to recover your common files and store reading and flashcard study time inside this session.
- `Click ⭕/🟢`→ Open dropdown list → `Right click`:
	- `Open in new window`: Open new window with all synced books
	- `Rename`
	- `Delete`
- `Ctrl+N` starts an unsynced window.
- `Alt+Click` on tabs to close documents - `[x]` button was removed
___
### Stats & profile

#### Reading stats

- Reading with `autoscroll` track per document and per day, shown on a Home dashboard.
- Most-read ranking, Today/Total totals, 365-day heatmap, and annual graph; master total survives renaming or moving a book.
___
#### Profile memory & sync

- All reading time, flashcard data, and per-book state in a single `tumatraPDF-data_1.json` that can be exported, imported, and synced (e.g. via Google Drive).
- Home "🔁 Sync" button opens the Profile Sync window; 9 rolling local backups; `profileDataDir` setting points at a synced folder; portable→install import.
- Profile Sync: Set data folder (one root for profile + backups + stats + study), correct merge (keeps the larger daily total and the newer per-card state), honest UI with a status line and keyboard-navigable Backups.
___
### Editing

#### Image editing (right-click on an image)

- Replace Image, Edit With Paint, Crop/Resize, and Copy Image to clipboard — shown only when an image is under the cursor.
- Crop/Resize edits are written back into the PDF on Save; aspect preserved with Shift-drag.

#### Edit mode

- Paste a clipboard image into the current page and place it with a click; drag handles to move/crop it; Apply writes edits back into the PDF.
- Toggle on/off from the toolbar; mutually exclusive with Arch Tools and Flashcards.

#### Capture tool (Shift+S)

- Draw a rectangle over the page to capture it at the document's DPI and copy it to the clipboard.
- Works on the page the drag started on; auto-exits after capture; Esc cancels.
#### Arch Tools (scale & measure)

- For architecture PDFs: draw a line, enter its real length, then measure x/y/length in real-world units.
- Radius (3-click arc) and Ref Line (full-page guide line); erase individual lines with E.
- Conflict gates keep Arch Tools, Flashcards, AutoScroll, Scan Mode, and Trim from overlapping.
#### Editable bookmarks sidebar

- Add bookmarks for the current page; right-click to Rename, Delete, or Update; drag to reorder and indent with a white guide line.
- Changes persist into the PDF on save and survive reopen.
___
### Update

#### AUTO update system

- The app checks its own GitHub Releases (`update.txt`) daily instead of upstream SumatraPDF checks; downloads, validates (MZ header + size), and self-replaces (portable) or runs the installer (installed).
- Fully isolated from upstream SumatraPDF: own directory, registry keys, settings file, and exe name.

---
## Download + Update System

- **Releases:** built by GitHub Actions from `v*` tags — grab `TumatraPDF-portable-win64.exe` (just run) or `TumatraPDF-install-win64.exe` (installer) from [Releases](https://github.com/FelipeTesta/TumatraPDF/releases)
- **In-app auto-update:** daily check of the repo's `update.txt`; when a newer version exists, the app downloads it, validates it, and self-replaces (portable) or runs the installer (installed). No installer, no admin needed for portable.
- **Isolation:** installing TumatraPDF does NOT touch an existing upstream SumatraPDF install (separate directories, registry keys, exe and settings names).
- **Privacy:** the only network call the app makes on its own is the update-check GET to this repo. All upstream SumatraPDF data collection (crash upload, symbol download, translation links) was removed — crash dumps stay local.

### Windows may ask before running

TumatraPDF is not code-signed, so the first time Windows runs a file downloaded from the internet it can show a warning ("Windows protected your PC") and ask you to confirm.

Click **"More info" -> "Run anyway"**; or right-click the file -> Properties -> tick **"Unblock"** at the bottom -> Apply; or in PowerShell: `Unblock-File .\TumatraPDF-portable-win64.exe`.

The downloaded installer carries the Mark of the Web (warns once when you launch it); the app it installs is extracted locally from the embedded payload and never carries the mark, so the installed app does not warn. The portable is the downloaded file itself — it keeps the mark and must be unblocked once ("More info -> Run anyway" / Properties -> Unblock); we removed the startup self-unblock because it is an ineffective SmartScreen workaround for an unsigned binary AND a classic AV false-positive trigger. Windows remembers the approval for that file.

### Antivirus false positives on unsigned builds

If an antivirus flags or quarantines the portable or installer: compare the file's SHA-256 with the hashes published in the release notes. If the hashes match, the file is exactly what we published and the detection is a false positive. Add an exclusion for the file in your AV (per-AV steps vary) rather than disabling protection. If the detection persists, open an issue with the AV product name and the detection ID.

## Architecture

Based on SumatraPDF:
- **Language:** C, C++
- **Build:** Premake5 → Visual Studio 2022
- **UI:** Native Win32 API
- **PDF engine:** MuPDF
- **Formats:** PDF, EPUB, MOBI, CBZ/CBR, FB2, CHM, XPS, DjVu, Markdown (.md/.markdown)

## Status

History + fixes: see [CHANGELOG.md](CHANGELOG.md) and [LOG.md](LOG.md).
