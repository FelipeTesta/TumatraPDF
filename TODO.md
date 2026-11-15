# TODO

1. Open work ONLY.
2. Completed features/fixes live in [LOG.md](LOG.md) (per-session) + [CHANGELOG.md](CHANGELOG.md) (per-release).
3. User manual-test items = items only the user can validate.
4. Housekeeping (2026-10-20, s36-ae; re-triaged s36-at, s36-cu/cv): all `[x]` blocks archived — details live in LOG per session entry; this file lists open work only.

## HOME responsiveness — plan (2026-11-13)

- [ ] Malformed-PDF repair-on-open (feature CANDIDATE; opened 2026-11-15 by R9v incident): offer in-app repair (MuPDF-clean content-stream sanitize pass, fitz clean=True equivalent) when open succeeds but page render throws FZ_ERROR_ARGUMENT / page fails to paint. Scope: detect-and-offer dialog (not auto-silent rewrite); original file NEVER modified in place (user picks target); reuse pattern of crash-guard docs. See LOG 2026-11-15 (s64-R9v) + tests/tmp/s64/ (fixture flow) for the exact incident + repair recipe.

Branch: `fix/home-responsiveness` — worktree created from `fix/profile-sync-cascade` because another agent is working on the shared working tree (no branch flip; their uncommitted changes untouched). Order: A → B → C → D. Environment: DPI 96, `-for-testing` launch, resize via Win32 `MoveWindow`, screenshots into `C:\Users\Testa\AppData\Local\Temp\opencode\home-analysis\` (capture + visual verify required). Known facts: binary renders stats band wider than window at ALL widths (heatmap clipped, Oct/Nov/Dec unreachable); source math `kThumbnailDx = 212` (`FileThumbnails.h`) vs measured 170 (edit HWND steps of 208px); `HomeStatsWnd::Paint` drops ranking col < 520px and band < 320px; toolbar routes `WM_SIZE → ScheduleUiUpdate → RelayoutFrame`, main toolbar rewraps (2–3 rows ≤1300px).

- [ ] A. Overlap "Stats" × "Recently Opened" — `HomePage.cpp` `LayoutHomePage`: when the stats band is dropped (insufficient height; reserve ≈ 210 DIP + 200 DIP band), `booksHdrY = l.rcStats.IsEmpty() ? hdrY : ...` (≈ :2326) collides with the "Stats" title drawn at `hdrY` (≈ :2282) + "⟳ Sync" button. Fix: when `l.rcStats.IsEmpty()`, also hide/collapse `chrome->hdr` and `chrome->syncBtn` (or offset books header); verify at height ≤ ~500px.
- [ ] B. Heatmap overflow right edge — `HomeStatsWnd::Paint` (≈ :1293): 53-col GitHub-style grid, `cellDx = (gridW - 52*cellGap - 11*monthGapPx)/53` with `月 gap`/cell gap; measured bright-pixel x-range 605..1497 @1500px window; year label centered ≈1215 vs expected ≈970. Rebuild first, re-measure; if persists, cap grid width to `heatX0 + gridW` (or drop band earlier by raising the reserve). Never render clipped.
- [ ] C. FLOW/homepage.dot — rewrite to current architecture: HOME painted on the canvas double buffer (`CanvasAboutUI.cpp` → `DrawHomePage`); chrome = VirtWnd tree (`win->homeRoot`: Stats hdr, Sync btn, books hdr, view icons grid/list, openDoc link, tip); stats band + heatmap/Graph modes (Read/Flashcards tabs); search edit = native HWND child of canvas; `RelayoutFrame` home block (`showHomeOverlay`); Canvas `WM_SIZE` → `UpdateCanvasSize` + `HwndInvalidate`; frame `WM_SIZE` → `ScheduleUiUpdate(WM_UPDATE_UI)`; `kWinMinDx=500` / `kSidebarMinDx=150`; thumb step 208px (170+38+2×40) observed in binary. Remove the extinct `hwndHomePage` / `homeDy` / `TUMATRA_HOMEPAGE` content. Keep process-map focus (no code architecture dumps).
- [ ] D. Toolbar narrow — long-text buttons ellipsized already ≤1500px; wrap to 2 rows ≤1300px, 3–4 rows ≤950px/620px (`Toolbar.cpp`: `TBSTYLE_WRAPABLE` + rewrap timer; main toolbar only; secondary toolbars are single-row by design). Investigate `ToolbarRewrapAfterSizing` + explicit `cx` handling; below a width threshold collapse text buttons to icon-only (or route to overflow "…" menu). Biggest item; may be split into its own session.
- [ ] Verify — rebuild (`bun cmd/build.ts`, workdir `sumatrapdf-src/`); 7-size resize sweep (1680×1000 → 620×380) with the same probe method; confirm: no overlap at low heights, heatmap fits or band cleanly dropped, toolbar usable at narrow widths, list-view toggle sanity, empty state; dark theme (light if applicable).
- [ ] Informativos — `LOG.md` entry (Fixed/Changed), `TODO.md` check items closed, `FLOW/homepage.dot`; commit only on explicit command.

## Open-source references

1. https://github.com/nomacs/nomacs

## Execution queue (effort/gain)

User-locked order:

1. **Bookmarks Improve** (spec pointer → `## Bookmarks Improve` below).
2. **Edit/Align** (spec pointer → `## Edit/` below).
3. **Page Counting Format** (spec pointer → `## Page Counting Format` below).

