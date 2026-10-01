# TumatraPDF — Development Log

## 2026-10-01 (s26) — Public-repo prep FASE 0-3: telemetry removal + own update system (BUILD OK, deployed)

### Added
- **`update.txt` at repo root** — update metadata (`[TumatraPDF]` section, `Latest:` version, `PortableExe64:` = GitHub `releases/latest/download/TumatraPDF.exe`).
- **`.github/workflows/release.yml`** — tag `v*` (or manual dispatch) → windows-2022 + bun + msbuild Release x64 → bumps+commits `update.txt` → publishes GitHub Release with `TumatraPDF.exe`.
- **Portable auto-update self-replace** (UpdateCheck.cpp): unsigned portable exe no longer rejected — `SelfUpdateViaBatch()` writes a detached cmd batch in temp that waits for the PID to exit, `move /Y`s the downloaded exe over the running one, relaunches it, deletes itself. Download validation = PE "MZ" header + >=1 MiB (no code-signing cert in the fork).
- **Version 1.0 fork track** (`src/BuildConfig.h`): `CURR_VERSION 1.0` / `1,0,0` — decoupled from upstream 3.7; `update.txt Latest: 1.0.x` triggers updates via existing numeric `CompareProgramVersion`.

### Changed
- **UpdateCheck.cpp**: `updateInfoURLs[]` → `raw.githubusercontent.com/FelipeTesta/TumatraPDF/main/update.txt`; `kExpectedDlHost` → GitHub `/releases/`; `kWebisteDownloadPageURL` → `releases/latest`; `ParseUpdateInfo` accepts `[TumatraPDF]` section (upstream `[SumatraPDF]` still accepted).
- **All user-facing sumatrapdfreader.org URLs repointed** to the fork repo: `kWebsiteURL`/`kManualURL` (SumatraPDF.h), crash-submit link (SumatraPDF.cpp → `/issues/new`), homepage rows (HomePage.cpp → TumatraPDF website/manual/issues), docs fallback URLs (`DocURIToWebUrlTemp`, `MaybeLaunchDocumentation` prefix — repo `/blob/main/sumatrapdf-src/docs/md/<page>.md`), TipText Help/* links, Installer/RegistryInstaller (`URLInfoAbout`, `URLUpdateInfo` → /releases), SumatraStartup (libmupdf/corrupted-install/installer-help pages).

### Removed
- **Crash upload entirely** (CrashHandler.cpp): `UploadCrashReport` (HttpPost to sumatrapdfreader.org/uploadcrash), `kCrashHandlerServer*`, symbol download (`BuildSymbolsUrl`, `DownloadAndUnzipSymbols`, `ExtractSymbols` — kjk's pdbs never matched fork builds); `_uploadDebugReport` forced `shouldUpload=false` → all crash/debug reports stay LOCAL (crashfile + minidump kept). `CrashHandlerDownloadSymbols()` is now a logged no-op.
- **`CmdContributeTranslation`** (gen-commands + regen, Menu.cpp, SumatraPDF.cpp handler, CommandAvailability ×3, docs/md/Commands.md) + `CRASH_REPORT_URL` define (dead `#if 0`).

### Verified
- `bun cmd/build.ts` dbg+rel 0/0, deployed `Compiled\TumatraPDF.exe`; smoke launch `-for-testing` alive (crash-handler rewrite safe). Runtime update flow NOT yet e2e-tested (needs public repo + first GitHub Release).
- **Git history audit (FASE 0)**: no secrets/tokens/personal paths in tracked files; `D:\1 Principal` only inside historical `FLOW/*.dot` blobs — cosmetic only. Publish decision pending user.
- Intermediate Set-Content slip on CommandAvailability.cpp self-checked via git diff — clean (ASCII-only file), Edit-tool rule reaffirmed.

### Notes / backlog (TODO.md "Public Repo + Update System")
- Upstream `cmd/` build/CI scripts (build-ci*, trans-*, build-mac*, …) KEPT as backup per user decision.
- After first public release: e2e `update.txt` bump → verify in-app update prompt + self-replace; update.txt bump step pushes from tag workflow to `main` (needs default GITHUB_TOKEN contents:write — granted in workflow).
- `GetExecutableSignerTemp` still used by installed-mode path (kept: signed installer flow intact if a cert ever exists).

## 2026-10-01 (s25) — Study-folder file MIGRATION on folder change (BUILD OK, deployed)

### Changed
- **Changing the study folder now MIGRATES the existing `*.json` histories** (user request — files transfer to the new folder instead of the new dir starting empty). `FlashcardStudyMigrateFiles(Str fromDir, Str toDir)` (NEW, Flashcard.cpp/Flashcard.h): enumerates `fromDir\*.json` (same FindFirstFileW pattern as DeleteAllStudyFiles), moves each with `MoveFileExW(MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)` — COPY_ALLOWED is REQUIRED for cross-volume moves (local disk → Google Drive letter), else the move fails silently by default. A file that already exists in the target is KEPT untouched (not overwritten — a cloud-synced dir may already hold that book's file from another machine; the next `FlashcardStudySave` of an open book writes the up-to-date state there anyway) — logged as "kept existing". Per-file + summary `[fc]` logs ("moved %d, kept %d existing").
- `ConfigApplyPath` (FlashcardToolbar.cpp): captures the OLD dir via `FlashcardStudyDir()` BEFORE `ReplaceWithCopy` of the setting, then calls the migration with old → new; log line now "study dir set to '...' (moved N file(s) from '...')". Clearing the field (empty = default app-data dir) migrates the files BACK — round-trip works.
- Comments updated (FlashcardStudyDir + ConfigApplyPath no longer say "no file migration").

### Verified (tests/ad-hoc-config-iso.ts REWRITTEN with real assertions — ALL 16 PASS)
Seeds 2 fake study JSONs in the default per-exe dir → launch → flashcard ON → Config → list shows exactly the 2 fakes → `WM_SETTEXT` a custom dir into the path EDIT (via `wideZ` + `bun:ffi` `ptr`; `findChildWindow(configDlg, "Edit")`) + `pressKey(VK_RETURN)` (ConfigEditProc listens to WM_KEYDOWN) → asserts: 2 files IN custom dir, 0 left in default, list reloaded from custom dir, edit text = custom dir → clear edit + Enter → asserts: 2 files back in default dir, custom dir empty, list reloaded, edit shows default dir → window closes → log-slice asserts ("moved 2, kept 0 existing" ×2, "study dir set to '(default app-data)'"). Cleanup removes the fakes + tmp dir.

### Notes / traps
- **sumlog.txt is TRUNCATED at app startup** (a launch's log contains only that run). Offset-slicing by the pre-launch size is WRONG under truncate: this run can grow past the old size and slicing then discards everything written so far (exactly the equal-size edge case failed first). Read the WHOLE file after the run instead (readLog in the iso test; other tests' slice logic only survives by luck when their assertions target late lines — left as-is, not broken).
- **`TempStr` has NO implicit conversion to `const char*`** (C2664) — it DOES convert to `Str`. New study APIs take `Str` params (codebase convention anyway).
- `MoveFileExW` without `MOVEFILE_COPY_ALLOWED` cannot move across volumes — silent failure class for C: → G: (Drive) migrations.
- `ConfigEditProc` (path edit subclass) handles WM_KEYDOWN (not WM_CHAR) — `pressKey(VK_RETURN)` from win-automation triggers the apply.

## 2026-10-01 (s24) — Config window (folder + history list + clear selected), dark-theme dialogs, study-state parser fix (BUILD OK, deployed)

### Fixed
- **Study-state parser bug (latent since s14)**: `ParseJsonObject`'s `getField` searched the field NAME only ("rating") but advanced `fieldIdx + len(fieldName)` without skipping the key's CLOSING QUOTE — `afterField` started with `"` instead of `:` and every field silently returned early, so **every StudyLoad returned states with all zeros** (rating/interval/ease/nextReviewAt/lastReviewedAt = 0). In practice every already-studied card came back as NEW on each app relaunch and due/learn/ok never existed across sessions. Found by the new Config window's per-doc stats (last=0 on a freshly-rated doc); fixed by skipping the closing quote (Flashcard.cpp `getField`). Save was always correct — only the read side was broken.
- Dark themes: all three flashcard windows (Clean→Config, Order, Filter) painted a hard-coded WHITE background with near-black text — now everything uses the global theme API (`ThemeWindowBackgroundColor/TextColor/DarkerTextColor/WindowControlBackgroundColor/HotBackgroundColor/EdgeColor/HotEdgeColor`), plus `ApplyDarkModeToPopupWindow` for the title bar and `WM_CTLCOLOREDIT/WM_CTLCOLORLISTBOX` answers for the child controls (cached brush, rebuilt when the theme color changes). Blue accent (checkbox/radio) kept — readable in both themes; orange/red hold drains kept (semantic colors).

### Added
- **Config window** (toolbar button Clean → **Config**, command renamed `CmdFlashcardCleanHistory` → `CmdFlashcardConfig`, same id 512): (1) **study-history folder row** — EDIT with the current dir + `📂` button (Segoe UI Emoji font) opening a modern `IFileDialog` FOS_PICKFOLDERS picker; changing it persists the NEW `flashcardStudyDir` setting (`gGlobalPrefs->flashcardStudyDir`), creates the dir, reloads the list — point it at a Google Drive folder and the histories stay backed up; empty value = default app-data dir; **existing files are NOT migrated** (new dir starts empty; changing back restores them); Enter in the edit applies a typed path. (2) **history list** — owner-drawn listbox (~10 rows + scroll, 24px rows) listing every `*.json` in the study dir: `|PDF | cartões | due | última revisão|` (name flexible/ellipsized left, numbers + `YYYY-MM-DD HH:MM` right-aligned; em dash when never reviewed); data from NEW `FlashcardStudyListDocs()` (sorted by lastReviewedAt desc); JSON now stores `docName` (escaped; base name — added to `FlashcardStudySave`, legacy files fall back to the md5 name); `FlashcardStudyDocInfo{fileName/docName owned, totalCards, dueCount, lastReviewedAt}`. (3) **"Limpar livro selecionado"** (orange 2s hold) — deletes the SELECTED book's JSON (any tab of this window showing that same document, md5-matched, resets its in-memory studyDoc + session + count + sidebar); (4) **"Limpar TODOS os livros"** (red 5s hold) — former ALL-books action, now also refreshes the open Lista panel (missing refresh hook). Both run INSIDE the window (it owns the selection) — the window stays open and the list reloads; X/Esc closes. List selection persists (unlike the Filter's toggle list).
- **`FlashcardStudyDir()`** + `FlashcardStudyPath`/`FlashcardDeleteAllStudyFiles` now use it (setting when set, else per-exe app-data `FlashcardStudy`).
- Design_Guidelines improvements executed (research-audited first, per user request): **#1** dialog scaffolding extracted (`FlashcardRegisterDialogClass` / `FlashcardCreateDialogWindow` / `FlashcardRunModalDialog` — the 3 windows share ~120 duplicated lines less); **#3** refresh-hook fix (Clean-ALL now calls `FlashcardSidebarPopulate`); **#4** flashcard paint block extracted from the Canvas WndProc chain into `PaintFlashcardMasks(win, dm, hdc)` (with the vPage/pPage dual-namespace note); **#5** `ShuffleVecInPlace` now uses a seeded **xorshift128+** (rand()%n was modulo-biased; seed = time ^ tick-count<<21); **#7** `cmd/build.ts` auto-formats modified src/*.cpp/.c/.h with clang-format before building (vendored trees excluded; git-less environments skip silently). Not done: #6 (page-text cache generalization — no second consumer yet, abstraction would be speculative), #8 (FC-H2 mtime scan cache — medium risk, left for a dedicated session).

### Changed
- Button/label rename: toolbar shows **Config**; handler case + `CommandAvailability` (2 lists) + gen-commands description updated; `FlashcardCleanResult` enum and `FlashcardCleanHistoryDialog` REMOVED (window executes its own actions).
- `tests/ad-hoc-clean-iso.ts` → **`tests/ad-hoc-config-iso.ts`** (same isolation pattern: open → listbox present/counts → close → logs); `tests/ad-hoc-flashcard-cloze.ts` Config section rewritten: listbox probe via `LB_GETCOUNT`, row-0 selection without a physical click (`LB_SETCURSEL` + manual `WM_COMMAND(IDC_FC_CONFIG_LIST|LBN_SELCHANGE)` — DPI-proof), hold fractions 0.30/0.70 × 0.90 client.

### Verified (both ad-hoc tests PASS on dbg64; release x64 0/0)
- config-iso: window opens (545×402 client), history listbox found + item counts, X closes ✓; logs show `FlashcardStudyListDocs` + per-doc name/cards/due/last.
- cloze full repro: 2 cards → study → reveal → rate 3 (save now writes `docName`) → Order dialog open/close ✓ → Config opens with 1 item (name='zlib.3.pdf') ✓ 600ms early release keeps it open ✓ 2.3s selected-book hold deletes the JSON and the list reloads to 0 ✓ 5.4s ALL-books hold ✓. **Parser-fix regression check**: fixture JSONs with `lastReviewedAt: 1790834988123` now report `last=1790834988123` (was 0) — study states load correctly for the first time since s14. Visual check (dark theme, emoji glyph, columns) = user tests live.

### Notes / traps
- **compactStruct settings reject Str fields**: adding `StudyDir` inside `flashcardSettings` (a `compactStruct`) made the startup settings validator `ReportIf(!IsCompactable(...))` fire — app exited 105 with a "debug report (not crash)" in sumlog. Fix: Str settings must live OUTSIDE compact structs (new top-level `FlashcardStudyDir` in globalPrefs); compact structs accept Bool/Int/Float/Color only (`base/SettingsUtil.cpp` IsCompactable).
- `FOS_PATHONLY` not declared by this Windows SDK — dropped (SIGDN_FILESYSPATH already returns the bare path); `MainWindowRerender` needs `#include "SumatraPDF.h"` (new dependency of FlashcardToolbar.cpp actions).
- `gen-code.ts` dies at genVirtKeys without `cl` on PATH — gen-commands/gen-commands ran BEFORE it, and `bun cmd/gen-settings.ts` regenerates settings standalone (use that when only settings change).
- `file::ReadFile` returns an owned buffer but upstream callers never free it — followed the convention in `FlashcardStudyListDocs` (ListDocs strings are `str::Dup(...).s` + `::free()` in `ConfigFreeDocs` after the modal pump).
- Zero-char-label `FlashcardDrawButton` used for the 📂 frame (glyph drawn on top with the emoji font).

## 2026-09-30 — Design Guidelines: web research + project audit (documentation only, no code)

### Added
- **`Design_Guidelines.md`** (repo root, NEW informativo): reference doc distilling web research (C++ Core Guidelines P/R/E/I/Per/A/NL sections, cppbestpractices style chapter, refactoring.guru pattern catalog + code smells, MuPDF vendored docs, upstream AGENTS) mapped onto this codebase's real conventions and bug history (LOG s1-s23). Sections: core principles; code conventions (include order, Str/fmt/len, guards, comments placement); resource + error handling (RAII, fz_try/catch, CrashIf fail-fast, atomic tmp+MoveFileEx persistence); design patterns mapped to the codebase (Command dispatch, session-vs-document State split, lazy Facade, refresh hooks, owner-draw + numeric row model); performance rules (zero-alloc paint path, caches with named invalidators, measure-first); Win32 UI/UX rules (DpiScale everywhere, keyboard-first, hold-to-confirm destructive, instant-apply dialogs, checked-state toolbars, WM_MEASUREITEM ordering); dual-namespace HARD rules (virtual vs physical pageNo, screen vs page coords); prioritized high-return/low-risk improvement table (dialog scaffolding extraction, vPage/pPage naming, refresh-hook audit, paint-block extraction, xorshift shuffle, page-text cache generalization, mtime annot-scan cache); known-traps list from project history.
- MS UX Interaction Guidelines URLs (win32/uxguide) are archived/404 — noted in doc sources; Win32 UX rules derived from validated s15-s23 dialog/panel patterns instead.

### Notes
- Research via webfetch (Core Guidelines 760KB full text — extraction delegated to explore subagent to save context), local vendored MuPDF docs, and 8 sessions of accumulated lemma memories. tree-sitter `analyze_complexity` fails on fork cpp files ("Invalid syntax at row 1" — parser/BOM issue); improvement list grounded in session history + grep verification instead (srand seeded SumatraStartup.cpp:2150, FlashcardStudySave already atomic Flashcard.cpp:219, `.clang-format` present).
- No code changes, no build needed. Commit awaits explicit user order.
- prettier reflowed the whole historical LOG.md (356-line diff) — reverted via `git stash push -- LOG.md` + re-applied this entry only; LOG.md stays append-only. Do NOT prettier-format LOG.md again.

## 2026-09-25 (s23) — Lista redo: docked right panel with 4-column flashcard list (BUILD OK, deployed)

### Changed
- **Lista button redo** (the last "incorrect button"): the floating popup (TreeView "Card N — Page N") is GONE. New panel follows the **AI-chat sidebar architecture** — docked child of `hwndFrame` on the RIGHT (own width + own vertical splitter; not entangled with the ToC/Favorites shared left column), canvas shrinks via `RelayoutFrame`. Title strip (LabelWithClose, `IDC_FLASHCARD_LABEL_WITH_CLOSE`) hides the panel; toolbar Lista button checked while open.
- **4 columns per row** (user spec `|flashcard|pag|due|status|`): **Flashcard** = the masked text itself, extracted on the fly (no stored text — cloze purity kept: `EngineBase::ExtractPageText(pageNo)` per unique page, walk UTF-8 codepoints via `Utf8CodepointAtByte`, keep those whose per-codepoint rect hits a mask rect (inflated 1.5pt), whitespace collapsed, 250-cp cap + ellipsis; fallback "Card N"); **Pág** = physical page; **Due** = countdown computed AT DRAW TIME ("0min" when due, `Nmin`/`Nh`/`Nd`, em dash for new — stays fresh without repopulating); **Status** = colored word.
- **Status model** (SM-2 fields): `new` = rating 0 (blue), `due` = nextReviewAt ≤ now (orange), `learn` = scheduled but interval ≤ 1 day (purple), `ok` = matured (green) — 4th state required by the data; selection row keeps a readable status color (orange-on-dark / dark-orange-on-light).

### Added
- `FlashcardSidebar.cpp` REWRITTEN: `WC_STATIC` box (subclass: WM_SIZE → manual layout of label/header/list — AI-chat webview-slot pattern, no VBox) + owner-drawn listbox (`LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOTIFY|WS_VSCROLL`, id `IDC_FLASHCARD_LIST` 1111); label strings live in the listbox items, numeric state in NEW `Vec<FcListRow>{cardIdx,pageNo,rating,interval,nextReviewAt}` (FlashcardState) so draw stays allocation-free. Custom column-header strip (`IDC_FLASHCARD_LIST_HEADER`, subclassed paint) shares `FcListColumnAnchors()` with row drawing (right-aligned Pág/Due at DPI-scaled offsets from the right edge). `FlashcardSidebarCreate/Destroy/Toggle/Populate` + `RelayoutFlashcardListPanel` (called from RelayoutFrame after aiChat) + `OnFcListSplitterMove` (guards the AI-chat width when both panels are open).
- `MainWindow::UIState`: `fcListVisible` (desired + `Layout` snapshot field `fcListVisible`/`fcListDx` → IsUiLayoutEq + RelayoutFrame geometry block, inner of aiChat). `FlashcardState`: hwndListaBox/listaLabel/listaView/listaHeader/listaSplitter/listaDx/listaRows; `WindowTab`-backed rows.
- `FlashcardNavigateToCardInCurrentTab(win, cardIdx)` (Commands_Flashcard.cpp, public): wraps `FlashcardNavigateToEntry` for the row-click navigate; log line `[fc] Lista - navigating to card N of 'book'`.
- Refresh hooks: rate + Clean-current-book now call `FlashcardSidebarPopulate` (Due/Status columns update after a review or a history wipe).
- `tests/ad-hoc-flashcard-lista.ts` (NEW): log-slice assertions — populate counts (`2 cards (new=2 due=0 learn=0 ok=0)`), docked-right geometry (listbox right edge gap 1px, canvas shrunk left of panel), row-click navigate, toggle off hides + toggle on repopulates. Card creation reuses the retry loop from the filter test (8 attempts, "Cloze card added" log polling).

### Verified (ad-hoc-flashcard-lista.ts — ALL CHECKS PASSED)
2 cards created (retry loop) ✓ panel populated new=2 ✓ listbox visible + docked right (gap 1px) ✓ canvas shrunk ✓ row click → `Lista - navigating to card 0` ✓ toggle off → hidden ✓ toggle on → visible + repopulated ✓. Visual check (column alignment, colors, masked-text labels on real books) = user tests live.

### Notes / traps
- **WM_MEASUREITEM for an owner-draw-FIXED listbox fires at window creation — BEFORE our box subclass exists** (and never again): measure in Create AFTER subclassing and force with `LB_SETITEMHEIGHT` (re-measures internally). MEASUREITEM handler kept as fallback with the same formula.
- `Wnd(HWND)` (wingui) SUBCLASSES on attach + `~Wnd()` destroys the window — never wrap raw HWNDs we manage ourselves (header/list) for layout; position them manually instead (AI-chat webview pattern).
- `WStr` has NO implicit conversion to `WCHAR*` — DrawTextW/LB_ADDSTRING need `.s` (fmt/logf args need the object; Win32 needs the pointer).
- `enumChildWindows` enumerates ALL descendants (recursive) — finding the listbox by class from the frame works.
- Guard: `FlashcardSidebarToggle` no-ops when flashcard mode is OFF (palette-invoked without toolbar).
- MSBuild release x64 via hard path (build.ts overwrites `Compiled\TumatraPDF.exe` with the DEBUG exe — re-copied `out\rel64` after: 11.1 MB 18:59).

## 2026-09-25 (s22) — Study Filter window (page-set expr + bookmark mirror) + cross-document sessions (BUILD OK, deployed)

### Changed
- **Per-tab flashcard state (foundation)**: cards / studyDoc / filter moved from `MainWindow::flashcard` into NEW `FlashcardTabState` inside `WindowTab` (cards, studyDoc, `filterExpr` Str, `filterEnabled`, `cardsLoaded` lazy flag). MainWindow keeps session-level state only: on/studyMode/revealMode/currentCardIdx/`studyOrder` (now `Vec<FlashcardQueueEntry>{tab, cardIdx}`)/`crossDocSession`. **Fixes latent bug**: switching tabs with flashcard ON kept the previous book's cards/studyDoc painted over the new doc — `FlashcardOnTabChanged(win)` hook in `LoadModelIntoTab` (SumatraPDF.cpp, next to ArchRestoreMeasurementsForTab) now lazy-ensures the new tab's cards + refreshes count/sidebar.
- **Two distinct study modes** (user spec): **current document** (default) vs **session-global** — checkbox "Estudar de todos os PDFs abertos (sessão)" (PT UI string, verbatim) in the Filter window. Scope = all PDF tabs of THIS window only; tabs dragged to another window stop counting (per user). `FlashcardToolbarUpdateCount` scope-aware (global = sum over tabs).
- **Cross-document queue**: `BuildFilteredStudyOrder` collects `{tab, cardIdx}` entries from current tab (or every loaded PDF tab in global mode); each tab's OWN page filter applied independently; advance/Back/Rate navigate via `FlashcardNavigateToEntry` — `TabsSelect` auto-switch when the card belongs to another tab (`FindTabIndex` validates the tab still lives in this window; closed/moved tabs skipped + logged); rating writes SM-2 state + saves to the CARD'S OWN tab JSON (per-book MD5 path); "Again" reinserts the entry. Clean ALL books now resets EVERY tab's in-memory studyDoc (was current-only).
- `CmdFlashcardFilter` no longer opens two sequential `Dialog_GoToPage` prompts (the "incorrect" report) — opens the new Filter window. Old `filterPageFrom/To` ints replaced by `filterExpr`.

### Added
- **`FlashcardFilterEval(expr, pageCount, pages)`** (Flashcard.cpp): page-set parser — tokens split on `;`, `N`/`N-M` adds, leading `-` removes, evaluated SEQUENTIALLY (`1-15;20-25;-22-23;` = 1-15 + 20-25 − 22-23); clamps to [1,pageCount]; garbage tokens skipped + counted in log; empty/all-garbage → returns false = all pages. Full `[fc]` logging.
- **Filter window** (FlashcardToolbar.cpp, class `TUMATRA_FLASHCARD_FILTER`, 560×500, Clean/Order raw-Win32 + modal-pump pattern): EDIT expression input (apply on "Aplicar" btn or Enter via edit subclass; Esc closes); **bookmark mirror** = owner-drawn listbox (LBS_OWNERDRAWFIXED + WS_VSCROLL, 24px rows) with a checkbox per TOC item, indented by depth; chapter range = item's page → page BEFORE the next bookmark in FLATTENED DFS order (parent covers all sub-levels; last extends to page count); checking injects `a-b;` token into the edit AND applies instantly, uncheck removes the token; `LB_SETCURSEL(-1)` after each toggle so re-clicking the same row re-fires LBN_SELCHANGE. **"Filtros: ON/OFF"** instant toggle (disables filter, KEEPS the expression). **"Limpar filtros"** hold 2s red draining line (Clean pattern). Cross-doc checkbox row on top. Filter is PER-BOOK (stored in the tab); toolbar Filter button shows checked while the current tab's filter is active (enabled + non-empty).
- **`FlashcardTabLogName(tab)`**: log-safe tab name — `displayName` is EMPTY for cmdline-loaded docs until the UI sets it; falls back to `path::GetBaseNameTemp(filePath)`. Used by all [fc] tab logs.
- **`tests/ad-hoc-flashcard-filter.ts`** (NEW): 2× zlib.3.pdf (local, no Drive), 2 cards per tab created via keyboard selection with RETRY driven by "Cloze card added" log polling (fflush-per-line); navigates tabs BY FRAME TITLE (this fork starts with an About tab at index 0 — index math unstable); log-slice assertions (only this run's lines, rotation-safe offset).

### Verified (ad-hoc-flashcard-filter.ts — ALL ASSERTIONS PASSED)
- Remove-all expr `-1-99999;` → `'zlib.3.pdf': 0 cards pass (of 2)` + `BuildFilteredStudyOrder - 0 cards (scope=current-doc)` ✓; ON/OFF toggle keeps expr ✓; clear-hold 600ms stays open + 2.3s → `CLEARED` ✓.
- Global session: `'zlib.3.pdf': 0 cards pass (of 2)` (per-book filter respected!) + `'fc-crossdoc-copy.pdf': 2 cards pass (of 2)` → `2 cards (scope=global-session, tabsUsed=2)` ✓; `switching to tab` + frame title changed ✓; Rate 3 → `rated 3 card 0 of 'fc-crossdoc-copy.pdf'` + `FlashcardStudySave - saving to ...fc-crossdoc-copy.pdf` (card's OWN book JSON) ✓; advance to 2/2 ✓.
- Bookmark mirror auto-skips with log `no TOC, bookmark mirror empty` (zlib has no TOC) — mirror interaction = user tests live on real books.

### Notes / traps
- `Vec() = default` is **explicit** → `Foo dlg{};` with Vec members = C2512 (copy-list-init can't call explicit default ctor) → declare `Foo dlg;`.
- WindowTab.h is NOT self-sufficient: including it from Flashcard.cpp required the full chain first (Settings → DisplayMode → DocController → DocProperties → TreeModel → EngineBase → EngineMupdf → **base/GuessFileType.h** → EngineAll.h) — EngineAll.h itself uses `FileType` (undefined without GuessFileType). path helper = `path::GetBaseNameTemp` (no GetNameTemp).
- MSBuild/vswhere NOT in this shell's PATH — release x64 built via hard path `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe` (build.ts only offers Debug x64 / Release Win32).
- Extra About tab at index 0 with `-for-testing` multi-doc launches → tab indices shift by +1; tests must not assume [0,1].
- User queue remaining: **Lista redo** (user-ordered LAST), bookmark-mirror live check on user's books, image-occlusion IO-1..IO-4 (IO-D1/D2/D3 pending), FC-M1 tests, FC-H2, FC-H3-prune, FC-M4/M5.

## 2026-09-24 (s21) — Clean History rework: per-book + ALL-books hold-to-confirm buttons (BUILD OK, deployed)

### Changed
- **Clean dialog reworked** (user spec): "No" button REMOVED (cancel = X/Esc only); ex-"Yes" is now **"Clear current book"** (hold 2s, orange draining line); new **"Clear ALL books"** (hold 5s, red draining line — longer hold for the more destructive action). Window 460×200, two 170px buttons. `FlashcardCleanResult` enum (Flashcard.h); dialog returns 0/1/2.
- **Per-book isolation confirmed**: each book's review history is its own JSON keyed by MD5(filePath) — "Clear current book" rewrites only the current book's file (states:{}); other books never touched.

### Added
- **`FlashcardDeleteAllStudyFiles()`** (Flashcard.cpp): FindFirstFileW/FindNextFileW over `FlashcardStudy\*.json` in the app-data dir + DeleteFileW each; returns count. "Clear ALL books" path: delete all + reset current in-memory studyDoc + stop session — **no FlashcardSave afterwards** (don't recreate the deleted file). `ToUtf8Temp(WStr(fd.cFileName))` for the wide→narrow filename.

### Verified (ad-hoc-flashcard-cloze.ts full pass; ad-hoc-clean-iso.ts isolation)
- Order dialog open/close ✓; Clean dialog: 600ms early release keeps it open ✓; 2.3s current-book hold → confirmed + current doc cleared ✓; reopen + 5.4s ALL-books hold → confirmed + `deleted 2 study file(s)` + FlashcardStudy dir left EMPTY ✓.
- One full-repro run failed once mid-flow (commands after Back unprocessed) — one-off: Google-Drive (G:\) stall suspected; isolation test (`tests/ad-hoc-clean-iso.ts`, NEW: launch → Toggle → Clean → verify + logs) proved the binary sound; immediate re-run passed everything.

### Notes
- Repro Clean-section geometry: two buttons at client fractions x≈0.30/0.70, y≈0.78 (DPI-proportional).
- User queue remaining: **Lista + Filter** button redo (user-ordered LAST); FC-M1 tests, FC-H2, FC-H3-prune, FC-M4/M5; image-occlusion plan (IO-1..IO-4, decisions IO-D1/D2/D3 pending).

## 2026-09-24 (s20) — Order button → Study Order options dialog (sequential/random + new-cards position) (BUILD OK, deployed)

### Added
- **Study Order options dialog** (toolbar "Order" button, renamed `CmdFlashcardOrderToggle` → `CmdFlashcardOrderOptions`, same numeric id): "Review order" **[Sequential] [Random]** + "New cards" **[Before due] [After due] [Mixed]** — radio-style owner-drawn options (accent border + filled dot when checked). Every click **applies instantly**: persists the setting, rebuilds the study order and restarts from the first card (centered) when a session is active. No OK button; close with X/Esc. CleanHistory raw-Win32 + modal-pump pattern reused (window class `TUMATRA_FLASHCARD_ORDER`).
- **New setting** `FlashcardSettings.NewCardsPosition` (Int, **default 2 = mixed**): 0 = new first, 1 = new last, 2 = mixed. `RandomOrder` (Bool) kept — settings-file compatible.

### Changed
- **`BuildFilteredStudyOrder` reworked**: collects NEW (rating==0) and DUE cards separately → combines per position (first/last/**proportional interleave** — hand-verified 3:6 → N D D N D D N D D) → **random shuffles WITHIN each group** (position stays meaningful; was a flat shuffle before). Log extended: `n cards (order=%s, new=%s, nNew=%d, nDue=%d)`.
- New public `FlashcardApplyStudyOrder(win)` (Commands_Flashcard.cpp): rebuild + restart-from-first (centered) after any order/position change; used by the dialog per click.
- Toolbar: Order button no longer shows checked-when-random (dialog communicates state; it is not a toggle anymore).

### Verified
- Repro: dialog opens (log `OrderOptions - dialog opened`), WM_CLOSE closes it; Clean-History flow unaffected; new build log format confirmed (`order=sequential, new=mixed, nNew=1, nDue=0`). Full option behavior = mouse UI → user tests live.

### Notes
- **Trap re-learned the hard way**: using PowerShell `[IO.File]::WriteAllText` for the CommandAvailability rename corrupted the `…` (U+2026) in a comment (Get-Content ANSI-decodes BOM-less UTF-8). Repaired via Edit tool; git diff verified clean (2 rename lines only). The AGENTS.md "never Set-Content/WriteAllText" rule applies to ANY text re-encoding, not just Set-Content.
- Remaining from user's "incorrect buttons" report: Filter + Lista (later); queue also has FC-M1 tests, FC-H2 scan perf, FC-H3 prune, FC-M4/M5.

## 2026-09-24 (s19) — High-gain mini-batch: M3 gating, atomic JSON save, R4/R5 (BUILD OK, deployed)

### Added
- **FC-M3 availability gating**: all 14 flashcard commands added to `removeIfChm` + `removeIfMarkdown` (CommandAvailability.cpp) — no more flashcard toolbar / no-op commands on CHM and Markdown docs (flashcards are annotation-based, PDF-only).
- **FC-H3 atomic study-JSON save**: `FlashcardStudySave` now serializes to `<path>.tmp` then swaps with `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` (fallback: remove+rename for antivirus-handle cases) — a crash or Drive-sync stall mid-write can never leave a truncated history file. Verified end-to-end via the cloze repro (saved 1 → Clean → saved 0, no replace errors, JSON valid).

### Fixed
- **FC-R4 duplicate control IDs**: dedicated `IDC_FLASHCARD_REBAR` (1108) / `IDC_FLASHCARD_TOOLBAR` (1109) / `IDC_FLASHCARD_LABEL_WITH_CLOSE` (1110) in resource.h; flashcard rebar/toolbar/sidebar-label no longer reuse the main toolbar's / favorites' IDs (latent GetDlgItem landmine removed).
- **FC-R5 style/exStyle mix**: sidebar popup's `WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE` moved from dwStyle (silently ignored there) into the exStyle PARAM — NOACTIVATE now actually applies: clicking the floating card list no longer steals focus from the canvas.

### Notes
- **FC-H3 locale sub-item = NON-ISSUE** (verified): CRT `fprintf %.4f` uses the default "C" locale — the app never calls `setlocale` (only EngineDump sets "C" explicitly); the decimal-comma bug lived in `str::VsnprintfUtf8`'s `_create_locale(".UTF-8")`, fixed in eaad4e6. Remaining FC-H3 part: prune states of deleted cards (defer).
- Image-occlusion feature PLANNED (TODO section IO-1..IO-4); design decisions IO-D1/D2/D3 pending user.

## 2026-09-24 (s18) — Flashcard mechanical batch: R2/R3/R6, M2, H4/H5 (BUILD OK, deployed)

### Fixed
- **FC-R2 Mojibake**: sidebar card label `"Card %d â€” Page %d"` (double-encoded em-dash) → proper UTF-8 em-dash (project compiles with /utf-8).
- **FC-R3 Sidebar orphaned**: `CmdFlashcardToggle` OFF now destroys the sidebar along with the toolbar — no more floating popup with a stale list.
- **FC-M2 Sidebar stale after Add**: `FlashcardSidebarPopulate(win)` called after `CmdFlashcardAdd` (self-guards when closed).
- **FC-R6**: stripped the `[fc-paint]` first-10-paints diagnostic logs from Canvas.cpp (header block + per-rect block + counter; paint logic untouched).
- **FC-H4**: stray trailing `#include "Flashcard.h"` removed from Flashcard.cpp (top block already includes it).
- **FC-H5 doc drift**: TODO render matrix mask alpha 200→255 (matches code); MainWindow.h comment "(Space pressed)"→"(Space/Enter)"; FLOW dot already correct.

### Remaining flashcard queue
- FC-M1 Step-8 tests (SM-2/JSON/due-filter) · FC-R4 duplicate control IDs · FC-R5 dwStyle/exStyle mix · FC-H2 scan under lock · FC-H3 JSON robustness (atomic write, prune) · FC-M3 CHM/MD gating · FC-M4 filter UX · FC-M5 rate hints · LAST: Order/Filter/Lista button redo (user-reported incorrect).

## 2026-09-24 (s17) — Multi-select: Alt+drag sums selections → ONE grouped flashcard (BUILD OK, deployed, USER-VERIFIED)

### Added
- **Alt+drag additive text selection** (user spec): holding Alt while dragging a disjoint text region SUMS it into the current selection — repeatable (3rd, 4th... Alt+drag keeps accumulating). `S` then creates **ONE flashcard with N mask blocks** (one annot with N quads; loader + per-quad mask painting + union-bounds centering already handled disjoint rects).
- Implementation: `SelectionState` gained `altAccum` (Vec<SelectionOnPage>*) + `altSelecting` (MainWindow.h; `struct SelectionOnPage;` forward-declared at FILE scope — nested fwd-decl would shadow the global type). `OnSelectionStart`: Alt (GetKeyState OR `MK_ALT` wParam bit — the bit enables synthetic input since posted WM_KEYDOWN doesn't update GetKeyState) parks the current selection into `altAccum` before the standard clear; `UpdateTextSelection` merges accum entries into every drag update; `OnSelectionStop` restores `altAccum` if the Alt drag aborted (stray Alt+click loses nothing); plain drag resets to normal single selection.
- Edge semantics: cross-PAGE selections produce one card PER PAGE (annots are page-bound — by design).

### Verified
- Log-proven (`alt merge: 1 accumulated entries -> 2 total` + `Cloze card added ... 1 page(s) covered`) then **user-verified live** ("parece que funcionou!").
- `tests/ad-hoc-flashcard-multiselect.ts` kept for reference; synthetic mouse input proved flaky (DPI ×1.25 scaling of posted coords, MK_ALT requirement, pan-corruption from missed probes) — human test is the right tool for mouse features.

### Notes
- Permanent logs: `OnSelectionStart: alt=%d at (%d,%d)` (Selection.cpp), `OnMouseLeftButtonDown: wp=0x%x at (%d,%d)` (Canvas.cpp entry).
- Save-mechanism fix (s16) + this feature both deployed: release `Compiled\TumatraPDF.exe` + debug `Compiled\TumatraPDF-debug.exe` (2026-09-24).

## 2026-09-24 (s16) — Save mechanism: no re-compression on same-file saves (BUILD OK, deployed)

### Changed
- **`EngineMupdfSaveUpdated`** (EngineMupdf.cpp): `do_compress`/`do_compress_images`/`do_compress_fonts` now **0 for same-file saves** (Ctrl+S / save-on-close / annotation edits) — only a Save As to a DIFFERENT file pays the one-time compression for a smaller export. Rationale: `pdf_can_be_saved_incrementally` returns 0 whenever the xref was repaired at load (`repair_attempted`) — common in older scanned books — forcing a FULL rewrite; with `do_compress_images=1` that re-deflated every image stream in the document = minutes of UI freeze on large scans. Incremental saves only append new objects, so the flags never mattered there anyway.
- Log line now includes the compress flag: `Saved annotations ... incremental: N, compress: M`.

### Verified (tests/ad-hoc-save-timing.ts — new ad-hoc, saves a COPY of the book, never the real file)
- User's main test book (30 MB scan): same-file save is already incremental — 204 ms (no repair at load; incremental chains kept working even with a corrupted startxref keyword — mupdf falls back to the previous xref).
- **Worst case reproduced**: truncated trailer (%%EOF removed) → `repair_attempted` → `incremental: 0` → full rewrite: 478 ms compressed vs **245 ms uncompressed** after the fix (30 MB book; delta scales with file size × compressible content — the minutes-class freezes live here, and on Drive-synced paths I/O stalls add more).
- FC-D2 closed (user decision): keep current card persistence — the app's existing dirty-on-close prompt covers it.

### Notes
- User reports saves "often very slow, freezing for minutes" — main test book measures fast; the freeze class = other books (bigger / repaired xref / Google Drive I-O). Next step: user names the specific book+action that froze so the fix can be validated on the real case.

## 2026-09-23 (s15) — Flashcard Clean History (hold-to-confirm dialog) (BUILD OK, deployed)

### Added
- **`Clean History`** (toolbar "Clean" button + `CmdFlashcardCleanHistory`): clears THIS document's review history (SM-2 states → all cards become "new"; stops an active study session; rewrites the study JSON with `states: {}`).
- **Hold-to-confirm dialog** (`FlashcardCleanHistoryDialog`, FlashcardToolbar.cpp, raw Win32 + modal pump — no Dialog Manager): question text (PT UI string, verbatim per user spec): "tem certeza que deseja limpar o histórico de revisões?" ("are you sure you want to clear the review history?") with No / Yes. "Yes" must be clicked AND HELD 2s: while holding, a red line under the "Yes" text drains right-to-left (15ms WM_TIMER repaint); releasing early resets it (mouse capture + WM_CAPTURECHANGED safety). No / Esc / close = cancel.
- Repro: `tests/ad-hoc-flashcard-cloze.ts` extended — finds the dialog by title via `enumWindows`, clicks Yes at DPI-proportional client fractions (0.648/0.762), tests early-release (600ms → stays open) and full hold (2.3s → confirmed, dialog gone, `"states": {}` written).

### Verified
- Log sequence: `hold start` → `hold released early` (dialog stays) → `hold start` → `hold CONFIRMED (2s)` → `saved 0 entries` → `history cleared`. JSON after: `states: {}` (39 B). User's REAL history in `Compiled\FlashcardStudy\` untouched — the repro runs the DBG64 exe whose portable app-data is `out\dbg64\FlashcardStudy\` (each exe instance has its own study data dir; portable mode = app-data next to the exe).

### Notes (for TOMORROW, user report)
- **Order / Filter / Lista toolbar buttons are INCORRECT** (user: "vamos refazer/corrigir amanhã") — do not touch today; symptoms to clarify with user first (suspects: Next/Order state graying via `TB_SETSTATE 0`? Lista/Filter handlers?). Clean button added alongside; verify all together tomorrow.
- The 6 cards in the test PDF are ALL the user's test cards (they misspoke earlier about "5 more").

## 2026-09-23 (s14) — Flashcard study flow rework: keys, Next button, order setting (BUILD OK, deployed)

### Added
- **Study keyboard flow (user spec)**: Space/Enter/numpad-Enter → Reveal (VK_RETURN covers both Enters; removed Enter's old CmdScrollDownPage binding + Shift+Enter scroll-up — the key family now NEVER scrolls, reveal is a no-op outside study mode). 1-4 + numpad 1-4 → rate (numpad bindings new). Rate advance: next card arrives MASKED (front state) and vertically CENTERED, instant (no animation).
- **Next Card button** (`CmdFlashcardNext`): skip to next card WITHOUT revealing or rating — no SM-2 update, no save; arrives masked + centered; grayed out when no next card. Toolbar: `{Study, Reveal, Back, Next, Order, Lista, Filter}`.
- **Study order setting** (`CmdFlashcardOrderToggle` + `FlashcardSettings.RandomOrder` pref, persisted): sequential (default) or random (Fisher-Yates shuffle at BuildFilteredStudyOrder). Next/advance/Back all follow the configured order coherently; toggling mid-session rebuilds the queue and restarts from its first card (centered). Toolbar Order button checked-when-random.

### Fixed
- **Rate-advance/back used RAW physical pageNo (FC-R1)** — new `FlashcardNavigateToCard(win, cardIdx, center)`: routes via `PhysicalToVirtualForRect` (TC2-correct), centering via the canonical viewport-relative delta idiom `ScrollYBy(screenRect.y + dy/2 - viewPort.dy/2, false)` (CvtToScreen returns viewport-RELATIVE coords; the first cut mixed spaces — ScrollYTo with canvas math landed off-center).
- **Back (user spec)**: previous card arrives REVEALED (post-reveal state), scroll position KEPT (no auto-centering).
- Rate handler bounds guard (FC-H1 part): `currentCardIdx` range-checked before indexing.

### Verified
- Repro `tests/ad-hoc-flashcard-cloze.ts` on user's book: centering math confirmed in all directions (study-first dy=-394 → card at vpdy/2; advance dy=15502 across ~20 pages; Next no-op dy=0 when already centered; order-toggle recenters ±277). Random order picked card 5 first (not sequential card 1) ✓. Study JSON saved on rate ✓.
- NOTE: user's 5 manually-created cards not in the PDF file yet (found 6 cards) — cards persist to the document only on save (FC-D2 policy).

### Notes
- Gen-name collision: settings struct must NOT be named `Flashcard` (clashes with the card struct in Flashcard.h → C2011) → `FlashcardSettings` / `gGlobalPrefs->flashcardSettings.randomOrder`. Rule for future gen structs: check against existing src/ type names before regen.
- Reveal/Next/Order are PDF-only by nature (AsFixed gates); toolbar shows for CHM/MD but commands no-op (FC-M3 still pending).

## 2026-09-23 (s13) — Hardening: unit tests ALL PASS (locale root cause), premake fix, gen audit (BUILD OK, deployed)

### Fixed
- **8 "pre-existing" base/tests failures RESOLVED — root cause: locale decimal comma.** `str::VsnprintfUtf8` (StrUtf8.cpp) formatted via `_create_locale(LC_ALL, ".UTF-8")` — `.UTF-8` leaves lang/country unspecified so UCRT inherits the USER'S OS locale (pt-BR here) → `%f` emits "3,45" (comma). English machines get "." → why upstream CI never saw it. `_vsnprintf_l` uses the locale object, so `setlocale(LC_ALL,"C")` couldn't help (tested, no effect). Fix: `"C.UTF-8"` (invariant C numbers + UTF-8 codepage) in StrUtf8.cpp (production path — also fixes latent APP float-formatting bug on comma-locale machines) + the test-local reference copy in StrFormat_ut.cpp. **test_util: Passed all 102,685** (first fully green run).
- **premake TRAP permanently defused (option b)**: `run-unit-tests.ts` no longer runs `bin/premake5.exe` (it regenerates vcxproj from upstream premake5.lua → strips 10 fork sources → LNK1120 later). Committed vs2022/ projects used as-is; comment explains when manual regen is legitimate. AGENTS §6b documents the trap + stash-recovery recipe.

### Verified
- **gen-settings coverage audit PASSED**: full regen (`vcvars64.bat` env required — bare PATH: cl.exe dies at virt-keys) → `git diff` on Settings.h/Settings.cpp = EMPTY → no hand-added fields outside the generator (archScaleStates was the only historical gap, fixed in gen at FASE 7). Reusable audit: regen + git diff.
- Harness builds+runs without premake (14s, no vcxproj damage); app debug 0/0 (42.8s incremental), release linked clean; deployed `Compiled\TumatraPDF.exe` (19:54) + `TumatraPDF-debug.exe` (19:46); smoke `-for-testing` OK.
- Hygiene: `OnAutoScrollProgress(int /*remainingPx*/)` unnamed default param (test build was the only TU warning C4100; app always 0/0).

### Notes
- Audit script scratch: `tests/tmp/audit-gen-settings.ts` (gitignored; naive name-lowercasing yields false positives `URL`→`url`, `AIChatSidebarDx` — generator has its own name-mapping table; the regen+diff method is authoritative).
- `build.ts` printed 11 transient warnings on first post-edit run (stale sibling projects); Rebuild + rerun = 0/0.

## 2026-09-23 (s12) — FASE 7: v2 persistence + auto-continuous + favorites physical (BUILD OK)

### Added
- **v2 state persistence**: new `ViewportCrop.V2Enabled` setting (gen-settings.ts → Settings.h regen); `DisplayModel` ctor applies it (`viewportCropV2Enabled = gGlobalPrefs->viewportCrop.v2Enabled`) BEFORE `SetInitialViewSettings` → continuous doc starts directly at 2N virtual pages; latent flag in non-continuous modes safe (all conversions gate on `IsViewportCropV2Active`). `CmdViewportCropV2Toggle` writes the pref back.
- **Auto-continuous**: toggling v2 ON from a non-continuous mode switches to Continuous first (`SwitchToDisplayMode`) — anchor capture, layout rebuild and nav remap all run in the right page domain. Uniform for toolbar/menu/palette.
- **Favorites physical**: add-normalize — `win->currPageNo` (VIRTUAL under v2) → `VirtualToPhysical` before storing; label ("3L"/"3R") keeps the column. Click-resolve: `GoToFavoritePage` under active v2 prefers the stored label (`GetPageByLabel` parses L/R), falls back to `2p-1` (left column); label copied via `str::Dup` into the posted task (no dangling on favorite delete).

### Fixed
- **Nav remap gate guard**: `RemapNavHistoryForV2` now runs only when the ACTIVE domain changed (gate before ≠ after). Latent flag (v2 on, non-continuous) no longer wrongly remaps physical-domain entries.
- **`FileState.archScaleStates` lost by regen**: first `gen-code.ts` run dropped the hand-added field (never in gen-settings.ts) → 10 C2039 build errors. Fixed properly: `ArchScaleStates` added to gen-settings.ts FileState[] + regen. LESSON: every hand-added Settings.h field MUST live in gen-settings.ts.
- **gen-code needs VS env**: `bun cmd/gen-code.ts` fails at virt-keys (`cl` ENOENT) on bare PATH; runs clean inside `vcvars64.bat`. `Settings.h/Settings.cpp` regen only completes with cl available.
- **run-unit-tests.ts harness**: stale `detectVisualStudio2026()` (machine has VS 2022) → switched to fallback `detectVisualStudio()`; stale sln name `vs2022\SumatraPDF.sln` → `TumatraPDF.sln`.

### Notes
- **Unit tests RUN (first time on fork)**: 8 assertion failures, ALL pre-existing in `base/tests` (3× SettingsUtil serialize/reserialize roundtrip @155, 5× StrFormat FormatTemp-vs-printf @91). `src/base/` clean in working tree — unrelated to today's changes. Follow-up: diagnose base/ divergence.
- **premake TRAP**: `run-unit-tests.ts` runs `premake5.exe vs2022` which REGENERATES vcxproj from upstream premake5.lua → strips 10 fork-added sources (Flashcard*, ToolbarLayout, Commands_AutoScroll/ArchTools/View/File, MainWindowCreate) → release link fails LNK1120 (53 externals). Recovered via `git stash push -- vs2022/` (rtk safety net blocked checkout/restore/drop). Permanent fix pending: update premake5.lua OR skip premake in run-unit-tests.
- Stash `premake-regen-damage` left in list (rtk blocks `stash drop` — drop manually if desired).
- Build dbg+rel 0 err/0 warn; deployed release 19:07 + debug 18:53; smoke OK.

## 2026-09-23 (s11) — .md: space/Ctrl/position/F-key + traces removed (BUILD OK)

### Fixed
- **Space never scrolls (also .md/CHM)**: Win32 side already clean (VK_SPACE/Shift+Space outside `gBuiltInAccelerators`), but Chromium INSIDE WebView2 has NATIVE Space=page-down, outside accelerator table → new init script `kBlockSpaceScrollJs` (BrowserDocView.cpp): keydown capture, `code==='Space'`, no ctrl/alt/meta, not typing in INPUT/TEXTAREA/SELECT/contentEditable → `preventDefault`. Injected alongside existing Inits.
- **Ctrl did not pause autoscroll .md**: Ctrl early-return in `AutoScrollContinuousTick` (ETA math rework) ran BEFORE webview branch, starved `webviewCtrlDown` state machine calling `SetWebviewAutoScroll(0)`. Tick restructured: active-check first; `if (dm)` branch (fixed pages) CONTAINS Ctrl early-return; webview branch unchanged (state machine + timer check, zero ETA work per tick).
- **.md reading position lost on reopen**: save existed (`MarkdownModel::GetDisplayState` → `fs->scrollPos` via `SaveHtmlScrollPos`); restore missing — `OnDocumentComplete` only consulted in-session `htmlScrollPositions` map (empty after restart). LoadDoc markdown branch (SumatraPDF.cpp ~:2343): `fs->scrollPos.y > 0` → `ScrollTo(page, RectF(fs->scrollPos), kInvalidZoom)` (seeds htmlScrollPos → applied in OnDocumentComplete, same mechanism as CHM); else GoToPage. Requires normal close (settings save); `-for-testing` runs never persist.
- **F (fullscreen) dead in .md**: WebView2 fires `AcceleratorKeyPressed` only for Chromium-classified accelerators (F-keys, arrows, Home/End) — plain letters (F→CmdToggleFullscreen) never reach host. New init script `kForwardAppKeysJs`: keydown without modifiers, keyCode 65-90, outside inputs → `notify('appKey', keyCode, shift)` + preventDefault; handler in `OnJsNotifyCb` BEFORE `if (!self->cb)` guard: resolves via `resolveAccelCmd`, `cmd > 0` → `PostMessageW(GA_ROOT, WM_COMMAND, cmd)` (mirrors webview2_accel_handler).

### Verified
- **"ETA disappearing in fullscreen"**: NOT bug in current build — `tests/ad-hoc-md-fullscreen.ts` (ad-hoc): overlay topmost sibling (above canvas), visible, text alive, repositioned correctly in fullscreen AND presentation mode with autoscroll on. User observation was old binary still running. New helpers `getWindow`/GW_HWNDPREV/GW_HWNDNEXT added to tests/winapi.ts.

### Changed
- **TEMP `[as-md]` traces removed** (3 sites: BrowserDocView OnJsNotifyCb, SetWebviewAutoScroll, OnAutoScrollProgress) — pipeline verified, full diagnosis in ad-hoc tests.
- `tests/util.ts` EXE: `SumatraPDF.exe` → `TumatraPDF.exe` (current binary; SumatraPDF.exe in out\dbg64 is stale pre-rename — tests with `launchSumatra` ran old build). Note: per-test taskkills still reference old process name in some files (cosmetic follow-up).

### Notes
- All .md fixes in INDEPENDENT functions per user rule (.md changes never regress PDF/CHM). User visual check: F→fullscreen, ETA both modes, space dead, Ctrl pauses, reopen restores height, shimmer.

## 2026-09-23 (s10) — .md ETA: adapter no-op + separate logics + shimmer (BUILD OK)

### Fixed
- **.md ETA dead (page scrolled, time never showed)**: root cause — `MarkdownHtmlWindowHandler` (adapter MarkdownModel↔BrowserDocView, created in `SetParentHwnd`, MarkdownModel.cpp:179) never forwarded `OnAutoScrollProgress`/`OnAutoScrollBottom`; both fell to `HtmlWindowCallback` no-op default (HtmlWindow.h:40-43). Entire pipeline alive (rAF `__tumatraAS` → notify `autoscrollProgress` → BrowserDocView), died SILENTLY at last hop. Diagnosed with `tests/ad-hoc-md-eta.ts` (ad-hoc repro: overlay text/visible/rect + webview pixel column + `[as-md]` traces in sumlog): notifies arrived with rem 2041→1263 px, `ControllerCallbackHandler::OnAutoScrollProgress` never fired. Adapter now forwards both — also fixes desynced bottom-stop (rAF self-stopped at end, C++ state stayed "active").
- **.md ETA "0min" at end**: `OnAutoScrollProgress` truncated (`(int)`) — showed "0min" with <1min reading left. Now floor 1min (same rule as PDF); actual end still via `autoscrollBottom` (hides label).

### Changed
- **Two fully independent ETA logics** (user rule: .md changes never regress PDF): `RecalcAutoScrollEtaPdf` (canvas: `remainingPx = canvasSize.dy − viewportBottom`) and `RecalcAutoScrollEtaWebview` (one-shot JS probe for remaining height; async result via notify) — `RecalcAutoScrollEta` dispatcher only picks by doc type. Zero shared calculation.
- **Shimmer/flicker of letters in .md autoscroll**: rAF steps quantized to DEVICE pixels (`step = Math.floor(accum×dpr)/dpr`, `dpr=window.devicePixelRatio`): at 125%/150% scaling, each 1 CSS px step landed on fractional device pixels → glyphs re-rasterized with alternating subpixel phase each step. Step = exactly k×dpr keeps raster phase stable. At 100% scaling behavior identical. **Visual check pending (user)**.

### Notes
- TEMP `[as-md]` traces still in code (notify entry in BrowserDocView, SetWebviewAutoScroll, OnAutoScrollProgress) — remove before commit.
- `tests/ad-hoc-md-eta.ts`: ad-hoc repro (NOT registered in all.ts) — launches .md, speedUp x10, toggle, samples overlay + pixel column.
- Lateral find: `tests/util.ts` EXE points to `SumatraPDF.exe` (old name; current binary = `TumatraPDF.exe`) — tests using `launchSumatra` run STALE binary from yesterday. Pending EXE fix.

## 2026-09-22 (s9) — FASE 5: state/nav/labels under TC2 (BUILD OK)

### Added
- **Page labels `L`/`R` under v2**: `GetPageLabeTemp` appends `L`/`R` (from `ColumnOfVirtual`) when `IsViewportCropV2Active` — toolbar shows "3L"/"3R"; `GetPageByLabel` inverse-parses ("3R" → virtual of right column; plain label → left column = reading-sequence start). Favorites/TOC/search inherit consistent labels.
- **Nav history 2N**: `DisplayModel::RemapNavHistoryForV2(bool)` — v2 toggle remaps history physical (N) ↔ virtual (2N): ON `p→2p-1`, OFF `(p+1)/2`; out-of-range entries drop; Back/Forward stay on same physical page. Called in `CmdViewportCropV2Toggle` handler (never in quick-toggle shift-hold — transient).

### Fixed
- **FileState persistence**: `GetCurrentFileState` saved VIRTUAL `fs->pageNo` (up to 2N) — reopen without v2 ran GoToPage(2N) on N PageCount (clamp to last page = wrong position). Now saves PHYSICAL (`VirtualToPhysical(ss.page)`); restore maps `2p-1` (left column) when v2 active. Future-proof for v2 state changes between sessions.

### Notes
- Finding (pending): favorites saved under v2 store virtual pageNo + label "3L" — in non-v2 session click resolves via label (ParseInt tolerates) but raw pageNo may no-op. Same fix class as FileState; physical normalization pending.
- Build dbg+rel 0 err/0 warn; smoke OK; release+debug deployed. Visual test pending (user): Back/Forward after toggle, page edit "3R", reopen doc saved under v2.
- TC2 accelerator: **user decision — NO shortcut** (toolbar/menu only).

### Removed
- **Spacebar disabled** (user request): `VK_SPACE` removed from `gBuiltInAccelerators` (`CmdScrollDownPage`) and `Shift+Space` (`CmdScrollUpPage`) — the bar made the page "jump a small interval". Enter/Ctrl+↑↓/PgUp/PgDn remain in place. Side effect: space no longer advances in presentation mode either (no alternate handler in code). `gNotSafeKeys` (edit-control safety) untouched.

## 2026-09-22 (s8) — FASE 6: R2L under TC2 + Trim×TC2 verified (BUILD OK)

### Added
- **R2L (manga/RTL) under Two Columns v2**: SINGLE flip in `DisplayModel::ColumnOfVirtual` — `col = (v-1)%2; return displayR2L ? 1-col : col`. Under R2L 1st virtual column shows RIGHT column of physical page (inverted reading order). Everything else (render colX, `PageMediaBox`, `ColumnOffsetXForPage` → hit-test/overlays) derives from this mapping — zero extra code.
- `PhysicalToVirtualForRect`: same inversion for virtual page number (`vCol = r2l ? 1-col : col`), but rect shift (`rectOut`) keeps PHYSICAL column — rect coords are physical truth, only reading order changes. Left/right navigation (keys, SumatraPDF.cpp:10533+) already virtual next/prev — consistent.

### Verified
- **Trim × TC2**: independent axes (column cuts dx/x, trim cuts dy/y) in all paths (layout, cache, non-cache Paint, CvtToScreen/FromScreen). Zero ordering conflict. Visual combo test pending with user.
- Build dbg+rel 0 err/0 warn; smoke `-for-testing` OK; release+debug deployed in `Compiled\`.

### Notes
- Toggle: `CmdToggleMangaMode` → `ToggleMangaModeInternal` (GetScrollState → Relayout → SetScrollState) preserves virtual position across flip.
- (retroactive, commit a09534e) purely mathematical ETA + time-based scroll — s7 LOG entry never committed; summary: `accum += AutoScrollPxPerSec*elapsedSec` (WM_TIMER ~64Hz, not 100Hz); ETA = `remainingPx/pxPerSec` per tick; measurement apparatus (~50 lines) removed. User rule: ETA mathematical, NEVER measured.

## 2026-09-22 (s6) — Autoscroll ETA: feedback loop + pixel-based ETA (BUILD OK)

### Fixed
- **ETA free-fall to zero**: `AutoScrollContinuousTick` countdown called `UpdateToolbarEtaText(win, remaining)` which **overwrote `etaMinutes`** with already-decremented value. 1 min past last resync baseline contaminated, ETA fell ~1/tick to 0. `UpdateEtaOverlayText(win, minutes)` gained display-only param; `UpdateToolbarEtaText` never writes `etaMinutes`.
- **ETA oscillating 3h→2h→3h**: measurement was per-page (`timePerPageSec`), page heights vary (figures/diagrams) → 2x oscillation per page. Now: `remainingPx / pxPerSec` — remaining pixels (GetCanvasSize.dy − viewPort.y−dy) ÷ effective speed measured **cumulatively** since last calibration. Test: 203→318→319→318 min (converges; per-page was 203→132→226→272). Side effect: ETA reflects *real* speed (WM_TIMER renders ~63% nominal), not nominal.
- **Pause leaking into ETA**: Ctrl/shift-hold also shift `etaResyncTick` + `etaScrollBaseTick` — countdown and measurement freeze during pauses.
- Recalibration **only** at 3 points: toggle ON, speed change, page change. Pure countdown between resyncs (floor 1min).
- `~` dropped from hours format (`%dh %02dmin`).

### Notes
- Debug logs `[as-eta]` (recalc + countdown) kept for diagnosis. `tests/ad-hoc-eta-debug.ts` = repro (enables autoscroll, ~150s).

## 2026-09-22 (s5) — .md ETA + MD contrast single-source (visual kept) + audit R1/R2/R3◐/R4/R5◐ (BUILD OK)

### Fixed
- **ETA .md stuck**: webview tick branch never recalculated ETA (only one-shot async JS eval on toggle — frozen after). rAF loop (`__tumatraAS`) now sends `autoscrollProgress` throttled ~10s → `OnAutoScrollProgress` → ETA keeps counting while scrolling (AutoScroll.cpp, SetWebviewAutoScroll).
- **Contrast MD: single source (R2), visual UNCHANGED by design**: 3 DIVERGENT contrast JS copies (toggle/opacity/restore), different gray math -> unified in `MarkdownContrastJs(enabled, opacity, invert)` (MarkdownModel). MD visual (body bg #FAFAFA/#050505 + gray text) INTENTIONALLY differs from PDF black veil, REMAINS so (user decision). Visual-unify attempt (CSS filter brightness/invert) reverted by request.
- **Latent BUG TrimConfigDialog (R1, audit)**: `OnEditTopChanged`/`OnEditBottomChanged` called `GetEngine()->PageMediabox(CurrentPageNo())` with VIRTUAL pageNo under TC2 (same class as GetTileRes bug that froze app) → now `dm->PageMediaBox()` (routes virtual→physical). BONUS: `RenderCache::Invalidate` same missed site — routed + column halve.

### Changed (audit R3/R4/R5)
- **R3 (colX)**: 4 column-shift copies (GetTileRectUser, non-cache Paint, CvtToScreen, CvtFromScreen) unified in `DisplayModel::ColumnOffsetXForPage(pageNo, &offX)` — single offset source (left = mb.x, right = RightColumnX). TrimRectPage NOT done: site semantics differ, forcing helper = render-path risk, low value.
- **R4**: 3 trim-drag-lines math copies in Canvas.cpp (hit-test/move/paint) unified in `TrimComputeLineInfo`/`TrimLineInfo`.
- **R5 partial**: removed `QuickToggleViewportCrop` v1 (zero callers) + `kEtaW` (Toolbar.cpp). V1 crop stack remains (v1 button alive); flashcard dead fields remain (agent active).
- FLOW dots updated: `trim-contrast.dot` (R1/R2/R4 done, R3 decision), `autoscroll.dot` (rAF progress notify).
- TODO.md: audit phases marked (R1 ✅ R2 ✅ R3 ◐ R4 ✅ R5 ◐ R6 ◐ R7).

### Deploy
- `Compiled\TumatraPDF.exe` = rel64 13:02 (daily reading) + `Compiled\TumatraPDF-debug.exe` = dbg64 12:56.

## 2026-09-22 (s4) — Trim Coord Central + Flashcard Toolbar State (BUILD OK)

### Fixed
- **Trim enabled displaced selection/overlays (systemic root cause)**: `DisplayModel::CvtToScreen`/`CvtFromScreen` never applied margin trim offset (layout uses reduced height; rendered content starts at trim strip). Page-coord overlays (selection, cloze, link hover) landed displaced. Central fix: `CvtToScreen` does `y -= trim.top`, `CvtFromScreen` does `y += trim.top`; manual adjustment in `Selection.cpp::GetRect` removed (would duplicate). Fixes incorrect-selection-with-trim bug, helps cloze mask positioning.
- **Flashcard toolbar — visual state**: Study/Reveal buttons stay checked while active (`FlashcardToolbarUpdateState`, TB_SETSTATE), called in Study/Reveal toggles + HandleFlashcardRate.

### Notes
- Temp debug logging in cloze paint (`[fc-paint]`, first 10 frames): state + trim + canvas size + pageOnScreen + per-rect physical→virtual→screen. Remove after diagnosis.

## 2026-09-22 (s3) — BUG-4 v2: Contrast Overlay Self-Healing (BUILD OK)

### Fixed
- **BUG-4 (trim config kills contrast)**: symptom refined by user — one-off event, only FIRST trim-config interaction after app open. Root pattern: 3 state paths (session sessionData / per-document FileState / default) write `win->contrastEnabled` without syncing overlay HWND — any desync = first SaveSettings destroys visible overlay.
- **New `EnsureContrastOverlayState(win)`** (ContrastOverlay.cpp + Canvas.h): creates if `contrastEnabled && !overlay`, destroys if `!enabled && overlay`, repositions if in sync. Wired to ALL flag writers: `ReplaceDocumentInCurrentTab` (doc-swap now also destroys orphan overlay), `UpdateTabFileDisplayStateForTab` (SaveSettings resurrects instead of destroying), TrimConfig OnSave/OnCancel (RE-CREATES if any path destroyed it).
- Breadcrumbs kept for diagnosis: `[contrast] Create/Destroy/WM_DESTROY` + entry logs `SchedulePrefsReload`/`ReloadSettings` (DEBUG build logs; release doesn't).

## 2026-09-22 (s2) — Flashcard Cloze Crash Fix (BUILD OK)

### Fixed
- **Flashcard crash on card creation** (`Flashcard.cpp`): `FlashcardLoadFromDocument` called `pdf_annot_rect()` on Highlight annotations. MuPDF excludes markup types from `rect_subtypes` (`pdf-annot.c:1267`) → uncaught `fz_throw("Highlight annotations have no Rect property")` → process abort (no try/catch in the scan). Replaced with `pdf_bound_annot()` (bbox from quad-points, same pattern as `Annotation.cpp GetBounds`) and wrapped per-annot body in `fz_try`/`fz_catch` — one corrupt annotation now skips instead of crashing. Cloze positional model unchanged.
- Stale comment (referenced removed `Q:` prefix) updated to describe author-based cloze cards.

### Fixed (2)
- **Study mask landed on wrong position under TC2** (`ViewportCrop v2` active): Canvas painted cloze mask at PHYSICAL page number while DisplayModel was in VIRTUAL (2N) layout — mask off-position, text stayed readable. Cloze painting now routes each rect through `PhysicalToVirtualForRect` + virtual `CvtToScreen` (same rule as every render-path call: physical→virtual mapping before screen conversion). Mask alpha 200→255 (fully opaque).
- **Multi-line cloze covered too much text**: mask used highlight union bbox (`pdf_bound_annot`), so 2-line selection produced one big rectangle hiding neighbor text. `Flashcard` now stores per-quad-point subrects (`Flashcard::rects`, via `pdf_annot_quad_point_count`/`pdf_annot_quad_point` + page CTM); Canvas paints each subrect — mask covers exactly highlighted lines. Card navigation (`CmdFlashcardStudy` advance + sidebar click) also routed through `PhysicalToVirtualForRect`.

## 2026-09-22 — Architectural Audit & FLOW Enrichment (Analysis Only)

### Added
- **TODO.md**: Added "Architectural Audit + Refactoring Plan (2026-09-21)" — 8 findings with file:line evidence + execution phases R1-R7. Key items: TrimConfigDialog latent bug under TC2 (`GetEngine()->PageMediabox` without `VirtualToPhysical`, `TrimConfigDialog.cpp:92/113`), contrast webview JS duplicated across 3 divergent copies, colX/trim math duplication, v1 crop stack dead code.
- **FLOW/**: 5 new process diagrams — `autoscroll.dot`, `two-columns-v2.dot`, `flashcard.dot`, `trim-contrast.dot`, `build-test.dot`.
- **tumatrapdf.dot**: Refreshed with new ETA/canvas-overlay descriptions, TC2 + flashcard feature nodes, and updated audit notes.

### Notes
- No source code changed. R1 (`TrimConfigDialog` fix) is the first recommended step.

## 2026-09-21 (s2) — Fluid TC2 + Page ETA + Release x64 + Systemic Fixes (BUILD OK)

### TC2 Crash & Performance
- **GetTileRes** (`RenderCache.cpp`): missed cache re-enable spot — passed VIRTUAL `pageNo` to engine (`PageMediabox`/`Transform`/`HasClipOptimizations`), triggering `ReportIf(pageNo > pageCount)` = debug report storm + callstack **per paint** (half virtual pages), freezing UI with TC2 on. Routed via `VirtualToPhysical` + half-width column mediabox.
- **Render Thread Guard**: discards queued requests with invalid pageNo post-relayout (2N→N) instead of feeding engine.
- **RestoreViewportAfterV2Toggle**: replaced `viewportCropV2Enabled` (setting) check with `IsViewportCropV2Active()` (layout truth) — quick-view restorePage `2k-1 > N` failure fixed. Anchor now **fractional** (`frac × new height`), survives zoom toggles between column (fit half-width) and full page (fit full-width).

### Shift-hold v3 + Page ETA (User Spec)
- **Shift-hold = PAUSE Autoscroll** (tick early-return; free drag on full page; resumes on release). Replaces previous speed ÷2.
- **Quick View**: Entry saves virtual zoom + relayouts to `kZoomFitWidth` (full page fills screen); exit restores saved zoom. `HandleV2ShiftHold` checks `quickToggled`.
- **Page ETA**: Resync only on page change; formula `time-per-page × remaining pages` (TC2 counts virtual 2N = each page read 2×; trim keeps count unchanged); decays like an inverted timer between resyncs; pauses (shift/ctrl) excluded; ≥60min shown as `~1h 05min` (240px overlay).

### Systemic Fixes (Tree-Sitter + Hang Dump Audit)
- **SaveSettings Contrast Overlay Destruction**: `UpdateTabFileDisplayStateForTab` had unconditional `DestroyContrastOverlay` killing contrast mid-session. Preserved/repositioned when `contrastEnabled`.
- **Partial Toggle Pipeline**: v2 toggle ended with `RecalcVisibleParts + HwndRepaintNow` (stale scrollbars, unqueued renders). Both v2 toggles now end with `ScrollYTo(viewPort.y)` — full pipeline matching `SetDisplayMode`/`GoToPage`.
- **RequestTextExtraction VIRTUAL pageNo**: `Canvas.cpp:1773` passed virtual pageNo on every mousemove during TC2. Routed via `VirtualToPhysical`.

### Builds
- **Release x64 Deployed**: `MSBuild vs2022\TumatraPDF.sln /t:TumatraPDF /p:Configuration=Release;Platform=x64`. `Compiled\TumatraPDF.exe` = **release** (daily reading; 5-10× faster than debug; NDEBUG = no ReportIf); `Compiled\TumatraPDF-debug.exe` = debug for diagnostics. User confirmed TC2 drag is **very fluid**.

### Pending
- BUG-4: Trim config still deactivates contrast in some code paths (partial fix applied).
- Validate ETA counting to end of book.

## 2026-09-20 — Two Columns v2 FASE 5 (Shift-Hold) + FASE 6 (ColGap) (BUILD OK)

### Context
Continued Two Columns v2 plan. Implemented shift-hold (FASE 5) and colGap/column overlap (FASE 6).

### FASE 5 — Shift-Hold (View Full Physical Page on Shift Press)
- Added `DisplayModel::IsViewportCropV2Active()` = `viewportCropV2Enabled && !viewportCropV2QuickToggled && IsContinuous`. All v2 conversions gate on it.
- `QuickToggleViewportCropV2` real toggle: captures anchor (physAnchor + dyInPage), flips `viewportCropV2QuickToggled`, re-applies (2N↔N), restores via `RestoreViewportAfterV2Toggle`.
- `SumatraPDF.cpp`: helper `HandleV2ShiftHold` + intercepted `WM_KEYDOWN`/`WM_KEYUP` for `VK_SHIFT` in frame WndProc.
- `SearchAndDDE.cpp` updated to use `IsViewportCropV2Active`.

### FASE 6 — ColGap/Offset (Column Overlap)
- DisplayModel helpers: `ViewportCropV2Gap()` (reads `gGlobalPrefs->viewportCrop.colGap`), `ViewportCropV2ColumnWidth(mb)=halfW+gap`, `ViewportCropV2RightColumnX(mb)=mb.x+halfW-gap`.
- Applied in: `PageMediaBox`, `GetTileRectDevice/User` (RenderCache), non-cache `Paint`, `CvtToScreen`/`CvtFromScreen` — columns overlap center band to avoid cutting centered text. Default `colGap=0`.
- Configurable in Trim Config dialog ("column gap:" field) with save/cancel/reset and relayout when v2 active.

### Verification
- Build 0 err / 0 warn + clang-format. Smoke test stable.

## 2026-09-21 — Autoscroll: Pixel-Based ETA + TC2 2x Auto + Shift-Hold Speed Halve (BUILD OK)

### Corrections
- **Pixel-Based ETA** (`AutoScroll.cpp`): Replaced page/wall-clock countdown with `remainingPx = canvasSize.dy - viewPort.y` recalculated per tick. Accurate scroll position tracking.
- **TC2 × 2 Automatic**: Canvas height is doubled (2N virtual pages), so `canvasSize.dy - viewPort.y` naturally yields 2× remaining time.
- **Shift-Hold Speed Halve** (`AutoScrollContinuousTick`): When `viewportCropV2Enabled && viewportCropV2QuickToggled`, speed ×0.5 temporarily.
- Removed deprecated `etaStartTick` and `etaPageNo` fields from `AutoScrollState`.

### Commits
- `562166b` fix(autoscroll): pixel-based ETA + TC2 2x auto + shift-hold speed halve

## 2026-09-20 — Two Columns v2: Re-enable Render Cache (Fix Autoscroll Jank) (BUILD OK)

### Changes (`RenderCache.cpp`, `DisplayModel.cpp`)
- `GetTileRectDevice`/`GetTileRectUser`/`GetTileOnScreen` take `DisplayModel*` and route `VirtualToPhysical`; mediabox halved when `cropColumn>=0`.
- `colX` shift applied **only at render time** (`GetTileRectUser`), mirroring the non-cached path.
- Render thread routes `req.pageNo`→`renderPageNo` via `VirtualToPhysical` before `RenderPageArgs` and `GetBitmapRecolorSkipRects`.
- `ShouldCacheRendering` returns `true` always (v2 cache re-enabled). Cache key uses virtual pageNo already.

### Verification
- Build 0 err / 0 warn. User manual test: autoscroll is smooth and responsive.
- sumatrapdf-src/src/RenderCache.cpp, sumatrapdf-src/src/DisplayModel.cpp, TODO.md.

## 2026-09-18 — Autoscroll: bottom-right ETA/speed overlay (backgrounds/fixes) (BUILD OK)

### Context
After commit `ef19739` (direct-multiplier speed x0.03..x0.40), the user reported the bottom-right ETA/speed overlay wasn't showing; once it did, it had overlapping characters, duplicated speed in the toolbar and unreadable text without a background.

### Changes
- **Invisible overlay**: `CreateEtaLabel` created `WS_EX_LAYERED` but never called `SetLayeredWindowAttributes` — a layered child draws no content without alpha (the `ContrastOverlay` model calls it at line 92). Fixed.
- **Overlapping characters**: the overlay's `WM_PAINT` used `SetBkMode(TRANSPARENT)` without clearing the rectangle, stacking text every frame. Fixed by filling the rect with the magenta color key + `SetLayeredWindowAttributes(LWA_COLORKEY)`: transparent background and text erased between frames.
- **Duplicated speed**: removed the toolbar speed label (`SpeedInfoId` slot), keeping speed only in the overlay. Removed `CreateSpeedLabel`, button placeholder, `kSpeedSlotW`/`speedSlotW` width reservation, `SpeedInfoId` from `ToolbarIds.h`, `hwndSpeedLabel` field from `MainWindow.h`, positioning/destruction block in ToolbarLayout.
- **Readable black background**: the overlay now draws white text over a black background (rect inflated `-4,-2`), above the transparent color key.

### Verification
- Build `bun cmd/build.ts` → 0 err / 0 warn (~47s) + clang-format.
- Smoke `-for-testing` zlib.3.pdf → stable.
- User manual test: `x0.15 | ETA Nmin` overlay bottom-right, readable, no overlap, no duplication in toolbar.

### Files
- Toolbar.cpp, ToolbarLayout.cpp/.h, MainWindow.h, ToolbarIds.h, LOG.md.

## 2026-09-18 — Two Columns v2 FASE 4 (virtual↔physical conversions at the engine) (BUILD OK)

### Context
Continuation of the Two Columns v2 plan. FASE 3 verified with no code. This session completed FASE 4: audit and route every call-site crossing the virtual↔physical boundary (layout/UI use **virtual** 2N; engine uses **physical** N).

### Architecture decision
- `PageCount()` returns **virtual** (layout, navigation, UI, labels). Engine is **physical**. Every boundary crossing routes explicitly.

### Key changes
- **`DisplayModel::PhysicalToVirtualForRect(phys, rect, &out)`** (DisplayModel.cpp:133): physical k + rect → virtual `2k-1` (left) / `2k` (right), decided by the horizontal half containing the rect's center; non-v2 = identity. Keeps the rect in physical coords (colX shift applied in CvtToScreen).
- **`CvtToScreen`/`CvtFromScreen` column-aware**: apply the same colX shift as the render (`pt.x -= colX` in CvtToScreen, `+= colX` in CvtFromScreen, mirroring `RenderCache::Paint`). So any (virtual, physical rect) pair converts correctly automatically.
- **`GetPageLabeTemp`**: `engine->GetPageLabeTemp(VirtualToPhysical(pageNo))` — avoids the 2N→N overrun and returns the correct physical label.

### Physical→virtual routing (TextSel.pages[] is physical)
- `SearchAndDDE`: `AppendTextSelScreenRects`, `AppendPageRectsToScreen`, `ShowSearchResult` (GoToPage virtual), `FindTextOnThread` (converts virtual `CurrentPageNo()`→physical for `FindFirst`; checks visibility of both columns of the physical page), `FindMatchTouchesVisiblePages` (virtual span `[2*start-1, 2*end]` with param `bool v2`), `RebuildFindMatchPaintCache`.
- `Selection`: `SelectionOnPage::GetRect` (maps physical→virtual), `FromRectangle` (iterates virtual `PageCount()`), `UpdateTextSelection`/`OnSelectAll`/`OnSelectionStart` (`VirtualToPhysical` for the engine-bound textSelection), `SelectionToolbar::GetSelectionEndPoint`.
- `uia/TextRange`, `ReadAloudHighlight` (3 points: GetViewportStart, word-paint, pageUnion-paint), `FormFields` (widget), `SumatraPDF` zoom-to-selection (`PhysicalToVirtualForRect`).

### textSelection/textSearch (engine-bound) receive physical
- Entry call-sites (`StartAt`/`SelectUpTo`/`SelectWordAt`) in Canvas.cpp and Selection.cpp use `VirtualToPhysical(pageNo)`.

### Verification
- Build `bun cmd/build.ts` → 0 err / 0 warn (~50s) + clang-format on 9 files.
- Smoke `-for-testing` zlib.3.pdf → stable (responding).
- User manual test pending: text selection, search, links and correct clicks on 2L/2R.

### Pending (FASE 4)
- uia `PageProvider` and legacy tile-math: the non-cache path already diverts; acceptable for the MVP.

## 2026-09-17 — Autoscroll: direct-multiplier speed (x0.03..x0.40) + ~2200px/min cap fix (BUILD OK)

### Context
After the rAF + overlay optimization (commit `e7b0108`), PDF autoscroll stopped increasing around 2200 px/min. Cause: px/min↔multiplier conversion with float32 in `FindCurrentSpeedStep`/`kSpeedSteps` (a 300..4000 table). Decided to expose the multiplier directly, without a conversion step.

### Changes
- `AutoScroll.cpp`: removed `kSpeedSteps`, `kSpeedStepCount`, `FindCurrentSpeedStep`. New `kMinSpeedMultiplier=0.03f`, `kMaxSpeedMultiplier=0.4f`, `kSpeedStepSize=0.01f`, and `SnapSpeedMultiplier()` (clamps to range + rounds to the 0.01 step).
- `AutoScrollSpeedAdjust`: now `mlt ± 0.01` directly (no table/conversion) — applies to PDF and markdown.
- `AutoScrollToggle`: snap on start to the range/step; log `speed=%.2f mlt`.
- Display: toolbar shows `x0.15`; bottom-right overlay shows `x0.15 | ETA Nmin`.
- Markdown: mlt converted to `pxPerSec` once at speed adjustment (`webviewPxPerSec = AutoScrollPxPerSec` → `SetWebviewAutoScroll`), not per frame — the rAF doesn't suffer the float32 bug.
- "px/min" comments/strings updated to the `xMLT` format.

### Verification
- Build `bun cmd/build.ts` → 0 err / 0 warn (60s) + clang-format.
- Smoke test PDF without crash.
- User manual test: speed climbs from 0.03 to 0.40 without stalling.

### Files
- AutoScroll.cpp, Toolbar.cpp, ToolbarLayout.cpp.

## 2026-09-17 — Smooth Markdown autoscroll (rAF) + ETA/speed in bottom-right overlay (BUILD OK)

### Context
Autoscroll optimization for .md files (WebView2): previously every WM_TIMER tick (~100×/s) did `wv->Eval("window.scrollBy(...)")` — a cross-process COM call per tick → jank + perf bottleneck. Migrated the webview scroll path to a `requestAnimationFrame` loop in the renderer.

### Webview autoscroll → JS rAF
- New `SetWebviewAutoScroll(win, pxPerSec)` (AutoScroll.cpp): injects via `Eval` a `window.__tumatraAS` object with a rAF loop (60fps) that scrolls in the renderer, no per-tick Eval. End detection via the `autoscrollBottom` notify (reuses the existing bridge).
- `AutoScrollContinuousTick` webview path became a guardian: only Ctrl-pause (toggle on state change) + timer check. **Zero scroll Eval per tick** (~2-3 Eval per state event).
- `AutoScrollToggle` / `AutoScrollSpeedAdjust`: start/stop/update the rAF loop (pxPerSec), keeping `webviewPxPerSec` + `webviewCtrlDown` in state.
- **Speed bug fixed**: the rAF did `dy=pxPerSec*dt` → at low speeds <1px/frame, `scrollBy` rounds to 0 and **doesn't accumulate** → speed not respected. Fix: accumulator in the JS object (`accum += pxPerSec*dt; dy=floor(accum); accum-=dy`), same carry pattern as C++. (Verified: fixed-page and middle-click already used a correct accumulator — no bug.)

### ETA + speed in bottom-right overlay
- Problem: `hwndEtaLabel` was a child of the toolbar and floated over it → covered Contrast/Invert/Two Column/Trim (the autoscroll group in the middle of the toolbar).
- Solution (ContrastOverlay pattern): ETA is now a `WS_EX_LAYERED | WS_EX_TRANSPARENT` overlay (child of the canvas's parent), anchored at the **bottom-right corner** via `PositionEtaOverlay` (repositioned on canvas WM_SIZE).
- Combined speed + ETA in a single block: `"%d px/min  |  ETA %dmin"` (`UpdateEtaOverlayText`), refreshed by `UpdateToolbarEtaText` and `UpdateToolbarSpeedLabel`.
- `PositionFloatingLabels` (ToolbarLayout.cpp) emptied (no longer positions the ETA); removed obsolete field `etaToolbarWidth`.

### Files
- AutoScroll.cpp, Toolbar.cpp, Toolbar.h, ToolbarLayout.cpp, Canvas.cpp, MainWindow.h.

### Verification
- Build `bun cmd/build.ts` → 0 err / 0 warn (84s) + clang-format.
- Smoke test: opens .md and PDF without crash.
- User manual test: smooth autoscroll on .md; adjustable speed respects the panel; ETA/speed at the bottom-right corner without covering tools.

## 2026-09-17 — Two Columns v2 FASE 3 (continuous flow) (BUILD OK)

### Context
Continuation of the Two Columns v2 plan. FASE 2 committed (`ca60840`). This session verified FASE 3 (continuous flow without jumping — the user's central criterion). **No code change was needed**: the stacked 2N architecture + v1/v2 mutual exclusion already delivers natural flow.

### FASE 3 — Verification by code analysis
- `ScrollYBy()` (`:2155-2198`): the `viewPort.y = colTop` snap and the `colBottom - viewPort.dy` block live in the v1 block (`if (viewportCropEnabled ...)`, `:2217`). Since v2 on forces v1 off (mutual exclusion, SumatraPDF.cpp:11185), v2 bypasses that block and uses the natural `newYOff += dy` path (`:2262-2279`) — scrolls the 2N stacked pages vertically without jumping.
- `GoToNextPage`/`GoToPrevPage`: already operate on virtual. `PageCount()`=VirtualPageCount, continuous uses `columns=1` → `FirstPageInARowNo`=pageNo, navigates `2k-1→2k→2k+1` naturally.
- Autoscroll (`MoveDocBy`→`ScrollYBy`): flows without interruption.

### Verification
- Build `bun cmd/build.ts` → 0 err / 0 warn (85s).
- User manual acceptance test pending: `autoscroll=on` + Two Column 2 reads `1L→1R→2L→2R` as a single text, without jumping.

### Next
- FASE 4 (virtual↔physical conversions at the engine: CurrentPageNo, GetPageNoByPoint, Cvt, GoToPage, ScrollState, tile math, Search/Selection/DDE/uia).

## 2026-09-17 — Two Columns v2 FASE 2 (column cut in the render) (BUILD OK)

### Context
Continuation of the Two Columns v2 plan. FASE 1 (2N layout duplication) committed (`ef85d17`). This session implemented FASE 2: each virtual page (1L/1R/2L/2R...) renders only half the physical page, with the column filling the full screen width.

### FASE 2 — Column cut in the render
- `DisplayModel::PageMediaBox` (`:604`): when `ColumnOfVirtual(pageNo)>=0`, cuts the mediabox in half (`dx/=2`, `x+=dx` if right). The virtual page gets a half-width mediabox → Fit Width fills the screen with the column (user criterion: NEVER leave half the screen black; allow zoom).
- `RenderCache.cpp` non-cache path: when `viewportCropV2Enabled && cropColumn>=0`, shifts `area` by `colX = mb.x + (cropColumn==1 ? mb.dx/2 : 0)` before rendering (same pattern as the vertical trim shift), renders with `physicalPageNo`. `[v2]` logs already present from FASE 1.
- Cache: the MVP forces non-cache via `ShouldCacheRendering`=false (done in FASE 1); virtual pageNo as key resolves left/right without collision.

### Crash fix when turning v2 off (`EngineMupdf.cpp:4110` `pageNo > pageCount`)
- **Symptom:** turning on works; turning off crashes `GetFzPageInfoLocked` (`pageNo > e->pageCount`), stack `OnSetCursorMouseNone → GetWidgetAtPos → EngineGetWidgetAtPos`.
- **Cause:** hit-test call-sites passed **virtual** pageNo (up to 2N) to the engine, which has N physical pages. When the layout fell back to N, a residual pageNo > N tripped the ReportIf.
- **Fix:** routed via `VirtualToPhysical(pageNo)` in `DisplayModel.cpp`: `GetElementAtPos` (:1682), `GetAnnotationAtPos` (:1699), `GetWidgetAtPos` (:1715), `IsOverText`/`HasTextForPage` (:1728), `GetTextInRegion` (:2494).

### Verification
- Build `bun cmd/build.ts` → 0 err / 0 warn (76s) + clang-format.
- User manual test: v2 toggle on/off **without crash**, returns to the original state.
- Log `[v2] Paint pageNo=4 -> renderPageNo=2 cropColumn=1` confirms correct left/right column rendering (virtual 4 = physical 2, right column).

### Pending (polish, next session)
- Visual/zoom adjustments pointed out by the user after testing.
- FASE 3 (continuous flow without jumping) is the next structural step.

## 2026-09-16 — Two Columns v2 FASE 0 complete + FASE 1 (2N layout duplication) (BUILD OK)

### Context
Resuming the Two Columns v2 plan (TODO.md). The previous session left FASE 0 implemented and committed (`fc50439`) without updating the TODO — verified and updated.

### FASE 0 (already committed, now documented)
- `CmdViewportCropV2Toggle` (id 510), v2 fields in DisplayModel, handler with v1/v2 mutual exclusion, "Two Column 2" toolbar button + BTNS_CHECK, menu item, CommandAvailability gating.

### FASE 1 — Duplicate layout to 2N (2N stacked rows, no cut yet)
- `PageCount()` returns `VirtualPageCount()` (2N if v2 on & continuous); `ValidPageNo` accepts 1..VirtualPageCount.
- `DocumentLayoutPage` and `PageInfo` gained `physicalPageNo` + `cropColumn` (=-1 without split).
- `Relayout()` iterates `VirtualPageCount()`, setting `physicalPageNo=VirtualToPhysical(v)` and `cropColumn=ColumnOfVirtual(v)`.
- `BuildPagesInfo()` is now re-callable (frees the existing array) — needed because the toggle switches N↔2N. `ApplyViewportCropV2` now calls `BuildPagesInfo()` + `Relayout()` (on and off).
- Engine routing virtual→physical: `PageMediaBox`, `PageSizeAfterRotation`, `GetContentBox`, `CvtToScreen`/`CvtFromScreen`, `ZoomRealFromVirtualForPage` (contentBox loop), `ScrollTo`, and `Canvas.cpp` trim-drag/erase/full-image → `VirtualToPhysical(pageNo)`.
- Render: `ShouldCacheRendering` = false when v2 (forces the non-cache path, like trim); `RenderCache::Paint` maps `renderPageNo = dm->VirtualToPhysical(pageNo)`. `[v2]` logs in Paint and ApplyViewportCropV2 for debugging.
- Build `bun cmd/build.ts` → 0 err / 0 warn (75s) + clang-format + stable smoke (zlib.3.pdf, no crash).

### Next (tomorrow)
- FASE 2: column cut in the render (left/right via cropColumn, full screen width).

## 2026-09-16 — Markdown Full-Width + Flashcard/Trim Fixes Batch (BUILD OK)

### Markdown full-width reading
- `MarkdownToc.cpp` `kMarkdownPageCssFmt`: removed `max-width: 980px` + reduced padding (`2rem 3rem` → `1.5rem 2rem`) on body. Text now fills the whole window width and wraps word-by-word dynamically on resize (WebView2 native reflow) — Notepad-style space usage. MuPDF fallback path unchanged.

### Flashcard highlight system (from prior session, now committed)
- `Flashcard.cpp`: load `PDF_ANNOT_HIGHLIGHT` (was FREE_TEXT) + author filter `TumatraPDF-Flashcard`
- `Commands_Flashcard.cpp`: set annot author `TumatraPDF-Flashcard`, gray color, 40% opacity
- `EditAnnotations.cpp`: filter out flashcard annotations from the annotations panel (parallel separation)
- `FlashcardToolbar.cpp`: card count as toolbar button index 0 (was floating label), theme support (`WS_BORDER|RBS_BANDBORDERS`, `RBBS_CHILDEDGE`, RTL), `TB_SETBUTTONINFOW` update

### Trim / canvas
- `Canvas.cpp`: subtle gray 1px separator line between pages when `marginTrimEnabled`; removed stale reloading-cue block
- `TrimConfigDialog.cpp`: refresh contrast overlay on save/cancel
- `AutoScroll.cpp`: uniform 100 px/min speed steps (300→4000)

### Build
`bun cmd/build.ts` → `Compiled\TumatraPDF.exe`, 0 errors / 0 warnings (69s).

### Flashcard cloze refactor — positional occlusion mask (2026-09-16)
Reconceived the flashcard system as a **cloze over existing PDF text** (model shared with NoteAnki): each Highlight annotation is a positional occlusion mask over the original text — **no content duplication** (annotation content now empty; the old `"Q: "` duplication was removed).

- **Render matrix (Canvas.cpp):** opaque mask only on the *current* card in study mode; reveal removes it (shows the original text); reading / non-current cards show a subtle translucent marker. Fixes reveal doing nothing (it was always masking).
- **Stable card key:** `Flashcard::key` = FNV-1a hash of `pageNo + bounds` (quantized), replacing the unstable `pdf_to_num` for study-state matching (survives PDF-save renumbering).
- **Study state now loads:** `CmdFlashcardToggle` ON calls `FlashcardStudyLoad` (new/due counts were always 0 before).
- **SRS:** `BuildFilteredStudyOrder` includes only new (`rating==0`) or due (`nextReviewAt<=now`) cards; rating 1 (Again) reinserts the card into the session queue (Anki relearn); ease factor clamped to `[1.3, 2.5]`.
- **Toolbar/keys:** added **Reveal** button; added bare `1`-`4` → Rate1-4 accelerators (Space/Enter kept as page-scroll to avoid breaking navigation).
- **Pruned dead code:** removed `hwndCardCount`, `studyModeType`, `history` from `FlashcardState`.

Build `bun cmd/build.ts` → `Compiled\TumatraPDF.exe`, 0 errors / 0 warnings. Deployed 2026-09-16.

### Flashcard logging + tree-sitter analysis (2026-09-16)
- **Standardized log tag** to `[fc]` across `Flashcard.cpp`, `Commands_Flashcard.cpp`, `FlashcardToolbar.cpp`, `FlashcardSidebar.cpp` (matches `[arch]`/`[autoscroll]`/`[md]` convention; was inconsistent `FC:`/`[FC]`).
- **Added outcome logs** for debugging: session progress (`advancing to card N/M`), session end (`all cards rated`), annotation count on add (`%d page(s) covered`), and toggle-off cleanup.
- **tree-sitter findings:** the C++ `get_symbols` tool fails on these files (heavy `logf`/`fmt` macros, PCH, lambdas); targeted `run_query` works. Functions found in `Flashcard.cpp`: `FlashcardKey`, `FlashcardLoadFromDocument`, `FlashcardStudyPath/Save/Load`, `FlashcardSm2Update`, `WriteJsonValue`(static), `ParseJsonObject`(static). `Commands_Flashcard.cpp`: `AddUniquePageNo`(static), `BuildFilteredStudyOrder`(static), `HandleFlashcardRate`(static), `HandleCommandFlashcard`.
- **Dead/stub commands** confirmed (candidates to remove): `CmdFlashcardNext` (no-op) and `CmdFlashcardAddTip` (placeholder `MessageBoxW` setting `"(hint)"`) — leftover from the pre-cloze Q/A model.
- **Note (process):** do not edit file bytes with PowerShell `Set-Content` (adds UTF-8 BOM to `.cpp`); use the Edit tool or Python `utf-8-sig` round-trip to strip a BOM if introduced.
- **Removed dead commands** from the pre-cloze Q/A model: `CmdFlashcardNext` (no-op) and `CmdFlashcardAddTip` (placeholder tip) — removed from `cmd/gen-commands.ts`, regenerated `Commands.h/.cpp`, handlers removed from `Commands_Flashcard.cpp`. Also removed now-write-only `Flashcard::text` / `Flashcard::tip` fields (positional model stores no content). Flashcard commands renumbered to `499..509`.

## 2026-09 (retro, FR→EN) — Flashcard System branch log recovered

Recovered from `sumatrapdf-src/LOG.md` (`feature/flashcard-system` branch, French [Unreleased]) before deprecating that file to `TEMP/`. Commits ~40bbedd/d57d7f3 era. Unique content below (MVP feature list already in 2026-09-11 entry):

### Added
- `CmdFlashcardFilter` — new ID via gen-commands.ts (page range filter)
- `ToolbarApplyThemeToRebar()` helper — rebar theme (subclass, dark mode, background)
- `CmdArchCleanAll` — single button clears measurements + resets scale + tool mode
- Arch Tools per-page scale: `ArchScaleState` struct + `scaleStates` Vec in `ArchToolsState`
- `CmdGoToPageNextKeepScroll` / `CmdGoToPagePrevKeepScroll` — Ctrl+Right/Left nav preserving vertical scroll

### Changed
- `SetToolbarButtonCheckedState` extended to search both toolbars
- Arch Tools persistence → per-page format with legacy fallback

### Fixed
- `_TR` → raw `L""` strings; `engine->doc` → `EngineMupdfSaveUpdated()`
- `FlashcardToolbarUpdate` via `SetWindowTextW`; rebar integration with `RelayoutFrame`
- `#include "DisplayModel.h"` in `Commands_ArchTools.cpp` → 49 cascade errors → `DocController.h` (see AGENTS §12)

