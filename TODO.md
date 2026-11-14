# TODO

1. Open work ONLY.
2. Completed features/fixes live in [LOG.md](LOG.md) (per-session) + [CHANGELOG.md](CHANGELOG.md) (per-release).
3. User manual-test items = items only the user can validate.
4. Housekeeping (2026-10-20, s36-ae; re-triaged s36-at, s36-cu/cv): all `[x]` blocks archived — details live in LOG per session entry; this file lists open work only.

## Execution queue (effort/gain)

User-locked order:

1. **Bookmarks Improve** (spec pointer → `## Bookmarks Improve` below).
2. **Edit/Align** (spec pointer → `## Edit/` below, PT verbatim).
3. **Page Counting Format** (spec pointer → `## Page Counting Format` below).

4. USER validation queue: capture tool Shift+S (s39, documented §Capture tool), Repair Document File (s42, steps in its section below), Profile Sync dark cascade (s40: import/cloud-revert windows), TOC Reorder toggle crash fix (LOG s54-b: selected bookmark + click items + Reorder on real build/machine).

## Lista vertical de abas

Do lado esquerdo do botão minimizar, vamos incluir um botão `🧾`, a ideia é exibir a lista de abas abertas em formato de lista vertical, ou seja:

```
Aba1
Aba2
Aba3
```

Dessa forma melhorando a leitura dos nomes dos arquivos grandes. Clicar na aba da lista redireciona a aba ativa para a aba clicada.

## OTHER FEATURES

[ ] - Incluir no settings una opção para desabilitar o botão `scan mode` do toolbar; garantir a adaptação dos outros botões no espaço ao fazer isso;
[ ] - Revisar README para constar todas novas funcionalidades, mapear tudo pelo LOG; aproveitar e caveman-compress os conteúdos para ficarem bem objetivos, baixa verbosidade;

## Performance optimizations (verified 2026-11-04, evidence LOG s38)

Source: external agent's perf map (archived TEMP/performance-code-arch.dot), claims verified against code (LOG s38). Safest first:

- [ ] **OPT-7** coordinate-space guards (START): `#if isDebug` asserts in `DisplayModel::CvtToScreen/CvtFromScreen` + TC2 colX consistency check (debug-only, no behavior change; prerequisite for OPT-3). STATUS: in progress s38-o1.
- [ ] **OPT-6** perf regression tests: cache hit rate, RelayoutFrame frequency, trim/crop toggle latency; ad-hoc tests, fail on >5% regression. STATUS: baseline tests shipped (startup/cache-behavior/toggle-latency); CI fail-gate metrics (cache hit rate, relayout frequency) need new C++ control cmds — residual open.
- [x] **OPT-5** WebView2 memory budget flags: cap Chromium baseline (shared env already exists — WebView.cpp gSharedEnvironment). STATUS: flags shipped [--js-flags=--max-old-space-size=256, --disk-cache-size=33554432] at WebView.cpp:1922-1938 AdditionalBrowserArguments (user verify pending).
- [ ] (DEFER) OPT-1 adaptive cache cap — after OPT-7; needs memory accounting + tests.
- [ ] (DEFER) OPT-3 partial trim-cache invalidate — real gain (Full FreeForDisplayModel storm, RenderCache.cpp:426) but stale-tile risk on rect.y shift; needs stamping/versioning.
- [x] (REJECTED) OPT-2 true LRU — hash-rejection reasons sound (no STL base/, compaction invalidates positions).
- [x] (REJECTED) OPT-4 TC2 tile dedup — premise wrong: left/right render different page halves (colX shift RenderCache.cpp:347-368); dedup would show wrong half.

## Findings status — audited 2026-11-04

### Closed (verified)

- Toolbar critical ordering — all 4 toolbars strip theme before `TbAddButtons` (`Toolbar.cpp:2907/:3076`, `FlashcardToolbar.cpp:465`, `Commands_Edit.cpp:126`). s36-ax lineage.
- Dark Mode Epoch legacy path dead — `ToolbarApplyThemeToRebar` (`Toolbar.cpp:3181`) uncalled; epoch bumped by `ApplyThemeColorsToDarkMode` (`Theme.cpp`).
- ASan `build-asan.ts` targets `TumatraPDF.sln` correctly.
- Settings names-blob migration fixed (s36-br, commit `6df3dc3`).
- dmlib rebar interception fixed (`ToolbarArchEditRebarSubclassProc` + `TBSTYLE_EX_MIXEDBUTTONs`, user-verified s36-bb lineage).
- Flashcard annotation author-token rule → AGENTS.md §12 (2026-10-18).
- TC2 un-routed engine-> sites all verified physical, zero edits — Commands_Edit.cpp:726, Commands_Flashcard.cpp:324, RenderCache.cpp:506, RenderCache.cpp:1212 (LOG s37-a).
- WebView2 adapter — complete: `docs/md/WebView2-Adapter-Pattern.md` + `TestMarkdownAdapter=55` wired in `cmd/control.ts`/`SumatraControl.cpp` (LOG s37-c/s37-d); read-side residual noted (no mutation path exercised).