4. USER validation queue: capture tool Shift+S (s39, documented §Capture tool), Repair Document File (s42, steps in its section below), Profile Sync dark cascade (s40: import/cloud-revert windows), TOC Reorder toggle crash fix (LOG s54-b: selected bookmark + click items + Reorder on real build/machine).

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
- [ ] Review README to list all new features, mapped from LOG; caveman-compress contents for objectivity and low verbosity.

## Performance optimizations (verified 2026-11-04, evidence LOG s38)

Source: external agent's perf map (archived TEMP/performance-code-arch.dot), claims verified against code (LOG s38). Safest first:

- [ ] **OPT-7** coordinate-space guards (START): `#if isDebug` asserts in `DisplayModel::CvtToScreen/CvtFromScreen` + TC2 colX consistency check (debug-only, no behavior change; prerequisite for OPT-3). STATUS: in progress s38-o1.
- [ ] **OPT-6** perf regression tests: cache hit rate, RelayoutFrame frequency, trim/crop toggle latency; ad-hoc tests, fail on >5% regression. STATUS: baseline tests shipped (startup/cache-behavior/toggle-latency); CI fail-gate metrics (cache hit rate, relayout frequency) need new C++ control cmds — residual open.
- [ ] (DEFER) OPT-1 adaptive cache cap — after OPT-7; needs memory accounting + tests.
- [ ] (DEFER) OPT-3 partial trim-cache invalidate — real gain (Full FreeForDisplayModel storm, RenderCache.cpp:426) but stale-tile risk on rect.y shift; needs stamping/versioning.

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

## Two Columns v2 — pending USER live-confirmations (code complete, all E2E-verified)

- [ ] User confirm on a real book: autoscroll acceptance over TC2 (1L→1R→2L→2R, no jump), drag speed, OCR text selection per column. The three s35 fixes (stall-on-hover, drag-slow, per-column selection) are code-verified + E2E PASS — see LOG s35/s36-l.

## Flashcards

### Tests

- [ ] **FC-X1 live test** (shipped s36-m): cross-page cloze — user test on a real book (2-page passage).
- [ ] **FC-M1** tests: SM-2 (intervals/ease cap/relearn), study-JSON roundtrip, due filter — after Image Occlusion lands.
- [ ] **FC-M5** rate keys 1-4 have no visual hint (no rate buttons/tooltips on toolbar).
- [ ] User live tests (queue): Filter window + global session (cross-tab jumping, per-book saves); List panel visual on real books; Settings window (folder picker + Drive backup, clear-selected, dark theme); bookmark-mirror on real TOC books (test PDF has none — automated test skipped the mirror).
- [ ] User live tests (s36-az batch): study-time + cards totals format on a real book; session-timer trio (checkbox/minutes edit/progress line/expiry beep+overlay) visual on light+dark; flashcard toolbar single centered labels + checked=bold+underline (no white face); home hides fc/arch toolbars and restores them.

