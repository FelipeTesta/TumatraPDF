# TODO

1. Open work ONLY.
2. Completed features/fixes live in [LOG.md](LOG.md) (per-session) + [CHANGELOG.md](CHANGELOG.md) (per-release).
3. User manual-test items = items only the user can validate.
4. Housekeeping (2026-10-20, s36-ae; re-triaged s36-at, s36-cu/cv): all `[x]` blocks archived — details live in LOG per session entry; this file lists open work only.

## USER validation queue

Items ONLY the user can verify live (real book / real build). Flat list; details in LOG per session.

- [ ] Shift+S capture tool (s39): rect capture at doc DPI → clipboard on a real book; auto-exit, ESC cancel, doc-close reset.
- [ ] Capture persistence: capture lifetime/behavior on a real book (no save-to-file yet — see Capture residual).
- [ ] Repair Document File (s42): right-click flagged PDF → Repair → badge gone, same filename, no `.repair-tmp` leftovers; reopen → still opens fine, warnings absent (LOG s42 steps).
- [ ] Repair flow verify: after repair, reopen file and confirm warnings stay absent; report which warnings (if any) survive via Show Errors.
- [ ] TOC Reorder crash path (s54-b / s52 / s53): selected bookmark + click items + Reorder toggle on real build/machine.
- [ ] Bookmark ghost-delete (s62): delete ghost bookmark + Ctrl+S once → ghost must vanish permanently; rename Enter (s61, watch `IDOK rawEditLen` log) + rename popup dark theme.
- [ ] Profile Sync dialog dark+light (s36-bn/bp/cs): theme bg/brush fix landed 2026-11-04 — verify on real build.
- [ ] Profile Backup dialog (fixed 2026-11-04, user verify): FillRect COLOR_WINDOW + brushes → theme bg.
- [ ] ImageSaveCropResize dialog (fixed 2026-11-04): `GetSysColorBrush(COLOR_BTNFACE)` erase/brush/class bg → theme color; verify dark + light.
- [ ] Profile dialogs dark (s36-cs original set): launch dark, open each dialog, all controls dark bg + readable text (7 P2.5 rows previously verified).
- [ ] Profile Sync dark cascade (s40): import/cloud-revert windows — theme check on real build.
- [ ] Storage unification Set Folder picker end-to-end (s64-R9o): Set Folder (FOS_PICKFOLDERS) → profileDataDir set → next-launch migration via ProfEnsureProfileDirAndMigrate; Sync dialog visuals (dialog visuals = user live check; 2 click attempts missed Home Sync target); reading-sidecar migration on first autoscroll tick (not exercised by probe).
- [ ] Flashcard Ctrl+Tab attribution (s64-R9n): study tick + rate credit the card's OWNING WindowTab (studyOrder[curIdx].tab), not the focused tab; verify with Ctrl+Tab mid-study on a real book.
- [ ] Adaptive prewarm on a lighter machine (s64-R9p): warm jobs untested (dev box memory-degraded); verify no burden on a weaker machine / N degrade.
- [ ] RandomOrder default feel (s64-R9q, fresh settings): verify default random-order behavior on a fresh settings/profile.
- [ ] Highlight boost (s64-R9k).
- [ ] HOME shipped s63: deep-scroll depth visual on real history (`-for-testing` has 0 files → `maxScrollY ≈ 0`; deep scroll UNTESTED); per-size check, empty vs populated.
- [ ] FC-X1 cross-page clozes (s36-m): user test on a real book (2-page passage).
- [ ] Image batch writeback s32-B: manual File > Save after an in-memory writeback to confirm on-disk persistence on a real book.
- [ ] Two Columns v2 autoscroll TC2 (s35): real book — autoscroll acceptance over TC2 (1L→1R→2L→2R, no jump), drag speed, OCR text selection per column (s35 fixes code-verified + E2E PASS — LOG s35/s36-l).
- [ ] BUG-3 remaining cases (s36-bw): user's exact steps (file + item) to reproduce anything still dead in the right-click image context menu.
- [ ] BUG-4 contrast one-shot (s36-az): first trim-config open after launch — does the contrast overlay still vanish? (Fix v2 deployed 2026-09-22; note: 2026-10-25 report was a separate save-reload wipe, FIXED.)
- [ ] TOC-first / torn sync s45/s46 (session-tabs): closed books not correctly removed from session — awaiting user validation (s45 pre-sync + s46 7 latent sync bugs).
- [ ] TC2 save-corruption revalidation (s36-bz, Bug7): save corrupts PDF under TC2 — fix deployed (VirtualToPhysical pageNo + corrupt-annot guard); revalidate on a real book.
- [ ] Flashcards — user live tests (queue): Filter window + global session (cross-tab jumping, per-book saves); List panel visual on real books; Settings window (folder picker + Drive backup, clear-selected, dark theme); bookmark-mirror on real TOC books (test PDF has none — automated test skipped the mirror).
- [ ] Flashcards — user live tests (s36-az batch): study-time + cards totals format on a real book; session-timer trio (checkbox/minutes edit/progress line/expiry beep+overlay) visual on light+dark; flashcard toolbar single centered labels + checked=bold+underline (no white face); home hides fc/arch toolbars and restores them.
- [ ] Stats: USER visual review — `tests/tmp/s36-f1-grid.png` + `s36-f1-graph.png` (dark); LIGHT theme pair added but UNVERIFIED (user: dark first, light later).
- [ ] Ctrl+click flashcard highlight opens List (s34-b): ctrl+click detects `TumatraPDF-Flashcard` author → sidebar; needs a book with flashcard highlights.
- [ ] s34-g user validation: Arch Measure Radius + Ref Line UX, resize-enlarge on a real book, flicker-free drag confirm (shipped + E2E 2026-10-11).
- [ ] TAB SYSTEM (s36-bu→s36-co, LOG): all dev done — revalidation on a real book.
- [ ] Design rules partial: TBSTYLE_EX_MIXEDBUTTONS (`Toolbar.cpp:2931/:3072`, `FlashcardToolbar.cpp:482`) + custom-draw POSTPAINT centering shipped — visual verify pending.
- [ ] Crop Align s64-R8 + s64-R9: Page Crop box cut == drawn region + nomacs overlay visual; GUI rotate-drag draws diagonal box + rotated handles (vision check).

