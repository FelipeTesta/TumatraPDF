# TODO

1. Open work ONLY.
2. Completed features/fixes live in [LOG.md](LOG.md) (per-session) + [CHANGELOG.md](CHANGELOG.md) (per-release).
3. User manual-test items = items only the user can validate.
4. Housekeeping (2026-10-20, s36-ae; re-triaged s36-at, s36-cu/cv): all `[x]` blocks archived — details live in LOG per session entry; this file lists open work only.

## Execution queue — top 5 (effort/gain)

User-locked order:

1. **HARDENING PACK** (do before any new feature):
   - (a) TC2 un-routed `engine->` sites — `Commands_Edit.cpp:726` `GetElements(pageNo)`, `Commands_Flashcard.cpp:324` `GetFzPageInfo(card.pageNo)`, `RenderCache.cpp:506` `GetTileRes` (comment claims routing, body unverified), `RenderCache.cpp:1212` non-cache Paint.
   - (b) Test* control handlers marshal `TB__`/`TTM__` queries to UI thread — `SumatraControl.cpp:938` → `Toolbar.cpp:2747` sends from control pipe thread; AV risk, LOG s36-az.
   - (c) WebView2 adapter open part — create `docs/md/WebView2-Adapter-Pattern.md` (adapter `MarkdownModel.cpp:181` verified forwarding all 9 `HtmlWindowCallback` virtuals — document pattern + list) + add `TestMarkdownAdapter` to `cmd/control.ts`.
2. **Capture tool** Shift+S (spec pointer → `## Capture tool` below).
3. **Bookmarks Improve** (spec pointer → `## Bookmarks Improve` below).
4. **Edit/Align** (spec pointer → `## Edit/` below, PT verbatim).
5. **Page Counting Format** (spec pointer → `## Page Counting Format` below).

## Findings status — audited 2026-11-04

### Closed (verified)

- Toolbar critical ordering — all 4 toolbars strip theme before `TbAddButtons` (`Toolbar.cpp:2907/:3076`, `FlashcardToolbar.cpp:465`, `Commands_Edit.cpp:126`). s36-ax lineage.
- Dark Mode Epoch legacy path dead — `ToolbarApplyThemeToRebar` (`Toolbar.cpp:3181`) uncalled; epoch bumped by `ApplyThemeColorsToDarkMode` (`Theme.cpp`).
- ASan `build-asan.ts` targets `TumatraPDF.sln` correctly.
- Settings names-blob migration fixed (s36-br, commit `6df3dc3`).
- dmlib rebar interception fixed (`ToolbarArchEditRebarSubclassProc` + `TBSTYLE_EX_MIXEDBUTTONs`, user-verified s36-bb lineage).
- Flashcard annotation author-token rule → AGENTS.md §12 (2026-10-18).

### Open (live)

- Coordinate space mismatch — convention: `DisplayModel::CvtToScreen/CvtFromScreen` = PAGE ↔ CANVAS-CLIENT; `VirtWnd` `ev.pt` = LOCAL vs `ctx.bounds` = ROOT-relative; E2E posted coords may arrive DPI-virtualized ×1.25. Rule: before any hit-test, name the space of BOTH operands (local/root/client/page). Mixed = bug.
- WebView2 adapter — open part moved to Execution queue item 1c.
- TC2 virtual→physical routing — un-routed evidence at Execution queue item 1a; cache-path sites verified fixed; uia `PageProvider` MVP-acceptable.
- ReloadDocument / FORK_FIELD — no `FORK_FIELD` macro in source; manual copy persists `SumatraPDF.cpp:882-898`. Any new per-doc field must update both `UpdateFileStateForTab` (save) and `ReloadDocument` (restore) — pattern trap.

## Dev tooling / analysis — open items

- [ ] **B6-left** clang-tidy: only the 8 `clang-diagnostic-error` TUs remain (flag/std gap vs MSVC; platform-suffix files excluded) + the ~24k stylistic bulk (signed-bitwise, macro-to-enum, named-parameter, designated-initializers, redundant-parentheses, concise-preprocessor-directives, explicit-constructor) — a separate policy decision, not a defect list. Waves 1-5 closed: switch-default 128→0, narrowing 81→4 vendored, roundings 17→0, unchecked-conv 7→0 (1 justified NOLINT); analyzer 42 all triaged. Baseline: `out/lint/clang-tidy-summary.txt` (LOG s36-r/ac/ad/ag/ak/al) — **baseline file ABSENT (regenerate before comparing)**.

## Dependency updates — open items