### Study preload

- [ ] Flashcard study preload: pre-load next cards' pages/annots during study=on (esp. cross-doc session) to cut card-to-card load time; ADAPTIVE lookahead (budget by measured render time; low-priority background; caps so weak machines not burdened). P1+P2 shipped s64-R9p (EWMA-driven N, BELOW_NORMAL worker, epoch invalidation, caps, silent N=0 degrade; probe mechanism-verified; warm jobs untested — dev box memory-degraded). P3 = opt-in setting only (`flashcardPreloadSessionBooks`, lazy-tab background load NOT implemented). Awaiting user live validation + lighter-machine check of warm-job path.

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

**Reference:** `TEMP/performance-code-arch.dot` — full map of memory architecture, CPU hotspots, tool costs, profiling tooling.

### Critical bottlenecks identified

| #   | Component               | Consumption                  | Root cause                                                                                                              |
| --- | ----------------------- | ---------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| 1   | **RenderCache tiles**   | 128 × 1–4 MB = **~512 MB max** | Primary cache; Gdiplus::Bitmap 32bpp PARGB; TC2 doubles keys (virtual pages left/right); Trim/Crop invalidate cache on toggle |
| 2   | **MuPDF `fz_context`**  | 10–50 MB/engine               | Allocator/store/device cache + page cache; per-thread pool clones contexts                                              |
| 3   | **WebView2 (Markdown)** | 100–300 MB separate process   | Isolated Chromium; Contrast/auto-scroll use JS/CSS overlay                                                              |
| 4   | **GDI+ orphan bitmaps** | Leaks if cleanup fails        | Ref-count in `PutTile`/`DropCacheEntry`/`BlitPixmap`; `darkModeEpoch` mass invalidation                                 |

### Toolbar tool memory impact

| Tool              | Memory effect                                                                    | CPU                                        |
| ----------------- | -------------------------------------------------------------------------------- | ------------------------------------------ |
| **Contrast**      | PDF: layered child (WS_EX_LAYERED) ~0; MD: CSS filter 0                          | Low                                        |
| **Crop v2 (TC2)** | Doubles virtual pages → duplicated cache keys left/right; `colGap` overlap       | Medium (virtual↔physical routing)          |
| **Trim**          | `ShouldCacheRendering=true`; `FreeForDisplayModel()` on toggle → full re-render  | High on toggle                             |
| **Flashcards**    | MuPDF highlight annots + opaque mask GDI+ alpha blend; JSON per-doc              | Low                                        |
| **Arch Tools**    | Small page-coord vectors; SVG export allocates temp                              | Low                                        |
| **AutoScroll**    | Timer tick → cached blit ~1ms/frame; ETA overlay layered window                  | 60fps target = 16.6ms budget               |
| **Tabs**          | `VirtWnd` per tab; Home pinned idx 0 → ≥2 tabs/doc; tab groups live-sync         | Low                                        |

### Critical relations (see TEMP/performance-code-arch.dot)

- `RenderCache` → `fz_pixmap` → `Gdiplus::Bitmap` → `BlitPixmap` (cached path ~1ms)
- `darkModeEpoch` mass-invalidates cache → re-render storm
- `TrimConfigDialog` TEMP TRIM: snapshot + force OFF while dialog open → cache free mid-dialog
- `AutoScroll` uses cached blit; non-cached = 23ms SYNC engine render
- Coordinate space mismatches (PAGE/VIRTUAL/CANVAS-CLIENT/SCREEN) = recurring bug class
- Optimization candidates tracked at `## Performance optimizations` (OPT-1/3 deferred; OPT-2/4 rejected; OPT-5 shipped; OPT-6 residual CI gate)

---

## Edit/

### Crop Align

Instant visual deskew: the correction must NOT appear only at save — it must re-adjust instantly (visually); save only WRITES the change to the original file. Desired behavior:

1. page is tilted "/---/"
2. Click `Crop Align`
3. Activate cursor 'x' → click and drag OR click two points on the page, drawing a line (visually similar to MEASURE or SCALE)
4. App straightens the page "|---|" based on the angle of the drawn line, cropping the page to the original proportions
5. lights the 🔴 tab dot to signal the PDF has unsaved changes