- [ ] Loading fix (s64): on real launch confirm instant window with heavy books + logo-dots pulsing + white progress bar; evidence tests/tmp/s64/loading-heavy-*.png (dbg64; users get it via v1.0.19).

## Vertical tab list

Next to the minimize button, add a `🧾` button: show the list of open tabs as a vertical list, e.g.:

```
Tab1
Tab2
Tab3
```

Goal: make reading long file names easier. Clicking a tab in the list focuses that tab.

## OTHER FEATURES

- [ ] Add a settings option to disable the toolbar `scan mode` button; other buttons must adapt to the freed space.
- [ ] R2 latent rotated-90 bug triage (LOG s64-R2 — Edit/ notes archived 2026-11-15).
- [ ] Decision needed: branch protection "main must not contain merge commits" was BYPASSED on v1.0.17 push (b0fda73) — repo admin decision pending.

## Performance optimizations (verified 2026-11-04, evidence LOG s38)

Source: external agent's perf map (archived TEMP/performance-code-arch.dot), claims verified against code (LOG s38). Safest first:

- [ ] **OPT-7** coordinate-space guards (START): `#if isDebug` asserts in `DisplayModel::CvtToScreen/CvtFromScreen` + TC2 colX consistency check (debug-only, no behavior change; prerequisite for OPT-3). STATUS: in progress s38-o1.
- [ ] **OPT-6** perf regression tests: cache hit rate, RelayoutFrame frequency, trim/crop toggle latency; ad-hoc tests, fail on >5% regression. STATUS: baseline tests shipped (startup/cache-behavior/toggle-latency); CI fail-gate metrics (cache hit rate, relayout frequency) need new C++ control cmds — residual open.
- [ ] (DEFER) OPT-1 adaptive cache cap — after OPT-7; needs memory accounting + tests.
- [ ] (DEFER) OPT-3 partial trim-cache invalidate — real gain (Full FreeForDisplayModel storm, RenderCache.cpp:426) but stale-tile risk on rect.y shift; needs stamping/versioning.

- Reference: memory analysis tables -> TEMP/performance-notes.md (moved 2026-11-15)

## Findings status — audited 2026-11-04 (closed findings archived to LOG)

### Open (live)