- [x] **Suite baseline (pre-existing, NOT caused by the dependency batch) — RESOLVED (s36-au/ay/ba/bb), 0 entries fail.** History in LOG s36-au/ay/ba/bb (shutdown group, issue-5869, unit-tests exit 1, UI/timing). See LOG.
- [ ] If a real dependency regression appears: bisect by patch group (the 27 patches in `ext/patches/` are the only local delta over pristine mupdf 1.28.5; reverse-apply state: 22 applied / 19 absent / 6 conflict).
- [ ] Bun `1.3.14 → 1.4.3` (build/test scripts only) after re-running the suite. Bun version not pinned in repo files (`cmd/package.json` has no version).
- [ ] **F6 — explicitly deferred**: harfbuzz/freetype only when mupdf bumps its pins (mupdf 1.28.5 pins exactly our versions — upgrading independently diverges from the combination Artifex tests). UnRAR metadata drift: `ext/versions.txt` says 7.1.6 but `ext/unrar/version.hpp` reports RARVER 7.11.0 — verify against rarlab before any change.

## Refactoring backlog (tree-sitter audits; completed R1/R2/R4, 16A-D, C2-C5/F6, R-P1..R-P3 → LOG)

- [ ] **R3-left**: unify TrimRect math (TrimRectPage/Inverse) — sites semantics differ (mediabox shrink vs area/pt shift); forcing a helper = render-path risk. Only with a rendering regression test.
- [ ] **R5-left**: remove v1 crop stack — BLOCKED: v1 fields REMOVED from `DisplayModel.cpp:443-444` but `CmdViewportCropToggle` Scan Mode button + call-sites still live (`Toolbar.cpp:106`, `Menu.cpp:630`, `SumatraPDF.cpp:9954/10004/11599`). Do when/if v1 button is retired.
- [ ] **R7 / 16C-next**: continue god-file extraction — SumatraPDF.cpp (~14.2k lines, target ~10k), then Canvas.cpp (~5.7k; needs a dependency MAP first — mouse/keyboard interleaved with rendering, no established pattern), EngineMupdf.cpp (~6.7k, decouple rendering helpers per format).
- [ ] **16D**: typed `gGlobalPrefs` accessors (152 accesses in SumatraPDF.cpp) — only when tests exist.

## Two Columns v2 — pending USER live-confirmations (code complete, all E2E-verified)

- [ ] User confirm on a real book: autoscroll acceptance over TC2 (1L→1R→2L→2R, no jump), drag speed, OCR text selection per column. The three s35 fixes (stall-on-hover, drag-slow, per-column selection) are code-verified + E2E PASS — see LOG s35/s36-l.

## Flashcards

- [x] Timer capture core (shipped):
  - Study timer (100Hz tick) starts when study mode is ON, stops when OFF
  - Per-card cap: 60 seconds — stops counting time for that card (user advances manually, NO auto-advance)
  - Main total (flashcard-stats.txt sidecar, keyed by date) — survives rename/delete/move
  - Per-book reference: FileState.flashcardStudyTimeSec (persisted with file history)
  - No time captured while revealMode is ON
  - Files added: FlashcardStats.h/.cpp, timer ID kFlashcardStudyTimerID=16 in Canvas.h