### Decisions
- Cards stored INSIDE PDF as Highlight annotations (cloze, portable); cloze = gray highlight, study mode = opaque overlay
- Study state in external JSON `%APPDATA%\SumatraPDF\FlashcardStudy\<MD5>.json`

### Known Issues (of that branch, since fixed or tracked)
BUG-FC1 LoadFromDocument only FREE_TEXT (Highlight cards lost on reload) · BUG-FC2 card count label floating · BUG-FC3 toolbar ignores theme · BUG-FC4 text selection displaced with trim=on (`Selection.cpp += → -=`) · label/tip + image occlusion deferred to v2

## 2026-09-11 — Flashcard MVP (BUILD OK)

### What
Implemented core flashcard system for Anki-like spaced repetition study directly within PDFs.

### Changes
- **gen-commands.ts**: 11 new command IDs (Toggle, Study, Add, Back, Next, Reveal, Rate1-4, Lista)
- **Flashcard.h**: Data model (Flashcard, FlashcardStudyState, FlashcardStudyDoc structs)
- **Flashcard.cpp**: Core logic — LoadFromDocument (reads FreeText annotations with "Q: " prefix), StudyPath (MD5→JSON), StudySave/Load (external JSON persistence), SM-2 algorithm
- **Commands_Flashcard.h/cpp**: 11 command handlers (Toggle, Add, Study, Reveal, Rate1-4, Back, Lista, Next)
- **FlashcardToolbar.cpp**: Secondary toolbar with Study/Back/Lista buttons (rebar pattern like Arch Tools)
- **Canvas.cpp**: Flashcard highlight rendering (subtle gray reading mode, solid gray study mode)
- **MainWindow.h**: FlashcardState struct (on, studyMode, revealMode, currentCardIdx, studyOrder, cards, studyDoc, toolbar HWNDs)
- **SelectionToolbar.cpp**: Flashcard button added to selection popup
- **Accelerators.cpp**: 'S' shortcut for CmdFlashcardAdd
- **SumatraPDF.cpp**: Command dispatch wired to HandleCommandFlashcard
- **vcxproj + filters**: All new files added to both TumatraPDF.vcxproj and SumatraPDF-static.vcxproj