- Coordinate space mismatch — convention: `DisplayModel::CvtToScreen/CvtFromScreen` = PAGE ↔ CANVAS-CLIENT; `VirtWnd` `ev.pt` = LOCAL vs `ctx.bounds` = ROOT-relative; E2E posted coords may arrive DPI-virtualized ×1.25. Rule: before any hit-test, name the space of BOTH operands (local/root/client/page). Mixed = bug.
- ReloadDocument / FORK_FIELD — no `FORK_FIELD` macro in source; manual copy persists `SumatraPDF.cpp:882-898`. Any new per-doc field must update both `UpdateFileStateForTab` (save) and `ReloadDocument` (restore) — pattern trap.

## Dev tooling / analysis — open items

- [ ] **B6-left** clang-tidy: only the 8 `clang-diagnostic-error` TUs remain (flag/std gap vs MSVC; platform-suffix files excluded) + the ~24k stylistic bulk (signed-bitwise, macro-to-enum, named-parameter, designated-initializers, redundant-parentheses, concise-preprocessor-directives, explicit-constructor) — a separate policy decision, not a defect list. Waves 1-5 closed: switch-default 128→0, narrowing 81→4 vendored, roundings 17→0, unchecked-conv 7→0 (1 justified NOLINT); analyzer 42 all triaged. Baseline: `out/lint/clang-tidy-summary.txt` (LOG s36-r/ac/ad/ag/ak/al) — **baseline file ABSENT (regenerate before comparing)**.

## Dependency updates — open items

- [ ] If a real dependency regression appears: bisect by patch group (the 27 patches in `ext/patches/` are the only local delta over pristine mupdf 1.28.5; reverse-apply state: 22 applied / 19 absent / 6 conflict).
- [ ] Bun `1.3.14 → 1.4.3` (build/test scripts only) after re-running the suite. Bun version not pinned in repo files (`cmd/package.json` has no version).
- [ ] **F6 — explicitly deferred**: harfbuzz/freetype only when mupdf bumps its pins (mupdf 1.28.5 pins exactly our versions — upgrading independently diverges from the combination Artifex tests). UnRAR metadata drift: `ext/versions.txt` says 7.1.6 but `ext/unrar/version.hpp` reports RARVER 7.11.0 — verify against rarlab before any change.

## Refactoring backlog (tree-sitter audits; completed R1/R2/R4, 16A-D, C2-C5/F6, R-P1..R-P3 → LOG)

- [ ] **R3-left**: unify TrimRect math (TrimRectPage/Inverse) — sites semantics differ (mediabox shrink vs area/pt shift); forcing a helper = render-path risk. Only with a rendering regression test.
- [ ] **R5-left**: remove v1 crop stack — BLOCKED: v1 fields REMOVED from `DisplayModel.cpp:443-444` but `CmdViewportCropToggle` Scan Mode button + call-sites still live (`Toolbar.cpp:106`, `Menu.cpp:630`, `SumatraPDF.cpp:9954/10004/11599`). Do when/if v1 button is retired.
- [ ] **R7 / 16C-next**: continue god-file extraction — SumatraPDF.cpp (~14.2k lines, target ~10k), then Canvas.cpp (~5.7k; needs a dependency MAP first — mouse/keyboard interleaved with rendering, no established pattern), EngineMupdf.cpp (~6.7k, decouple rendering helpers per format).
- [ ] **16D**: typed `gGlobalPrefs` accessors (152 accesses in SumatraPDF.cpp) — only when tests exist.

## Flashcards

### Tests

- [ ] **FC-M1** tests: SM-2 (intervals/ease cap/relearn), study-JSON roundtrip, due filter — after Image Occlusion lands.
- [ ] **FC-M5** rate keys 1-4 have no visual hint (no rate buttons/tooltips on toolbar).

### Study preload

- [ ] Flashcard study preload: pre-load next cards' pages/annots during study=on (esp. cross-doc session) to cut card-to-card load time; ADAPTIVE lookahead (budget by measured render time; low-priority background; caps so weak machines not burdened). P1+P2 shipped s64-R9p (EWMA-driven N, BELOW_NORMAL worker, epoch invalidation, caps, silent N=0 degrade; probe mechanism-verified; warm jobs untested — dev box memory-degraded). P3 = opt-in setting only (`flashcardPreloadSessionBooks`, lazy-tab background load NOT implemented). Awaiting user live validation + lighter-machine check of warm-job path (see USER validation queue).

## Image Occlusion cards — PLANNED (awaiting user decisions)

Boxes over images/diagrams as cards — same Highlight-annot + mask/reveal/rate pipeline as text clozes; only the DRAWING MODE at creation is new. 4 steps (IO-1 state `boxMode`+`pendingBoxes`; IO-2 canvas rubber-band before selection branches; IO-3 screen→page conversion like cloze rects; IO-4 `S` creates ONE Highlight with N quads, Esc/Box-off discards).