### Open (live)

- Coordinate space mismatch — convention: `DisplayModel::CvtToScreen/CvtFromScreen` = PAGE ↔ CANVAS-CLIENT; `VirtWnd` `ev.pt` = LOCAL vs `ctx.bounds` = ROOT-relative; E2E posted coords may arrive DPI-virtualized ×1.25. Rule: before any hit-test, name the space of BOTH operands (local/root/client/page). Mixed = bug.
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

## Memory Performance Optimization (from performance.dot analysis 2026-11-04)

**Arquivo de referência:** `TEMP/performance-code-arch.dot` — mapa completo de arquitetura de memória, CPU hotspots, custos de ferramentas, profiling tooling.

### Gargalos Críticos Identificados

| #   | Componente              | Consumo                        | Causa Raiz                                                                                                                    |
| --- | ----------------------- | ------------------------------ | ----------------------------------------------------------------------------------------------------------------------------- |
| 1   | **RenderCache tiles**   | 128 × 1–4 MB = **~512 MB máx** | Cache primário; Gdiplus::Bitmap 32bpp PARGB; TC2 dobra chaves (virtual pages left/right); Trim/Crop invalidam cache no toggle |
| 2   | **MuPDF `fz_context`**  | 10–50 MB/engine                | Allocator/store/device cache + page cache; per-thread pool clona contextos                                                    |
| 3   | **WebView2 (Markdown)** | 100–300 MB processo separado   | Chromium isolado; Contrast/auto-scroll usam JS/CSS overlay                                                                    |
| 4   | **GDI+ bitmaps órfãos** | Vaza se cleanup falhar         | Ref-count em `PutTile`/`DropCacheEntry`/`BlitPixmap`; `darkModeEpoch` invalidação em massa                                    |

### Impacto das Ferramentas do Toolbar no Consumo

| Ferramenta        | Efeito Memória                                                                      | CPU                                 |
| ----------------- | ----------------------------------------------------------------------------------- | ----------------------------------- |
| **Contrast**      | PDF: layered child (WS_EX_LAYERED) ~0; MD: CSS filter 0                             | Baixo                               |
| **Crop v2 (TC2)** | Dobra virtual pages → chaves cache duplicadas left/right; `colGap` overlap          | Médio (roteamento virtual↔physical) |
| **Trim**          | `ShouldCacheRendering=true`; `FreeForDisplayModel()` no toggle → re-render completo | Alto no toggle                      |
| **Flashcards**    | MuPDF highlight annots + opaque mask GDI+ alpha blend; JSON per-doc                 | Baixo                               |
| **Arch Tools**    | Vetores page coords (pequeno); SVG export aloca temp                                | Baixo                               |
| **AutoScroll**    | Timer tick → cached blit ~1ms/frame; ETA overlay layered window                     | 60fps target = 16.6ms budget        |
| **Tabs**          | `VirtWnd` por aba; Home pinned idx 0 → ≥2 tabs/doc; tab groups live-sync            | Baixo                               |

### Pontos de Otimização Direta (para agente futuro)

- [ ] **Reduzir `MAX_BITMAPS_CACHED` (128 fixo)** → tornar adaptativo por VRAM/working set
- [ ] **Cache eviction policy** — hoje newest-first O(1) hit; LRU real exigiria hash table (rejeitado: sem STL, compaction, darkModeEpoch)
- [ ] **Trim/Crop cache preservation** — evitar `FreeForDisplayModel()` full no toggle; invalidar só tiles afetados
- [ ] **TC2 cache sharing** — left/right columns compartilham mediabox físico; deduplicar tiles idênticos
- [ ] **WebView2 memory budget** — limitar processos Chromium; `--memory-pressure-off` flags
- [ ] **Perf regression tests** — `ad-hoc-profile-live-writes.ts` já existe; adicionar métricas cache hit rate, RelayoutFrame freq, working set delta

### Relações Críticas (ver TEMP/performance-code-arch.dot)