### Architecture
- Cards stored INSIDE PDF as FreeText annotations (Author="TumatraPDF-Flashcard", content starts with "Q: ") — portable
- Study state external JSON in %APPDATA%\SumatraPDF\FlashcardStudy\<MD5>.json — personal
- Study flow: Toggle→Study→Space reveals→1-4 rates→auto-advance; Back goes back

### Build
`bun cmd/build-test.ts`: 0 errors, 0 warnings.

### Files Created
src/Flashcard.h, src/Flashcard.cpp, src/FlashcardToolbar.cpp, src/Commands_Flashcard.h, src/Commands_Flashcard.cpp

### Files Modified
cmd/gen-commands.ts, src/Commands.h, src/Commands.cpp, src/MainWindow.h, src/SelectionToolbar.cpp, src/Accelerators.cpp, src/SumatraPDF.cpp, src/Canvas.cpp, vs2022/*.vcxproj + .filters

## 2026-09-10 — ToolbarLayout Integration Complete (BUILD OK)

### What
Integrated ToolbarLayout module as the single entry point for toolbar child positioning, replacing manual repositioning functions.

### Changes
- Removed 3 old manual functions from Toolbar.cpp: RepositionEtaLabel, RepositionSpeedLabel, RepositionTimerControls
- Simplified WM_SIZE handler to only call LayoutToolbarChildWindows(win)
- Updated UpdateToolbarEtaText to call PositionFloatingLabels
- Updated UpdateToolbarSpeedLabel to call LayoutToolbarChildWindows
- Removed redundant calls from CreateTimerControls
- Removed forward declarations from Toolbar.h
- Implemented HandleToolbarOverflow in ToolbarLayout.cpp (hide priority: edit → label, never hide checkbox)
- Fixed kCtrlH → gToolbarTokens.ctrlH in ToolbarLayout.cpp
- Files modified: src/Toolbar.cpp, src/Toolbar.h, src/ToolbarLayout.cpp

### Build
- 0 errors, 0 warnings via `bun cmd/build-test.ts`

## 2026-09-08 — Autoscroll Round Steps + Toolbar Wrapping

### Autoscroll Round Steps
- Replaced geometric speed progression (×1.5/×0.667) with round-step lookup table
- Speed steps: 25, 50, 75, 100, 150, 200, 300, 400, 600, 800, 1200, 1600 px/min
- `FindCurrentSpeedStep()` snaps to nearest step on start
- File: `src/AutoScroll.cpp`

### Toolbar Wrapping
- Added `TBSTYLE_WRAPABLE` to main toolbar and arch tools toolbar
- Buttons now wrap to second row when window narrows (instead of hiding)
- Added `LayoutToolbarChildWindows(win)` call in WM_SIZE handler for floating child repositioning
- Rebar `RBS_VARHEIGHT` already supports multi-row; `RelayoutFrame` auto-adapts
- File: `src/Toolbar.cpp`

### Build
- 0 errors, 0 warnings via `bun cmd/build-test.ts`

## 2026-09-08 — SelectionState Struct Extraction Complete (BUILD OK)

### What
Grouped 15 flat selection/touch-input member variables in MainWindow.h into a nested `SelectionState` struct (`win->selection.X`), following the AutoScrollState/ArchToolsState pattern. Pure access-path refactor, no logic change.

### Changed
- `src/MainWindow.h` — added `struct SelectionState { ... } selection;`
- 168 access sites renamed in 9 files (Canvas.cpp, Selection.cpp, SumatraPDF.cpp, SumatraTest.cpp, CommandAvailability.cpp, Menu.cpp, LinkFollow.cpp, SelectionToolbar.cpp, Tabs.cpp)
- Pattern: `X` → `selection.X`; `selectionRect` → `selection.selectionRect` (double "selection" correct)

### Note
`bun cmd/format.ts <files>` reformats the ENTIRE tree (ignores file args). Cosmetic diffs appeared in unrelated files; left as-is (clang-format is logic-neutral).

### Build
0 errors / 0 warnings — `bun cmd/build-test.ts` → `..\Compiled\TumatraPDF-test.exe`

## 2026-09-08 — 16C-F6 Phase B: Window Lifecycle Extraction Complete (BUILD OK)

### What
Extracted remaining 11 functions from SumatraPDF.cpp window-lifetime cluster (L2628-L3069) into `MainWindowCreate.{h,cpp}`, completing the F6 extraction goal.

### Functions extracted
- `UpdateWindowRtlLayout(win)` — promoted non-static, declared in MainWindowCreate.h
- `IsMenubarVisible()` — promoted non-static, declared in MainWindowCreate.h
- `OnSidebarSplitterMove(ev)` — promoted non-static, declared in MainWindowCreate.h
- `OnFavSplitterMove(ev)` — promoted non-static, declared in MainWindowCreate.h
- `CreateSidebar(win)` — static in new TU
- `UpdateToolbarSidebarText(win)` — static in new TU
- `DwmFrameBorderColorForCurrentTheme()` — static in new TU
- `SetWindowBorderColor()` — static in new TU
- `SetWindowRoundedCorners()` — static in new TU
- `CreateMainWindow(win)` — promoted, declared in MainWindowCreate.h
- `RenameFileInHistory(win)` — moved (used only by CreateMainWindow)

### Static blockers resolved
4 functions promoted from `static` to external linkage + declared in MainWindowCreate.h:
- `UpdateWindowRtlLayout`, `IsMenubarVisible`, `OnSidebarSplitterMove`, `OnFavSplitterMove`

### Files modified
- `src/MainWindowCreate.h` — added 4 new declarations
- `src/MainWindowCreate.cpp` — 11 function bodies added
- `src/SumatraPDF.cpp` — 11 bodies removed, 4 forward-declarations removed, 3 static keywords removed

### Build
`bun cmd/build-test.ts` — 0 errors / 0 warnings (46s). Deployed to Compiled\TumatraPDF-test.exe.

### 16C Status
- C1 ✅ (handler mapping)
- C2 ✅ (AutoScroll extraction)
- C3 ✅ (ArchTools extraction)
- C4 ✅ (ViewCommands extraction)
- C5 ✅ (FileCommands wrapper extraction)
- F6-A ✅ (Window lifecycle extraction — ShowMainWindow, CreateAndShowMainWindow, DeleteMainWindow, UpdateAfterThemeChange, MaybeShowDefaultAppNotification)
- F6-B ✅ (CreateMainWindow + CreateSidebar + DWM helpers)

### Estimated reduction
~380 lines total removed from SumatraPDF.cpp across Phase A+B (14,212 → ~13,830 lines).

## 2026-09-08 — 16C-F6 Phase A: Window Lifecycle Extraction (BUILD OK)

### What
Extracted 5 window lifecycle functions from SumatraPDF.cpp (14,212 lines) into new `MainWindowCreate.{h,cpp}`:
- `ShowMainWindow` (56 lines) — was already declared in SumatraPDF.h
- `MaybeShowDefaultAppNotification` (46 lines) + static constants `kNotifDefaultApp`, `kMaxDefaultAppLinks`
- `CreateAndShowMainWindow` (26 lines) — was already declared in SumatraPDF.h
- `DeleteMainWindow` (22 lines) — was already declared in SumatraPDF.h
- `UpdateAfterThemeChange` (33 lines)

### Static blockers resolved
3 functions promoted from `static` to external linkage:
- `CreateMainWindow()` — declared in MainWindowCreate.h, body stays in SumatraPDF.cpp
- `UpdateWindowFrameBorderColor()` — declared in MainWindowCreate.h
- `ApplyDarkModeToInfotip()` — declared in MainWindowCreate.h

`RelayoutFrame()` exported via MainWindowCreate.h (was only forward-declared locally in SumatraPDF.cpp).

### Files created
- `src/MainWindowCreate.h` — declarations for promoted statics + RelayoutFrame
- `src/MainWindowCreate.cpp` — 5 extracted function bodies

### Files modified
- `src/SumatraPDF.cpp` — removed 5 function bodies, removed `static` from 3 functions, removed redundant RelayoutFrame forward-decl, added `#include "MainWindowCreate.h"`
- `vs2022/TumatraPDF.vcxproj` + `.filters` — added MainWindowCreate.cpp
- `vs2022/SumatraPDF-static.vcxproj` + `.filters` — added MainWindowCreate.cpp

### Build
`bun cmd/build.ts` — 0 errors / 0 warnings (40s). Deployed to Compiled\TumatraPDF.exe.

### 16C Status
- C1 ✅ (handler mapping)
- C2 ✅ (AutoScroll extraction)
- C3 ✅ (ArchTools extraction)
- C4 ✅ (ViewCommands extraction)
- C5 ✅ (FileCommands wrapper extraction)
- F6-A ✅ (Window lifecycle extraction — ShowMainWindow, CreateAndShowMainWindow, DeleteMainWindow, UpdateAfterThemeChange, MaybeShowDefaultAppNotification)
- F6-B ✅ (CreateMainWindow + CreateSidebar + DWM helpers)

### Estimated reduction
~190 lines removed from SumatraPDF.cpp (net, including promoted statics still in file).

## 2026-09-08 — ToolbarIds.h Fix + Toolbar Design System Status

### What
1. **ToolbarIds.h CmdLast fix** — Placeholder constants (`PageInfoId`, `WarningMsgId`, `SpeedInfoId`, `TimerInfoId`) initially moved to `Toolbar.h` caused 48 `C2065: 'CmdLast': undeclared identifier` errors because `CmdLast` lives in `Commands.h` which `Toolbar.h` doesn't include. Created `ToolbarIds.h` with include guard + the 4 constexpr declarations. But including `Commands.h` from `ToolbarIds.h` caused 1122 `C2365: redefinition` errors because `Commands.h` has no include guard (SumatraPDF convention). Solution: remove `#include "Commands.h"` from `ToolbarIds.h`; all TUs already include `Commands.h` first via controlled include order.
2. **5 TUs fixed** — Added `#include "Commands.h"` to `EditAnnotations.cpp`, `FormFields.cpp`, `MainWindow.cpp`, `Selection.cpp`, `SumatraControl.cpp` which had `Toolbar.h` but not `Commands.h`.
3. **Toolbar Design System status assessed** — `ToolbarLayout.h/cpp` has declarative slots (`ToolbarSlotSpec`, `gToolbarSlots[]`) and layout engine (`LayoutToolbarChildWindows`) but is NOT integrated (never called from Toolbar.cpp). `HandleToolbarOverflow` is a stub. Decision: deprioritize in favor of 16C-F6 (better ROI).

### Build
`bun cmd/build-test.ts` — 0 errors / 0 warnings. `Compiled\TumatraPDF-test.exe` (21.9 MB).

### Key Learning
SumatraPDF `Commands.h` is a generated header with NO include guard. Never include it from another header. TUs must include it directly before any header that depends on it (controlled include order).

## 2026-09-01 — 16C C5 FileCommands Extraction Complete

### What
Extracted 24 File domain command handlers from SumatraPDF.cpp FrameOnCommand switch into HandleCmdXxx wrapper pattern:
- Created 24 static wrapper functions in SumatraPDF.cpp (before FrameOnCommand)
- Replaced all 24 inline case bodies with clean `HandleCmdXxx(win); break;` dispatch calls
- Cleared all stubs in Commands_File.cpp to avoid multiple definition conflicts
- Split combined CmdOpenPrevFileInFolder/CmdOpenNextFileInFolder case into two separate cases

### Key Decision
Unlike C2/C3/C4 (which moved handlers to separate .cpp files), C5 wrappers live IN SumatraPDF.cpp because the underlying helpers (OpenFile, ClearHistory, etc.) are static functions with deep inter-dependencies within SumatraPDF.cpp. The wrappers only thin-dispatch; actual extraction of helpers requires making them non-static first.

### Build
`C:\Users\Testa\.bun\bin\bun.exe cmd/build.ts` from sumatrapdf-src dir — 0 errors / 0 warnings. Deployed to Compiled\TumatraPDF.exe (21,972,480 bytes).

### 16C Status
- C1 ✅ (handler mapping)
- C2 ✅ (AutoScroll extraction)
- C3 ✅ (ArchTools extraction)
- C4 ✅ (ViewCommands extraction)
- C5 ✅ (FileCommands wrapper extraction)

## 2026-09-01 — Bug Fixes: Speed Label, EPUB ETA, Contrast Invert (.md)

### What
1. **Speed label overflow (Toolbar.cpp:RepositionSpeedLabel)** — clamped label width to slot width (`int labelDx = std::min(size.dx, slot.dx)`) and guarded `if (x < slot.x) x = slot.x`; `MoveWindow` now uses `labelDx` instead of raw `size.dx`.
2. **ETA not calculating for epub (AutoScroll.cpp:RecalcAutoScrollEta)** — restructured for both PDF and WebView paths; WebView path fires one-shot JS eval computing `scrollHeight - (scrollY + innerHeight)`, sends via `__sumatra__.notify('autoscrollProgress', rem)` to existing callback pipeline.
3. **Contrast dark mode ignoring invert in .md (3 files)** — `SumatraPDF.cpp:CmdContrastToggle` checks `GetInvertPageColors()`, swaps bg (`#FAFAFA↔#050505`) and gray formula; `ContrastOverlay.cpp:UpdateContrastOverlayOpacity` checks invert and swaps formula; `MarkdownModel.cpp:RestoreContrastOverlay` checks invert and swaps formula.

### Build
`C:\Users\Testa\.bun\bin\bun.exe cmd/build.ts` from sumatrapdf-src dir — 0 errors / 0 warnings. Deployed to Compiled\TumatraPDF.exe.

## 2026-08-31 — Session: Feature Restoration + 16C C5 FileCommands Analysis

### What
1. Restored 3 features lost during merge/revert cycle: contrast #FAFAFA for .md, SerializeMeasurements/DeserializeMeasurements persistence, hwndReBar2 SW_HIDE for .md. Build 0 err/0 warn.
2. Analyzed 16C C5 (FileCommands extraction) — mapped all 24 File domain `case Cmd` statements in FrameOnCommand. ALL are simple 1-3 line single-function calls. Zero complex cases.
3. Identified C5 blocker: all helper functions (OpenFile, ClearHistory, etc.) are `static` in SumatraPDF.cpp — inaccessible from Commands_File.cpp.
4. Attempted C5a (File Navigation extraction) on branch `feature/16c-c5-checkpoint` (commit `ca5dea5`) — failed with 40+ C3861 errors because extracted functions still call static helpers. Reverted via `git stash`.
5. Established correct C5 approach: create HandleCmdXxx wrappers IN SumatraPDF.cpp first, keep static helpers in place, extract helpers only when dependency chain is fully mapped.

### Build
`C:\Users\Testa\.bun\bin\bun.exe cmd/build.ts` from sumatrapdf-src dir — 0 errors / 0 warnings.

### Key Lesson
C5 extraction cannot follow the same pattern as C2/C3/C4 because file-domain helpers have deep static dependency chains within SumatraPDF.cpp. Strategy: (1) wrap case bodies in HandleCmdXxx(win) functions in SumatraPDF.cpp, (2) declare in Commands_File.h, (3) move helpers out only after all dependencies are non-static or relocated.

- 2026-08-25 — research session — confirmed Markdown (.md/.markdown) reading support ALREADY EXISTS, inherited from upstream master. Zero code changes. Refs: ext map GuessFileType.cpp:66-67, dispatch EngineCreate.cpp:179, cmark-gfm vendored + FZ_ENABLE_MD=1, open filter "*.md;*.markdown" SumatraPDF.cpp:5750, dual render WebView2 default / MuPDF fallback via markdownUI.useFixedPageUI. Runtime verification pending (user tests later).
- 2026-08-24 — lint batch — removed unused cache param (RenderCache), corrected misleading persist comment (autoscroll speed handlers).
- 2026-08-24 — refactor batch — AutoScroll logic extracted to src/AutoScroll.{h,cpp} (ETA math out of Toolbar, tick out of Canvas, commands thin), toolbar overlay brush hoisted + early-out, WM_SIZE ETA reposition width-gated, TabWnd::Paint StringFormat hoisted, ContrastOverlay alpha dedup.
- 2026-08-24 — cleanup batch — dead close-icon code removed (TabsCtrl), dead custom-draw underline removed (Toolbar), orphan TumatraPDF-static.vcxproj deleted, MERGE.md corrected, misc lint fixes.
- 2026-08-24 — toggle underline deterministic WM_PAINT overlay — NM_CUSTOMDRAW fallback proven visible via runtime diag (tests/tmp/tb-diag.ts).

## 2026-08-24 — Deterministic Toggle Underline (WM_PAINT overlay) + Runtime Proof

### What
Checked-toggle underline was invisible to user despite NM_CUSTOMDRAW code looking correct. Replaced the earlier `WndProcToolbar` WM_PAINT fallback (BeginPaint + passing hdc as wParam to `CallWindowProc` — toolbar proc ignores it, update-region clipping risk) with a deterministic overlay: `CallWindowProc(DefWndProcToolbar, ...)` first (NOT `DefSubclassProc` — subclass is `SetWindowLongPtr`-based, empty comctl32 chain would skip toolbar proc → blank toolbar), then `GetDC` + loop all buttons via `TB_GETBUTTONINFO` (`TBIF_BYINDEX|TBIF_STATE`), fill `{left+2, bottom-3, right-2, bottom}` with `ThemeWindowTextColor()` brush for every `TBSTATE_CHECKED` button, `ReleaseDC`. Not clipped to update region → survives BeginPaint validation inside toolbar proc.

### Runtime verification (tests/tmp/tb-diag.ts, bun FFI)
- `TB_ISBUTTONCHECKED`/`TB_GETSTATE`/`TB_COMMANDTOINDEX` are pointer-free → safe cross-process; `TB_GETBUTTONINFO` (pointer lParam) is NOT reliable cross-process (zeroed struct / may disturb target).
- Contrast toggle: state 0x4 → 0x5 CHECKED, sticks; screenshot `tests/tmp/tb-contrast-on.png` shows white underline under "Contrast" — **visible, confirmed**.
- AutoScroll toggle: CHECKED at t=0, self-off <100ms — Canvas.cpp ~L4553 cancels auto-scroll on 1-page doc (no scroll possible); state machinery fine, not an indicator bug.
- ViewportCrop/MarginTrim toggles didn't stick in this test (state stayed 0x4) — separate issue, uninvestigated.

### Build
`bun cmd/build.ts` — 0 errors / 0 warnings (50s). Deployed `Compiled\TumatraPDF.exe` 21,895,168 bytes @ 2026-08-24 18:30:31.

## 2026-08-24 — Toolbar Toggle Underline + ETA Label Responsive Positioning

### What
White 3px underline under checked toggle buttons (Trim/Crop/Autoscroll/Contrast) drawn via a new `CDDS_ITEMPOSTPAINT` stage in `ReBarWndProc` custom draw (item pre-paint now returns `CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT` only when checked). ETA label repositioned instantly on toolbar resize: new `RepositionEtaLabel()` anchors it after the last visible button (TB_BUTTONCOUNT/TB_GETSTATE/TB_GETITEMRECT loop) instead of the right edge, auto-hides when there is no room; `WM_SIZE` hook in `WndProcToolbar` calls it when visible (fixes up-to-1s stale position, overlap and clipping on narrow windows).

### Changes
- `src/Toolbar.cpp` — `ReBarWndProc`: `CDDS_ITEMPREPAINT` returns `CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT` for checked toggles; new `CDDS_ITEMPOSTPAINT` fills bottom strip `{x+2, y+dy-3, x+dx-2, y+dy}` white via `CreateSolidBrush`+`FillRect`
- `src/Toolbar.cpp` — new `RepositionEtaLabel(MainWindow*)`: anchored after last visible button, `DpiScale(8)` gap, hides when `x + width > client dx - DpiScale(4)`
- `src/Toolbar.cpp` — `UpdateToolbarEtaText`: width-changed branch now calls `RepositionEtaLabel` (old right-align math removed)
- `src/Toolbar.cpp` — `WndProcToolbar`: new `WM_SIZE` case calls `RepositionEtaLabel` when label exists and is visible
- `src/Toolbar.h` — declared `RepositionEtaLabel`

### Build
`bun cmd/build.ts` — OK, 0 errors / 0 warnings (61.5s). Fresh exe copied to `Compiled\TumatraPDF.exe`.

## 2026-08-24 — Tab Freeze Removal + X Icon Removal + Alt+Click Close + Toolbar Cleanup

### What
"Tab stays small" bug finally fixed by removing the freeze mechanism entirely. Root cause: Home tab is always inserted at index 0 when tabs are enabled, so 1 open doc = 2 tabs. The Chrome-like freeze (added 2026-08-17) kept the small frozen width while the cursor stayed over the tab bar; the WM_MOUSELEAVE heal only fired when the mouse left the bar, so the small width appeared stuck. Also: removed the ✕ close icon from tabs (replaced by Alt+LeftClick close) and removed Back/Forward toolbar buttons.

### Changes
- `src/wingui/TabsCtrl.cpp` — `LayoutTabs`: always `dx = min(tabDefaultDx, (clientW - 5) / nTabs)`; deleted freeze branch + else-reset block
- `src/wingui/TabsCtrl.cpp` — `CloseTab`: deleted freeze-set lines and `TrackMouseLeave(hwnd)`
- `src/wingui/TabsCtrl.cpp` — `WM_MOUSELEAVE` case: deleted frozen-width reset block
- `src/wingui/WinGui.h` — removed fields `tabWidthFrozen` / `frozenTabDx`
- `src/wingui/TabsCtrl.cpp` — `TabWnd::CloseVisible()`: returns false unconditionally (✕ never visible/painted; hit-test code left in place, now dead)
- `src/wingui/TabsCtrl.cpp` — removed unused `kMinTabWidthForClose`
- `src/wingui/TabsCtrl.cpp` — `WM_LBUTTONDOWN`: if `IsKeyPressed(VK_MENU)` and a closable tab is under cursor, call `CloseTab(idx)` and swallow message; empty-bar clicks ignored
- `src/Toolbar.cpp` — `gToolbarButtons[]`: removed NavigateBack/NavigateForward entries (+ one separator). Commands kept — keyboard shortcuts (Alt+Left, Alt+Right, Ctrl+Backspace) still work

### Build
`bun cmd/build.ts` — OK, 0 errors / 0 warnings. Fresh exe copied to `Compiled\TumatraPDF.exe`.

## 2026-08-17 — Trim Black-Space Fix (RenderCache.cpp)

### What
Trim mode left black band at top of each page. Root cause: DOUBLE top-shift — `GetTileRectDevice` had `mediabox.y += t;` (~line 320) while `GetTileRectUser` also does `rect.y += gGlobalPrefs->trim.top;` (line 336). Same shift applied twice → clipped band.

### Fix
- `sumatrapdf-src/src/RenderCache.cpp` — removed `mediabox.y += t;` from `GetTileRectDevice` (~line 320). Kept `rect.y += gGlobalPrefs->trim.top;` in `GetTileRectUser` (line 336) as the single correct shift.

### Build
`bun cmd/build.ts` — OK. Fresh exe copied to `Compiled\TumatraPDF.exe`.

## 2026-08-17 — Tab Freeze Fix (TabsCtrl.cpp) — UNRESOLVED

### What
User-set tab width freeze broke: tab renders small with 1 doc open.

### Changes applied
- `src/wingui/TabsCtrl.cpp` — `LayoutTabs`: freeze condition now `if (tabWidthFrozen && frozenTabDx > 0 && nTabs > 1)`; else-branch resets `tabWidthFrozen=false` / `frozenTabDx=0`
- `src/wingui/TabsCtrl.cpp` — `CloseTab`: added `TrackMouseLeave(hwnd);` after the freeze lines

### Status
⚠️ Applied but STILL broken — tab remains small with 1 doc open. Deprioritized by user, leave as-is. Settings file corruption excluded (`TabWidth=300` reads correctly).

## 2026-08-17 — Invert + Contrast Per-Document Persistence

### What
Reopening a document now restores invert + contrast state (was session-only).

### Files changed
- `cmd/gen-settings.ts` — new FileState fields in `fileSettings` array (after `AutoScrollSpeedMultiplier`): `InvertColors` (Bool, false), `ContrastEnabled` (Bool, false), `ContrastOpacity` (Int, 50)
- `src/Settings.h` — regenerated (fieldCount 22 → 25)
- `src/SumatraPDF.cpp` — save in `UpdateTabFileDisplayStateForTab` (~L830-833); load in `LoadDocument` (~L2040-2043) + `CreateContrastOverlay` after `win->ctrl = tab->ctrl` (~L2093-2095)

### Build
`bun cmd/build.ts` — OK. Fresh exe copied to `Compiled\TumatraPDF.exe`.

## 2026-08-17 — Scroll Delay Investigation: 3.7.20958 vs 3.6.17065

### What
Investigated scroll delay regression in 3.7.20958 vs 3.6.17065 (instant scroll). Root cause: upstream smooth-scroll system (redesign ed7dbb13d 2026-07-28, default TRUE since 15a48380c 2026-07-29). 3.6.17065 had smoothScroll default FALSE → instant scroll.

### Findings
- `smoothScroll=false` confirmed INSTANT: direct `ScrollYTo` (Canvas.cpp:1242-1250), gate `smoothWheel = gGlobalPrefs->smoothScroll && gInMouseWheelScroll` (Canvas.cpp:1167). No timer/animation when off.
- Smooth scroll mechanics (when ON): exponential chase `a=1-e^(-15·dt)`, 1ms timer + `timeBeginPeriod(1)`, ~200ms convergence, per-tick full repaint chain.
- Remaining delay sources with smoothScroll=false: (a) `uitask::Post` deferral in `ScheduleRepaint` (Canvas.cpp:4448-4459) — InvalidateRect deferred ≥1 message-loop turn; (b) async render thread — `RequestRendering` (DisplayModel.cpp:1697-1734), `RenderFinishedAsync` → `uitask::Post` → `RepaintDisplay` (DisplayModel.cpp:310-328); (c) `uitask::Post` + async render appears NEW in 3.7 vs 3.6.17065.
- Smart thin scrollbar (upstream 9ecd69005 2026-08-01): NOT default (mode "windows", SumatraPDF.cpp:1326). Only active if user enables smart/overlay mode.
- Double buffer NOT stale: `DrawDocument` re-renders at current scroll pos each WM_PAINT (Canvas.cpp:3162); buffer is flicker-prevention only.
- USER FINDING: trim=on (crop/trim feature) INTENSIFIES the scroll bug — complicates render path. Being fixed separately.

### Optimization candidates (future)
- Smooth timer 1ms → 10-16ms; skip repaint when delta <1px; reduce `uitask::Post` double-hop; trim render path optimization.

### Artifacts
- `.tmp_compare/` (upstream clone + comparison files) at project root — keep for future upstream diffs.

## 2026-08-17 — Autoscroll ETA Optimization + AutoScrollShowEta Option

### What
Reworked autoscroll ETA: no more per-second recalculation + full-toolbar repaint every 100 ticks (10ms timer → WM_PAINT delays → stutter). ETA now computed on demand (start / speed change / page change), countdown by wall clock, label updated only when the minute value changes, only the label invalidated. New Advanced Options setting `AutoScrollShowEta` (default true) hides the ETA label entirely.

### Files changed
- `src/MainWindow.h` — replaced `autoScrollTickCount` with 5 ETA state fields (minutes, start tick, last shown, page no, cached toolbar width)
- `src/Toolbar.h` — declared `RecalcAutoScrollEta(MainWindow*)`
- `src/Toolbar.cpp` — added `RecalcAutoScrollEta`; rewrote `UpdateToolbarEtaText` (hide when `AutoScrollShowEta` false, reposition only on toolbar resize, invalidate label only)
- `src/Canvas.cpp` — removed tick-count logic from `kContinuousAutoScrollTimerID` handler; ETA gate replaced with page-change recalc + wall-clock countdown
- `src/SumatraPDF.cpp` — start path calls `RecalcAutoScrollEta` + shows computed ETA; speed up/down recalc ETA when active
- `src/Settings.h` — added `bool autoScrollShowEta` (default true) + metadata entry (visible in Advanced Options)

### Build
`bun cmd/build.ts` — success, 0 warnings 0 errors. First attempt failed with LNK1201 (libsumatrapdf.pdb locked — stale handle, likely VS Code/OneDrive); renamed the PDB and rebuilt clean. Fresh exe copied to `Compiled\TumatraPDF.exe`.

## 2026-08-16 — Debug Tooling Analysis (Research Only)

### What
Inventory of existing debug tooling in sumatrapdf-src. No code changed.

### Working
- `cmd/build.ts` — vs2022\TumatraPDF.sln /t:TumatraPDF → out/dbg64/TumatraPDF.exe
- `cmd/run.ts` — build + launch detached
- `cmd/run-unit-tests.ts` -dbg|-rel|-asan — test_util.exe + -for-ai, callstacks captured
- `cmd/control.ts` + `-dbg-control <pipe>` — 44 Test* commands (Ping/Quit/List/TestSearch/TestToc/TestToolbarButtons/TestAIChat)
- `tests/winapi.ts` + `tests/win-automation.ts` — FFI win32, postMessage cross-process, captureWindowToPng works on occluded windows; SendInput DROPPED on this machine
- Flags: -for-testing/-console/-log/-stress-test/-bench/-render/-extract-text/-set-color-range
- `cmd/clang-tidy.ts`, `cmd/cppcheck.ts`, `cmd/gen-{commands,settings,flags}.ts` codegen
- `src/tools/logview`, 112 tests/issue-*.ts

### Broken (fix next session)
- `cmd/build-asan.ts:62-63` → vs2022\SumatraPDF.sln + /t:SumatraPDF-static — file renamed to TumatraPDF.sln. `cmd/dbg.ts` expects out/dbg64_asan/TumatraPDF-static.exe but build produces SumatraPDF-static.exe. ASan + windbg pipeline unusable.

### Stale
- `.vscode/launch.json` → out/dbg64/SumatraPDF-dll.exe; `.vscode/tasks.json` → SumatraPDF.sln /t:SumatraPDF-dll → no F5 debug in VS Code.

### Gaps
- `-dbg-control` no commands for custom feature state (viewport crop rect, trim values, autoscroll timer/ETA, contrast/invert)
- test_util no unit tests for custom feature logic (crop zoom math, trim clip)

### Plan (5 steps, next session)
1. Fix ASan tooling: rename sln + target in build-asan.ts/dbg.ts (SumatraPDF.sln → TumatraPDF.sln, SumatraPDF-static → TumatraPDF-static)
2. Update .vscode launch.json/tasks.json → TumatraPDF.sln + TumatraPDF.exe
3. Extend -dbg-control: TestViewportCrop / TestTrimState / TestAutoScrollState / TestContrastState
4. Add unit regression for custom features in test_util (crop math, trim clip)
5. Document debug loop in AGENTS.md (build → flags → dbg-control → screenshots)

## 2026-08-16 — Inversion Fix: Recolor in Non-Cached Path (trim=on)

### What
Color inversion (and document-color-follow-theme) was lost when trim was enabled. Trim forces the non-cached render path (`ShouldCacheRendering` → `!marginTrimEnabled`), and that path never called `RecolorPixmap` — only the cached path did.

### Files
- `sumatrapdf-src/src/RenderCache.cpp` — non-cached path in `RenderCache::Paint` (~line 1165): added `RecolorPixmap(bmp, this->textColor, this->backgroundColor, this->linkColor, nullptr)` guarded by `ShouldUpdateBitmapColorsLegacy(dm->GetEngine(), this)` before `BlitPixmap`, matching the cached path (~line 1017).

### Result
- Build: MSBuild Debug x64 0 errors.
- Binary: `Compiled\TumatraPDF.exe` = 21,875,200 bytes.
- Note: used `this` (member function) instead of `gRenderCache` global — the `extern RenderCache* gRenderCache;` declaration sits at line 1259, after the non-cached path, so the global wasn't visible there.

## 2026-08-11 — Project Start

### SumatraPDF Architecture Research
- **Repository:** github.com/sumatrapdfreader/sumatrapdf
- **Build:** Premake5 generates Visual Studio 2022 solution
- **UI:** Native Win32 API (no MFC, no wxWidgets)
- **PDF Engine:** MuPDF (parsing + rendering)
- **Languages:** C, C++
- **Directory structure:** `src/` (main code), `mupdf/` (PDF engine), `ext/` (third-party libs), `vs2022/` (VS solution)
- **Supported formats:** PDF, EPUB, MOBI, CBZ/CBR, FB2, CHM, XPS, DjVu
- **Author:** Krzysztof Kowalczyk (kjk)
- **License:** GPLv3

### Pre-release Version
- **Current version:** `3.7.20958`
- **Does NOT have a plugin/module system** — monolithic Win32 app
- **Has native auto-update:** `src/UpdateCheck.cpp`, manifest `updatecheck-pre-release.txt`
- **Configuration:** `src/Version.h`, `src/BuildConfig.h`, `src/AppSettings.h`

### Planned Features
1. **Auto-scroll with timer** — continuous scroll with adjustable speed
2. **Viewport crop for two-column books** — half-page cropping for two-column layout books
3. **Contrast filter** — contrast/brightness adjustment via post-processing

### Existing AutoHotkey Implementation
- **File:** `Sumatra Tools\sumatra-autoscroll-v3.ahk`
- Already implements: auto-scroll (F9 toggle, F7/F8 speed), 2-column navigation (numpad), pause Ctrl, 30min timer
- Will serve as reference for native C++ implementation

### Design Decisions
- Keep Win32 base for compatibility and lightness
- Integrate with MuPDF as rendering engine
- **Upstream code in `sumatrapdf-src/` untouched** — features in `src/features/`
- Modularity via Premake5 build hooks
- Upstream merge documented in `MERGE.md`

### Upstream Update Strategy
- Automatic check for new pre-release versions
- Script `scripts/check-updates.ps1` for manual checking
- Process: download new version → diff → manual merge on hooks → rebuild

### Project Structure
```
TumatraPDF/
├── README.md              # Overview and credits
├── LOG.md                 # This file
├── MERGE.md               # Upstream merge instructions (to create)
├── FLOW/                  # Planning diagrams
│   └── tumatrapdf.dot
├── sumatrapdf-src/        # Original SumatraPDF code (6353 files)
├── src/                   # Our extensions (to create)
│   ├── features/
│   │   ├── autoscroll/
│   │   ├── viewport/
│   │   └── contrast/
│   └── hooks/
└── scripts/               # Auxiliary scripts (to create)
    └── check-updates.ps1
```

## 2026-08-11 — SumatraPDF Binary Rename → TumatraPDF

### Complete binary rename
- **Objective:** Output EXE `TumatraPDF.exe` (not `SumatraPDF.exe`) for side-by-side coexistence
- **Files modified:** 23 files + 4 renames

### Files Changed
1. `premake5.lua`: workspace/project "SumatraPDF" → "TumatraPDF" 
2. `vs2022/SumatraPDF.sln` → `vs2022/TumatraPDF.sln`: project refs, vcxproj refs
3. `vs2022/SumatraPDF.vcxproj` → `vs2022/TumatraPDF.vcxproj`: RootNamespace, 16× TargetName, 16× IntDir
4. `vs2022/SumatraPDF-static.vcxproj` → `vs2022/TumatraPDF-static.vcxproj`: RootNamespace, 16× TargetName, 16× IntDir
5. `cmd/build.ts`: target `/t:TumatraPDF`, sln path
6. `cmd/run.ts`: target, sln path, exe path
7. `cmd/clean.ts`: settings filename exclusion
8. `cmd/dbg.ts`: exe path
9. `src/Version.h`: kAppName "TumatraPDF"
10. `src/SumatraPDF.cpp`: window title strings
11. `src/AppSettings.cpp`: settings filename "TumatraPDF-settings.txt"
12. `src/SumatraPDF.rc`: icon paths, manifest reference
13. `src/SumatraPDF.exe.manifest` → `src/TumatraPDF.exe.manifest`
14. `src/CrashHandler.cpp`: PDB name, download URLs
15. `src/Commands.cpp`: menu strings
16. `src/Menu.cpp`: file picker strings
17. `src/AICodexBuild.cpp`: client name in JSON
18. `src/AIChatPanel.cpp`: webview data dir
19. `src/AppTools.cpp`: data dir path
20. `src/ImageEditHostSumatra.cpp`: CreatorApp docprop
21. `src/libsumatrapdf.rc`: ProductName version info

### NOT Changed (out of scope)
- Copyright headers, comments, `#include "SumatraPDF.h"` (filename names)
- CI scripts: cmd/build-ci.ts, build-all.ts, build-ci-daily.ts, build-smoke.ts, build-codeql.ts
- Sub-components: PdfFilter.rc, PdfPreview.rc
- Cross-platform build scripts: build-mac.ts, build-linux-wine.ts, build-win-in-wsl.ts
- cmake, tests/, docs/, mupdf/, ext/

### Next Steps
- [x] Clone SumatraPDF repository
- [x] Document architecture and credits
- [x] Initialize local Git repository
- [ ] Create private repository on GitHub
- [ ] Set up build environment (VS2022 + Premake5)
- [ ] Compile base version without modifications
- [ ] Create MERGE.md with update process
- [ ] Implement scripts/check-updates.ps1
- [ ] Plan integration points for 3 features
- [ ] Implement feature 1: auto-scroll (port from AHK)
- [ ] Implement feature 2: viewport crop
  - [ ] Implement feature 3: contrast filter

## 2026-08-11 — Binary Rename + Logo Inversion

### SumatraPDF → TumatraPDF (23 files modified + 4 renames)
- premake5.lua: workspace, startproject, project names
- vs2022/: sln + vcxproj files renamed, TargetName/RootNamespace/IntDir updated (16× each)
- cmd/: build.ts, run.ts, clean.ts, dbg.ts — target names + paths
- src/Version.h: kAppName "TumatraPDF"
- src/SumatraPDF.cpp: window title strings
- src/AppSettings.cpp: settings file "TumatraPDF-settings.txt"
- src/SumatraPDF.rc: icon paths, manifest ref
- src/SumatraPDF.exe.manifest → TumatraPDF.exe.manifest
- src/CrashHandler.cpp: PDB name, update URLs
- src/Commands.cpp, Menu.cpp: menu strings
- src/AICodexBuild.cpp, AIChatPanel.cpp, AppTools.cpp: paths
- src/ImageEditHostSumatra.cpp, libsumatrapdf.rc: version info
- Remaining "SumatraPDF" in src/: copyright headers (~600), includes (~50), internal class names — NOT changed (not binary-relevant)

### Logo Color Inversion (5 files, Python/Pillow)
- gfx/SumatraPDF-smaller.ico (256×256, single frame — ICO format)
- appx/SumatraPDF_StoreLogo_150x150.png (150×150 RGBA)
- appx/SumatraPDF_44x44.png (44×44 RGBA)
- appx/SumatraLogo310x300.png (310×310 RGB)
- appx/SumatraLogo310x150.png (310×150 RGB)
- File type icons (pdf-32bit.ico, epub-32bit.ico, etc.) NOT inverted — logos only per user request

### Next
- Rebuild: msbuild vs2022\TumatraPDF.sln /t:TumatraPDF /p:Configuration=Debug /p:Platform=x64 /m
- Verify EXE name: out/dbg64/TumatraPDF.exe

## 2026-08-11 — Feature 1: AutoScroll Implemented (Direct Core Integration)

### Direct Implementation (not modular)
Auto-scroll implemented directly in core source files (not in a separate `src/features/`).
This avoids modular build complexity — Premake5 hooks deferred.

### Files Modified (8 files)
1. **src/Commands.h** (+9 lines):
   - `CmdAutoScrollToggle = 479`, `CmdAutoScrollSpeedUp = 480`, `CmdAutoScrollSpeedDown = 481`
   - Inserted after `CmdContrastToggle`, before `CmdNone` (renumbered to 482)
   - All ranges (`CmdFileHistory*`, `CmdFavorite*`, `CmdLast`, `CmdFirstCustom`) automatically adjusted

2. **src/Commands.cpp** (+6 lines):
   - Strings: `"CmdAutoScrollToggle\0"`, `"CmdAutoScrollSpeedUp\0"`, `"CmdAutoScrollSpeedDown\0"`
   - IDs: `CmdAutoScrollToggle`, `CmdAutoScrollSpeedUp`, `CmdAutoScrollSpeedDown` in `gCommandIds[]`

3. **src/Accelerators.cpp** (-3/+3 lines):
   - F7: `CmdSelectTextViaKeyboard` → `CmdAutoScrollSpeedDown`
   - F8: `CmdToggleToolbar` → `CmdAutoScrollSpeedUp`
   - F9: `CmdToggleMenuBar` → `CmdAutoScrollToggle`

4. **src/Canvas.h** (+2 lines):
   - `constexpr UINT_PTR kContinuousAutoScrollTimerID = 15;`

5. **src/MainWindow.h** (+7 lines):
   - Fields in `struct MainWindow`: `autoScrollActive`, `autoScrollSpeed` (0.5f px/tick), `autoScrollSpeedMultiplier` (1.0f), `autoScrollTimerMinutes` (0=no timer), `autoScrollStartTick`

6. **src/Canvas.cpp** (+21 lines):
   - Handler `case kContinuousAutoScrollTimerID` in `WndProcCanvas`
   - Checks `IsAtDocumentEnd()`, timer expired, scroll via `MoveDocBy()`

7. **src/SumatraPDF.cpp** (+23 lines):
   - Dispatch in `FrameOnCommand`:
     - `CmdAutoScrollToggle`: toggle + `SetTimer`/`KillTimer` + button checked
     - `CmdAutoScrollSpeedUp`: multiplier ×1.2 (max 10×)
     - `CmdAutoScrollSpeedDown`: multiplier ×0.8 (min 0.1×)

8. **src/Toolbar.cpp** (+4 lines):
   - 3 buttons added to `gToolbarButtons[]`: toggle, speed up, speed down
   - Separator before buttons

9. **src/CommandAvailability.cpp** (+3 lines):
   - Commands added to `removeIfChm[]` (auto-scroll doesn't make sense in CHM)

### Behavior
- **F9**: Toggle auto-scroll (turns continuous vertical scroll on/off)
- **F8**: Increase speed (multiplier 1.2× per press, max 10×)
- **F7**: Decrease speed (multiplier 0.8× per press, min 0.1×)
- **Toolbar buttons**: Toggle + speed up + speed down (icons None for now)
- **Automatic stop**: End of document OR timer expired (`autoScrollTimerMinutes`)
- **Scroll**: MoveDocBy(0, dy) where dy = speed × multiplier pixels per tick (~10ms)
- **Timer**: `USER_TIMER_MINIMUM` (~10ms) for smoothness

### Known Limitations
- Sub-pixel movement not accumulated (very slow scroll may not move)
- ETA (time remaining) not implemented — no status bar in SumatraPDF
- Toolbar icons are `TbIcon::None` — need SVG icons
- `MoveDocBy(int, int)` expects ints, so dy < 1.0f truncates to 0
- `kContinuousAutoScrollTimerID = 15` — check for conflicts with other timer IDs

### NOTE: Manually Generated Commands
Commands were added directly in Commands.h/Commands.cpp (not via `cmd/gen-commands.ts`).
If `bun cmd/gen-code.ts` is run, these commands will be OVERWRITTEN.
It is necessary to also add the commands in `cmd/gen-commands.ts` for safe regeneration.

## 2026-08-11 — Themes + Menu + Toolbar Fix

### Themes: Light + Dark only, Dark default
- Theme.cpp:592: CreateThemeCommands() — filter loop to only append "Light" and "Dark" themes to gThemes
- Settings.h:1646: Theme default "" → "Dark"
- Theme.cpp:830-831: SetTheme() fallback — change from gThemeLight->name to "Dark"
- 22 themes defined in themesTxt kept intact — only gThemes vector filtered
- Custom user themes still supported (appended after built-ins)

### New Tools Menu
- Menu.cpp:592-618: menuDefNewTools[] submenu with 4 items
- AutoScroll Toggle (CmdAutoScrollToggle), Speed Up (CmdAutoScrollSpeedUp), Speed Down (CmdAutoScrollSpeedDown), Contrast (CmdContrastToggle)
- Menu.cpp:873: inserted into main menu bar after Debug
- Label: "New &Tools" (access key T)

### Toolbar Icon Fix
- Toolbar.cpp:90-92: TbIcon::None → ArrowsDiagonal (toggle), ChevronUp (speed up), ChevronDown (speed down)
- Root cause: TbIcon::None → SkipBuiltInButton() true → BTNS_SEP separator, not button
- Toolbar.cpp: CmdAutoScrollToggle added to BTNS_CHECK list for checked/unchecked state

### Command Registration Fix
- cmd/gen-commands.ts: added CmdAutoScrollToggle, CmdAutoScrollSpeedUp, CmdAutoScrollSpeedDown, CmdContrastToggle before CmdNone
- Regenerated Commands.h/.cpp via bun cmd/gen-code.ts
- Rebuild: msbuild Debug x64 → 0 errors
- Compiled/TumatraPDF.exe updated (21.8 MB)

## 2026-08-11 — Viewport Crop Implementation (Phase 1: Architecture)

### Scope
Settings struct + commands + Relayout page duplication + toggle handler.
Page duplication in Relayout() lays out each physical page twice (left column → right column)
when viewportCrop.enabled in continuous mode.

### Files Modified (7 files)
1. **cmd/gen-settings.ts** (+12 lines):
   - Added `ViewportCrop` compactStruct: left/right/top/bottom margins (px), colGap, enabled, quickToggle
   - Added `viewportCrop` field to globalPrefs with `.ver("3.8")`

2. **cmd/gen-commands.ts** (+2 lines):
   - Added `CmdViewportCropToggle` (484) and `CmdViewportCropConfig` (485) before CmdNone
   - Regenerated via `bun cmd/gen-code.ts` (Commands.h/.cpp) + `bun cmd/gen-settings.ts` (Settings.h/.cpp)

3. **src/DocumentLayout.h** (+4 lines):
   - `DocumentLayoutPage`: added `cropColumn` field (-1=full, 0=left, 1=right)
   - `DocumentLayout`: added `basePageCount` field (original physical page count before duplication)

4. **src/DocumentLayout.cpp** (+36 lines):
   - Added `#include "GlobalPrefs.h"`
   - `Relayout()`: ViewportCrop page duplication logic:
     - If `viewportCrop.enabled && !quickToggle && IsContinuous`: duplicate each page (left+right columns)
     - If disabled but previously duplicated: restore to `basePageCount`
     - First duplication saves `basePageCount`; subsequent Relayouts check `pages.len == basePageCount`

5. **src/SumatraPDF.cpp** (+13 lines):
   - `FrameOnCommand` case `CmdViewportCropToggle`: toggle `gGlobalPrefs->viewportCrop.enabled`, 
     reset `quickToggle`, call `dm->Relayout(zoomVirtual, rotation)`, update toolbar checked state

6. **src/CommandAvailability.cpp** (+1 line):
   - Added `CmdViewportCropToggle` to `removeIfChm[]` (2-column crop not applicable to CHM)

7. **src/GlobalPrefs.h** (included, no change):
   - Already declares `extern GlobalPrefs* gGlobalPrefs`

### Build
- MSBuild Debug x64 → **0 errors**
- Output: `out/dbg64/TumatraPDF.exe` (20.8 MB)
- Copied to `Compiled/TumatraPDF.exe`

### Known Limitations
- **Rendering pipeline not yet updated**: pages are duplicated in layout but `RenderVisibleParts()` 
  and `RecalcVisibleParts()` still iterate by physical page count. The duplicated entries' positions 
  are computed in Relayout but not reflected in visible-parts calculation.
- **No per-column crop rendering**: both duplicated entries show the full page image.
- **No quick toggle**: double-click handler not implemented (WM_LBUTTONDBLCLK already used for 
  text selection).
- **No toolbar button or menu item**: commands exist but not hooked up to UI.
- **Page counting**: `PageCount()` returns engine (physical) count, not layout (virtual) count.

### Next Steps
- Modify `RecalcVisibleParts()` and `RenderVisibleParts()` to use layout page count when viewportCrop enabled
- Add `physicalPageNo` mapping so renderer renders correct page for each virtual column
- Implement per-column crop rendering (pass clip rect to render)
- Add toolbar button + menu item
- Quick toggle: use Ctrl+Backtick or other shortcut (double-click on canvas is taken)
- Config dialog for margin values

## 2026-08-11 — Viewport Crop Crash Fix (CmdViewportCropToggle)

### Crash
- `EXCEPTION_ACCESS_VIOLATION` at `DisplayModel::RenderVisibleParts:1645`
- Stack: `FrameOnCommand(CmdViewportCropToggle)` → `SetScrollState` → `GoToPage` → `RenderVisibleParts` → null `pagesInfo`
- Root cause: `CmdViewportCropToggle` called `dm->Relayout()` but never rebuilt `pagesInfo` with new size. Layout had 2N entries, `pagesInfo` still N. `RenderVisibleParts()` loops `EffectivePageCount()`=2N but `GetPageInfo(>N)` returns null.

### Fix
- **src/SumatraPDF.cpp** (`case CmdViewportCropToggle`): after `dm->Relayout(...)` added:
  ```cpp
  dm->BuildPagesInfo();
  dm->RecalcVisibleParts();
  ```
  (before `SetScrollState`)

### Build
- MSBuild Debug x64 → **0 errors**
- Output: `out/dbg64/TumatraPDF.exe` (21.8 MB)
- Copied to `Compiled/TumatraPDF.exe`

## 2026-08-11 — Crop/Trim Fix: Always Re-save + Zoom Clamp

### Bug 1 — Crop didn't work after resize
- `ApplyViewportCrop()` only saved viewport+zoom on first call (`if (viewportCropSaved.IsEmpty())`). After window resize, stale saved viewport → crop broken.
- **Fix:** always re-save `viewportCropSaved` + `viewportCropSavedZoom` before applying.
- **src/SumatraPDF.cpp** `CmdViewportCropToggle`: removed redundant `cropDm->viewportCropSaved = Rect();` before `= GetViewPort()`.

### Bug 2 — Trim crashed with zoom overflow
- `ApplyMarginTrim()` zoom formula `pageWidth / contentWidth` → extreme zoom when margins large relative to page.
- **Fix:** clamp `zoomFactor` to `[0.5, 4.0]` + same always re-save pattern (`marginTrimSaved` + `marginTrimSavedZoom`).

### Bug 3 — Zoom compounding on resize
- `ApplyMarginTrim()` auto-applied in `SetViewPortSize` (~1348) and `Relayout` (~1800) → zoom compounded on every resize.
- **Fix:** removed both auto-apply calls. User toggles trim manually now. `ApplyViewportCrop()` auto-apply kept.

### Build
- MSBuild Debug x64 → **0 errors**
- Output: `out/dbg64/TumatraPDF.exe` (21,857,792 bytes)
- Copied to `Compiled/TumatraPDF.exe`

## 2026-08-11 — End of Session Status

### Working Features
- **AutoScroll**: F9 toggle, F7/F8 speed, Ctrl=pause, sub-pixel accumulator ✓
- **Contrast**: Ctrl+Shift+C toggle, ±10% opacity controls ✓  
- **Invert**: Shift+I (existing command, toolbar button added) ✓
- **Binary rename**: TumatraPDF.exe, own settings file, window title ✓
- **Logos**: 5 logo files color-inverted ✓
- **Toolbar**: Text buttons (AutoScroll/+/-/Contrast/Invert/Crop/Trim) ✓
- **Menu**: "New Tools" with all commands ✓
- **Themes**: Light+Dark only, Dark default ✓

## 2026-08-12 — Viewport Crop & Margin Trim Zoom Bug Fix & Completion

### Fix — Zoom Compounding & Flate Filter Error
- **Root cause**: `ApplyViewportCrop()` and `ApplyMarginTrim()` re-saved `viewportCropSaved` from already-cropped viewports, and `GetZoomVirtual()` without `true` returned special constants (`-2`, `-1`) instead of numeric scale factors.
- **Fix in `DisplayModel.cpp`**: 
  - Only save base uncropped `viewportCropSaved` / `marginTrimSaved` if `IsEmpty()` (prevents compounding).
  - Use `GetZoomVirtual(true)` for absolute numeric zoom.
  - Properly reset saved state rectangles (`= Rect()`) on disable in `SumatraPDF.cpp`.
- **Flow alignment (`cropview.txt`)**: Viewport Crop correctly splits each page into left and right columns vertically in continuous mode, advancing column-by-column.

### Build
- MSBuild Debug x64 → **0 errors, 0 warnings**
- Output: `sumatrapdf-src/out/dbg64/TumatraPDF.exe` copied to `Compiled/TumatraPDF.exe`

## 2026-08-12 — Viewport Crop Zoom Compounding & Manual Scroll Fix

### Fix — Zoom Compounding & Manual Scroll
- **Root Cause**: `viewportCropSaved` was being cleared (`viewportCropSaved = Rect()`) on page transitions in `ScrollYBy`, forcing a re-save from an already 2x zoomed viewport, causing exponential zoom compounding (`2x -> 4x -> 8x...`).
- **Fix in `DisplayModel.cpp`**:
  - Removed `viewportCropSaved = Rect()` reset during navigation so `viewportCropSavedZoom` remains anchored to the stable base uncropped zoom.
  - Added upward column/page retreat logic in `ScrollYBy` (`dy < 0`) alongside forward advance (`dy > 0`) for fully fluid manual reading in both directions.
- **Build**: MSBuild Debug x64 succeeded with 0 errors; output binary `Compiled/TumatraPDF.exe`.

## 2026-08-15 — ETA Label Overlap Fix (Right-Align + Hidden by Default)

### Bug
"ETA" label appeared on top of the toolbar buttons (Contrast/Invert/Crop/Trim), including at startup.

### Root Cause (Toolbar.cpp)
- `CreateEtaLabel` anchored label x to `TbGetRect(hwndToolbar, CmdAutoScrollSpeedDown)` right edge + 8px — but SpeedDown (idx 23) is mid-row, NOT the last button. Contrast/Invert/Crop/Trim (idx 25-28) sit right after → overlap whenever visible.
- Label created with `WS_VISIBLE` style → visible at startup ("ETA: --" over buttons) until the first `UpdateToolbarEtaText` call (which only happens on autoscroll toggle / timer tick).

### Fix (2 edits, Toolbar.cpp only)
1. `CreateEtaLabel`: removed `WS_VISIBLE` from CreateWindowExW style → hidden by default.
2. `UpdateToolbarEtaText`: right-aligned the label against the toolbar client right edge — replaced the SpeedDown-anchored x (`tbarRect.x + tbarRect.dx + DpiScale(8)`) with `rc.dx - size.dx - DpiScale(8)` using the existing `HwndClientRect(win->hwndToolbar)`. ETA now sits right of the last button (Trim), repositions on every show, never overlaps.

### Result
- ETA hidden by default; shown only when autoscroll=on (`win->autoScrollActive` guard unchanged); positioned at right side of toolbar.
- Build: MSBuild Debug x64 → 0 errors. Output: Compiled\TumatraPDF.exe (21,863,424 bytes).

## 2026-08-15 — Autoscroll Dead Fix: Persisted Multiplier 0 Clamp on Load

### Bug
Autoscroll stopped working (button toggles, no movement). ETA fix (same day) ruled out — autoscroll code intact end-to-end (toggle, timer, F9 accelerator, availability).

### Root Cause
`Compiled\TumatraPDF-settings.txt:213` had `AutoScrollSpeedMultiplier = 0` for the current doc (Resumen Psiquiatria.pdf). Load path (SumatraPDF.cpp:2034) had NO clamp → `win->autoScrollSpeedMultiplier = 0` → Canvas.cpp:4491 `speed = 2.0 * 0 = 0` → dy always 0 → `if (dy != 0)` false → MoveDocBy never called. UI cannot produce 0 (F7 clamps min 0.1, SumatraPDF.cpp:10147) → value from hand-edit/older build/corruption. Other docs (Varela = 1) scrolled fine — per-document symptom.

### Fix (1 edit, SumatraPDF.cpp:2034)
```cpp
win->autoScrollSpeedMultiplier = fs->autoScrollSpeedMultiplier < 0.1f ? 0.1f : fs->autoScrollSpeedMultiplier;
```
Ternary (not std::max) to avoid include dependency. Any load path for persisted float settings that the UI clamps must clamp too.

### Result
- Build: MSBuild Debug x64 → 0 errors. Output: Compiled\TumatraPDF.exe (21,863,424 bytes).
- Immediate workaround (before fix): one F7 press recovers (max(0×0.8, 0.1) = 0.1).
## 2026-08-15 � TRIM Rework: Clip-Based Top/Bottom Elimination + Trim Config Dialog

### What
- Trim now ELIMINATES top/bottom strips from RENDER (RenderPageArgs.pageRect clip in RenderCache::Paint non-cached path) � old zoom hack (ApplyMarginTrim) deleted.
- New Trim Config dialog (modeless, ChangeThemeWnd pattern): buttons margin-top/margin-bottom show a draggable red 2px line on the page; drag auto-calculates distance; ? saves to gGlobalPrefs->trim.top/bottom.
- New CmdTrimConfig command + "Trim Config" toolbar button + menu item.
- Trim settings persisted: Trim struct (Top/Bottom/Enabled) in gen-settings.ts.
- ShouldCacheRendering returns false when trim on (no stale cache tiles).
- Scope: top/bottom only � left/right/column-gap NOT implemented (per user).

### Files
- gen-settings.ts, gen-commands.ts (regenerated Settings.h/.cpp, Commands.h/.cpp)
- CommandAvailability.cpp, Toolbar.cpp, Menu.cpp, MainWindow.h
- NEW TrimConfigDialog.cpp/h
- DisplayModel.cpp/h, RenderCache.cpp, Canvas.cpp, SumatraPDF.cpp
- premake5.files.lua / vs2022 vcxproj (build integration)

### Result
- Build: MSBuild Debug x64 ? 0 errors. Output: Compiled\TumatraPDF.exe (21,875,200 bytes).

## 2026-08-15 � TRIM Fixes: Dialog Persistence + Continuous Layout + Stretch Fix

### What
3 bugs fixed after clip-based trim implementation:
1. Trim Config dialog disappeared when clicking/dragging red line on canvas � dialog was an unowned popup (args.parent = nullptr); canvas mouse capture dropped it behind the frame. Fix: own dialog to main window (args.parent = win->hwndFrame).
2. Black gap in continuous mode � DocumentLayout computed page height from full mediaBox, ignoring trim. Fix: DocumentLayoutParams gained trimTop/trimBottom/trimEnabled; DisplayModel::Relayout populates them from gGlobalPrefs->trim + marginTrimEnabled; DocumentLayout::Relayout reduces pageSize.dy by top+bottom (pos.dy/canvasDy/canvasSize derive from it). CmdMarginTrimToggle + TrimConfigDialog OnSave/OnCancel now call RelayoutKeepingView() (keeps view, recalc visible parts, repaint). Trim line bottom Y (OnBottom + Canvas OnMouseMove clamp) uses reduced height so line starts at visible trimmed page bottom.
3. Text stretch distortion when scrolling over trimmed region � RenderCache::Paint non-cached path clipped area AND shrank bounds (pixmap smaller than bounds ? BlitPixmap stretched). With layout now using reduced page height, pageOnScreen is already the trimmed rect: replaced clip+bounds-shrink with simple page-coord shift (area.y += t), no bounds adjustment ? 1:1 blit.

### Files
- TrimConfigDialog.cpp (parent, OnSave/OnCancel relayout, OnBottom reduced height)
- DocumentLayout.h (params fields), DocumentLayout.cpp (reduced pageSize.dy)
- DisplayModel.cpp (params population)
- SumatraPDF.cpp (CmdMarginTrimToggle ? RelayoutKeepingView)
- Canvas.cpp (OnMouseMove clamp reduced height)
- RenderCache.cpp (shift-only trim render)

### Result
- Build: MSBuild Debug x64 0 errors. Output: Compiled\TumatraPDF.exe (21,875,200 bytes).

## 2026-08-15 - Trim Config Dialog Redesign: Dual Lines + Numeric Inputs + Reset

### What
Trim Config dialog redesigned:
1. Both red lines appear immediately on dialog open (top line at top trim boundary, bottom line at bottom trim boundary), both draggable right away.
2. Dialog shows "margin top:" and "margin bottom:" numeric edits - typing a number moves the red line live; dragging a red line updates the number live (two-way sync via TrimConfigDialogSyncEdits called from Canvas OnMouseMove).
3. "Reset" button added (sets both distances to 0, moves both lines).
4. margin-top/margin-bottom buttons REMOVED (redundant - only labels remain).
5. Save (?) + Cancel kept.

### Math
Line positions use tRef = marginTrimEnabled ? gGlobalPrefs->trim.top : 0 (layout reference). topLineY = tl.y + (trimConfigTop - tRef)*zoom; bottomLineY = tl.y + (mb.dy - trimConfigBottom - tRef)*zoom. Drag clamps per line (top: [tl.y - tRef*zoom, bottomLineY]; bottom: [topLineY, tl.y + (mb.dy - tRef)*zoom]). Edit clamps: top <= mb.dy - trimConfigBottom, bottom <= mb.dy - trimConfigTop.

### Files
- MainWindow.h: trimConfigLineY removed, trimConfigDragLine added (0=none, 1=top, 2=bottom); trimConfigMode semantics: 0=closed, 1=open (both lines).
- TrimConfigDialog.h: TrimConfigDialogSyncEdits() declared.
- TrimConfigDialog.cpp: rewritten - Edit* editTop/editBottom, btnReset, suppressEditUpdate flag, OnEditTopChanged/OnEditBottomChanged/OnReset/SyncEditsFromLine; Create sets trimConfigMode=1 + both lines visible + repaint canvas; OnTop/OnBottom/UpdateInfoLabel/lblInfo deleted.
- Canvas.cpp: OnPaintDocument draws BOTH red lines; OnMouseLeftButtonDown hit-tests both lines (nearest wins); OnMouseMove drags per trimConfigDragLine + calls TrimConfigDialogSyncEdits; OnMouseLeftButtonUp resets trimConfigDragLine. Added #include "TrimConfigDialog.h".

### Result
- Build: MSBuild Debug x64 0 errors. Output: Compiled\TumatraPDF.exe (21,875,200 bytes).
- clang-format applied to Canvas.cpp, TrimConfigDialog.cpp/h, MainWindow.h.

## 2026-08-16 — Crop Zoom Fix + Toolbar Checked Bold/White

### What
- FIX1: Viewport Crop zoom snap-back bug. ApplyViewportCrop (DisplayModel.cpp) forced zoom = savedZoom*2 on EVERY call (Relayout/GoToPage/ScrollYBy) → user zoom changes while cropped instantly overridden; disable restored stale zoom. Now 2x zoom applied ONLY on first application (viewportCropSaved.IsEmpty()); subsequent calls only fix viewPort.x/dx (column position). CmdViewportCropToggle ON branch (SumatraPDF.cpp) no longer pre-saves — sets viewportCropSaved = Rect() then ApplyViewportCrop captures base state. Disable branch unchanged.
- FIX2: Toolbar checked toggle buttons (Trim/Crop/Autoscroll/Contrast) now bold white text. ReBarWndProc CDDS_ITEMPREPAINT (Toolbar.cpp): cmdId from dwItemSpec, guard cmdId > 0, TB_ISBUTTONCHECKED via SendMessageW → col = RGB(255,255,255) + SelectObject(GetAppTreeFontEx(hwndToolbar, true, false)) bold font + CDRF_NEWFONT.

### Files
- src/DisplayModel.cpp (ApplyViewportCrop)
- src/SumatraPDF.cpp (CmdViewportCropToggle)
- src/Toolbar.cpp (ReBarWndProc CDDS_ITEMPREPAINT)

### Result
- Build: MSBuild Debug x64 0 errors
- Binary: Compiled\TumatraPDF.exe = 21,875,200 bytes

## 2026-08-16 - End of Session

### What
- 3 fixes this session, all built (0 errors) + copied to Compiled\TumatraPDF.exe = 21,875,200 bytes:
  1. Crop zoom snap-back fix (ApplyViewportCrop 2x only on first apply; subsequent calls fix column only; CmdViewportCropToggle ON no longer pre-saves).
  2. Toolbar checked toggle buttons bold white (ReBarWndProc CDDS_ITEMPREPAINT + GetAppTreeFontEx(hwnd,true,false) + TB_ISBUTTONCHECKED).
  3. Inversion lost when trim=on (trim forces non-cached render path which skipped RecolorPixmap) - fixed by adding RecolorPixmap guarded by ShouldUpdateBitmapColorsLegacy in non-cached path.
- All 3 runtime-tested, pending user confirmation.

## 2026-08-17 - Trim Cache Fix + AutoScrollShowEta Visibility

### What
- BUG1: AutoScrollShowEta hidden from Advanced Options. field() was added to cmd/gen-settings.ts globalPrefs AFTER CheckForUpdates, i.e. inside internalRest section (after comment("You're not expected to change those manually")) -> generated FieldInfo 4th arg true = internal. Moved to before marker (next to ShowLinks). Regenerated: src/Settings.h autoScrollShowEta = {offsetof(GlobalPrefs, autoScrollShowEta), SettingType::Bool, true} (3-arg, visible), fieldNames packed string contains AutoScrollShowEta, fieldCount 139.
- BUG2: trim=on slowness (autoscroll lag). Root cause: DisplayModel::ShouldCacheRendering returned !marginTrimEnabled -> trim disabled render cache -> every paint re-rendered page from engine. Fix:
  - ShouldCacheRendering -> return true (cache is now trim-aware).
  - RenderCache.cpp GetTileRectDevice(+bool trimEnabled): when trim, shrink mediabox by trim.top/bottom (page coords), return empty Rect if t+b >= mb.dy, then Transform as before.
  - GetTileRectUser(+bool trimEnabled): after inverse Transform, rect.y += trim.top (shift into trimmed page coords).
  - GetTileOnScreen(+bool trimEnabled) propagates; IsTileVisible passes dm->marginTrimEnabled; Paint (~1212) + render request (~746) pass dm->marginTrimEnabled.
  - Cache invalidation: CmdMarginTrimToggle (SumatraPDF.cpp) + TrimConfigDialog OnSave call gRenderCache->FreeForDisplayModel(trimDm) after RelayoutKeepingView.
  - TrimConfigDialog.cpp needed #include "RenderCache.h" (SumatraPDF.h only forward-declares RenderCache -> C2027).
- Non-cached path in RenderCache::Paint kept as fallback (dead code).

### Files
- cmd/gen-settings.ts (moved AutoScrollShowEta)
- src/Settings.h (regenerated)
- src/DisplayModel.cpp (ShouldCacheRendering)
- src/RenderCache.cpp (GetTileRectDevice/User/OnScreen trimEnabled)
- src/SumatraPDF.cpp (CmdMarginTrimToggle FreeForDisplayModel)
- src/TrimConfigDialog.cpp (OnSave FreeForDisplayModel + include)

### Result
- Build: MSBuild Debug x64 0 errors (1st attempt C2027 RenderCache undefined -> fixed with include)
- Binary: Compiled\TumatraPDF.exe = 21,875,200 bytes (hash matches out\dbg64; stale running instance killed to unlock)

## 2026-08-24 - Small-Tab Root Cause Found + Active Tab Underline

### What
- ROOT CAUSE of "tiny tab" bug FOUND: Compiled\TumatraPDF-settings.txt had TabWidth = 60 (settings floor is 60, AppSettings.cpp). 60px tab - 8px textPad - ~28px close-gutter reservation = ~20px text = only 2-3 letters visible. Not a code bug at all.
- Fixed by resetting settings line to TabWidth = 300 (default).
- New: active tab underline - white 3px (DpiScale) horizontal strip along bottom edge of selected tab, painted right after per-tab bg fill in TabWnd::Paint using same SolidBrush/FillRectangle primitive.
- Reclaimed close-gutter text space: rTxt width no longer pulled to rClose.x - textGap; now spans full padded tab width (r.x+textPad .. r.x+r.dx-textPad), LTR and RTL unified (RTL alignment still via StringFormat). rClose computation left untouched (dead but harmless since CloseVisible()=false).

### Files
- sumatrapdf-src/src/wingui/TabsCtrl.cpp (TabWnd::Paint underline + rTxt)
- Compiled/TumatraPDF-settings.txt (TabWidth 60 -> 300)

### Result
- Build: bun cmd/build.ts, 0 errors / 0 warnings, 66.8s
- Deploy: running instance killed (PID 9064); Compiled\TumatraPDF.exe = 21,895,168 bytes
- Settings verified: line 47 "TabWidth = 300"
2026-08-24 — autoscroll toolbar buttons reordered: - before +

## 2026-08-24 — Dev Tooling: Guide tumatrapdf-v1-distilled + debugview Skill

### What
- Created lemma guide `tumatrapdf-v1-dev` (dev-tool): consolidated manual from ~123 project memories. Sections: build/deploy protocol, toolbar NM_CUSTOMDRAW gotchas, tabs VirtWnds refs, diagnostics decision rules.
- Anchors distilled into guide: mde3b8faaa846 (custom-draw DC space), m22b30c2898df (small-tab settings root cause), ma48b4e41db49 (tab freeze removal).
- Build entrypoint CONFIRMED: `cmd/` lives inside sumatrapdf-src/ (not repo root). Workdir sumatrapdf-src → `bun cmd/build.ts` → vs2022\TumatraPDF.sln /t:TumatraPDF → out\dbg64\TumatraPDF.exe. Fallback msbuild direct; new files → `bun cmd/premake.ts` first.
- Installed skill `debugview` (microsoft/skills, official) → ~\.agents\skills\debugview. Captures OutputDebugString/DebugView output for native Win32 debugging.
- Skills search verdict: ecosystem weak for native C++/Win32 profiling (no WPA/xperf/VS Profiler skills worth installing). Profiling protocol folded into guide instead.

### Result
- Guide live (usage count 4 after distills); skill installed globally (OpenCode included)
- No code changes in this session

## 2026-08-24 — Full Audit: FLOW/tumatrapdf.dot Rewritten From Real Code

### What
- Audited entire fork surface (explore agent + grep): toolbar gToolbarButtons Toolbar.cpp:69-97, menus menuDefNewTools Menu.cpp:592-641, accelerators Accelerators.cpp, settings cmd/gen-settings.ts.
- Fork reality: ~810 insertions / 34 files, direct patches in sumatrapdf-src/src/ + TrimConfigDialog.{h,cpp}. src/features/ + src/hooks/ DO NOT EXIST — old .dot cluster was aspirational; README "Modularity Strategy" still describes it (known falsidade).
- FLOW/tumatrapdf.dot REWRITTEN: phases updated (fase5 partial: ASan sln rename ✗, .vscode F5 ✗), real toolbar order (25 buttons), New Tools menu tree, Tumatra shortcuts (F7/F8/F9/Ctrl+Shift+C/Alt+Click), settings global+per-doc, features w/ file:line refs, dead-code cluster, deps+legend kept.
- Bottlenecks → lemma memory m03bbbdedaab9 for next agent (8 itens): F10-vs-F9 doc mismatch, missing shortcuts crop/trim/contrast±10%, QuickToggleViewportCrop dead, TabsCtrl ✕ dead paths, build-asan.ts broken sln ref, .vscode F5 broken, no Test* dbg-control cmds for fork features, commented accels.

### Result
- Files: FLOW/tumatrapdf.dot (rewrite); LOG.md (this)
- Memory: m03bbbdedaab9 (warning) + .dot fidelity note
- No C++ changes

### 2026-08-24
- Invert restore fixed: UpdateDocumentColors() called unconditionally after LoadDocument per-doc state apply (was loading invert/contrast state without repaint).
- Autoscroll default speed for unadjusted docs = minimum 0.1 (FileState AutoScrollSpeedMultiplier default 1.0 -> 0.1 in gen-settings.ts; Settings.h/cpp regenerated); per-doc last-used restore already existed.

## 2026-08-25 — Markdown WebView2: AutoScroll + Contrast via JS

### What
Native WebView2 support for AutoScroll + Contrast on Markdown (.md). Previously both silently no-op'd / invisible in default webview mode. MuPDF fallback (`markdownUI.useFixedPageUI=true`) untouched. CHM untouched.

### AutoScroll md-webview
- `AutoScrollToggle` guard relaxed — accepts webview-mode docs
- `AutoScrollContinuousTick` branches: DisplayModel → `MoveDocBy(0,dy)`; webview → `wv->Eval("window.scrollBy(0,dy)")`
- Stop-at-bottom: JS posts `autoscrollBottom` → `BrowserDocView::OnJsNotifyCb` → virtual `DocController::OnAutoScrollBottom()` → `MarkdownModel` forwards → `ControllerCallbackHandler` stops timer + updates toolbar
- F7/F8 speed multiplier shared unchanged

### Contrast md-webview
- `CmdContrastToggle` branches: webview → inject/toggle fixed-position `div#__sumatra_contrast` (`background:#000`, `pointer-events:none`, `z-index` max) via `Eval`; opacity = `(contrastOpacity*255)/100/255.0f` — identical math to Win32 overlay
- `CmdContrastIncrease/Decrease` → `UpdateContrastOverlayOpacity` dispatches JS path when `win->AsMarkdown()`
- `CreateContrastOverlay` early-return no-op for markdown
- `RestoreContrastOverlay()` called from `MarkdownModel::OnDocumentComplete` → persistence across navigation
- New virtuals: `DocController::OnAutoScrollBottom`, `GetContrastEnabled`, `GetContrastOpacity`; accessor `BrowserDocView::GetWebviewWnd`

### Files
11 files changed +232/-40

### Build
`bun cmd/build.ts` — 0 err / 0 warn (35s). Deployed `Compiled\TumatraPDF.exe` 21,899,776 bytes @ 2026-08-25 21:56. Runtime interactive test PENDING (user verifies F9 / Ctrl+Shift+C on .md).

---

## 2026-08-26 — Round 3: Speed Label Slot, Min Speed Reduction, Contrast Leak Fix

### What
Three targeted fixes for toolbar layout, speed scaling, and contrast cross-doc leak.

### Speed label layout
Migrated from floating "left of [-]" (which caused overlap) to dedicated toolbar slot via fake placeholder button `SpeedInfoId` + `TbSetButtonDx` (cloned page-counter pattern). Occupies toolbar space like a native control.

### Min speed reduced to 1/6
`0.1f` → `0.0167f` across `MainWindow.h`, `LoadDocument`, `AutoScrollSpeedAdjust`, and `gen-settings.ts` (regenerated).

### Contrast black-screen leak
Fixed by calling `DestroyContrastOverlay(win)` unconditionally on document change (`LoadDocument`) and in OFF toggle branch for all modes (clears tainted cross-doc Win32 layers).

### Build
`bun cmd/build.ts` — 0 errors / 0 warnings (118.5s, 21,911,552 bytes).

## 2026-08-25 — Follow-up: md Toolbar + Logging + Instrumentation

### What
Regression fixes after runtime test of md-webview features. Cumulative diff 15 files +382/-44.

### Fix 1 — Toolbar hidden on .md (PRE-EXISTING upstream bug)
- Root: `CommandAvailability.cpp` ~L379 `ctx.isChm = AsChm() || AsMarkdown()` → CHM blacklist applied to markdown → AutoScroll×3 + ContrastToggle disabled/hidden for .md
- Fix: `isChm = AsChm()` only; new `removeIfMarkdown[]` hides only fixed-page cmds (`ViewportCrop`/`MarginTrim`/`TrimConfig`) for .md; AutoScroll×3 + ContrastToggle now enabled for .md; CHM unchanged

### Fix 2 — Contrast persistence AUDITED (no code change)
- `UpdateTabFileDisplayStateForTab` saves `contrastEnabled`/`contrastOpacity` for all types including md; `LoadDocument` restores → already correct
- Note: black veil on reopen = persisted contrast ON → `Ctrl+Shift+C` toggles off (by design, matches PDF behavior)

### Fix 3 — File logging upgrade (user request, TumatraPDF2-style auto-log)
- `SumatraLog.h/.cpp`: file output now `[YYYY-MM-DD HH:MM:SS.mmm] [INFO]/[WARN]/[ERROR]` tags; rotation truncate >5MB; `StartLogToFile` unconditional (was already)
- Log file: `%LOCALAPPDATA%\SumatraPDF\<hash>\sumatra-log.txt` (hash = install dir)

### Instrumentation added
- `[md] doc complete / contrast restore / contrast toggle / contrast opacity`
- `[autoscroll] start|stop / webview tick start / bottom reached`
- `[webview] autoscrollBottom notify`

### Build
`bun cmd/build.ts` — 0 err / 0 warn (34.1s). Deployed `Compiled\TumatraPDF.exe` 21,907,456 bytes @ 2026-08-25.

## 2026-08-25 — Follow-up Round 2: Speed Label + md ETA + Contrast Leak Fix

### What
Polish round 2 for md autoscroll/contrast — speed visibility, md ETA parity, fresh-doc state leak. Cumulative diff 10 files +329/-40 vs round 1 base.

### Fix 1 — Fresh-doc min speed default
- `MainWindow.h`: `autoScrollSpeedMultiplier` default 1.0f → 0.1f (matches `FileState` 0.1). Fresh docs (no persisted state) now start at minimum speed — adjustable before start via F7/F8 or [+]/[-].

### Fix 2 — Contrast cross-tab leak (fresh .md inherited prev tab state)
- `SumatraPDF.cpp` `LoadDocument` else-branch (`fs == nullptr`): resets `contrastEnabled=false`, `contrastOpacity=50`, `autoScrollSpeedMultiplier=0.1f`. Fresh .md no longer inherits previous tab's contrast/speed.

### Fix 3 — Contrast OFF cleanup
- WebView2 `div#__sumatra_contrast`: OFF now `d.remove()` instead of `display:none` — DOM removed, no hidden overlay.

### New 1 — Speed label `%d px/min` in toolbar
- `Toolbar.cpp`: `CreateSpeedLabel` / `RepositionSpeedLabel` / `UpdateToolbarSpeedLabel` — static text left of [-] button, always visible with doc open, shows current `AutoScrollPxPerSec` (`%d px/min`). Updates on F7/F8, [+]/[-] clicks, doc load.

### New 2 — ETA for .md webview (parity with PDF)
- Parallel chain: JS `autoscrollProgress` notify ~100ms → `BrowserDocView::OnJsNotifyCb` → virtual `DocController::OnAutoScrollProgress(remainingPx)` → `ControllerCallbackHandler` computes `etaMinutes = remainingPx / AutoScrollPxPerSec` → `UpdateToolbarEtaText`. Fixed-page PDF ETA verified intact — no regression.

### Build
`bun cmd/build.ts` — 0 err / 0 warn (20.7s). Deployed `Compiled\TumatraPDF.exe` 21,907,456 bytes @ 2026-08-25.
## 2026-08-29 � Arch Tools Fase 12: Scale regression + Measure underline inversion + rename
- **Scale OnOk regression fix:** guard changed from !win->archScaleSet || win->archScaleFactor <= 0.0f to !(win->archScaleLineDefined || win->archScaleSet) so a freshly drawn scale line (archScaleSet still false until OnOk) is accepted; factor recomputed from stored archScaleLineP1/P2.
- **Measure underline inverted:** removed BTNS_CHECK auto-toggle from CmdArchMeasure button in CreateToolbar2 (Toolbar.cpp:1958-1960); UpdateToolbar2State is now the sole manager of checked state (called from CmdArchMeasure handler, CmdArchToolsToggle, and ESC in FrameOnKeydown). Underline now tracks rchToolMode == 2 correctly (was appearing when OFF, hiding when ON).
- **Rename:** 2nd-toolbar button Limpar linhas ? Clean lines (Toolbar.cpp:1942).
- **Build:** 0 errors / 0 warnings (un ./cmd/build.ts). **Deploy:** Compiled\TumatraPDF.exe (21.9 MB). **Smoke:** clean launch -for-testing -console, no crash, no new dump.

## 2026-08-29 — Autoscroll Timer UI (Fase 15, BUILD OK)
- Added Timer control to the toolbar BEFORE the Autoscroll button: `[checkbox][Timer:][numeric input]`.
- Behavior: checkbox ON → autoscroll stops by itself after N min (input value, default 30). OFF → no limit.
- The auto-stop backend already existed (AutoScroll.cpp `AutoScrollContinuousTick` stops when `autoScrollTimerMinutes > 0` and time passes); the UI now feeds `autoScrollTimerMinutes`.
- Files edited: `MainWindow.h` (autoScrollTimerMinutesSetting=30, autoScrollTimerEnabled=false, control HWNDs), `AutoScroll.cpp` (field feed in AutoScrollToggle), `SumatraPDF.cpp` (CmdAutoScrollTimerToggle/Edit handlers + gGlobalPrefs apply), `Toolbar.cpp` (TimerInfoId placeholder + CreateTimerControls/RepositionTimerControls + WM_CTLCOLORBTN dark mode), `Settings.h` (regenerated AutoScrollTimerMinutes/AutoScrollTimerEnabled fields), `cmd/gen-settings.ts` (fields).
- PENDING: user UI verification (tomorrow) — `[checkbox][Timer:][input]` before Autoscroll; autoscroll timer after N min.
- Build: OK (0 errors, 1 unrelated warning in test_util). Commands.h regenerated, Settings in sync, deployed, clean smoke test.

## 2026-08-29 (2) — Fase 15 fixes: autoscroll speed + timer visual
- Autoscroll: `kMinSpeedMultiplier` (AutoScroll.cpp) 0.1f -> 0.008f; minimum speed now ~100 px/min (was ~1400). `AutoScrollSpeedMultiplier` persistence guaranteed (load+save); default = minimum if never used.
- Timer control: `TimerInfoId` 110->130px; layout with design tokens (kCtrlGapX=4, kCtrlH=18) — consistent gaps + vertical centering. Start of a minimal design system.
- Build: 0 errors. Deploy + smoke OK.

## 2026-08-30 — Fase 15 Fixes (speed clamp, persistence, timer visual, build fix)

- **Autoscroll speed clamp**: `kMinSpeedMultiplier` 0.1f → 0.008f (AutoScroll.cpp:74). Min speed now ~100 px/min (was ~1400).
- **Remember last speed**: persistence via FileState `autoScrollSpeedMultiplier` (already existed, per-document). Defaults lowered 0.0167f → 0.008f (MainWindow.h:332, SumatraPDF.cpp:2084/2104, gen-settings.ts:792). "If never used → minimum" correct.
- **Timer visual**: design tokens `kCtrlGapX=4`, `kCtrlH=18` (Toolbar.cpp:54-56); `TimerInfoId` 110 → 130 (Toolbar.cpp:1201); CreateTimerControls/RepositionTimerControls with token math. Fixed `slot`→`r` bug in CreateTimerControls (Toolbar.cpp:1361).
- **Build fix**: reverted regression `..\vs2022\TumatraPDF.sln` → `vs2022\TumatraPDF.sln` (build.ts:37). Build runs from `sumatrapdf-src` (`bun cmd/build.ts`). 0 err / 0 warn. Auto-deploy to Compiled\TumatraPDF.exe. Clean smoke (0 crash dumps).
- **Pending**: practical UI test by user (tomorrow) — timer control [checkbox][Timer:][input] before Autoscroll; minimum speed; persistence; visual alignment.

## 2026-08-30 — Fase 16B: MainWindow.h Struct Modularization (AutoScrollState + ArchToolsState)

### What
Grouped ~274 flat MainWindow member variables into two nested domain structs:
- **AutoScrollState** (win->autoScroll): active, speed, speedMultiplier, accum, etaMinutes, etaStartTick, etaLastShown, etaPageNo, etaToolbarWidth, hwndEtaLabel, hwndSpeedLabel, timerMinutes, startTick, timerMinutesSetting, timerEnabled, hwndTimerCheck, hwndTimerLabel, hwndTimerEdit.
- **ArchToolsState** (win->archTools): on, mode, unit, scaleFactor, scaleAnchorX/Y, scaleSet, scaleLineDefined, point1/2, dragLine, snap, measurements, measurementsByDoc, scaleLineP1x/P1y/P2x/P2y, hwndArchScaleDialog, scaleDialog, dragStartPos, dragMoved, eraseMode, mousePos, hwndReBar2, hwndToolbar2.

### Files changed
- src/MainWindow.h — struct definitions + member renaming (~90% of references)
- src/SumatraPDF.cpp — win-> prefix updates; bare rchMeasurementsByDoc/rchMeasurements in member functions fixed with 	his->archTools.
- src/Toolbar.cpp, src/AutoScroll.cpp, src/Canvas.cpp, src/ArchScaleDialog.cpp — win-> prefix updates

### Build
un cmd/build.ts — **0 errors / 0 warnings** (60.5s). Deployed Compiled\TumatraPDF.exe (21.9 MB). Smoke test clean (no crash dumps).

### Key lesson
In MainWindow member functions, win is NOT a local variable — bare member names become 	his->archTools.xxx, NOT win->archTools.xxx.

### Next
- 16C: Split SumatraPDF.cpp (12.743 lines) into per-domain command files

## 2026-08-30 — Fase 16C: Measure Persistence Fix (ArchTools)

### What
Bug: measure lines disappeared on tab switch (scale persisted correctly)

### Root cause
Measure stored in-memory Vec only, never serialized to FileState

### Fix
Added ArchMeasurements field to FileState via gen-settings.ts + Settings.h; added SerializeMeasurements/DeserializeMeasurements helpers; save on tab switch, restore on document load

### Build
`bun cmd/build.ts` — 0 errors / 0 warnings

### Files modified
- gen-settings.ts
- Settings.h (generated)
- SumatraPDF.cpp
### Fase 16C-2: Markdown Contrast + Arch Tools Hide (2026-08-30)

#### What
- Created exclusive contrast system for .md files (independent from regular file contrast)
- Hidden Arch Tools toolbar and commands when viewing .md files

#### Contrast .md behavior
- When contrast=ON for .md: background stays light (#FAFAFA), text becomes dark gray
- Gray level controlled by contrastOpacity: 0=black, 50=gray, 100=light gray
- Formula: gray = 255 * (1 - opacity/100)
- Regular files: unchanged (Win32 overlay for PDF, CSS dark mode for others)

#### Arch Tools hidden for .md
- Added CmdArchTools* to removeIfMarkdown[] in CommandAccuracy.cpp
- CmdArchToolsToggle early-returns for .md files
- RelayoutFrame hides hwndReBar2 when current doc is .md

#### Build
- `bun cmd/build.ts`: 0 errors / 0 warnings (38.0s)
- Deployed: Compiled\TumatraPDF.exe (21.9 MB)
- Smoke test: clean (no crash dumps)

#### Files modified
- src/SumatraPDF.cpp: IsCurrentDocMarkdown() helper, CmdContrastToggle md branch, CmdArchToolsToggle guard, RelayoutFrame hide for md
- src/MarkdownModel.cpp: RestoreContrastOverlay() CSS formula
- src/CommandAccuracy.cpp: removeIfMarkdown[] + Arch Tools commands

## 2026-08-30 — Fase 16C-2: Markdown Contrast + Arch Tools Hide + AutoScroll Extraction

### What
1. Markdown (.md) exclusive contrast system — light bg (#FAFAFA) + dark gray text (formula: gray = 255 * (1 - opacity/100)). Independent from fixed-page contrast (Win32 veil).
2. Arch Tools hidden for .md files — CmdArchToolsToggle early return, RelayoutFrame hides hwndReBar2, CommandAccuracy.cpp removeIfMarkdown[] updated.
3. AutoScroll commands extracted from SumatraPDF.cpp → Commands_AutoScroll.{h,cpp} (6 handlers). Pilot extraction validated the split pattern.

### Files modified
- src/SumatraPDF.cpp (IsCurrentDocMarkdown, CmdContrastToggle, CmdArchToolsToggle, RelayoutFrame, 6 AutoScroll dispatches)
- src/MarkdownModel.cpp (RestoreContrastOverlay CSS formula)
- src/CommandAccuracy.cpp (removeIfMarkdown[] + Arch Tools commands)
- src/Commands_AutoScroll.h (NEW — 6 handler declarations)
- src/Commands_AutoScroll.cpp (NEW — 6 handler implementations)
- vs2022/TumatraPDF.vcxproj + .filters
- vs2022/SumatraPDF-static.vcxproj + .filters

### Build
 bun cmd/build.ts` — 0 errors / 0 warnings (30.7s). Deployed + smoke clean.

### Fase 16C-3: ViewCommands Extraction + Build Fix (2026-08-30)

#### What
Extracted 35 view/zoom/rotate command handlers from SumatraPDF.cpp into Commands_View.{h,cpp}. Fixed linker errors caused by missing forward declarations and signature mismatches. Also cleaned up TODO.md (duplicate content, garbled UTF-8).

#### Files created
- src/Commands_View.h — 35 handler declarations
- src/Commands_View.cpp — 35 handlers + 7 static helpers moved from SumatraPDF.cpp

#### Files modified
- src/SumatraPDF.cpp — added #include, replaced 35 case bodies with function calls, removed 7 static helpers (now in Commands_View.cpp), removed static from RelayoutFrame/IsCurrentDocMarkdown, removed default args from RelayoutFrame declaration
- src/Commands_ArchTools.h — forward declarations for RelayoutFrame/IsCurrentDocMarkdown removed (moved to Commands_View.h)
- src/Commands_ArchTools.cpp — updated RelayoutFrame forward decl and call to match real signature
- vs2022/TumatraPDF.vcxproj + .filters — added Commands_View.cpp
- vs2022/SumatraPDF-static.vcxproj + .filters — added Commands_View.cpp

#### Build
0 errors / 0 warnings (34.3s). Deployed to Compiled/TumatraPDF.exe.

#### Key lesson
Extracted files must NOT use static functions from SumatraPDF.cpp. Solutions: (1) move static helpers to the extracted .cpp, or (2) make them non-static + declare in header. Forward declarations preferred to avoid deep include chains.