- [ ] **IO-D1** grouping: boxes session → 1 grouped card (recommended, matches Alt+multi-select) vs 1 card/box (Anki default)? — NOTE (s36-m): the cross-page cloze already ships this mechanism for multi-page (`FlashcardGroupToken` + `Flashcard::extraPages`); reuse it for box groups (same card, several rect groups on one page) instead of a second scheme.
- [ ] **IO-D2** mode entry: toolbar "Box" button only? key `B`? both?
- [ ] **IO-D3** scope: boxes cover image/diagram regions only (no text selection) — confirm.

## Stats

- [ ] Future feature (user spec): per-book `book_stats` button on the right side of each book row in Home stats, opening a per-book info window:
  - Reading heatmap: grid of squares (like the stats one), 1 square = 1 page; while autoscroll is ON, entering a page AND leaving to the next counts +1 for that page.
  - Window sketch:
  ```
    |*Book name*|
    |Reading: 00h 25m |
    | 7/15 Pages | 46% |
    |Flashcards: `info` |
    |Reading Heatmap:|
    |⬛⬛⬛⬛⬜|
    |⬜⬜⬜⬜⬛|
    |⬜⬜⬛⬛⬛|
  ```

## Home page responsiveness — shipped s63 (see CHANGELOG v1.0.17)

- [ ] USER validation on real history: scroll depth (`-for-testing` has 0 files → `maxScrollY ≈ 0`; deep scroll UNTESTED); per-size check empty vs populated.
- [ ] Item D — toolbar overflow/clips at narrow widths (pre-existing): NOT addressed.

## Design rules — implementation pending

- [ ] Buttons/inputs must center content (text/icons) horizontally AND vertically (ToolbarLayout controls, timer/speed inputs, new UI elements). Rule defined 2026-09-08. Flashcard toolbar done (s36-ax, custom-draw POSTPAINT centering); main toolbar autoscroll button label done; rest pending. PARTIAL — TBSTYLE_EX_MIXEDBUTTONS (`Toolbar.cpp:2931/:3072`, `FlashcardToolbar.cpp:482`) + custom-draw POSTPAINT centering shipped; visual verify pending (see USER validation queue).

## Research (low priority)

- [ ] **OCR image** (right-click on image → OCR text): upstream vendors tesseract 5.5.2 (Artifex fork) for `mutool draw -F ocr.*` — cheapest path for the fork; decide separately (binary size cost). Lightweight near-zero-dependency alternatives also acceptable.

## Profile Memory — shipped (F2→F7; F8 partial)

Feature shipped: single profile file `tumatraPDF-data_1.json` (exportable/syncable/importable), Drive-synced folder distribution with atomic replace + conflicted-copy tolerance, Portable→Install import, local 9×8h rolling backups + recovery dialog, overwrite-vs-use-existing conflict UX, Home `⟳ Sync` button + Profile window (s36-bn/bp/bq, LOG). Open items:

- Gap s36-bf: ProfImportLegacy first-run-only — reconciliation of reading-stats.txt/flashcard-stats.txt day rows at startup pending (32reading days from legacy sidecars can go missing; recovery done manually 2026-10-31: added 20261017/20261019 rows to tumatraPDF-data_1.json).
- [ ] **F8 — Endgame centralization** (dual-write → profile as single source): migrate FlashcardStudyListDocs to iterate profile books; deprecate dual-write in per-book *.json + sidecars WHEN stable (flashcard Config window keeps reading legacy fallback); keep per-machine day rows for multi-PC. Slices 1-2 (live stats ticks → profile; Config list merge) shipped s36-bs/s36-bt — LOG.

## Bugs open