- `RenderCache` → `fz_pixmap` → `Gdiplus::Bitmap` → `BlitPixmap` (cached path ~1ms)
- `darkModeEpoch` invalida cache em massa → re-render storm
- `TrimConfigDialog` TEMP TRIM: snapshot + force OFF enquanto dialog aberto → cache livre mid-dialog
- `AutoScroll` usa cached blit; non-cached = 23ms SYNC engine render
- Coordinate space mismatches (PAGE/VIRTUAL/CANVAS-CLIENT/SCREEN) = bug class recorrente

---

## Edit/

### Crop Align

a reorientação não deve aparecer somente quando salvar, isso deve ser reajustado instantaneamente (visualmente falando), o save só ESCREVE a mudança no arquivo original; nos queremos o seguinte comportamento:

1. página está inclinada "/---/"
2. Clico `Crop Align`
3. Ativa o cursor 'x' → clico e arrasto OU clico em dois pontos da página, desenhando uma linha (visualemente similar ao MEASURE ou SCALE)
4. App endireita a página "|---|" baseado no angulo da linha desenhada, recortando a página na mesma proporção original
5. acende o 🔴 para sinalizar que o PDF tem alterações;

Leia novamente os informativos do projeto e memórias lemma antes de seguir delegando;

veja como funciona o alinhamento dessa página: https://easyprotools.com/jpg/jpg-orientation-editor/
a ideia é recortar a página de forma similar;

###

