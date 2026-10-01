# TODO

## Public Repo + Update System Plan (2026-10-01) — IN EXECUTION

Goal: make repo public-ready, strip upstream Sumatra data-collection, add own release + auto-update.
User decisions: keep ALL upstream build/CI scripts in `cmd/` as backup (future cross-platform builds); portable auto-replace update strategy (option A); audit git history first, decide rewrite later. Parallel agent works flashcards — DO NOT touch `Flashcard*`, `Commands_Flashcard.cpp`, flashcard LOG/TODO sections.

### FASE 0 — Git history audit (DONE 2026-10-01)

- [x] Secrets: none committed. `cmd/build-ci.ts` reads `R2_SECRET`/`BB_SECRET` from `../secrets/` (outside repo).
- [x] No `C:\Users\Testa` paths in current tree or tracked files.
- [x] `D:\1 Principal` + old FLOW labels only inside **historical** `FLOW/*.dot` blobs (commits d57d7f3, 0f548cd...). No credentials anywhere. Current tree clean.
- [ ] USER DECISION PENDING: publish with history as-is (recommended — cosmetic paths only) vs fresh-start squash.

### FASE 1 — Telemetry/data-collection removal (upstream Sumatra) — DONE s26

- [x] `CrashHandler.cpp`: `UploadCrashReport`+`kCrashHandlerServer*`+symbol download (`BuildSymbolsUrl`/`DownloadAndUnzipSymbols`/`ExtractSymbols`) removed; `_uploadDebugReport` forced local-only (`shouldUpload=false`); `CrashHandlerDownloadSymbols()` no-op. KEEP local dump + `ShowCrashHandlerMessage`.
- [x] `CmdContributeTranslation` removed (gen-commands + regen, Menu, SumatraPDF.cpp, CommandAvailability x3, docs/md/Commands.md).
- [x] `HomePage.cpp` rows -> TumatraPDF website/manual/issues.
- [x] `AdvancedSettingsDialog.cpp` `kSettingsDocsUrl` already -> repo docs.
- [x] `Installer.cpp` + `RegistryInstaller.cpp` URLs -> repo.
- [x] Comment URLs kept (attribution).

### FASE 2 — Own update system (UpdateCheck.cpp repoint + portable auto-replace) — DONE s26

- [x] `updateInfoURLs[]` -> `raw.githubusercontent.com/FelipeTesta/TumatraPDF/main/update.txt`; parser accepts `[TumatraPDF]` (and `[SumatraPDF]`) sections.
- [x] Signature gate replaced by PE-"MZ" + >=1 MiB sanity (unsigned portable exe accepted); `kExpectedDlHost` -> GitHub `/releases/`.
- [x] `SelfUpdateViaBatch()`: detached cmd batch waits for PID exit -> `move /Y` new exe -> relaunch -> self-delete.
- [x] `update.txt` at repo root (`Latest: 1.0` + `releases/latest/download/TumatraPDF-win64.exe`).
- [x] Fork version `1.0` via `src/BuildConfig.h` (upstream 3.7 abandoned; `Latest: 1.0.x` > 1.0 triggers).

### FASE 3 — Release pipeline (GitHub Actions) — DONE s26 (workflow, untested)

- [x] `.github/workflows/release.yml`: tag `v*` -> windows-2022 + bun + msbuild Release x64 -> bump+commit update.txt -> softprops/action-gh-release with TumatraPDF.exe.
- [ ] Flavor E2E after repo goes public: tag v1.0.0 -> verify release asset + update.txt bump -> in-app update prompt from v1.0 -> self-replace.
- [ ] Document in BUILD.md + docs/md/Version-history.md (at first public release).

### FASE 4 — Tests + docs — DONE s26/s26b