- [ ] TOC Reorder / bookmark toolbar: click-nav FIXED (s51-d — WM_LBUTTONDOWN gated behind tocReorderMode, default OFF = pass-through); top bar (Add + Reorder toggle, accent underline when ON) DONE s51-e; rename/update/delete right-click-menu only. s52 crash FIXED (TOC custom-draw use-after-free; `IsTocItemInCurrentTree` guard + ReloadTocTree after OutlineEdit). s53 wedge FIXED (async debounced ReloadTocTree + O(1) ownerTree validity check). Build 0/0; regressions PASS. Details: LOG s51→s55. USER live validation pending — see queue.
- [ ] Session Tabs: closed books not correctly removed from session — [s45] MainWindowTabClosed pre-syncs active group on last-tab close; build 0/0, ad-hoc-tab-session(-sync/-e2e) PASS — awaiting USER validation (keep [ ]). [s46] audited: 7 latent sync bugs fixed (RemoveTab choke point + suppression guard + MoveTab pinned guard + drag/reorder sync + boot-restore SaveSettings + SyncActiveGroupToEmpty suppression); #8 missing-file-at-restore = pending design decision (skip vs error).
- [ ] Bug7 (s36-bz): save corrupts PDF under TC2 — root cause verified + fix deployed (VirtualToPhysical pageNo in Selection.cpp FromRectangle + corrupt-annot guard in EngineMupdfSaveUpdated); user revalidation pending — see queue.
- [ ] **BUG-3** right-click image context menu: s34 E2E confirmed Replace/Paint/Crop work + submenu visibility fixed; s36-ao live-checked the rest — `Image > Save` works, menus filter correctly, silent null-bitmap returns now notify (fdcafde). REMAINING: user's exact steps (file + item) to reproduce anything still dead — see queue.
- [ ] **BUG-4** Contrast × Trim Config ONE-SHOT vanish (first trim-config open after launch). Fix v2 (`EnsureContrastOverlayState` invariant + self-healing) deployed 2026-09-22 — awaiting user confirmation of the exact flow; `[contrast]` breadcrumbs in debug build. NOTE (s36-az): 2026-10-25 "contrast stopped working" = save-reload state wipe (FIXED — ReloadDocument preserves extras; LOG s36-az); if it still reproduces, it is a separate flow.
- [ ] **Ctrl+click flashcard highlight opens List** — implemented s34-b (Canvas ctrl+click detects `TumatraPDF-Flashcard` author → FlashcardSidebarShow; turns flashcard mode on if off). USER live-test pending — see queue.
- [ ] **s34-g user validation pending**: Arch Measure Radius + Ref Line UX, resize-enlarge on a real book, flicker-free drag confirm (shipped + E2E 2026-10-11) — see queue.
- [ ] Revisar outros elementos com divergência .md vs PDF (padrão: PDF) — auditoria ampla ainda não feita; fixes pontuais desta sessão: home overlay + F (fullscreen).

## Tab system

Add a "Tab Group" button next to the main button at the top of the window, before the tabs begin (left of the document tabs). Goal: make the existing Tab Group dynamic. On click, show a dropdown listing the existing 'Tab groups'; clicking one closes all current documents and reopens all documents of the selected tab group; opening another document inside an open tab group auto-syncs (saves) for future sessions.

The app always restores the last tab group used by the user; if none exists, open `new`.

'Ctrl+n' opens a window with a `New` tab group — not synced. To save such a session as a tab group, add a `Save` button to the dropdown list; clicking it opens an editor/input to name the group; immediately after, the group auto-syncs on close or when documents are added to the session. Dragging a document out of a session removes it from that group automatically and moves it into a non-synced `new` tab group.

- [ ] Open (user action): TAB SYSTEM — all dev done s36-bu→s36-co (LOG), awaiting user revalidation on real book. Build 0/0; ad-hoc-tab-session.ts + ad-hoc-tab-session-e2e.ts + ad-hoc-tab-menu.ts PASS.



## Vertical_Toolbar (PARKED — DO NOT DO)

Idea frozen by the user ("NAO FAZER") — kept for reference only: when the window is narrower than 1/3 of the screen, adapt the toolbar to a left vertical bar (buttons top-to-bottom, converted to symbols with name on hover), keeping a top horizontal bar only for numeric inputs (page, timer) and remaining buttons; when home=on keep hiding all buttons.

## Page Counting Format

The app already displays pages in other counting formats; we need a TOOL to change the PDFs so the file's page numbers match the real book numbers. Similar to Adobe: a tool to re-number page ranges, especially at the start of documents.

Example: pages 1-50 counted in roman, from page 51 on in decimal → page 51 becomes the new page 1. Typing 1 in the page field lands on physical page 51, making the book/page match exact. We can also create an ABSOLUTE page function to ignore cosmetic counting; in this example: `1` → page 51; `(1)` → page 1.

## Capture tool — shipped v1 (s39)

- [ ] residual: no save-to-file for captures

## Repair document file — shipped v1 (s42)