Reference: how this page straightening works: https://easyprotools.com/jpg/jpg-orientation-editor/ — goal is to crop the page similarly;

- [ ] 1. line must draw while clicking, similar to `measure`/`scale`
- [ ] 2. `Crop` button on the same Edit toolbar — a bounding box (like the image editor) that grabs the page edges and allows resizing/cropping the page


- [ ] Crop Align s64-R8 — Page Crop nomacs parity + wrong-cut fix (2026-11-13): R2 rotateDelta==0 raw-dict write dropped crop origin + no y-flip (fitz y-down cropBox vs PDF y-up mediabox). Fix: real PDF-space rect in both EngineMupdf.cpp + EngineMupdf_Preview.cpp (pdfX0=mb.x0+cropBox.x0; pdfY0=mb.y1−cropBox.y1; pdfX1=mb.x0+cropBox.x1; pdfY1=mb.y1−cropBox.y0). Cut region == drawn box (mediabox 100,200,312,492 for box [100,100,412,592] fitz). Canvas paint: nomacs-parity overlay — 4 darkened strips outside region (ThemeWindowBackgroundColor alpha 150) + 2px ThemeEdgeColor border + 8 handles (ThemeWindowTextColor alpha 230). Tests ad-hoc-s64-crop.ts (headless) + ad-hoc-s64-gui.ts (GUI screenshots) PASS. Build 0/0. Regressions issue-2799, align-deskew, bookmark-edit, apply-gating, esc-frame PASS --no-build. MD5 CE247DB... (out/dbg64 == Compiled). [ ] user live validation pending: Page Crop box cut == drawn region + nomacs overlay visual.

- [ ] Crop Align s64-R9 — Page Crop rotated (nomacs parity, 2026-11-13): ArchToolsState +cropAngle/rotateStart/rotating; Canvas.cpp rotated polygon border (center ± R(θ)·(±w/2,±h/2)) + 8 rotated handles + GraphicsPath clip-exclude overlay outside poly; GetCropResizeHandleAt inverse-rotates pt to box-local frame for axis-aligned hit-test; CalculateCropResizedRect inverse-rotates drag delta into local axes; drag-down outside poly → rotate mode (startAngle + Δatan2, Shift snap 22.5°); ESC resets cropAngle=0. EngineMupdf.h PageGeomEdit.rotatedCrop; EngineMupdf_Preview.cpp ApplyPreviewGeometry(..,rotatedCrop) → zero-origin boxes [0 0 W H], content matrix +angleDeg (align sign); save path same zero-origin boxes. Commands_ArchTools.cpp crop apply passes cropAngle+rotatedCrop, clears cropAngle=0. SumatraControl.cpp TestAlignApply 7th arg rotatedCrop. Headless test ad-hoc-s64-rotated-crop.ts PASS (rotated [0 0 412 592] fm=1; angle=0 R8 preserved [100 100 512 692]). PyMuPDF sign probe: mb=cb=[0 0 412 592], single Fm0, nonwhite, sign +angleDeg. Build 0/0; MD5 D5109D81D75274E53021FAF4CAD84828. Regressions issue-2799, align-deskew, s64-apply-gating, s64-esc-real, s64-crop PASS --no-build. [ ] user live validation pending: GUI rotate-drag draws diagonal box + rotated handles; vision check.
## Bookmarks Improve — shipped (s43→s62); USER revalidation pending

- [ ] USER live validation on real build/machine: on s62 build, delete the ghost bookmark + Ctrl+S once → ghost must vanish permanently; rename Enter (s61, watch `IDOK rawEditLen` log) + rename popup dark theme. (All dev items shipped — see LOG s43-a..s62.)

## Page Counting Format

The app already displays pages in other counting formats; we need a TOOL to change the PDFs so the file's page numbers match the real book numbers. Similar to Adobe: a tool to re-number page ranges, especially at the start of documents.

Example: pages 1-50 counted in roman, from page 51 on in decimal → page 51 becomes the new page 1. Typing 1 in the page field lands on physical page 51, making the book/page match exact. We can also create an ABSOLUTE page function to ignore cosmetic counting; in this example: `1` → page 51; `(1)` → page 1.