- [x] Crop — done (prior rounds).
- [x] Align R1 — DONE s50-R1 (2026-11-08): CmdArchAlign=525 dispatch case + handler toolMode=5 in Commands_ArchTools.cpp, alignLine struct + FinalizeAlign math in Canvas.cpp (axis=nearer, aspect-preserved cropRectPage, angle readout), toolbar button, ESC/doc-close clear; 3 `// TUMATRA-FORK(s50):` markers added to SumatraPDF.cpp; build 0 err/0 warn; probe honest limit — mode toggles, posted-coords line not visually verified (DPI ×1.25 trap). PNGs tests/tmp/s50/align-base.png + align-probe.png. LOG s50-R1. lemma fragment saved.
- [x] Align R2 — DONE s50-R2-Z (2026-11-08): headless TestGeomSave=65 control cmd (QueuePageGeometry + EngineMupdfSaveUpdated to outPath, no window), ad-hoc-align-save.ts PASS (queues 90° rotation+crop on page 1, saves, reopens, extract-text OK). /Rotate honest limit: only multiples of 90°, small angles (5°) round to 0 silently. LOG s50-R2. lemma saved.
- [x] Align R3 — DONE s50-R3 (2026-11-08): arbitrary-angle deskew at save via Form XObject (Technique A). EngineMupdf.cpp ApplyPendingGeometryEdits split: 90° multiples → /Rotate (R2 unchanged); else → pdf_new_xobject with CTM = T(newBboxCenter) * R(angleDeg) * T(-cropW/2, -cropH/2), XObject content `q -cropX0 -cropY0 cm <orig> Q`, page contents `q /FmN Do Q`, merged Resources, MediaBox/CropBox = rotated bbox. /Rotate unchanged. Exports: pdf_page_contents, pdf_add_object_drop. Test: ad-hoc-align-deskew.ts 5° PASS. Regressions: align-save, bookmark-edit/persist/reorder, repair-replace PASS. LOG s50-R3. lemma saved.
- [x] Edit toolbar "..." residue — DONE s50-R5 (2026-11-08): investigated Commands_Edit.cpp EditToolbarCreate gEditToolbarButtons — FIRST entry {CmdEditAddText, _TRN("+Text")} is a real wired command (CmdEditAddText -> CmdCreateAnnotFreeText). grep of Toolbar.cpp/Commands_Edit.cpp/MainWindow defaults found no "…"/"…" text placeholder or icon-less text item. Live probe (full-frame PrintWindow PNG, Edit open + Crop Align toggled) confirms NO separate "…" button — residue absent; first Edit item = "+Text" (visual "+Te..." narrow truncation, not debug residue). Nothing to remove/relabel. Crop Align present on Edit toolbar. Probe v2 tests/tmp/s50r5/probe-crop-align.ts PASS: fabricated valid 2-page PDF verified loaded (Ping + title) before UI, TestToolbarButtons control cmd (no enumChildWindows hang), PNG at tests/tmp/s50r5-probe/s50r5-probe-frame.png. Build 0 err/0 warn. Honest limit: Edit toolbar confirmed visually via PNG only (control-pipe text dump covers main toolbar hwndToolbar).
- [x] Crop Align checked-state sync + +Text truncation — DONE s50-R6 (2026-11-08): (1) Crop Align button on Edit toolbar did not reflect ON state visually. Root cause: HandleCmdArchAlign only called UpdateToolbar2State (syncs Arch toolbar), not Edit toolbar. Fix: added ToolbarUpdateStateForWindow(win, false) in HandleCmdArchAlign (Commands_ArchTools.cpp:113) to sync Edit toolbar checked state from archTools.toolMode==5. NOTE: this fixed checked-state SYNC only, not the instant toggle-off (that is s50-R8 below). (2) "+Text" rendered truncated "+Te...". Root cause: EnforceToolbarTextButtonWidths only cleared BTNS_AUTOSIZE for Autoscroll button; other text buttons retained BTNS_AUTOSIZE so comctl overrode explicit width on later layout passes. Fix: clear BTNS_AUTOSIZE for ALL text buttons when pinning explicit cx (Toolbar.cpp:1390-1408). Build 0 err/0 warn.
- [x] Crop Align instant-toggle-off (double-dispatch) — DONE s50-R8 (2026-11-08): single click on Edit toolbar "Crop Align" appeared ON then instantly OFF (toolMode 0→5→0, cannot draw). Root cause: rapid double-dispatch of CmdArchAlign (two WM_COMMAND posts <150ms apart) flips toolMode 0→5→0 at HandleCmdArchAlign; confirmed by probe tests/tmp/s50r8/double.ts. Double-post source not isolated (Edit vs Arch toolbar creation/subclass paths near-identical; dmlib third-party). Fix: idempotency guard in HandleCmdArchAlign — second CmdArchAlign within ~100ms of first with no canvas interaction in between is ignored (toolMode stays 5); timestamps alignLastToggleMs/alignCanvasInteractMs added to MainWindow.h ArchToolsState; Canvas mouse-down/move log [arch] + stamp alignCanvasInteractMs when toolMode==5 to prove draw state survives. Build 0 err/0 warn (clang-format applied); probe tests/tmp/s50r8/single.ts (stays ON ≥2s) PASS; probe tests/tmp/s50r8/double.ts (exactly ONE outcome = ON) PASS; ad-hoc-align-save.ts + ad-hoc-align-deskew.ts PASS. LOG s50-R8. USER must validate live on real machine.
- [x] Crop Align instant visual deskew (apply-at-finalize) — DONE s50-R9 (2026-11-10): deskew now applies INSTANTLY when user finalizes alignment line (click+drag or two clicks). Page rights itself visually ("/---/" → "|---|") in live view without waiting for Ctrl+S. Canvas.cpp FinalizeAlign: after QueuePageGeometry, calls EngineMupdf::ApplyPendingGeometryEditsNow() (new virtual in EngineBase/EngineMupdf) to mutate live PDF doc, then dm->BuildPagesInfo() + dm->RelayoutKeepingView() to rebuild page geometry and refresh view preserving scroll anchor (pattern from ApplyViewportCropV2). EngineMupdfSaveUpdated checks appliedInMemory flag — if true, skips ApplyPendingGeometryEdits (anti-double-apply) and goes straight to pdf_save_document; resets appliedInMemory on success. Aspect ratio preserved (mediabox W:H) per existing FinalizeAlign crop math (lines 2925-2934) — matches jpg-orientation-editor straighten+crop behavior. Zoom clamp via RelayoutKeepingView preserves virtual zoom + view anchor. Guard widened 100ms→500ms (already kAlignToggleGuardMs=500). All [arch] logs use ArchElapsedMs() relative-ms format. Build 0 err/0 warn; ad-hoc-align-deskew.ts (5°) PASS; ad-hoc-align-save.ts (90°) PASS; ad-hoc-bookmark-edit.ts (dirty funnel regression) PASS. LOG s50-R9. USER re-test: click Crop Align → draw line → page rights instantly + 🔴 lights → Ctrl+S writes change (no notification UI).

## Bookmarks Improve — shipped [v1] s43-b (2026-11-06)

Done s43-a (engine + UI shell):

- Button 'Add' at TOP of TOC panel: creates "Page N" bookmark at end of list for CURRENT page (TocAddBookmark → EngineMupdf::OutlineEdit add → tocAdded overlay).
- Right-Click context menu: 'Delete' (item+subtree), 'Update' (clicked bookmark pageNo := current page), 'Rename' (noop placeholder → s43-b).
- Engine edit layer: EngineBase::HasUnsavedOutline() + OutlineEdit(op,...); modifiedOutline flag + tocAdded overlay; GetToc shows overlay even on outline-less PDFs; dirty state mirrors annotation pipeline → EngineMupdfSaveUpdated triggers Ctrl+S/close-save flow; resets on save/doc-close/FinishLoading.
- Commands CmdBookmarkAdd=530/Delete=531/Update=532/Rename=533 via gen-commands.ts+gen-code; gated via ctx.hasToc.
- Tests: ad-hoc-bookmark-edit.ts (2-page outline-less PDF: add → TOC contains entry → dirty=1), registered in before-release.ts. Build 0 err/0 warn; probe PASS.