- [x] Build dbg+rel 0 errors; smoke launch OK. E2E: repo PUBLIC, v1.0.4 released via Actions (update.txt bumped by workflow), all CI deps fixed (NASM, WebView2 nuget, bin/ tools, EngineAll.h include).
- [x] LOG.md + README + BUILD.md + Version-history.md updated (TumatraPDF 1.0 section).
- [ ] Remaining polish: old stray local branches (master, feature/*) vs git-flow conventions; in-app update prompt manual validation from a released 1.0 build (daily throttle — set TimeOfLastUpdateCheck or wait).

---

## Architectural Audit + Refactoring Plan (2026-09-21)

Audit done with tree-sitter + explore agent over `sumatrapdf-src/src/`. Findings have file:line evidence. Process diagrams live in `FLOW/*.dot`.

### Findings

1. **TC2 column-shift math — 4 copies.** `RenderCache.cpp:354-359`, `RenderCache.cpp:1227-1233`, `DisplayModel.cpp:1735-1740` (CvtToScreen), `DisplayModel.cpp:1776-1782` (CvtFromScreen). -> helpers `ShiftToColumn` / `UnshiftFromColumn`.
2. **Trim rect math — 3 copies, sign-flip bug already hit once.** `RenderCache.cpp:327-336`, `RenderCache.cpp:1240-1250`, `DisplayModel.cpp:1461-1464`, inverse in `Selection.cpp:48`. -> `TrimRectPage` / `TrimRectInverse`.
3. **Trim-drag line math — 3 identical copies in Canvas.cpp** (`1645-1650`, `2090-2096`, `3632-3638`). -> `TrimDragComputeLines(win, dm, x, y, &topY, &botY)`.
4. **Contrast webview JS — 3 divergent copies** (`ContrastOverlay.cpp:126-141`, `SumatraPDF.cpp:11151-11161`, `MarkdownModel.cpp:435-446`). Gray math drifts between copies. -> single `MarkdownContrastJs(opacity, invert)` (owned by MarkdownModel).
5. **Layer violations:** `TrimConfigDialog.cpp:92,113` calls `GetEngine()->PageMediabox(CurrentPageNo())` **without VirtualToPhysical** — real bug under TC2 (even virtual pages). 9 sites in Canvas/RenderCache should use `DisplayModel::PageMediaBox()` (already does VirtualToPhysical + column split).
6. **Dead code:** `QuickToggleViewportCrop` v1 zero callers (`DisplayModel.cpp:343-355`); v1 crop stack parallel to v2 (`DisplayModel.h:340-349`, 6 fields); flashcard dead fields (`hwndCardCount`, `CmdFlashcardNext`, `studyModeType`, `history`); `kEtaW` stale in `ToolbarLayout.h:22`; `CmdCommandPaletteOnlyTabs` commented in `Accelerators.cpp:84` but docs stale.
7. **Bad coupling:** `Commands_AutoScroll.cpp:35-38` reads toolbar button state directly (`IsDlgButtonChecked`) — route via `SetToolbarButtonCheckedState`; `ContrastOverlay.cpp:119-141` reaches into webview JS (should live in MarkdownModel).
8. **God files:** `SumatraPDF.cpp` back to 14,231 lines (target ~10k); `Canvas.cpp` 5,652; `EngineMupdf.cpp` 6,689. 16C continues.

### Execution phases (effort x result) — status 2026-09-22

| Phase | Action                                                                                                                                                                                                                           | Files                                               | Effort | Risk                 | Gain                                      |
| ----- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------- | ------ | -------------------- | ----------------------------------------- |
| R1 ✅ | TrimConfigDialog VirtualToPhysical bug + `dm->PageMediaBox()`; BONUS: RenderCache::Invalidate same class also routed                                                                                                             | TrimConfigDialog, RenderCache                       | Low    | Low                  | High (latent bug) — DONE                  |
| R2 ✅ | Centralize `MarkdownContrastJs` (3 divergent copies killed) + PDF-parity filter semantics                                                                                                                                        | ContrastOverlay, MarkdownModel, SumatraPDF          | Low    | Low                  | High (kills drift + PDF/MD parity) — DONE |
| R3 ◐  | colX math unified → `DisplayModel::ColumnOffsetXForPage` (4 copies → 1). TrimRectPage/Inverse NOT done: site semantics differ (mediabox shrink vs area/pt shift + distinct guards), forcing helper = render-path risk, low value | DisplayModel, RenderCache                           | Medium | Medium (render path) | High (colX part done)                     |
| R4 ✅ | `TrimComputeLineInfo(TrimLineInfo)` (3 copies)                                                                                                                                                                                   | Canvas.cpp                                          | Low    | Low                  | Medium — DONE                             |
| R5 ◐  | Pruned: QuickToggleViewportCrop v1 (0 callers) + kEtaW. LEFT: v1 crop stack (v1 button still live), flashcard dead fields (flashcard agent ACTIVE)                                                                               | DisplayModel, Toolbar                               | Low    | Low                  | Medium                                    |
| R6 ◐  | Contrast JS → MarkdownModel done via R2; Commands_AutoScroll decouple NOT done                                                                                                                                                   | Commands_AutoScroll, ContrastOverlay, MarkdownModel | Low    | Low                  | Low-Medium                                |
| R7    | 16C continuation: SumatraPDF.cpp 14.2k -> ~10k or Canvas.cpp extraction                                                                                                                                                          | SumatraPDF.cpp                                      | High   | Medium-High          | High                                      |

### Fixes 2026-09-22 (user-reported)

- [x] **ETA .md stuck**: webview branch never recalculated ETA (only one-shot async eval at toggle); rAF loop now throttles `autoscrollProgress` notify ~10s → ETA keeps counting while scrolling (SetWebviewAutoScroll, AutoScroll.cpp).
- [x] **Contrast .md: drift eliminated, visual kept**: 3 divergent copies of contrast JS → single source `MarkdownContrastJs` (MarkdownModel). MD visual (body bg + gray text) INTENTIONALLY different from PDF veil — remains (visual parity attempt reverted by user request).
- [x] Deploy: `Compiled\TumatraPDF.exe` (rel64, 13:02) + `Compiled\TumatraPDF-debug.exe` (12:56).

---

## Two Columns v2 (Crop View) — MIGRATION PLAN (in progress)

### Goal

Create a **second mode** "Two Column v2" (`CmdViewportCropV2Toggle`) **fully independent** of the existing one (`CmdViewportCropToggle`), so v1 can later be removed without conflicts. v2 implements the cropview concept: each half of the page becomes a **"virtual page" stacked vertically** (`1L → 1R → 2L → 2R → ...`), full screen width, so autoscroll flows as continuous text **without jumping**.

### Current state (v1)

- v1 = viewport manipulation (`ApplyViewportCrop` in `DisplayModel.cpp:250`): ×2 zoom + horizontal shift (`viewPort.x`) between columns. `PageCount()` = N physical, **no duplication**.
- Central bug: in `ScrollYBy()` (`DisplayModel.cpp:2165`) at the end of the left column it does `viewPort.y = colTop` (snap to the top of the right one) = the **jump** described in this TODO.
- Autoscroll → `MoveDocBy` (`WindowTab.cpp:169`) → `ScrollYBy(dy,false)` → same jump.
- Render: the non-cache path `RenderCache.cpp:1140-1195` already cuts a sub-rectangle via `RenderPageArgs(...,&area)` (`:1182`); cached uses `req.pageRect` (`RenderCache.cpp:984`). `EngineMupdf::RenderPage` (`:5024`) cuts by `pageRect`. **Base ready for columns.**

### User decisions

1. **Separate** "Two Column 2" button (does not replace v1).
2. v2 **as independent as possible** (make removing v1 later easy).
3. Plan **as detailed as possible**.

### Design — v2 independence

- New fields in `DisplayModel`: `viewportCropV2Enabled`, `viewportCropV2QuickToggled` (do NOT reuse v1 fields).
- New methods: `ApplyViewportCropV2()`, `QuickToggleViewportCropV2()`, `VirtualToPhysical()`, `ColumnOfVirtual()`, `VirtualPageCount()`.
- New command `CmdViewportCropV2Toggle` (independent of `CmdViewportCropToggle`).
- v1 and v2 **mutually exclusive** (turning one on turns the other off).
- Render: cut the column via `pageRect`/`area` + `physicalPageNo`; cache key by **virtual pageNo** (resolves left/right without touching `BitmapCacheEntry`).

### Affected files

`DisplayModel.{h,cpp}`, `DocumentLayout.{h,cpp}`, `RenderCache.{h,cpp}`, `Canvas.cpp`, `SumatraPDF.cpp`, `cmd/gen-commands.ts`, `Toolbar.cpp`, `Menu.cpp`, `CommandAvailability.cpp`, `Accelerators.cpp`. (v2 settings: reuse `ViewportCrop` — margins belong to the PDF, not the mode.)

### Phases (each phase: `bun cmd/build.ts` 0 err/0 warn + clang-format + manual smoke `-for-testing`)

#### FASE 0 — Parallel infra (command + state + button) — IN PROGRESS

- [x] `cmd/gen-commands.ts`: added `CmdViewportCropV2Toggle` (before `CmdNone`). → `Commands.h: CmdViewportCropV2Toggle = 512`.
- [x] `bun cmd/gen-code.ts`: Commands.h/Commands.cpp regenerated. _(The `cl` not-found error at the end is only in the virt-keys part; ignore, see Accelerators in FASE 5)_.
- [x] `DisplayModel.h`: added fields `viewportCropV2Enabled`, `viewportCropV2QuickToggled` + methods `ApplyViewportCropV2()`, `QuickToggleViewportCropV2()`, `VirtualToPhysical()`, `ColumnOfVirtual()`, `VirtualPageCount()`.
- [x] `DisplayModel.cpp`: implement `VirtualPageCount()` (2N if v2 on & continuous, else N) and helpers `VirtualToPhysical`/`ColumnOfVirtual` (`:112-131`).
- [x] `SumatraPDF.cpp`: handler `case CmdViewportCropV2Toggle:` (`:11177`, cloned from v1, mutual exclusion, committed).
- [x] `Toolbar.cpp`: new button `{TbIcon::Text, CmdViewportCropV2Toggle, _TRN("Two Column 2")}` (`:106`) + `BTNS_CHECK` (`:395`).
- [x] `Menu.cpp`: new menu item (`:631`).
- [x] `CommandAvailability.cpp`: gating (`:232`,`:262`).
- [x] Build + test: toggle on/off **with no visual change** (v2 still visual-off). ✅ FASE 0 complete (committed in fc50439).

#### FASE 1 — Duplicate layout to 2N

- [x] `DisplayModel::PageCount()` (`:102`): returns `VirtualPageCount()` (2N if v2 on & continuous; else N). `ValidPageNo` accepts 1..VirtualPageCount.
- [x] `ValidPageNo` (`:150`): accept virtual (1..VirtualPageCount()). ✅
- [x] `Relayout()` (`:1277`): `layout.Reset(VirtualPageCount())`, iterate 2N filling `physicalPageNo=VirtualToPhysical(v)`, `cropColumn=ColumnOfVirtual(v)`, mediaBox/zoomReal of the physical page.
- [x] `BuildPagesInfo()` (`:738`): allocates `VirtualPageCount()` and is now re-callable (frees the existing array) — needed when v2 switches N↔2N.
- [x] `CopyDocumentLayoutToPageInfo` (`:685`): copies `physicalPageNo`+`cropColumn`.
- [x] `DocumentLayoutPage` (`DocumentLayout.h:13`): + `int physicalPageNo` + `int cropColumn = -1`.
- [x] `PageInfo` (`DisplayModel.h:22`): + `int physicalPageNo` + `int cropColumn = -1`.
- [x] Engine routing: `PageMediaBox`, `PageSizeAfterRotation`, `GetContentBox`, `CvtToScreen`/`CvtFromScreen`, `ZoomRealFromVirtualForPage` (contentBox loop), `ScrollTo` → `VirtualToPhysical(pageNo)`.
- [x] Render: `ShouldCacheRendering` = false when v2 (forces non-cache, like trim); `RenderCache::Paint` maps `renderPageNo = dm->VirtualToPhysical(pageNo)`.
- [x] `ApplyViewportCropV2` now calls `BuildPagesInfo()` + `Relayout()` (toggling on/off rebuilds pagesInfo 2N/N). The OFF handler in SumatraPDF.cpp uses `ApplyViewportCropV2`.
- [x] `Canvas.cpp`: `PageMediabox` in trim-drag/erase/full-image → `VirtualToPhysical(pageNo)`.
- [x] Build 0 err/0 warn (75s) + clang-format + stable smoke (zlib.3.pdf 3 pages, no crash). ✅ FASE 1 complete — 2N stacked rows, column cut still missing.

#### FASE 2 — Column cut in the render ✅ COMPLETE

- [x] `DisplayModel::PageMediaBox` (`:604`): when `ColumnOfVirtual(pageNo)>=0`, cut the mediabox in half (`dx/=2`, `x+=dx` if right). The virtual page gets a half-width mediabox → Fit Width fills the screen with the column (user's central criterion: no black half).
- [x] Non-cache path `RenderCache.cpp:1162`: when `viewportCropV2Enabled && cropColumn>=0`, shift `area` by `colX = mb.x + (cropColumn==1 ? mb.dx/2 : 0)` (same pattern as the vertical trim shift) and render with `physicalPageNo`.
- [x] Cached path: MVP = force non-cache (already done in FASE 1 via `ShouldCacheRendering`=false). Evaluate perf in FASE 7.
- [x] Cache: virtual pageNo as key already works (left/right don't collide).
- [x] **Turning-off crash fix** (`EngineMupdf.cpp:4110` `pageNo>pageCount`): hit-test passed **virtual** pageNo (up to 2N) to the engine (which has N). Routed via `VirtualToPhysical` in `DisplayModel.cpp`: `GetElementAtPos` (:1682), `GetAnnotationAtPos` (:1699), `GetWidgetAtPos` (:1715), `IsOverText`/`HasTextForPage` (:1728), `GetTextInRegion` (:2494).
- [x] Build 0 err/0 warn (76s) + clang-format + manual test: v2 on/off **without crash**, returns to original state. Log `[v2] Paint pageNo=4 -> renderPageNo=2 cropColumn=1` confirms correct left/right column rendering. ✅ FASE 2 complete — column cut with full screen width. (Polish pending: see next session.)

#### FASE 3 — Continuous flow (no jump) — CENTRAL CRITERION

- [x] `ScrollYBy()` (`:2155-2198`): nothing to remove for v2 — snap `viewPort.y = colTop` and `colBottom - viewPort.dy` block live inside v1 block (`if (viewportCropEnabled ...)`). Mutual exclusion (v2 on ⇒ v1 off, SumatraPDF.cpp:11185) makes v2 bypass block, fall into natural `newYOff += dy` path (`:2262-2279`) scrolling 2N stacked pages vertically, no jump.
- [x] `GoToNextPage`/`GoToPrevPage` already virtual: `PageCount()`=VirtualPageCount, continuous uses `columns=1` → `FirstPageInARowNo` = pageNo, navigates `2k-1→2k→2k+1` naturally (`:2034-2040`, `:2080-2091`).
- [x] Autoscroll (`MoveDocBy`→`ScrollYBy`): continuous, no break.
- [x] **Acceptance test**: `autoscroll=on` + Two Column 2 reads `1L→1R→2L→2R` as single text, no jump. (Verified by code analysis; user manual test pending.)

> **FASE 3 note:** zero code change — stacked 2N architecture + mutual exclusion already delivers continuous flow. Build 0 err/0 warn.

#### FASE 4 — virtual↔physical conversions in engine ✅ COMPLETE

- [x] Decided: `PageCount()` returns **virtual** (layout/nav/UI), engine uses **physical**. Every boundary-crossing call-site routes.
- [x] `DisplayModel::PhysicalToVirtualForRect(physPageNo, rect, &rectOut)` (DisplayModel.cpp:133): physical k + rect → virtual 2k-1 (left) / 2k (right); non-v2 = identity. Rect stays in physical coords (colX shift applied in CvtToScreen).
- [x] `CvtToScreen`/`CvtFromScreen` now **column-aware**: same colX shift as render (`pt.x -= colX` / `+= colX`, mirroring `RenderCache::Paint`). Any virtual page + physical rect converts correctly.
- [x] `GetPageLabeTemp` converts `VirtualToPhysical` before engine (avoids 2N→N overrun).
- [x] Search/Selection (TextSel.pages[] is **physical**) — routed via `PhysicalToVirtualForRect` in:
  - `AppendTextSelScreenRects` (SearchAndDDE), `AppendPageRectsToScreen`, `ShowSearchResult` (GoToPage virtual), `FindTextOnThread` (CurrentPageNo→physical for FindFirst + 2-column visibility check), `FindMatchTouchesVisiblePages` (virtual span `[2s-1,2e]` with `bool v2`).
  - `SelectionOnPage::GetRect`, `FromRectangle` (iterates virtual `PageCount()`), `UpdateTextSelection`/`OnSelectAll`/`OnSelectionStart` (VirtualToPhysical for engine textSelection), `SelectionToolbar::GetSelectionEndPoint`.
  - `uia/TextRange`, `ReadAloudHighlight` (3 sites), `FormFields`, `SumatraPDF` zoom-to-selection.
- [x] `textSelection`/`textSearch` (engine-bound) receive **physical** at entry call-sites (StartAt/SelectUpTo/SelectWordAt via `VirtualToPhysical`).
- [x] Build 0 err/0 warn + smoke `-for-testing` (stable). Manual test pending: selection, search, links and correct clicks on 2L/2R.
- [ ] (pending) uia `PageProvider` and legacy tile math — non-cache path already diverts; acceptable for MVP.

#### FASE 5 — State, navigation & labels

- [x] **Shift-hold (quick toggle) v3** (`QuickToggleViewportCropV2`): `IsViewportCropV2Active()` = `viewportCropV2Enabled && !viewportCropV2QuickToggled && IsContinuous`. **Shift-hold PAUSES autoscroll** (free drag on full page; resumes on release). Quick view: enter = save virtual zoom + relayout `kZoomFitWidth` (full physical page fills width); exit = restore saved zoom; fractional anchor (frac × new height). Hook: `HandleV2ShiftHold` (checks `quickToggled`) + `WM_KEYDOWN`/`WM_KEYUP` for `VK_SHIFT` in frame WndProc. Both v2 toggles end with `ScrollYTo` (full pipeline — fixes "stuck until continuous off/on").
- [x] `ScrollState.page` — live session already virtual (GetScrollState/GoToPage operate in PageCount domain); real gap was persistence: `GetCurrentFileState` now saves **PHYSICAL** pageNo (`VirtualToPhysical(ss.page)`); session restore maps physical→virtual left column (`2p-1`) when v2 active (SumatraPDF.cpp restore). No 2N pageNo risk in non-v2 session.
- [x] NavigationModel/history: 2N pages — `DisplayModel::RemapNavHistoryForV2(bool)`: toggle ON maps each physical entry → virtual left column (`2p-1`); OFF → `(p+1)/2`; out-of-range entries drop; called in `CmdViewportCropV2Toggle` handler (Back/Forward point at same physical page). Quick-toggle (shift-hold) does NOT remap (transient).
- [x] Page labels: `GetPageLabeTemp` under v2 appends `L`/`R` (from `ColumnOfVirtual`) — toolbar shows "3L"/"3R"; `GetPageByLabel` accepts suffix ("Go to 3R" → right column of physical page 3; no suffix → left column). Applies to favorites/TOC/search (consistent labels).
- [x] `Accelerators.cpp`: **user decision 2026-09-22 — NO shortcut** (toolbar/menu only; shift-hold peek remains). `gBuiltInAccelerators` (Accelerators.cpp:17) is hand-editable table, outside gen-markers — adding later is 1 line if mind changes.
- [ ] Build + test: back/forward, history, coherent thumbnails — build OK (dbg+rel 0 err), smoke OK; **visual test pending (user)**: Back/Forward after v2 toggle; page edit "3R"; reopen doc saved with v2.
- [ ] **Finding 2026-09-22**: favorites saved under v2 store VIRTUAL pageNo (`win->currPageNo`) + label "3L" — clicking in a non-v2 session falls into ParseInt("3L")=3 (physical, tolerable) but virtual pageNo > N may no-op. Physical normalization pending at favorite creation (same class as FileState fix).

> **Autoscroll TC2 perf note (2026-09-18 → 2026-09-20):** v2 forced non-cache path (`ShouldCacheRendering`=false), limiting autoscroll speed (sync render per frame ~23ms). **✅ RESOLVED (2026-09-20)**: v2 cache re-enabled. Tile functions (`GetTileRectDevice/User/GetTileOnScreen`, RenderCache.cpp:308+) now take `DisplayModel*`, route `VirtualToPhysical` + cut mediabox per column; colX shift only at render (`GetTileRectUser`); render thread routes `req.pageNo`→physical. UI thread only blits ready tiles (~1ms). Cache key (virtual pageNo 2k/2k-1) already resolves left/right. Commit `feat(two-column-v2): re-enable render cache for TC2`. Autoscroll "MUCH better", only stutters at max speed (acceptable).

#### FASE 6 — Trim + R2L + colGap

- [x] **colGap/offset (column overlap)** (`ViewportCrop.colGap`): columns no longer cut flush at middle — each column shows a slice of the other half. DisplayModel helpers: `ViewportCropV2Gap()` (reads `gGlobalPrefs->viewportCrop.colGap`), `ViewportCropV2ColumnWidth(mb)=halfW+gap`, `ViewportCropV2RightColumnX(mb)=mb.x+halfW-gap`. Applied in: `PageMediaBox`, `GetTileRectDevice/User` (RenderCache), non-cache `Paint`, `CvtToScreen`/`CvtFromScreen`. Configurable in Trim Config dialog (new "column gap:" field).
- [x] **Trim** (`marginTrimEnabled`): integration verified — independent axes (column cut acts on dx/x, trim on dy/y) across all paths: layout (`DocumentLayout` cuts `pageSize.dy`), cache (`GetTileRectDevice/User`), non-cache `Paint` (crop.x + inflate cvp), `CvtToScreen/CvtFromScreen`. No ordering conflict. Visual combo test `TwoColumn2 + Trim` pending (user).
- [x] **R2L** (`displayR2L`): implemented 2026-09-22 — SINGLE flip in `ColumnOfVirtual` (DisplayModel.cpp): `int col = (v-1) % 2; return displayR2L ? 1 - col : col`. Everything derives from it (`PageMediaBox`, `ColumnOffsetXForPage` → render/hit-test). `PhysicalToVirtualForRect` same: PHYSICAL column (rectOut shift) unchanged, only virtual page number (`vCol`) flips. Toggle via `CmdToggleMangaMode` → `ToggleMangaModeInternal` (GetScrollState → Relayout → SetScrollState) preserves position.
- [ ] Build + test combos: `TwoColumn2 + Trim`, `TwoColumn2 + R2L` — build OK (dbg+rel 0 err), smoke OK; **visual test pending (user)**: enable TC2 + manga mode → first virtual column must show the RIGHT column.

#### FASE 7 — Polish & regressions

- [x] **v2 state persistence** (2026-09-23): `ViewportCrop.V2Enabled` setting (gen-settings.ts); `DisplayModel` ctor applies before `SetInitialViewSettings` (continuous doc starts at 2N; latent flag safe in non-continuous — conversions gate on `IsViewportCropV2Active`); toggle writes pref. "Reset on tab switch/close" n/a: flag is per-DisplayModel (per tab), no leak between tabs.
- [x] **Non-continuous modes** (2026-09-23): toggling v2 ON from non-continuous auto-switches to Continuous first (`SwitchToDisplayMode`) — anchor/rebuild/nav-remap all run in the right domain. Nav remap now gated on actual domain transition (latent toggle no longer remaps). Visual test by user pending.
- [x] **Unit tests + smoke** (2026-09-23): harness fixed (VS2026 detector → fallback, stale `SumatraPDF.sln` name → `TumatraPDF.sln`); smoke OK. ~~8 pre-existing failures~~ **RESOLVED same day (s13): locale decimal comma** — `_create_locale(LC_ALL, ".UTF-8")` inherited the user's OS locale (pt-BR → "3,45"); fix = `"C.UTF-8"` in StrUtf8.cpp + StrFormat_ut.cpp reference copy. **test_util now: Passed all 102,685.**
- [x] Final build + deploy `Compiled/TumatraPDF.exe` (release 19:54 + debug 19:46, 2026-09-23, post s13).

**Found-issues (2026-09-23):**

- [x] **premake TRAP FIXED (option b, s13)**: run-unit-tests.ts no longer runs `premake5.exe` (strips 10 fork sources → LNK1120). AGENTS §6b documents the trap + `git stash push -- sumatrapdf-src/vs2022/` recovery. Residual: if premake must run manually (adding files), re-add fork sources afterwards; premake5.lua update = MERGE surface, deferred.
- [x] **Unit test failures RESOLVED (s13)**: same root cause as above — locale, not code. All 102,685 pass.
- [x] **gen-settings coverage audit PASSED (s13)**: full regen → `git diff` Settings.h/Settings.cpp = empty → no hand-added fields outside the generator. Reusable check: regen + git diff (NOT static name analysis — generator has its own name-mapping table: "URL"→url, "AIChatSidebarDx").

### NOTES

- `cl.exe` (VS) not in PATH in this session: `bun cmd/gen-code.ts` fails only at virt-keys generation (`Accelerators.cpp`). To regenerate Commands.h/Commands.cpp, run gen-code and ignore the `cl` error (enum/arrays already written before).
- Forcing non-cache in v2 is acceptable for MVP (same premise as trim), but may re-render while scrolling — evaluate perf in FASE 2/7.

---

## Two Columns (Crop View)

- The button must behave as follows:

**Crop view OFF:**

```
| ? full screen width or user zoom ? |
|           page 1 (original)             |
|    left column | right column         |
|                 page 2                  |
|    left column | right column         |
```

**Crop view ON:**

```
| ? full screen width ? |
|        page 1           |
|      left column        |
|         page 1          |
|       right column      |
|        page 2           |
|       left column       |
|        page 2           |
|       right column      |
```

**Current problem:** when "touching" the bottom edge of the page, the app automatically jumps to the top right, leaving no time to read the text fluidly as a continuation.

**Solution:** the next rendered page must be (1) the same page again focused on the right half, then (2) the next page focused on the left half. This way, with autoscroll=on, the text flows continuously without jumps, as if it were single-column text.

## Margin

Margin configuration:

```
______________
| left margin | top margin | right margin |
| left margin | COLUMN 1 | column gap | COLUMN 2 | right margin |
| left margin | bottom margin | right margin |
______________
```

When `trim=on`, it must crop these pixels from the zoom, optimizing screen usage.

## TRIM

Develop TRIM functions without affecting CROP for now.

Two buttons: `[Trim]` (on/off) and `[Trim Config]`:

- Opens a window with "margin-top" / "margin-bottom" buttons
- Clicking either one shows a 2px horizontal red line with 0px distance from top/bottom
- The line can be clicked and dragged down/up
- Dragging automatically computes the distance from the top to the new line position
- Clicking ✓ saves the distances

**Status:** ✅ IMPLEMENTED (2026-08-15) — clip-based top/bottom elimination + Trim Config dialog (margin-top/margin-bottom buttons, draggable red 2px line, ✓ save). Left/right/column-gap NOT implemented (user decision).

## AUTOSCROLL TIMER

Add a checkbox to the toolbar, right after autoscroll: `[✓ Timer | Numeric Input]`.

**Idea:** when timer=✓, autoscroll turns itself off after N minutes (numeric input — default 30min).

## ARCH TOOLS (Scale + Measure)

New "Arch Tools" tab in the left sidebar (new tab next to Contents/Favorites).

Two functions, INDEPENDENT PER DOCUMENT (each tab/doc keeps its own state):

### Scale

- Enable Scale mode
- Select an existing VECTOR LINE (hit-test on segments extracted from the PDF) OR draw a line (2 clicks) with SNAP on vector points/endpoints
- Enter the real measurement of that line (e.g. "5 m") in an input
- Computes scale factor = pageLength / realMeasure
- The drawing "adopts the scale" in the document (per-document scale, anchored on the drawn line)

### Measure

- Requires a defined scale (otherwise, warn to define Scale)
- Click 2x to draw a temporary line (with SNAP)
- Computes (via scale): width (x = |pageX|), height (y = |pageY|), linear length (√(x²+y²))
- Shows readout (x/y/length) next to the line + list in the tab

### Technical

- Vector extraction: custom fz_device (stroke_path) collecting segments per page (page coords) → cache in DisplayModel/EngineMupdf
- SNAP: nearest endpoint within threshold (screen px)
- Coordinates: convert screen↔page via DisplayModel (CvtScreenToPage/CvtPageToScreen) → measurements in page units, zoom-independent
- Per-doc state: FileState (archScaleFactor, archScaleAnchorX/Y, archUnit, archScaleSet) + MainWindow (mode, points, measurement list)
- Overlay: draw in OnPaintDocument (TrimConfigDialog/ContrastOverlay pattern) with GDI+
- Persistence: UpdateTabFileDisplayStateForTab / SetDisplayState

**Status:** ✅ IN TESTING (2026-08-28) — Phases 1-4 build-verified (infra+shell, vector extraction+SNAP, overlay+interaction+Scale/Measure math, FileState persistence+polish); awaiting user runtime validation (interactive Scale/Measure).

**Refinements:**

- Phase 5: floating Scale window, click-click+drag, Measure table + Clear lines — awaiting runtime validation
- Phase 5b: automatic [arch] logging + Draw line bug fix (arm mode, do not pre-set archDragLine=1) — awaiting runtime validation
- Phase 6: [Clear lines] visible, measure label on scale line, crosshair cursor while drawing, Esc cancels mode, minidump-on-crash (sumatrapdfcrash.dmp) — awaiting runtime validation
- Phase 6b: fixed OnOk crash (atof null) — validate drawn line + float>0 before atof — awaiting runtime validation
- Phase 7: UI redesign — sidebar removed, 'Arch Tools' toolbar button opens 2nd toolbar (Scale/Measure/Reset Scale); lines visible only with Arch Tools=on; erase mode (hold 'e' = red cursor + click deletes single line); flicker fix — awaiting runtime validation
- Modularity: gated by global pref archToolsEnabled (default on); off → zero UI + zero processing cost
- Phase 8: Scale dialog activates existing line + shows real length in edit; label bg measures text (no leak); erase 'e' works with Arch Tools on (red cursor + overlay); 2nd bar has 'Clear lines' (CmdArchClear) + 'Reset Scale' (scale-only) — awaiting runtime validation
- Phase 9: 'Clear lines' clears only measurements (not scale); erase 'e' works with Arch Tools on (outside draw-mode gate); 'e' cursor without black background (red overlay on canvas) — awaiting runtime validation
- Phase 10: simplified Scale — no line clicking; type new measure + OK recomputes scale (uses existing line); drawing a new line replaces the previous one; pre-fill current length in dialog — awaiting runtime validation
- Phase 11: scale editing works (OnOk uses archScaleSet; toggle does not clear archScaleLineDefined); erase 'e' hit-tests stored lines directly (not PDF segment); white underline draws for active toggle (no gAnyToggleChecked early-out) — awaiting runtime validation
- Phase 12: fixed OnOk regression (archScaleLineDefined||archScaleSet guard, allowed drawing new scale line); fixed inverted Measure underline (removed BTNS_CHECK auto-toggle, state managed by UpdateToolbar2State); renamed 'Limpar linhas'→'Clean lines' — awaiting runtime validation
- Phase 13: canonical scale factor (meters) + label respects current unit + remember last unit (ArchUnit) + configurable decimal separator (ArchDecimalSeparator, default ',') + Shift constrain horizontal/vertical — awaiting runtime validation
- Phase 13b: scale and measurements independent per document (HashMap archMeasurementsByDoc, save/restore on tab switch, discarded on close) — awaiting runtime validation

## Phase 15 — Autoscroll Timer (BUILD OK)

- [x] `cmd/gen-commands.ts`: add `CmdAutoScrollTimerToggle = 494`, `CmdAutoScrollTimerEdit = 495`; run `bun cmd/gen-commands.ts`
- [x] Check `cmd/gen-settings.ts` has `AutoScrollTimerMinutes` + `AutoScrollTimerEnabled`; regen `Settings.h` if needed
- [x] Rebuild (0 err / 0 warn)
- [x] Deploy `out\dbg64\TumatraPDF.exe` → `Compiled\TumatraPDF.exe`
- [x] Smoke test launch (`-for-testing -console`)
- [ ] UI check: `[checkbox][Timer:][input]` before Autoscroll; timer stops autoscroll after N min

## Phase 15 Fixes (BUILD OK)

- [x] Autoscroll speed clamp 0.1f → 0.008f (AutoScroll.cpp:74)
- [x] Remember last speed: FileState persistence + defaults 0.0167f → 0.008f (MainWindow.h, SumatraPDF.cpp, gen-settings.ts)
- [x] Timer visuals: design tokens kCtrlGapX/kCtrlH + TimerInfoId 130 + slot→r bug fix (Toolbar.cpp)
- [x] Build fix: revert build.ts:37 `..\vs2022` → `vs2022\TumatraPDF.sln`; build 0 err/0 warn; deploy Compiled\TumatraPDF.exe; clean smoke
- [x] BUILD.md created (build method documented separately)
- [ ] Practical UI test by user (tomorrow): timer control, minimum speed, persistence, alignment

## Phase 16 — Modularization (refactoring, baseline commit 968b4ec)

### 16A — Toolbar.cpp design tokens (low risk, visual payoff)

- [x] Replace ad-hoc DpiScale(4/8/10/12/50/70/110) with named tokens (kCtrlGapX/kCtrlH exist; add kPadX, kLabelW, kSlotW)
- [x] Fix hardcoded 18px checkbox → kCtrlH token
- [x] Fix inconsistent gaps (2/20/22/64px) between checkbox/label/edit
- [x] Build + smoke test

<!-- 16A DONE 2026-08-30: tokens kPadX/kGapX/kTextAnchor/kPagePad/kEtaW/kSpeedSlotW/kSlotW/kLabelW/kToolbarPadY/kToolbarPadY2 added; checkbox 18px→kCtrlH; build 0/0, deploy, smoke clean. -->

### 16B — MainWindow.h structs (medium risk)

- [x] Group ~274 members into domain structs (AutoScrollState, ArchToolsState, TimerState)
- [x] Build + smoke test

### 16C — Split SumatraPDF.cpp (12,743 lines, high value, incremental)

- [x] C1: Map handler clusters (grep `case Cmd` + static helpers per domain)
- [x] C2: Extract AutoScrollCommands (smallest, well-known)
- [x] C3: Extract ArchToolsCommands
- [x] C4: Extract ViewCommands
- [x] C5: Extract FileCommands → 24 HandleCmdXxx wrappers in SumatraPDF.cpp, dispatch converted, Commands_File.cpp stubs cleaned (2026-09-01)
- [x] Build + smoke test after EACH extraction (dispatch stays in SumatraPDF.cpp)

### 16D — Typed gGlobalPrefs accessors (last)

- [ ] Evaluate need (152 accesses in SumatraPDF.cpp) — only with tests

<!-- 16B DONE 2026-08-30: AutoScrollState + ArchToolsState structs created in MainWindow.h; ~90% of .cpp refs renamed via mechanical PowerShell; bare identifiers in measurement functions fixed with this->; build 0 err/0 warn, deploy, smoke clean. -->
<!-- 16C-2 DONE 2026-08-30: Markdown contrast (light bg + dark gray text), Arch Tools hidden for .md, AutoScroll commands extracted → Commands_AutoScroll.{h,cpp}; build 0 err/0 warn, deploy, smoke clean. -->
<!-- 16C-C5 DONE 2026-09-01: 24 HandleCmdXxx wrappers in SumatraPDF.cpp, dispatch converted, stubs cleared, build 0/0 -->

---

## Effort x Result Analysis — Next Steps (2026-09-01)

### Decision Matrix

| Action                                                         | Effort                                                        | Result                                       | Risk                                     | ROI   | Priority |
| -------------------------------------------------------------- | ------------------------------------------------------------- | -------------------------------------------- | ---------------------------------------- | ----- | -------- |
| **16C-F6: Extract init/window creation from SumatraPDF.cpp**   | Medium (C2-C5 pattern already established)                    | High (-800~1200 lines, god file ~10k)        | Medium (static deps, but proven pattern) | ????? | 1        |
| **Canvas.cpp: Extract event handling**                         | High (5091 lines, mouse/keyboard interleaved with rendering)  | High (2nd largest file)                      | High (rendering code, no pattern)        | ????? | 2        |
| **16D: Encapsulate gGlobalPrefs**                              | High (152 accesses in SumatraPDF.cpp, touches many files)     | High (reduces global coupling significantly) | High (many files, easy to break)         | ????? | 3        |
| **EngineMupdf.cpp: Decouple rendering helpers**                | Medium (extract helpers per format)                           | Medium (isolates MuPDF dependency)           | Medium (deep coupling)                   | ????? | 4        |
| ~~MainWindow.h: FileState struct~~ → **SelectionState struct** | Low (established pattern with AutoScrollState/ArchToolsState) | Medium (organizes god class)                 | Low                                      | ????? | 5        |
| **Toolbar.cpp: Refine further**                                | Low (2368 lines, already has design tokens)                   | Low (already modular enough)                 | Low                                      | ????? | 6        |

### Recommendation: ideal path

**Phase 16C-F6** (remaining SumatraPDF.cpp extraction):

- Keeps C2-C5 extraction momentum
- Already validated pattern (HandleCmdXxx wrappers)
- Shrinks god file from ~11,200 to ~10,000 lines
- Effort: medium — reuses existing infra

**Then: FileState struct** (MainWindow.h):

- Low-risk quick win, organizational improvement
- Complements AutoScrollState/ArchToolsState already done

**Avoid for now:**

- Canvas.cpp extraction (high risk, no pattern, interleaved rendering)
- gGlobalPrefs encapsulation (too disruptive without unit tests)

### Suggested Execution Order

1. [x] 16C-F6: Extract init/window creation from SumatraPDF.cpp (reduced from 14,212 to ~13,830 lines) — 2026-09-08
2. [x] MainWindow.h: SelectionState struct (15 grouped selection/touch members) — 2026-09-08
3. [ ] 16D: gGlobalPrefs typed accessors (only when tests exist)
4. [ ] Canvas.cpp: Map dependencies before extracting
5. [ ] EngineMupdf.cpp: Decouple rendering helpers per format

## Design System Rules (2026-09-08)

### Buttons and Inputs — Content Centering

- **Rule:** Buttons and inputs must center content (text/icons) horizontally AND vertically within their bounds
- Applies to: ToolbarLayout controls, toolbar buttons, timer/speed inputs, and any new UI elements
- **Status:** ⚠️ Rule defined, implementation pending

## Known Bugs (2026-09-09)

### BUG-1: Invert=on — Clipboard image copy broken

- **Status:** ✅ Resolved (2026-09-10)
- **Description:** When Invert (contrast overlay) is on, copying images to clipboard fails
- **Fix:** Added `case WM_NCHITTEST: return HTTRANSPARENT;` to WndProcContrastOverlay — overlay now click-through

### BUG-2: Trim=on — Text selection displaced

- **Status:** ✅ Resolved (2026-09-10)
- **Description:** When Trim is on, text selection lands displaced from rendered text position
- **Fix:** Added trim.top offset in SelectionOnPage::GetRect() before CvtToScreen — selection now follows render position

### BUG-3: Right-click image context menu — most functions broken

- **Status:** Pending
- **Description:** Right-click context menu on images: only Copy to clipboard works; other options (Save as, Copy link, etc.) do not work
- **Note:** May never have been implemented in original SumatraPDF — investigate before fixing

### BUG-4: Trim Config disables Contrast overlay

- **Status:** Fix v2 (self-healing) deployed 2026-09-22 — awaiting test of exact flow (1st trim config open after app launch + confirm)
- **Symptom refined by user:** the bug is ONE-SHOT — only on the FIRST interaction with trim config after opening the app (invert=off, on confirm the contrast vanished). Hypothesis: conflict between session state (sessionData), per-document state (FileState) and default state (ReplaceDocumentInCurrentTab has 3 paths writing `win->contrastEnabled`) + settings.txt FileWatcher race (`WatchedFileSetIgnore` vs `SchedulePrefsReload` → `ReloadSettings` nukes whole `gGlobalPrefs`).
- **Fix v2 (invariant + self-healing):** new `EnsureContrastOverlayState(win)` — creates if `contrastEnabled && !overlay`, destroys if `!enabled && overlay`, repositions if in sync. Called at ALL flag write sites: `ReplaceDocumentInCurrentTab` (doc-swap now also destroys orphan overlay), `UpdateTabFileDisplayStateForTab` (SaveSettings), TrimConfig OnSave/OnCancel (RE-CREATE overlay if any path destroyed it). `[contrast]` breadcrumbs kept (Create/Destroy/WM_DESTROY) for diagnosis via debug build.
- **Systemic lesson:** persistence functions (SaveSettings/*ForTab) must NOT have UI side effects (destroying HWNDs). Documented in lemma m3a65456efb04.

### BUG-5..9: .md regressions (2026-09-23) — ALL FIXED (LOG s10/s11)

- **BUG-5** .md ETA dead: adapter `MarkdownHtmlWindowHandler` did not forward `OnAutoScrollProgress/Bottom` (no-op default). Fixed + ETA split into independent logics (`RecalcAutoScrollEtaPdf`/`RecalcAutoScrollEtaWebview`). Lesson: adapter trap documented in FLOW/autoscroll.dot + lemma.
- **BUG-6** .md shimmer: rAF steps quantized in device px (`floor(accum*dpr)/dpr`). Visual verification pending.
- **BUG-7** Space scrolling .md/CHM: WebView2 internal Chromium has native Space — `kBlockSpaceScrollJs` (init script).
- **BUG-8** Ctrl not pausing .md: tick restructured (Ctrl early-return only in dm branch; webview branch = state machine).
- **BUG-9** .md reading position not restoring + F dead in .md: restore via `ScrollTo` in LoadDoc (same path as CHM) + `kForwardAppKeysJs` forwards A-Z letters to host (WebView2 only fires AcceleratorKeyPressed for F-keys/arrows). "ETA vanishing in fullscreen" = stale binary, non-bug (verified by ad-hoc-md-fullscreen.ts).
- New ad-hoc tests: `ad-hoc-md-eta.ts`, `ad-hoc-md-fullscreen.ts` (do not register in all.ts). `tests/util.ts` EXE → `TumatraPDF.exe` (was stale SumatraPDF.exe). Cosmetic follow-up: per-test taskkills cite old process name.

## Flashcard System (Cloze over existing text)

exemplo:

texto texto texto texto texto texto
texto texto texto texto texto texto
↓
seleção
texto texto [texto texto texto texto
texto texto texto] texto texto texto
↓
resultado esperado:
texto texto [______________________
________________] texto texto texto

resultado incorreto:
[_________________________________
__________________________________]

### Model (decided 2026-09-16)

- **1 highlight = 1 cloze card.** A text selection becomes a MuPDF Highlight = positional mask over the ORIGINAL PDF text.
- **NO content duplication:** the original text is already in the PDF. The card stores only `annotId`, `pageNo`, `bounds` (highlight rect). **Highlight content stays EMPTY** (do not write `"Q: text"`).
- **Study=on:** opaque mask covers the cloze rect → `[_____]`. **Reveal:** removes the mask → original text visible.
- Multiple clozes in the same sentence = multiple highlights/cards (each selection = 1 card).
- **Future (post-MVP):** select several sentences → 1 single cloze (keep list numerals outside the cloze). Does not block MVP.

### Render matrix (Canvas) — study on/off NEVER covers text at the wrong time

| State        | flashcard.on | studyMode | revealMode | Action per card                                                           |
| ------------ | ------------ | --------- | ---------- | ------------------------------------------------------------------------- |
| Off          | false        | —         | —          | nothing (zero cost)                                                       |
| Reading      | true         | false     | false      | subtle translucent marker (`30,128,128,128`) on ALL — text visible        |
| Study        | true         | true      | false      | opaque mask (`255,100,100,100`) ONLY on current card — text hidden        |
| Study+Reveal | true         | true      | true       | NO mask on current card — original text visible (PDF 40% highlight stays) |

> Rule: **opaque mask only in `studyMode && !revealMode` and only on the current card rect.** Otherwise, never cover text. The current bug (FC-A) draws the mask on all cards and does not remove it on reveal.

### Current code status (reference)

- Commands: 11 IDs (CmdFlashcardToggle..CmdFlashcardAddTip) via gen-commands.ts
- `Flashcard.cpp`: `LoadFromDocument` (parses `Q:`/`T:` — to remove), SM-2, study save/load (load **never called** — bug FC-B)
- `FlashcardToolbar.cpp`: secondary toolbar (Study/Back/List + counter via button idx 0)
- `Commands_Flashcard.cpp`: handlers
- `FlashcardSidebar.cpp`: floating List sidebar
- `Canvas.cpp:3390-3418`: study/reading overlay (reveal broken)
- `EditAnnotations.cpp:1300-1313`: `Author == TumatraPDF-Flashcard` filter (✅ already done)

### Implementation Plan (positional Cloze)

#### Step 1 — Render: correct study/reveal matrix (Canvas.cpp:3390-3418) [CRITICAL]

- Restructure the card loop: opaque mask **only on the current card** (`studyOrder[currentCardIdx]`) when `studyMode && !revealMode`.
- Reveal: do **not** draw the mask on the current card (original text shows).
- Other cards in study: translucent marker (not opaque) — otherwise all clozes appear masked.
- Reading (on, !study): translucent marker on all.
- **Verify:** `text → study=on → [_____] → reveal → 'text'` (user example).

#### Step 2 — Stable card key (Flashcard.h/cpp) [pre-persistence]

- `annotId = pdf_to_num(...)` is **unstable** (MuPDF renumbers on save).
- New key: `u64 key = hash(pageNo, bounds)` (rounded bounds), used to match `studyDoc` and order.
- Keep `annotId` only as annotation reference, not as key.

#### Step 3 — Persistence: load studyDoc on Toggle ON (Commands_Flashcard.cpp) [FC-B]

- `CmdFlashcardToggle` ON (after `FlashcardLoadFromDocument`): call `win->flashcard.studyDoc = FlashcardStudyLoad(tab->filePath.s)`.
- Ensures correct `newCount`/`dueCount` after restart.
- **Verify:** create card, study, close, reopen → counter keeps state.

#### Step 4 — Creation without duplication (Commands_Flashcard.cpp, CmdFlashcardAdd) [FC-D/G]

- Stop writing `content = "Q: <text>"` → empty content (highlight is just a mask).
- Adjust `FlashcardLoadFromDocument` to load by `author == TumatraPDF-Flashcard` (without depending on the `Q:` prefix), with retrocompat for old cards with `Q:`.
- After creating, **reload `cards`** (FC-G) so counter/list reflect the new card.
- Decide persistence: save the PDF after adding (parity with AddTip) OR rely on the reader auto-save — align both paths.

#### Step 5 — Real SRS: filter by due (Commands_Flashcard.cpp, BuildFilteredStudyOrder) [FC-C]

- `studyOrder` = cards with `rating==0` (new) OR `nextReviewAt <= now` (due).
- rating==1 (Again): **reinsert the card into the current queue** (relearn, like NoteAnki) instead of discarding.
- `FlashcardSm2Update` (Flashcard.cpp): clamp `easeFactor` to `[1.3, 2.5]`.

#### Step 6 — Keyboard shortcuts (Accelerators.cpp) [align NoteAnki]

- `Space`/`Enter` = CmdFlashcardReveal (Space already reveals; confirm/add Enter).
- `1`-`4` = CmdFlashcardRate1-4 (rating; today toolbar-only).
- Keep `S` = Add (already exists, L148-149).

#### Step 7 — Prune dead code [FC-H]

- Remove `hwndCardCount` from `FlashcardState` (counter became button idx 0).
- Remove/implement `CmdFlashcardNext` (no-op), `studyModeType`, `history` (decide real undo or remove).
- Remove `T:`/`tip` parsing if the position-only model does not use it (CmdFlashcardAddTip `(hint)` placeholder).

#### Step 8 — Tests (tests/ + test_util)

- `FlashcardSm2Update`: intervals, ease cap, relearn rating==1.
- Study JSON parser (save/load roundtrip).
- Due filter (new + due).
- (Optional GUI) mask disappears on reveal via screenshot.

#### Step 9 — Build + deploy + LOG

- `bun cmd/build.ts` → `Compiled/TumatraPDF.exe` (0 err / 0 warn).
- Update LOG.md + README (render matrix, cloze model).

### Files to Modify

| File                         | Change                                                                  |
| ---------------------------- | ----------------------------------------------------------------------- |
| `src/Canvas.cpp`             | study/reveal render matrix (Step 1)                                     |
| `src/Flashcard.h`            | stable `key` field; remove `tip`/`text` if unused                       |
| `src/Flashcard.cpp`          | Load by author without `Q:`; key hash; SM-2 cap/relearn                 |
| `src/Commands_Flashcard.cpp` | Add without duplication; load studyDoc; due filter; relearn; prune dead |
| `src/MainWindow.h`           | Remove `hwndCardCount`/dead                                             |
| `src/Accelerators.cpp`       | Space/Enter reveal + 1-4 rating                                         |
| `src/FlashcardToolbar.cpp`   | (if applicable) counter adjustments                                     |

### References

- NoteAnki Cloze system: `D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\NoteAnki\CLOZE.md` (mask over source text, no duplication)
- AnnotCreateArgs: `src/Annotation.h:72-93` (opacity 0-100, col, bgColor)

### Review 2026-09-23 — agent audit findings (triage AFTER user reports observed errors)

#### Confirmed bugs

- [x] **FC-R1 TC2 routing gap in navigation** — FIXED 2026-09-23 (s14): all study navigation now goes through `FlashcardNavigateToCard` (routes `PhysicalToVirtualForRect`; used by study-first, rate-advance, Next). Back intentionally keeps scroll position (post-reveal spec).
- [x] **FC-R2 Mojibake** — FIXED 2026-09-24: proper UTF-8 em-dash in FlashcardSidebar.cpp card label (compiled with /utf-8).
- [x] **FC-R3 Sidebar orphaned on flashcard OFF** — FIXED 2026-09-24: `CmdFlashcardToggle` OFF now calls `FlashcardSidebarDestroy` next to `FlashcardToolbarDestroy`.
- [x] **FC-R4 Duplicate control IDs** — FIXED 2026-09-24 (s19): dedicated `IDC_FLASHCARD_REBAR`/`IDC_FLASHCARD_TOOLBAR`/`IDC_FLASHCARD_LABEL_WITH_CLOSE` (resource.h 1108-1110); toolbar rebar/toolbar + sidebar label no longer reuse the main toolbar's / favorites' IDs.
- [x] **FC-R5 dwStyle/exStyle mix** — FIXED 2026-09-24 (s19): sidebar popup now passes `WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE` in the exStyle PARAM (they were silently ignored inside dwStyle) — NOACTIVATE actually applies now: clicking the floating list doesn't steal focus from the canvas.
- [x] **FC-R6 Leftover `[fc-paint]` diagnostic logs** — STRIPPED 2026-09-24 from Canvas.cpp (both the first-10-paints header block and the per-rect log; paint logic untouched).

#### Decisions pending (user)

- [x] **FC-D1 Reveal key binding** — RESOLVED 2026-09-23 (s14, user spec): Space + Enter + numpad Enter → Reveal (no-op outside study mode; Enter's scroll bindings removed so the key family never scrolls).
- [x] **FC-D2 Card persistence policy** — RESOLVED 2026-09-24 (user): keep current behavior — the app already prompts on close for unsaved changes (Ctrl+S / exit prompt). Related fix: same-file saves no longer re-compress (EngineMupdfSaveUpdated, LOG s16) — saves are fast again even on repaired-xref scans.

#### Missing vs plan

- [ ] **FC-M1 Step 8 tests**: none — Sm2Update (intervals, ease cap, relearn), study-JSON roundtrip, due filter.
- [x] **FC-M2 Sidebar not refreshed after `CmdFlashcardAdd`** — FIXED 2026-09-24: `FlashcardSidebarPopulate(win)` called after Add (self-guards when sidebar is closed).
- [x] **FC-M3 No availability gating** — FIXED 2026-09-24 (s19): all 14 flashcard commands added to `removeIfChm` + `removeIfMarkdown` (CommandAvailability.cpp) — no more flashcard toolbar/no-op commands on CHM/MD docs. (Menu-bar entry intentionally absent — toolbar/palette UX by design.)
- [x] **FC-M4 Filter dialog UX** — REDONE 2026-09-25 (s22): two GoToPage prompts replaced by Filter window (page-set expr `1-15;20-25;-22-23;` + bookmark mirror + ON/OFF + clear w/ 2s hold); filter now re-applies to active session (`FlashcardApplyStudyOrder`). REMAINS: expression compares PHYSICAL pageNo while TC2 shows virtual "3L" labels — decide hint in dialog or virtual-input mapping.
- [ ] **FC-M5 Rate keys 1-4 have no visual hint** (no rate buttons/tooltips on the toolbar).

#### Robustness / hygiene (low)

- [x] **FC-H1** FIXED 2026-09-23 (s14): `HandleFlashcardRate` now range-checks `currentCardIdx` (both bounds) before indexing; `FlashcardNavigateToCard` guards `cardIdx` too.
- [ ] **FC-H2** Full-document scan under `docLock` on Toggle ON (UI freeze on huge PDFs; could pre-check /Annots per page before `fz_load_page`).
- [ ] **FC-H3 Study JSON**: ~~locale-dependent float~~ (NON-ISSUE 2026-09-24: CRT fprintf uses the default "C" locale — app never calls setlocale; the decimal-comma bug was in str::VsnprintfUtf8's `_create_locale`, fixed in eaad4e6) — ~~atomic write~~ DONE 2026-09-24 (s19: tmp + MoveFileExW REPLACE_EXISTING, remove+rename fallback, verified via repro); **REMAINING**: prune states of deleted cards (save only entries whose key matches an existing card).
- [x] **FC-H4** FIXED 2026-09-24: stray trailing `#include "Flashcard.h"` removed from Flashcard.cpp (top include block already has it).
- [x] **FC-H5** FIXED 2026-09-24: TODO render matrix alpha 200→255 (matches code); MainWindow.h comment "(Space pressed)"→"(Space/Enter)"; FLOW dot was already correct.

#### Verified OK (no action)

Steps 1-5, 7 implemented; render matrix correct in Canvas (per-quad + PhysicalToVirtualForRect + central trim offset); stable key; `fz_try/fz_catch` scan; toolbar checked states; SM-2 ease cap + relearn reinsert.

### Feature (PLANNED, awaiting user decisions): Image Occlusion cards (boxes over images/diagrams)

Requested 2026-09-24 ("cards created by drawing boxes, integrated into the current system" — user spec, translated).

**Integration insight**: a card = `{pageNo, bounds, rects[]}` from a Highlight annot with N quads — a box over an image is just a page-space rect. SAME annot, SAME loader, SAME mask painting / reveal / rate / SM-2 / Clean History / centering: ZERO study-pipeline changes. Only the DRAWING MODE at creation is new.

**Plan (4 steps):**

- [ ] **IO-1 State**: `FlashcardState` (MainWindow.h) gains `boxMode` (bool) + `Vec<RectF> pendingBoxes` (drawing session on the current page).
- [ ] **IO-2 Canvas drawing**: "Box" toolbar button (checked state) toggles boxMode; hook in `OnMouseLeftButtonDown` BEFORE text-selection/pan branches: drag = rubber band; boxes accumulate; existing flashcard paint renders pending boxes translucent.
- [ ] **IO-3 Coordinate conversion**: screen→page via pageNo-from-point + `CvtFromScreen` (same math as cloze; TC2/trim-aware); rects stored in PHYSICAL page space (like text-cloze rects).
- [ ] **IO-4 Creation**: `S` (CmdFlashcardAdd) branches: if pendingBoxes → create ONE Highlight annot with boxes as quads (same author/color path) → 1 card with N masks. Esc or Box-toggle-off discards pending boxes. Mouse feature → USER tests live (no automation).

**Design decisions PENDING (user thinking):**

- [ ] **IO-D1 Grouping**: boxes session → 1 grouped card (consistent with Alt+multi-select; recommended) vs 1 card per box (Anki default)?
- [ ] **IO-D2 Mode entry**: toolbar "Box" button only? key 'B'? both?
- [ ] **IO-D3 Scope**: boxes may cover image/diagram regions (NO text selection involved) — confirm.

**Queue triage (2026-09-24, high gain / low effort):**

- [x] **FC-M3 gating** — DONE 2026-09-24 (s19): all flashcard commands in `removeIfChm`/`removeIfMarkdown`.
- [x] **FC-H3 atomic write** — DONE 2026-09-24 (s19): tmp + MoveFileExW REPLACE_EXISTING (remove+rename fallback), verified via repro. Locale float = non-issue (CRT default "C"; see FC-H3 above).
- [x] FC-R4/FC-R5 hygiene bundle — DONE 2026-09-24 (s19): dedicated IDs + exStyle properly applied.
- [ ] FC-H3 prune (save only states with existing cards) — medium, defer.
- [ ] FC-M1 tests (SM-2/JSON roundtrip/due filter) — after image occlusion.
- [ ] FC-H2 scan perf, FC-M5 rate hints — later. (FC-M4 done s22 — see queue triage above.)
- [x] **Order button redo** — DONE 2026-09-24 (s20): opens Study Order options dialog (Sequential/Random + new-cards Before due/After due/Mixed, default mixed, instant apply, shuffle-within-groups). See LOG s20.
- [x] **Clean History rework** — DONE 2026-09-24 (s21): dialog reworked — "Clear current book" 2s hold (orange) / "Clear ALL books" 5s hold (red, deletes study JSONs + resets every tab's state); "No" button removed, X/Esc cancels. See LOG s21. **Superseded 2026-10-01 (s24)**: button renamed Clean → **Config**; the dialog became the Config window (folder picker + history list + clear-SELECTED/clear-ALL).
- [x] **Config window (Clean → Config)** — DONE 2026-10-01 (s24): study-history folder row (EDIT + 📂 IFileDialog picker; `flashcardStudyDir` setting; Drive folder = backup; folder change MIGRATES existing JSONs — `FlashcardStudyMigrateFiles`, MOVEFILE_COPY_ALLOWED, target files kept — added s25) + per-book history list (|PDF|cartões|due|última revisão|, ~10 rows + scroll, `FlashcardStudyListDocs`, JSON stores `docName`) + "Limpar livro selecionado" (2s hold, deletes the selected book's JSON; md5-matched tabs reset) + "Limpar TODOS os livros" (5s hold). All flashcard windows now theme-aware (dark ok). **Study-state parser FIX (latent since s14)**: getField missed the key's closing quote → every StudyLoad returned all-zero states (studied cards came back as "new" on each relaunch). See LOG s24.
- [x] **Design_Guidelines improvements** — DONE 2026-10-01 (s24): #1 dialog scaffolding extracted (3 windows), #3 refresh-hook fix (Clear-ALL populates Lista), #4 `PaintFlashcardMasks` extraction from Canvas, #5 xorshift128+ shuffle, #7 build.ts auto clang-format of modified files. NOT done (justified): #6 page-text cache generalization (no second consumer), #8 FC-H2 mtime cache (medium risk, dedicated session).
- [x] **Data tools (s26-data)** — DONE 2026-10-01: renames Scan Mode / Two Columns (display names only, ids intact); RESYNC (auto adopt-by-docName on open + Settings "Link to PDF..." for renames); rolling backups 1d/3d/7d + Restore Backup; Import from PDF (partner's NEW cards only, study history untouched); ALL fork labels → English + Shortcuts.md; BUILD.md exe-size explainer; Settings window two button rows. See LOG s26-data.
- [x] **Sync markers + Re-check + List rename + Scan fit-width (s27)** — DONE 2026-10-01: study JSONs persist `docPath`; Settings list shows green/red/grey sync dots + Re-check button; adopt-by-name re-saves docPath; toolbar "Lista"→"List"; Scan Mode always starts from fit-width. See LOG s27.
- [ ] **Review Section button (user item 8, NEXT)**: window with a search field on top + multi-select list `|PDF | cards | due | last reviewed | tags|` + **Open Session** button → launches a NEW app window with all selected books (works together with Filter / global-session cross-doc study). Data: `FlashcardStudyListDocs` + persisted `docPath` (prerequisite DONE s27). Multi-select = checkbox rows (Filter bookmark-mirror pattern); Open Session = new process with the selected paths (same exe + file paths as args). Tags data model TBD (nothing stored yet).
- [x] **Filter button redo** — DONE 2026-09-25 (s22): Filter window — page-set expression, TOC bookmark mirror (checkbox injects chapter range), Filtros ON/OFF, Limpar filtros (2s hold), cross-doc session checkbox in same window. See LOG s22.
- [x] **Cross-document study sessions** — DONE 2026-09-25 (s22): scope=global-session queue over all PDF tabs of same window, auto tab-switch on advance, SM-2 saved per book; per-tab cards/studyDoc/filter state (fixes stale cards on tab switch).
- [x] **Lista button redo** — DONE 2026-09-25 (s23): floating popup replaced by docked RIGHT panel (AI-chat pattern, own splitter) with 4-column owner-drawn list: Flashcard (masked-text label via ExtractPageText, no stored text) | Pág | Due (fresh countdown at draw time) | Status (new=blue/due=orange/learn=purple/ok=green); click row → navigate; toggle via button/label-close; refresh after rate + clean. See LOG s23.
- [ ] Bookmark-mirror live check on user's real TOC books (test PDF has no TOC — automated test auto-skipped mirror).
- [ ] User live tests: Filter window + global session (checkbox, cross-tab jumping, per-book saves); Lista panel visual check (columns/colors/labels on real books); Config window (folder picker + Drive backup, list columns, clear-selected, dark theme, 📂 glyph).