- [x] Home page: "Flashcards" tab under "Most Read" space (same structure as Read, showing book list + flashcard times)
- [x] Calendar hover label: show "Read: Xh Ym / Flashcards: Xh Ym" for each day
- [x] Graph: add peru-colored line for flashcard study time (separate from reading time)
- [x] Study time capture (STATS, no visual feedback): while study mode is ON and the answer is NOT revealed, credit time to the stats. Each card contributes at most 60s (per-card cap; the accumulator resets on card advance). Main total (flashcard-stats.txt sidecar, keyed by date) survives rename/delete/move; per-book reference: FileState.flashcardStudyTimeSec.
- [x] Study SESSION timer (like autoscroll's, user-configured): checkbox + "Timer:" + minutes edit (default 30) in the flashcard secondary toolbar (CmdFlashcardStudyTimerToggle / CmdFlashcardStudyTimerEdit). When enabled at Study start, a 2px silver progress line at the bottom of the screen shrinks symmetrically toward the center over the session duration (fullscreen: bottom of screen; windowed: ~48px above the taskbar). On expiry: study session ends, beep + "∴" overlay (3s, same as autoscroll). The session timer is INDEPENDENT of the 60s/card stats cap.
- [x] Cards-rated counts (s36-az): every rating (incl. Again) counts 1 card; sidecar gains a cards column (`flashcard-stats.txt`: `date sec cards`, 2-col legacy reads as 0), per-book `FileState.flashcardCardsDone`; Home flashcards ranking shows "N cards / time" and the totals line uses the user format `Flashcards: Today N cards / Xh Ym | Total: N cards / Xh Ym`.

### Tests

- [ ] **FC-X1 live test** (shipped s36-m): cross-page cloze — user test on a real book (2-page passage).
- [ ] **FC-M1** tests: SM-2 (intervals/ease cap/relearn), study-JSON roundtrip, due filter — after Image Occlusion lands.
- [ ] **FC-M5** rate keys 1-4 have no visual hint (no rate buttons/tooltips on toolbar).
- [ ] User live tests (queue): Filter window + global session (cross-tab jumping, per-book saves); List panel visual on real books; Settings window (folder picker + Drive backup, clear-selected, dark theme); bookmark-mirror on real TOC books (test PDF has none — automated test skipped the mirror).
- [ ] User live tests (s36-az batch): study-time + cards totals format on a real book; session-timer trio (checkbox/minutes edit/progress line/expiry beep+overlay) visual on light+dark; flashcard toolbar single centered labels + checked=bold+underline (no white face); home hides fc/arch toolbars and restores them.

## Image Occlusion cards — PLANNED (awaiting user decisions)

Boxes over images/diagrams as cards — same Highlight-annot + mask/reveal/rate pipeline as text clozes; only the DRAWING MODE at creation is new. 4 steps (IO-1 state `boxMode`+`pendingBoxes`; IO-2 canvas rubber-band before selection branches; IO-3 screen→page conversion like cloze rects; IO-4 `S` creates ONE Highlight with N quads, Esc/Box-off discards).

- [ ] **IO-D1** grouping: boxes session → 1 grouped card (recommended, matches Alt+multi-select) vs 1 card/box (Anki default)? — NOTE (s36-m): the cross-page cloze already ships this mechanism for multi-page (`FlashcardGroupToken` + `Flashcard::extraPages`); reuse it for box groups (same card, several rect groups on one page) instead of a second scheme.
- [ ] **IO-D2** mode entry: toolbar "Box" button only? key `B`? both?
- [ ] **IO-D3** scope: boxes cover image/diagram regions only (no text selection) — confirm.

## s32-B — image batch live-debug (user authorized)

Replace / Edit With Paint / top-level Copy Image shipped s31; s34 E2E covers the round-trips (replace, paint, crop → writeback). REMAINING (user-side): manual File > Save after an in-memory writeback to confirm on-disk persistence on a real book.

## Design rules — implementation pending

- [ ] Buttons/inputs must center content (text/icons) horizontally AND vertically (ToolbarLayout controls, timer/speed inputs, new UI elements). Rule defined 2026-09-08. Flashcard toolbar done (s36-ax, custom-draw POSTPAINT centering); main toolbar autoscroll button label done; rest pending. PARTIAL — TBSTYLE_EX_MIXEDBUTTONS (`Toolbar.cpp:2931/:3072`, `FlashcardToolbar.cpp:482`) + custom-draw POSTPAINT centering shipped; visual verify pending.

## Theme Colors Compliance (P2.5) — dark-mode white controls

Dialog frame dark via `dmlib::setDarkWndSafe()` but child controls (edit/static/listbox/button) paint white in dark mode — missing `WM_CTLCOLOR` handling. Fix pattern = `ArchScaleDialog.cpp:278-288` (`WM_CTLCOLORDLG/STATIC` + `SetBkColor`/`SetTextColor` + `BackgroundBrush()`; theme API `ThemeWindowControlBackgroundColor()`/`ThemeWindowTextColor()`/`ThemeHighlightColor()`). Verify: launch dark, open each dialog, all controls dark bg + readable text.

Scan 2026-11-04 (full src sweep, LOG s36-cs):

| Priority | Dialog/Window            | File                                                                | Fix pattern                                                                                                                                                                                                          |
| -------- | ------------------------ | ------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1        | Password Dialog          | `SumatraDialogs.cpp:178` `Dialog_GetPassword_Proc`                  | `WM_CTLCOLOREDIT/STATIC/BTN` handling                                                                                                                                                                                |
| 2        | Go to Page Dialog        | `SumatraDialogs.cpp:282` `Dialog_GoToPage_Proc`                     | `WM_CTLCOLOREDIT/STATIC` handling                                                                                                                                                                                    |
| 3        | Find Window              | `FindWindow.cpp`                                                    | `WM_CTLCOLOREDIT/STATIC/LISTBOX` handling                                                                                                                                                                            |
| 4        | Find Bar                 | `FindBar.cpp`                                                       | `WM_CTLCOLOREDIT` handling                                                                                                                                                                                           |
| 5        | Advanced Settings Dialog | `AdvancedSettingsDialog.cpp`                                        | Verify wingui controls use theme colors                                                                                                                                                                              |
| 6        | Trim Config Dialog       | `TrimConfigDialog.cpp`                                              | Verify wingui Edit controls use theme colors                                                                                                                                                                         |
| 7        | Print/Page Setup Dialogs | `PrintDialog.cpp`                                                   | `dmlib::setDarkWndSafe` + `WM_CTLCOLOR`                                                                                                                                                                              |
| 1/HIGH   | Image save/crop/resize   | `ImageSaveCropResize.cpp:1735-1743/:1746-1749/:2136`                | `GetSysColorBrush(COLOR_BTNFACE)` erase+brush+class bg → theme color; STATUS: FIXED 2026-11-04 (user verify pending)                                                                                                 |
| 2/MED    | Profile Backups dialog   | `ProfileBackupWnd.cpp:55/:264/:91/:196/:324`                        | FillRect COLOR_WINDOW + brushes → theme bg; STATUS: FIXED 2026-11-04 (user verify pending)                                                                                                                           |
| 2/MED    | Profile Sync dialog      | `ProfileSyncWnd.cpp:380/:305-308`                                   | hbrBackground COLOR_WINDOW+1 default → theme; CTLCOLORSTATIC/BTN partial exists; STATUS: FIXED 2026-11-04 (user verify pending)                                                                                      |
| 3/MED    | SumatraDialogs (9 procs) | `SumatraDialogs.cpp:364/:474/:715/:772/:883/:997/:1056/:1209/:1512` | NO-FIX: every proc calls `dmlib::setDarkWndSafe(hDlg)` in WM_INITDIALOG (dmlib `setWindowCtlColorSubclass` themes EDIT/STATIC/LISTBOX/BTN children); no WM_CTLCOLOR handler needed; STATUS: NO-FIX (audit-confirmed) |
| 3/LOW    | FormFields listbox       | `FormFields.cpp:239` direct `LISTBOX`                               | parented to `hwndCanvas`; `WM_CTLCOLORLISTBOX` sent to canvas parent → added handler in `Canvas.cpp WndProcCanvas` (theme bg + text); STATUS: FIXED 2026-11-04                                                       |

status as of scan: ImageSaveCropResize, Profile Backup, Profile Sync, FormFields fixed 2026-11-04 (user verify pending); SumatraDialogs 9 procs NO-FIX (dmlib setDarkWndSafe covers child controls :187/:291/:373/:490/:724/:776/:892/:1005/:1065/:1214/:1524); original 7 P2.5 rows previously verified.

OS-limited (not fixable): GetOpen/SaveFileNameW sites (`EditAnnotations.cpp:1216`, `ImageSaveCropResize.cpp:1143`, `PdfTools.cpp:78`, `SumatraPDF.cpp:4744/5344/5614/5709`, `WindowTab.cpp:260`) + `TaskDialogIndirect` (`AIChatCommon.cpp:274`, `Installer.cpp:753/1495/2295`, `SumatraPDF.cpp:4847`, `SumatraStartup.cpp:1354/1431/1487`, `UpdateCheck.cpp:388/614/658`).

## Test harnesses (tests/tmp/ — scratch, E2E-class until promoted to ad-hoc)

Scratch E2E harnesses live in `tests/tmp/` (gitignored): s34-replace/paint/crop/edit-e2e, s34-reload-colors, s34c-inpage-e2e, s34e-doubletoggle, s34g-findcrash, s34c-tb-probe, s35-tc2-auto, s35-tc2-sel, s33-arch-repro, s36pa1-paint (PA1 mask paint cost), ad-hoc-pa2-strip-scroll (promoted to tests/), s36ax-fctb (fc toolbar + trio + home cycle, 17/17), s36az-scroll (strip-under-fc), s36az-cards (cards sidecar), s36az-imagedrag (scan-vs-figure drag), s36ay-batch (contrast/save-reload/trim/speed — D-stage arch/edit asserts still unproven: the old comctl crash hit before them; re-run now that band-90 is fixed). Reusable helpers: `tests/winapi.ts`, `tests/win-automation.ts`; DPI + postMessage traps documented in lemma (m9fa780f3cf0b, m5317f7f3eb3d). Probe-writing rules: tumatrapdf-e2e guide (lemma id 34) — in-proc oracles only (sumlog witnesses, TestToolbarButtons dump, window-class enum); NEVER cross-proc TB_* scalars (wrong WM_USER offsets poison the toolbar) or PrintWindow on a comctl toolbar (re-entrant render → wedge/AV).

- [ ] **Control-server UI marshalling**: Test* handlers (TestToolbarButtons etc.) run TB__/TTM__ queries on the BACKGROUND control thread against UI-owned toolbars — inter-thread sends delivered mid-pump; combined with comctl rebanding/custom draw this reproducibly AVs (two s36-az crash runs, stack: WndProcToolbar fallthrough → comctl write to a dangling ptr). Proper fix = marshal Test* execution to the UI thread (SendMessage to a UI-owned window). The deterministic trigger (toolbar2 stacked band) is fixed; remaining risk documented in LOG s36-az.

### Regression tests wanted (NEXT_STEPS P4)

2/5 exist: `ad-hoc-b4-coordinate-invariants.ts` + `ad-hoc-settings-migration.ts` (registered in `tests/before-release.ts:40/:42`). Missing: WebView2 adapter completeness, ReloadDocument fork field preservation, toolbar-ordering verdict.

## Research (low priority)

- [ ] **OCR image** (right-click on image → OCR text): upstream vendors tesseract 5.5.2 (Artifex fork) for `mutool draw -F ocr.*` — cheapest path for the fork; decide separately (binary size cost). Lightweight near-zero-dependency alternatives also acceptable.

## Edit/

- [ ] Crop
- [ ] Align: Similar ao draw line do measure (archtools), vamos desenhar uma linha com dois pontos, e a partir dessa linha, realinhar a página removendo a distorção para que o alinhamento referente a linha que acabamos de desenhar fique 100% vertical ou horizontal; vamos recortar o excesso de página mantendo a mesma proporção original da página; após recortar, o usuário precisa salvar a alteração no documento para aplicar;

## Bookmarks Improve

1. Button 'Reorder' on/off: allows click:hold and repositioning bookmark order.
   - Dropping a bookmark onto another includes it as 1 indent level; dropping at the far left removes it from the level;
   - Show a dynamic horizontal line representing where the bookmark will land on drop, depending on cursor position.
2. Right-Click →'Rename'
3. Right-Click →'Delete'
4. Button 'Add': creates a bookmark at the end of the list for the CURRENT page.
5. Right-Click →'Update': changes the clicked bookmark to the current page.
   All changes must be saved into the document to persist.

## Page Counting Format

The app already displays pages in other counting formats; we need a TOOL to change the PDFs so the file's page numbers match the real book numbers. Similar to Adobe: a tool to re-number page ranges, especially at the start of documents.

Example: pages 1-50 counted in roman, from page 51 on in decimal → page 51 becomes the new page 1. Typing 1 in the page field lands on physical page 51, making the book/page match exact. We can also create an ABSOLUTE page function to ignore cosmetic counting; in this example: `1` → page 51; `(1)` → page 1.

## Capture tool

Shift+S: draw a rectangle that captures the image inside it at the document's DPI → copy to clipboard.

## Stats — open (shipped s35-c; design rounds s35-d..s36 archived to LOG)

- [ ] USER visual review pending: `tests/tmp/s36-f1-grid.png` + `s36-f1-graph.png` (dark); LIGHT theme pair values added but UNVERIFIED (user: dark first, light later).
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

# Bugs open

[ ] Bug7 (s36-bz): save corrupts PDF under TC2 — root cause verified + fix deployed (VirtualToPhysical pageNo in Selection.cpp FromRectangle + corrupt-annot guard in EngineMupdfSaveUpdated); user revalidation pending.

[x] Toolbars Flickering — FIXED (s36-bl): lastPackedAvailDx histerese key orphaned when state tables split (sToolbarPackStates @1302 / sToolbarRewrapStates @1632); guard timer 200ms never converged → infinite re-wrap on narrow window. Fix: write to both tables (Toolbar.cpp ~1446). Verification: PNG probes byte-identical after resize; user validation pending. LOG s36-bl.

- [ ] **BUG-3** right-click image context menu: s34 E2E confirmed Replace/Paint/Crop work + submenu visibility fixed; s36-ao live-checked the rest — `Image > Save` works (editor → `<book>_page_1.png`), menus filter correctly, and the silent null-bitmap returns now show a notification (fdcafde). No repro for the "non-image items dead" claim: those items are removed by design (link/comment/attachment). REMAINING: user's exact steps (file + item) to reproduce anything still dead.
- [ ] **BUG-4** Contrast × Trim Config ONE-SHOT vanish (first trim-config open after launch). Fix v2 (`EnsureContrastOverlayState` invariant + self-healing) deployed 2026-09-22 — awaiting user confirmation of the exact flow; `[contrast]` breadcrumbs in debug build. NOTE (s36-az): the "contrast parou de funcionar" report of 2026-10-25 was root-caused to the save-reload state wipe (FIXED — ReloadDocument now preserves extras; LOG s36-az); if BUG-4's one-shot vanish still reproduces after that fix, it is a separate flow.
- [ ] **Ctrl+click flashcard highlight opens List** — implemented s34-b (Canvas ctrl+click detects /T author TumatraPDF-Flashcard → FlashcardSidebarShow; turns flashcard mode on if off). USER live-test pending (needs a book with flashcard highlights).
- [ ] **s34-g user validation pending**: Arch Measure Radius + Ref Line UX, resize-enlarge on a real book, flicker-free drag confirm (shipped + E2E 2026-10-11).
      [x] ARCH/EDIT secondary toolbars white/blank buttons — FIXED (s36-bb/s36-ax lineage): root cause dmlib intercepts NM_CUSTOMDRAW before subclass chain (LOG s36-bb: only CDDS_POSTPAINT reaches ReBarWndProc); missing TBSTYLE_EX_MIXEDBUTTONs; fix replicated Flashcard pattern — dedicated rebar subclass (ToolbarArchEditRebarSubclassProc, ID 42) installed AFTER dmlib (LIFO→first) handles NM_CUSTOMDRAW wipe+redraw in ITEMPOSTPAINT, TBSTYLE_EX_MIXEDBUTTONs both toolbars, removed conflicting SetWindowSubclass(ReBarWndProc) ID 0; build 0/0; user-verified dark theme + full labels. LOG s36-bb/s36-ax/s36-bl.

## Profile Memory

Precisamos armazenar toda a informação de leitura e flashcards do usuário em um mesmo arquivo, esse arquivo precisa ser exportável, sincronizável e importável de outros lugares para caso o usuário transfira para outro computador.

Vamos explorar as possibilidades de compatibilização com o sync do Google Drive, ou seja, eu salvo a minha base de dados de perfil `profile_data` em uma basta synced pelo google drive, e quero sincronizar essa mesma base de dados em outro PC que usa a mesma conta do google drive, precisamos explorar como o google drive trabalha o arquivo quando chegam atualizações desde a nuvem ou quando envia, e mapear um processamento para não corromper os arquivos e funcionar corretamente;

Outro elemento, calcular a possibilidade e etapas para importar o `profile_data` desde o `TumatraPDF_Portable`→`TumatraPDF_Install`;

Arquivo json de memória pode ter um prefixo indetificador: "tumatraPDF-data_"[numero de referencia, se necessario]

- Gap s36-bf: ProfImportLegacy first-run-only — reconciliation of reading-stats.txt/flashcard-stats.txt day rows at startup pending (32reading days from legacy sidecars can go missing; recovery done manually 2026-10-31: added 20261017/20261019 rows to tumatraPDF-data_1.json).

Quando aconteça de falhar a internet e o google drive ficar indisponível temporariamente, precisamos armazenar as novas informações localmente até que volte a conexão e o app substitua o arquivo antigo do drive pela versão mais recente. O sistema do backup também será importante por isso. Vamos criar um backup local a cada +-8 horas e armazenar localmente 9 backups, o botão de recuperar sessão (backup recover) deve dar uma lista de sessões de backup com data e hora para permitir recuperar. Botão 'import' obviamente deve substituir o data local.

Caso eu selecione uma pasta destino para a 'data' e já exista um 'data' anterior nessa pasta, queremos que apareça uma opção: 'sobrescrever' ou 'usar data existente', ou algum mecanismo que fique claro essa distinção.

Podemos incluir na 'home', do lado direito do títuo "Stats", um botão `🔳 Sync` (símbolo + nome, pode escolher um símbolo que combine com o conceito). Clicar no botão abre a janela (ou similar) do flashcard/settings. Obviamente faltam informações nessa janela, podemos completar para deixar mais informativa e trabalhar melhor o visual dos botões e dos campos;

### Plan (s36 detail — F3/F4/F5)

#### F3 — Drive-safe file handling (pre-sync hardening)

Preconditions (verified):

- profile_data file already carries version + lastWriteTs header fields — verify before use (ProfileData.h/.cpp).
- Atomic local write via temp + MoveFileExW already exists — keep; never write directly to the synced path.
- Local 9 × 8h rolling backups under <profileDir>/backup/ + ProfileBackupWnd (9-slot dialog, 5s hold + progress) already exist via F2.
- legacy sidecar import (ProfImportLegacy) runs first-run only — gap fixed by startup reconcile pass below.

[x] Verify version + lastWriteTs exist in file header. (s36-bh/bi/bj)
[x] Startup load: treat {empty file, partial-JSON, stale lock, Drive placeholder stub} as corrupt → fall back to newest backup under backup/ instead of overwriting the synced path. (s36-bh/bi/bj)
[x] Startup reconcile pass: compare sidecars (reading-stats.txt, flashcard-stats.txt) + profile, adopt missing day rows (fixes s36-bf gap too). (s36-bh/bi/bj)
[x] On detected cloud-side revert (lastWriteTs older than newest local backup × N), offer restore prompt using F2 ProfileBackupWnd dialog. (s36-bh/bi/bj)
[x] Keep atomic write-via-temp+MoveFileExW for local saves; Drive-for-desktop streams/placeholders never receive direct writes. (s36-bh/bi/bj)

Trap/risk: two PCs writing simultaneously → Drive creates "conflicted copy" duplicates, never merges. Plan: never delete conflicted copies; on next load merge by newer lastWriteTs + per-machine day rows already sum-safe ({"DESKTOP-…": sec} attribution). Offline tolerance: keep working locally, push changes when Drive reconnects; local backups remain source of truth for restore.

Verify: kill-restore test with real instance (-appdata does NOT load settings for -for-testing instance), then manual file swap test (simulate cloud revert + corrupt placeholder).

#### F4 — profileDataDir setter (the actual switch)

[x] gen-settings already auto-added profileDataDir — no gen-code change needed. (s36-bi)
[x] Add cmd-line path or UI to point profileDataDir at a Drive-synced folder. (s36-bi)
[x] On change: migrate current profile + its backup/ dir, adopt tumatraPDF-data_<numero> naming. (s36-bi)
[x] Initial-load conflict UX: if a profile already exists in dest dir → dialog "sobrescrever" (overwrite) vs "usar data existente" (use existing), per user desiderata. (s36-bi)

Trap/risk: pointing at an existing dir with stale data can silently clobber user data — overwrite must be explicit opt-in, not default.

Verify: two -appdata-like profiles, alternating dirs; confirm backup/ migrates with the profile.

#### F5 — Portable→Install import

[x] Locate legacy dirs: Portable install dir pattern vs installed %AppData% layout — verify actual layout first (do not assume paths). (s36-bj)
[x] One-time import wizard or auto-detect on first run of Install build; reuse ProfImport path (books merge newest-wins proven — 2026-10-31 manual merge of 3 legacy json, zero loss). (s36-bj)
[x] Import replaces local data (user desiderata). (s36-bj)

Trap/risk: Portable path assumptions wrong → import reads empty/missing dirs; must verify layout before shipping.

Verify: copy real Portable data to temp, run import, diff state counts.

### Gap do roadmap original (sessão correta re-exportada 2026-11-01) — itens NÃO cobertos pelo F3/F4/F5 atual

- [x] **F6 — Home botão `🔁 Sync` + janela Profile** (roadmap original F3): botão à direita do título `Stats` na Home; abre janela Profile (molde Config): campos = pasta destino (profileDataDir), nome/versão arquivo, tamanho, última gravação, máquina, nº livros/cards/dias, dica pasta Drive; botões = Salvar Agora (ProfileDataSaveNow), Exportar, Importar, Abrir Pasta, Backups (reusa CmdProfileBackups dialog). + nota: único export atualmente órfão=ProfileDataBackupCount → wire aqui. — DELIVERED s36-bn (+ repair rounds s36-br..)
- [x] **F7 — Export/Import UI + checksum** (roadmap original F4 UX + F1 spec incompleta): — DELIVERED: (a) checksum s36-bp, (b) Export/Import buttons s36-bq, (c) Replace/Merge/Cancel prompt s36-bq
      (a) **checksum no header** do tumatraPDF-data_1.json (spec F1 original tinha version/machine/lastWriteMs/checksum; header atual só version/lastWriteTs/machine) = base p/ detecção de corrupção no Drive;
      (b) Exportar = copiar perfil (primary + opcionais loops de backup) p/ pasta escolhida — caminho natural p/ Drive (user escolhe pasta synced);
      (c) Import wizard + diálogo sobrescrever-vs-usar-existente quando destino já tem dados (opt-in explícito).
- [ ] **F8 — Endgame centralização** (transição dual-write → profile é fonte única): migrar FlashcardStudyListDocs p/ iterar livros do perfil; deprecar write dual nos per-book *.json + sidecars QUANDO estável (janela Config flashcard continua lendo fallback legacy); manter per-machine day rows p/ multi-PC.
  - [x] s36-bs slice1: live stats ticks → profile (ProfileDataAddReadingSec/AddFlashcardSec/AddFlashcardCard + dual-write gate kProfDualWriteSidecars); ad-hoc test registered in before-release.ts (verified s36-bs).
  - [x] s36-bt slice2: Config flashcard list merges profile-only books (FlashcardStudyListDocs merges gProfile.books rows with no json, dedup by docPath); ConfigClearSelected → ProfileDataDeleteBook(path); ConfigClearAll → ProfileDataClearStudyDocs(); ad-hoc test in before-release.ts (verified s36-bt).
- [x] NOTA s36-bm: F3 slices 1-3 (reconcile/conflicted-merge/cloud-revert), F4 setter+migração e F5 import Portable→Install silencioso JÁ implementados (branch fix/secondary-toolbars, s36-bf..s36-bj, committed (s36-bm follow-up commits)). Roadmap original consulta: sessão exportada session-3a61b9f2 (Temp).

- **P1-migration (verified s36-bo, degrades perDoc):** `gFileStateInfo` (bloco de nomes em Settings.h ~1700) está SEM "FlashcardStudyTimeSec" (44 campos vs 43 nomes; campos de índice ≥23 deslocados: flashcardCardsDone, invertColors, contrastEnabled ... archScaleStates). Round-trip consistente, mas arquivos PRE-s36-at carregam valores nos campos ERRADOS → `ProfPullHistory` (ProfileData.cpp:1277) snapshot `flashcardStudyTimeSec`/`flashcardCardsDone` errados no perDoc do perfil para usuários que fizeram upgrade com settings antigas. Fix planejado: (1) adicionar `FlashcardStudyTimeSec` ao FIM da tabela FileState em `cmd/gen-settings.ts`; (2) `bun cmd/gen-code.ts`; (3) migration em Settings.cpp LoadSettings() p/ version < 37 deslocando os campos de volta uma posição antes de deserializar. NOTA: campos readingTimeSec (índice 22) NÃO afetados.

## Tab system

Vamos incluir do lado direito do botão principal no topo da janela, antes de iniciar as abas (a esquerda das abas dos documentos), um botão "Tab Group", a ideia é melhorar e deixar mais dinâmico o Tab group que já existe; Ao clicar, deve exibir como dropdown a lista dos 'Tab groups' já existentes, ao clicar em algum deles, fecha todas os documentos atuais e reabre todos os documentos do tab group selecionado; ao abrir outro documento dentro de um tab group aberto, deve sincronizar automaticamente (salvar) para proximas sessões;
abrir o app sempre restaura o último tab group usado pelo usuário; se não houver, abrir `new`;

'Ctrl+n' abre uma janela com um tab group `New`, ou seja, não sincronizado; nesse caso, se o usuário quiser salvar essa sessão como uma tab group, vamos incluir um botão na lista dropdown `Save`, ao clicar abre um editor/input de texto para nomear o grupo, imediatamente depois disso, o grupo deve sincronizar automaticamente ao fechar ou incluir documentos na sessão; arrastar um documento para fora de uma sessão automaticamente o exclui desse group aberto e inclui em um tab group `new` não sincronizado;

- [ ] Open (user action): TAB SYSTEM — all dev done s36-bu→s36-co (LOG), awaiting user revalidation on real book. Build 0/0; ad-hoc-tab-session.ts + ad-hoc-tab-session-e2e.ts + ad-hoc-tab-menu.ts PASS.

- [ ] FUTURE (user spec 2026-11-01): per-session reading/flashcard totals on Home tab — sum sessions' aggregated reading/flashcard time per group, independent of books; BIG feature, plan separately.

## Sumatra → Tumatra — labels UI (DONE)

Renomeados "Sumatra" → "Tumatra" em strings visíveis ao usuário (labels/títulos/menus):

- [x] `src/SumatraPDF.cpp` — `"SumatraPDF - Printers"`, `"SumatraPDF Documentation"`, `_TRA("SumatraPDF crashed")` (x2), corpo da mensagem crash
- [x] `src/SumatraDialogs.cpp` — `_TRA("SumatraPDF Options")`
- [x] `src/Flags.cpp` — `_TRA("SumatraPDF - Show Printers")`
- [x] `src/Installer.cpp` — `L"SumatraPDF"` (título janela, x2)
- [x] `src/SumatraStartup.cpp` — `L"SumatraPDF"`, `L"SumatraPDF installer"`, `L"SumatraPDF installer usage"`
- [x] `src/MainWindowCreate.cpp` — comentário + `StrL("SumatraPDF is no longer the default app...")`
- [x] `src/Commands.cpp` — `"Help: SumatraPDF Website"`, `"Help: About SumatraPDF..."`, `"SumatraPDF File Picker"`

## Sumatra → Tumatra — itens não-label (revisão profunda pendente)

Itens que envolvem nomes de arquivo .exe, classes de janela internas, registry/CLSID, instalador/uninstaller, testes de path/CLI, recursos .rc — requerem revisão case-by-case antes de alterar (não são labels de UI diretos):

- [ ] `src/Installer.cpp:866,890` — `StrL("SumatraPDF.exe.copy")`, `StrL("SumatraPDF.exe")` (nomes de arquivo no instalador)
- [ ] `src/Uninstaller.cpp:51,63,372,386` — `"SumatraPDF.exe"`, `"SumatraPDF-settings.txt"`, `"Sumatra-Uninstaller.exe"` (arquivos de desinstalação)
- [ ] `src/RegistryPreview.cpp:194,210` — `StrL("SumatraPDF.exe")`, `StrL("SumatraPDF-data")` (caminhos de registry/preview)
- [ ] `src/base/tests/File_ut.cpp` — `"SumatraPDF.exe"`, `"SumatraPDF"` (asserts de teste de path)
- [ ] `src/SumatraUnitTests.cpp` — `"SumatraPDF.exe -bench ..."` (testes de linha de comando)
- [ ] `src/SumatraPDF.rc:76-80` — arquivo de ícone `SumatraPDF-smaller.ico` (recurso)
- [ ] `src/TestPlugin.cpp` / `tools/plugin-test.cpp` — `L"SumatraPDF Plugin Test"` (nome do plugin de teste)
- [ ] Classes de janela internas (manter por compatibilidade Win32, avaliar caso a caso):
  - `src/base/Win.cpp:1906` — `L"SumatraPDFClipboardOwner"`
  - `src/wingui/Wnd.cpp:66` — `L"SumatraWgDefaultWinClass"`
  - `src/previewer/PdfPreview.cpp:372` — `L"SumatraPDF_PreviewPane"`
  - `src/ScreenshotCapture.cpp:25` — `L"SumatraScreenshotOverlay"`
  - `src/SelectionToolbar.cpp:35` — `L"SumatraSelectionToolbar"`
  - `src/RefHoverInternal.h:9` — `L"SumatraPDFRefHover"`
- [ ] Registry/CLSID/DDE/user-agent (internos do Windows, avaliar impacto de registro/associação de arquivos):
  - `src/RegistrySearchFilter.cpp` — `"SumatraPDF IFilter"`, etc.
  - `src/SearchAndDDE.h` — `kSumatraDdeServer L"SUMATRA"`, structs `SumatraOpen*`
  - `src/base/Http_win.cpp:62` / `Http_mac.cpp:53` — user-agent `SumatraPdfHTTP`
  - `src/SumatraLog.cpp:13` / `SumatraLog_posix.cpp:9` — `gLogAppName = StrL("SumatraPDF")`
  - `src/SumatraPDF.h` — funções `SumatraLaunchBrowser`, `WndProcSumatraFrame`, `GetSumatraDataDirTemp`, etc.

## Vertical_Toolbar (PARKED — DO NOT DO)

Idea frozen by the user ("NAO FAZER") — kept for reference only: when the window is narrower than 1/3 of the screen, adapt the toolbar to a left vertical bar (buttons top-to-bottom, converted to symbols with name on hover), keeping a top horizontal bar only for numeric inputs (page, timer) and remaining buttons; when home=on keep hiding all buttons.