Done s43-b (reorder + rename + persist):

- Right-Click 'Rename': real modal dialog (ShowBookmarkRenameDialog, local to TableOfContents.cpp) → OutlineEdit op=4 rename → tocTree invalidate + dirty. CmdBookmarkRename=533 wired.
- Drag-reorder on TOC tree (WndProcTocTree subclass): click-hold past drag threshold arms drag; drop ON item body → 1 indent level deeper (child of target); drop on far-left strip → remove indent level (sibling at target's level); dynamic horizontal WHITE drop-guide line (dragGuideY) painted during drag; WM_LBUTTONUP copies `isDragging` local before ReleaseCapture (TabsCtrl capture-trap). Keyboard tree nav untouched.
- Outline PDF-write persist: EngineMupdf::outlineEdits serialized edit log replayed into PDF via pdf_new_outline_iterator + fz_outline_iterator_insert/update/delete at save (EngineMupdfSaveUpdated). libsumatrapdf.def exports the iterator symbols so PdfFilter/PdfPreview build 0/0 (s43-a blocker resolved). Ctrl+S / close-prompt save writes bookmarks to disk; reopen shows them. Verified E2E: add 2 → save → reopen → outline present.
- TestBookmarkSave=62 control command; tests/ad-hoc-bookmark-persist.ts (add → save → reopen → assert persisted) registered in before-release.ts. PASS.
- Build 0 err/0 warn all 3 targets (TumatraPDF, PdfFilter, PdfPreview). ad-hoc-bookmark-edit.ts PASS (regression).

Done s43-c (reorder/indent persist on disk):

- op=5 reorder-move now written to PDF: PersistOutlineEdits deletes source(targetTitle) then re-inserts under newParent (append as last child) / before moveBefore sibling (far-left unindent) / top-level end via fz_outline_iterator vtable; OutlineEditEntry + OutlineEdit signature carry moveBeforePageNo/moveBeforeTitle; TableOfContents::MoveTocItem passes farLeft target as moveBefore.
- TestBookmarkMove=63 control cmd (calls same OutlineEdit(5,...) path the drag uses — bypasses posted mouse, honest test-only).
- tests/ad-hoc-bookmark-reorder.ts: add 3 → reorder+indent → dirty=1 → TestBookmarkSave → dirty=0 → TestGetToc on saved file shows Chapter A(d0)/Chapter C(d1)/Chapter B(d0) persisted + reopen. Registered in before-release.ts. PASS.
- Build 0 err/0 warn all 3 targets (TumatraPDF, PdfFilter, PdfPreview). Regressions ad-hoc-bookmark-edit.ts + ad-hoc-repair-replace.ts PASS.

Remaining: **FIXED s56 (2026-11-12)** + **FIXED s57 (2026-11-12)** + **FIXED s58 (2026-11-12)** + **FIXED s59 (2026-11-12)**:
- s56: TOC bookmark cascade (Add/Reorder/Delete/Rename/Update all "dead") — cached tocTree session source of truth; OutlineEdit no longer deletes tree; callers mutate cached tree directly. LOG s56.
- s57: TOC bookmark add silent-death — `ReloadTocTree` choke-point `str::Dup` of selTitle (TempStr stored raw → invalid free after temp-arena reset); `CmdBookmarkDelete` now `FreeTocItemRec` the unlinked item; `MoveTocItem` passes `source->title` directly. Build 0/0; regressions PASS (issue-2799, ad-hoc-bookmark-edit/reorder/persist, ad-hoc-toc-reorder-crash, ad-hoc-repair-replace). LOG s57.
- s58: TOC bookmark delete heap corruption (root cause: `UnlinkTocItem` wrong-root param `root->child` + immediate free while TreeView/async referenced + restore re-selected deleted title) → survivor captured before unlink, correct root, free DEFERRED via `ReloadTocTaskData::itemToFree`; `CmdBookmarkRename` entry logging added; drag-reorder guide level: `WM_LBUTTONUP` uses stored `win->tocDragFarLeft` (matches painted guide), `WM_PAINT` draws FULL width for sibling drop (`farLeft=true`) vs indented HALF line (one tree level `DpiScale(hwnd,16)`) for child drop (`farLeft=false`). `ReloadTocTree` aligned to single 4-arg overload (defaulted `itemToFree=nullptr`). Build 0/0; regressions PASS (issue-2799, ad-hoc-bookmark-edit/reorder/persist, ad-hoc-toc-reorder-crash, ad-hoc-repair-replace); headless smoke alive ≥5s/step + clean exit. LOG s58.
- s59: TOC bookmark P1-P4 fix:
  - P1 Rename broken (3 bugs): (a) `str::Free(dti->title)` on potentially arena-allocated title → leak-replace fix (dies with tree); (b) `OutlineEdit(4)` called AFTER mutation → targetTitle==newValue; fix: capture oldTitle BEFORE, call OutlineEdit(oldTitle), THEN mutate; (c) Dialog Enter key ignored (not real dialog) → handle WM_KEYDOWN VK_RETURN/ESCAPE in RenameDialogWndProc; added instrumentation log `st.ok=%d newName.len=%d`.
  - P2 New bookmark dark-theme colors: `TocAddBookmark` hardcoded `color=0` (black) → `kColorUnset` (theme default) so custom draw uses theme colors in dark AND light.
  - P3 Reorder guide line position (child drop): `WM_MOUSEMOVE` set `newGuideY = rc.y` (top) for ALL drops; child drop (`farLeft=false`) must render at BOTTOM edge (`rc.y + rc.dy`) — a child is appended AFTER the hovered item. Fix: `newGuideY = newFarLeft ? rc.y : rc.y + rc.dy`.
  - P4 Dirty indicator (🔴) missing after outline edits: `EngineMupdfHasUnsavedAnnotations` only checked annotations+pageGeometry, not `modifiedOutline`; fix: add `|| epdf->modifiedOutline` so tab red dot lights on any bookmark add/delete/rename/update/move.
  - Build 0/0; regressions PASS (issue-2799, ad-hoc-bookmark-edit/reorder/persist, ad-hoc-toc-reorder-crash, ad-hoc-repair-replace). Deploy MD5 `442D52C8E85A9F3B616424D070E03D83`.
- s60: TOC bookmark rename Enter dead + dark theme; save-replay ghost bookmark:
  - P1 Rename: `IsDialogMessageW` did not route `VK_RETURN`→`WM_COMMAND IDOK` to dialog (focus in edit) → `st.ok` never set → ok-path dead. Fix: subclass EDIT (`RenameEditWndProc`) forwards `VK_RETURN`→`WM_COMMAND IDOK`, `VK_ESCAPE`→`WM_COMMAND IDCANCEL`; drop `IsDialogMessageW` from loop (plain `TranslateMessage`/`DispatchMessage`); keep `WM_COMMAND` IDOK/IDCANCEL + `WM_KEYDOWN` as belt-and-suspenders; added `logf "[toc] CmdBookmarkRename: RESULT ...`; dark theme via `ApplyDarkModeToPopupWindow` + `WM_CTLCOLOREDIT`/`WM_CTLCOLORSTATIC` theme brush. (Supersedes s59 P1(c) WM_KEYDOWN-only approach which did not fire.)
  - P2 Ghost bookmark: `CmdBookmarkUpdate` → `OutlineEdit(3,...,StrL(""),...)` empty newValue made `PersistOutlineEdits` case 3 write `wi.title` empty + `wi.uri=nullptr` → empty-title pageNo=-1 ghost written to file (came back after delete). Fix: case 3 = deliberate no-op skip (page dest not repointable via outline iterator; tocTree holds new pageNo); log skipped op; case 1/4/5 skip empty titles; case 5 null-target move = move to END (top-level append) matching in-memory `MoveTocItem(target=null)`; replay never creates empty-title entries and matches in-memory result. No repair of already-corrupted user files this round.
  - Build 0/0; regressions PASS (issue-2799, ad-hoc-bookmark-edit/reorder/persist, ad-hoc-toc-reorder-crash, ad-hoc-repair-replace); headless ad-hoc-s60-bookmark-replay PASS (no empty-title entries, count correct). Deploy MD5 `38A0139FB2B389D7B691CF3992C1E15D`.
- s61: Rename newName empty root cause; rename popup dark theme ordering; ghost replay hardening:
  - P1 Rename RESULT newName.len=0: zero-init `buf[512]` -> `len(buf)`==0 -> `GetWindowTextW(...,0)` capacity 0 -> empty capture (s60's routing fix reached the handler but captured nothing). Fix: explicit `dimofi(buf)` capacity, `wstr::Dup` heap copy (was dangling stack-view WStr), `wstr::Free` at fn end; `IDOK/VK_RETURN rawEditLen` logs.
  - P2 Rename popup WHITE in dark: `ApplyDarkModeToPopupWindow` called BEFORE children existed (dmlib themes only present children). Fix: call AFTER child create; popup handles `WM_ERASEBKGND` + `WM_CTLCOLORDLG`; CTLCOL* brush cache keyed on theme color.
  - P3 Ghost replay: op=2 empty-target early skip; op=5 re-insert never writes empty title (`mvItem.title` null -> skip+log); `OutlineIteratorFind` nullptr on empty titles.
  - Build 0/0; regressions PASS (issue-2799, ad-hoc-bookmark-edit/reorder/persist, ad-hoc-repair-replace, ad-hoc-s60-bookmark-replay); ad-hoc-toc-reorder-crash 1st FAIL = flaky app-quit timeout, 2x PASS after (6.7s/7.9s), not regression. Deploy MD5 `891A5C85C2A1C7C1115943A25C9392C5`.
- s62: Ghost bookmark deletable/repairable (op=2 empty-target):
  - Root cause: s61 op=2 empty-target early-skip made pre-s60-corrupted empty-title ghost UNDELETABLE (`CmdBookmarkDelete title='' pageNo=-1` queued op=2, replay skipped it, ghost rewrote to file on every save).
  - Fix: `OutlineIteratorFindEmptyTitle` (null/whitespace-title walk); op=2 empty-target now deletes that entry at save (repairs corrupted files); logs removal; non-empty deletes unchanged. New control cmd `TestBookmarkDelete = 69` (i32 pageNo + string title, empty title allowed via `ArgAt` presence check).
  - Headless repro PASS (tests/tmp/s62/ad-hoc-s62-ghost-repair.ts): fixture ghost + Real One/Two (/Next//Prev sibling links mandatory); TestBookmarkDelete(-1,"") -> Save -> PyMuPDF `get_toc()` on saved file: ghost GONE, real entries intact. In-app dumps skip null-title items — file-level check is authoritative.
  - Build 0/0; regressions PASS (issue-2799, ad-hoc-bookmark-edit/reorder/persist, ad-hoc-repair-replace, ad-hoc-toc-reorder-crash 4.8s, ad-hoc-s60-bookmark-replay). Deploy MD5 `2BB91FB89D55E0039DDBEF94BA6A0D76`.
- User live validation on real build/machine: delete the ghost + Ctrl+S once on s62 build -> ghost must vanish permanently; rename Enter (s61, watch `IDOK rawEditLen`) + rename popup dark theme.

## Page Counting Format

The app already displays pages in other counting formats; we need a TOOL to change the PDFs so the file's page numbers match the real book numbers. Similar to Adobe: a tool to re-number page ranges, especially at the start of documents.

Example: pages 1-50 counted in roman, from page 51 on in decimal → page 51 becomes the new page 1. Typing 1 in the page field lands on physical page 51, making the book/page match exact. We can also create an ABSOLUTE page function to ignore cosmetic counting; in this example: `1` → page 51; `(1)` → page 1.

## Capture tool — shipped [x] v1 (s39)

Shift+S: draw a rectangle that captures the image inside it at the document's DPI → copy to clipboard.

Residuals (open): no content-pixel verify (clipboard bitmap size only in test), no save-to-file. Capture tool v1 complete: rect in page coords, RenderPageArgs Export at doc DPI, clipboard copy, auto-exit, ESC cancel, doc-close reset, command availability gate.

## Repair document file — shipped [x] v1 (s42, user validate pending)

Context menu (right-click on document) → "Repair Document File" (below Show Errors): engine-corrected in-place save.
Mechanism: serialize healed copy to `<original>.repair-tmp` same dir → REOPEN temp and verify error count (parent tree / Optional Content / layer config clear; total must not increase) → atomic `MoveFileExW(REPLACE_EXISTING)` swap (Commands_Edit.cpp:1096) → tab reloads in-place keeping page+scroll. Fail/locked file = MessageBox, original untouched, temp cleaned. NO duplicates left behind.
Menu gated on: PDF + engine has recorded errors.
Fixable: structural (xref, parent tree, OC/layers) — cleared by write options (garbage+clean+compress). NOT fixable: `items left on stack` (unbalanced q/Q content stream — page stays broken; regenerate at source).
USER TEST steps: (1) open a PDF flagged "document with errors" (footer badge); (2) right-click → Repair Document File; (3) expect: flag gone on corrected doc, same filename, no `-corrected`/`.repair-tmp` leftovers; (4) reopen file → still opens fine, warnings absent. If badge persists → report which warnings survive (Show Errors).
Evidence: tests/ad-hoc-repair-replace.ts (damaged PDF on the fly: load errors → repair → 0 errors, no leftovers) PASS architect-rerun. LOG s42.

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

## Home page responsiveness — shipped s63 (2026-11-13, LOG s63)

- [x] HOME = single vertical scroll flow: sections (Stats hdr + ⟳ Sync → stats band → books hdr + view icons → search → thumbs/list) anchor to one origin shifted by `-homePageScrollY`; nothing hidden at short heights (old Stats-on-books overlap gone); tip = fixed footer; search HWND moves with flow + `SW_HIDE` when out of view; scrollbar range = total content height; scroll clamped.
- [x] Flow painted over fixed tip footer (books header over "Tip: ..." at short heights): chrome pass clipped to tip-band top (GDI region); tip paints separately unclipped on top; flow elements fully below the tip → `Visibility::Collapse` (hit-test).
- [x] Scroll-only layout-cache fast path removed (`ApplyHomeLayoutCache` et al.) — chrome desynced from thumbnails on scroll; full layout every paint now.
- [x] Empty-history HOME no longer force-left (x=50) — content centers horizontally at all sizes ("muito espaço vazio do lado direito do graph").
- [x] "Heatmap overflows right edge" = probe artifact (DPI-virtualized bun captures) — setProcessDpiAware() added to scratch probes; band fits everywhere; month labels thin by design.
- [ ] USER validation on real history: scroll depth (`-for-testing` has 0 files → `maxScrollY ≈ 0`; deep scroll UNTESTED); per-size check empty vs populated.
- [ ] Item D — toolbar overflow/clips at narrow widths (pre-existing): NOT addressed.

# Bugs open

[ ] Click bookmarks + bookmark editing tools missing; want a top bar with the edit buttons. [s51-d] click-nav FIXED: WndProcTocTree WM_LBUTTONDOWN gated behind tocReorderMode (default OFF -> pass-through to TreeView = selection + click-nav restored); ON -> drag-reorder intercept. Reorder button = ON/OFF toggle (accent fill when ON), default OFF. Top bar restyle (Add+Reorder) DONE s51-e: native ReBar+Toolbar strip above "Bookmarks" label; Add (CmdBookmarkAdd 531) + Reorder (9001 BTNS_CHECK, 3px ThemeWindowTextColor underline when ON); labels full, no truncation; dark+light theme safe (no RGB literals). Rename/Update/Delete remain right-click-menu ONLY. Build 0/0; regressions issue-2799 + ad-hoc-bookmark-edit/persist/reorder PASS. s51-f visual verification PASS (png1 dark OFF, png2 dark ON underline, png3 context menu intact incl Delete bookmark, png4 light ON). Probe skipped (no control-pipe sidebar/toggle command) -> USER live validation pending (keep [ ]). s52 crash FIXED: clicking bookmark Add/Reorder crashed via TOC custom-draw use-after-free (freed TocItem* as tree lParam after OutlineEdit deleted tocTree). Guard every consumer via IsTocItemInCurrentTree + immediate ReloadTocTree after OutlineEdit (TocAddBookmark/MoveTocItem); build 0/0; tests ad-hoc-bookmark-edit/reorder/persist + issue-2799 PASS -> USER live validation pending (tests/tmp/s52/user-crash-log.txt) (keep [ ]). s53 UI wedge FIXED: async debounced ReloadTocTree (tocReloadPending/tocReloading flags + TaskReloadTocTree uitask) + tocReloading guard in TocTreeSelectionChanged + O(1) ownerTree validity check (EngineBase.h TocItem::ownerTree + SetTocTreeOwner) eliminates O(n²) paint storm on large TOCs; build 0/0; tests issue-2799 + ad-hoc-bookmark-edit/reorder/persist PASS -> fix shipped s55-b (ReloadTocTree capture-before-OutlineEdit use-after-free), USER live validation pending (keep [ ]).

[ ] Session Tabs: sessions not updating the book list correctly — created a new session, opened some books, then closed them, but closed books were not correctly removed from the session; — [s45] Tabs.cpp MainWindowTabClosed pre-syncs active group to 0 files on last-tab close; build 0/0, ad-hoc-tab-session(-sync/-e2e) PASS — awaiting USER validation (keep [ ]). [s46] audited: 7 latent sync bugs fixed (RemoveTab choke point + suppression guard + MoveTab pinned guard + drag/reorder sync + boot-restore SaveSettings + SyncActiveGroupToEmpty suppression); #8 missing-file-at-restore = pending design decision (skip vs error).

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