- [ ] USER live validation (see USER validation queue)
- [ ] Malformed-PDF repair-on-open (feature CANDIDATE; opened 2026-11-15 by R9v incident): offer in-app repair (MuPDF-clean content-stream sanitize pass, fitz clean=True equivalent) when open succeeds but page render throws FZ_ERROR_ARGUMENT / page fails to paint. Scope: detect-and-offer dialog (not auto-silent rewrite); original file NEVER modified in place (user picks target); reuse pattern of crash-guard docs. See LOG 2026-11-15 (s64-R9v) + tests/tmp/s64/ (fixture flow) for the exact incident + repair recipe.

## Test harnesses

- [ ] **Control-server UI marshalling**: Test* handlers (TestToolbarButtons etc.) run TB__/TTM__ queries on the BACKGROUND control thread against UI-owned toolbars — inter-thread sends delivered mid-pump; combined with comctl rebanding/custom draw this reproducibly AVs (two s36-az crash runs, stack: WndProcToolbar fallthrough → comctl write to a dangling ptr). Proper fix = marshal Test* execution to the UI thread (SendMessage to a UI-owned window). The deterministic trigger (toolbar2 stacked band) is fixed; remaining risk documented in LOG s36-az.

### Regression tests wanted (NEXT_STEPS P4)

2/5 exist: `ad-hoc-b4-coordinate-invariants.ts` + `ad-hoc-settings-migration.ts` (registered in `tests/before-release.ts:40/:42`). Missing: WebView2 adapter completeness, ReloadDocument fork field preservation, toolbar-ordering verdict.

## Sumatra → Tumatra — non-label items (deep review pending)

Items involving .exe file names, internal window classes, registry/CLSID, installer/uninstaller, path/CLI tests, .rc resources — require case-by-case review before changing (not direct UI labels):

- [ ] `src/Installer.cpp:866,890` — `StrL("SumatraPDF.exe.copy")`, `StrL("SumatraPDF.exe")` (file names in the installer)
- [ ] `src/Uninstaller.cpp:51,63,372,386` — `"SumatraPDF.exe"`, `"SumatraPDF-settings.txt"`, `"Sumatra-Uninstaller.exe"` (uninstaller files)
- [ ] `src/RegistryPreview.cpp:194,210` — `StrL("SumatraPDF.exe")`, `StrL("SumatraPDF-data")` (registry/preview paths)
- [ ] `src/base/tests/File_ut.cpp` — `"SumatraPDF.exe"`, `"SumatraPDF"` (path-test asserts)
- [ ] `src/SumatraUnitTests.cpp` — `"SumatraPDF.exe -bench ..."` (command-line tests)
- [ ] `src/SumatraPDF.rc:76-80` — icon file `SumatraPDF-smaller.ico` (resource)
- [ ] `src/TestPlugin.cpp` / `tools/plugin-test.cpp` — `L"SumatraPDF Plugin Test"` (test plugin name)
- [ ] Internal window classes (keep for Win32 compatibility, evaluate case-by-case):
  - `src/base/Win.cpp:1906` — `L"SumatraPDFClipboardOwner"`
  - `src/wingui/Wnd.cpp:66` — `L"SumatraWgDefaultWinClass"`
  - `src/previewer/PdfPreview.cpp:372` — `L"SumatraPDF_PreviewPane"`
  - `src/ScreenshotCapture.cpp:25` — `L"SumatraScreenshotOverlay"`
  - `src/SelectionToolbar.cpp:35` — `L"SumatraSelectionToolbar"`
  - `src/RefHoverInternal.h:9` — `L"SumatraPDFRefHover"`
- [ ] Registry/CLSID/DDE/user-agent (Windows internals; evaluate registry/file-association impact):
  - `src/RegistrySearchFilter.cpp` — `"SumatraPDF IFilter"`, etc.
  - `src/SearchAndDDE.h` — `kSumatraDdeServer L"SUMATRA"`, structs `SumatraOpen*`
  - `src/base/Http_win.cpp:62` / `Http_mac.cpp:53` — user-agent `SumatraPdfHTTP`
  - `src/SumatraLog.cpp:13` / `SumatraLog_posix.cpp:9` — `gLogAppName = StrL("SumatraPDF")`
  - `src/SumatraPDF.h` — functions `SumatraLaunchBrowser`, `WndProcSumatraFrame`, `GetSumatraDataDirTemp`, etc.

## Open-source references

1. https://github.com/nomacs/nomacs