## Capture tool — shipped v1 (s39)

Shift+S: draw a rectangle that captures the image inside it at the document's DPI → copy to clipboard.

Residuals (open): no content-pixel verify (clipboard bitmap size only in test), no save-to-file. Capture tool v1 complete: rect in page coords, RenderPageArgs Export at doc DPI, clipboard copy, auto-exit, ESC cancel, doc-close reset, command availability gate.

## Repair document file — shipped v1 (s42, user validate pending)

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

- [ ] **BUG-3** right-click image context menu: s34 E2E confirmed Replace/Paint/Crop work + submenu visibility fixed; s36-ao live-checked the rest — `Image > Save` works (editor → `<book>_page_1.png`), menus filter correctly, and the silent null-bitmap returns now show a notification (fdcafde). No repro for the "non-image items dead" claim: those items are removed by design (link/comment/attachment). REMAINING: user's exact steps (file + item) to reproduce anything still dead.
- [ ] **BUG-4** Contrast × Trim Config ONE-SHOT vanish (first trim-config open after launch). Fix v2 (`EnsureContrastOverlayState` invariant + self-healing) deployed 2026-09-22 — awaiting user confirmation of the exact flow; `[contrast]` breadcrumbs in debug build. NOTE (s36-az): the "contrast stopped working" report of 2026-10-25 was root-caused to the save-reload state wipe (FIXED — ReloadDocument now preserves extras; LOG s36-az); if BUG-4's one-shot vanish still reproduces after that fix, it is a separate flow.
- [ ] **Ctrl+click flashcard highlight opens List** — implemented s34-b (Canvas ctrl+click detects /T author TumatraPDF-Flashcard → FlashcardSidebarShow; turns flashcard mode on if off). USER live-test pending (needs a book with flashcard highlights).
- [ ] **s34-g user validation pending**: Arch Measure Radius + Ref Line UX, resize-enlarge on a real book, flicker-free drag confirm (shipped + E2E 2026-10-11).

## Profile Memory — shipped (F2→F7; F8 partial)

Feature shipped: single profile file `tumatraPDF-data_1.json` (exportable/syncable/importable), Drive-synced folder distribution with atomic replace + conflicted-copy tolerance, Portable→Install import, local 9×8h rolling backups + recovery dialog, overwrite-vs-use-existing conflict UX, Home `⟳ Sync` button + Profile window (s36-bn/bp/bq, LOG). Open items:

- Gap s36-bf: ProfImportLegacy first-run-only — reconciliation of reading-stats.txt/flashcard-stats.txt day rows at startup pending (32reading days from legacy sidecars can go missing; recovery done manually 2026-10-31: added 20261017/20261019 rows to tumatraPDF-data_1.json).
- [ ] **F8 — Endgame centralization** (dual-write → profile as single source): migrate FlashcardStudyListDocs to iterate profile books; deprecate dual-write in per-book *.json + sidecars WHEN stable (flashcard Config window keeps reading legacy fallback); keep per-machine day rows for multi-PC. Slices 1-2 (live stats ticks → profile; Config list merge) shipped s36-bs/s36-bt — LOG.
- [ ] Theme fixes ×6 in profile dialogs: WM_CTLCOLOR fresh CreateSolidBrush leak (ProfileSyncWnd.cpp:38/:367, ProfileBackupWnd.cpp:284(?)); static HBRUSH cached-at-first-paint stale on theme switch (ProfileSyncWnd.cpp:429/:675, ProfileBackupWnd.cpp:202/:353); RGB(255,200,200) hold-Restore fill (ProfileBackupWnd.cpp:97) + RGB(192,192,192) session-timer progress line (Commands_Flashcard.cpp:950) + BLACK_BRUSH frame (ProfileBackupWnd.cpp:100) + forked sidebar selection palette (FlashcardSidebar.cpp:260/:271/:275) — all must use Theme API. Ref tests/tmp/s64/profile-flashcard-analysis.md §B.
- [x] Profile↔Flashcard storage unification (s64-R9o 2026-11-14): single data-root resolver `ProfileDataGetDataRootTemp()` (profileDataDir setting wins, else per-exe app-data); sidecars moved to `<root>\stats\` with copy-only migration on load (legacy kept, `[stats] sidecar migrated to data-root` log; legacy fallback read); `flashcardStudyDir` → legacy override shim (unset → `<root>\study`, no mass migration, read-fallback to legacy app-data `FlashcardStudy`); ProfileSyncWnd gained "Set Folder" button (id=7, FOS_PICKFOLDERS) → sets profileDataDir + SaveSettings, migrates on next launch via ProfEnsureProfileDirAndMigrate; gen-settings doc strings updated. Probe tests/tmp/s64/ad-hoc-s64-unify.ts: PASS (flashcard sidecar migrated + legacy kept + profile json under root; reading-stats.txt migrates on first autoscroll tick — not exercised). Sync-dialog PNG: 2 click attempts missed the Home "Sync" target — dialog visuals = user live check. Ref tests/tmp/s64/profile-flashcard-analysis.md §A.
- [x] Study-tick tab attribution fix (s64-R9n 2026-11-14): FlashcardStatsAddSeconds/AddCard now take the card's OWNING WindowTab (studyOrder[curIdx].tab), not win->CurrentTab(); tick + rate pass e.tab; null/closed tab skips per-doc credit. Probe ad-hoc-s64-tick-attr.ts: tick-b studied, CmdNextTab→focus tick-a, profile perDoc tick-b=3s / tick-a=0s (was misattributed to focused tab). Cap studyTimeSec + daily bucket stay window-level (commented in code).
- [x] **P1-migration RESOLVED (was stale TODO):** `gFileStateInfo` names blob missing "FlashcardStudyTimeSec" — fixed s36-br (commit 6df3dc3 2026-11-02): gen-settings.ts both flashcard fields appended at END (44/44 aligned), `AppSettings.cpp` 3-gen (gen1/2/3) `MigrateFileStateNames` remap in LoadSettings, test `tests/ad-hoc-settings-migration.ts` registered in before-release.ts. Verified s64-R9m 2026-11-14: build 0/0, migration test gen1/gen2/gen3 PASS, regressions PASS. NOTE: pre-s36-at profile perDoc corruption NOT auto-repairable — bleeding stopped, historical values wrong. Original description: `gFileStateInfo` (names blob in Settings.h ~1700) is MISSING "FlashcardStudyTimeSec" (44 fields vs 43 names; fields at index ≥23 shifted: flashcardCardsDone, invertColors, contrastEnabled ... archScaleStates).

## Tab system

Add a "Tab Group" button next to the main button at the top of the window, before the tabs begin (left of the document tabs). Goal: make the existing Tab Group dynamic. On click, show a dropdown listing the existing 'Tab groups'; clicking one closes all current documents and reopens all documents of the selected tab group; opening another document inside an open tab group auto-syncs (saves) for future sessions.

The app always restores the last tab group used by the user; if none exists, open `new`.

'Ctrl+n' opens a window with a `New` tab group — not synced. To save such a session as a tab group, add a `Save` button to the dropdown list; clicking it opens an editor/input to name the group; immediately after, the group auto-syncs on close or when documents are added to the session. Dragging a document out of a session removes it from that group automatically and moves it into a non-synced `new` tab group.

- [ ] Open (user action): TAB SYSTEM — all dev done s36-bu→s36-co (LOG), awaiting user revalidation on real book. Build 0/0; ad-hoc-tab-session.ts + ad-hoc-tab-session-e2e.ts + ad-hoc-tab-menu.ts PASS.

- [ ] FUTURE (user spec 2026-11-01): per-session reading/flashcard totals on Home tab — sum sessions' aggregated reading/flashcard time per group, independent of books; BIG feature, plan separately.

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

## Vertical_Toolbar (PARKED — DO NOT DO)

Idea frozen by the user ("NAO FAZER") — kept for reference only: when the window is narrower than 1/3 of the screen, adapt the toolbar to a left vertical bar (buttons top-to-bottom, converted to symbols with name on hover), keeping a top horizontal bar only for numeric inputs (page, timer) and remaining buttons; when home=on keep hiding all buttons.
