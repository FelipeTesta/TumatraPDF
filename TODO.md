# TODO

Open work ONLY. Completed features/fixes live in [LOG.md](LOG.md) (per-session) + [CHANGELOG.md](CHANGELOG.md) (per-release). User manual-test items = items only the user can validate.

## Housekeeping (quick)

- [ ] Old stray local branches (`master`, `feature/*`) vs git-flow conventions — prune after confirming nothing unmerged.
- [ ] TODO BUG-3 verify: right-click image menu flagged "most functions broken" once (may be unimplemented upstream) — live-check whether Crop/Resize/Replace/Paint forward path works; BUG-3 + s32-B can be tested together.

## Refactoring backlog (from 2026-09-21 tree-sitter audit — see LOG entries for completed R1/R2/R4, 16A-D, C2-C5/F6, SelectionState)

## Render performance — optimization points (tree-sitter audit 2026-10-10, s34-c; NO code changed yet)

- [ ] **R-P1 `BlitPixmapRegion` HALFTONE**: `SetStretchBltMode(HALFTONE)` unconditional per tile blit (base/Pixmap_win.cpp) — software bilinear on every paint×tile; BitBlt 1:1 ignores it. Fix: set mode only in stretch branches (or COLORONCOLOR for intermediate zoom states). Cheapest big win for autoscroll/zoom.
- [ ] **R-P1 arch-tools GDI+ churn (fork)**: OnPaintDocument creates Gdiplus::Graphics + Font/Brush per WM_PAINT when arch ON even with nothing to draw; MeasureString per measurement per paint. Fix: early-out + static GDI+ objects.
- [ ] **R-P1 `DrawDocument` O(N) loop**: iterates ALL pages per paint filtering by visibleRatio. Fix: visible-window iteration (DocumentLayout already computes it).
- [ ] **R-P2 `PaintTile` linear scans**: up to 4 scans per tile per paint (Find ×2 over cache[128], GetRenderDelay, RequestRendering→Exists→Find). Fix: hash keyed (dm,pageNo,rot,zoom,tile).
- [ ] **R-P2 `RecalcVisibleParts` full-layout copy per scroll tick**: DocumentLayout::Reset(nPages) + per-page copy on every scroll/zoom/autoscroll tick. Fix: reuse member / incremental window.
- [ ] **R-P2 hbmp blit DC churn**: CreateCompatibleDC+SelectObject+DeleteDC per tile (use SetDIBitsToDevice on DIB bmBits instead).
- [ ] **R-P2 `HdcCreateSimpleFont` per paint** for rendering/failed pages (cache per DPI).
- [ ] **R-P3 annotations full-doc scan**: EngineMupdfGetAnnotations loads every page (first ctrl+click / fc toggle / reload) — `[perf]` logs in place; async/lazy list build once real-book numbers land.
- Fine as-is: InvertPixmap (255-v) ≤ old ramp cost; debug-info Record/UpdateRenderInfo no-op when windows closed; FreeIfFull/FreeNotVisible bounded rare scans.

- [ ] **R3-left**: unify TrimRect math (TrimRectPage/Inverse) — sites semantics differ (mediabox shrink vs area/pt shift); forcing a helper = render-path risk. Only with a rendering regression test.
- [ ] **R5-left**: remove v1 crop stack (`DisplayModel.h` viewportCrop v1 fields) — BLOCKED: v1 button (Scan View) still live. Do when/if v1 is retired.
- [ ] **R6-left**: `Commands_AutoScroll.cpp:35-38` reads toolbar checkbox state directly (`IsDlgButtonChecked`) — route via `SetToolbarButtonCheckedState` idiom.
- [ ] **R7 / 16C-next**: continue god-file extraction — SumatraPDF.cpp (~14.2k lines, target ~10k), then Canvas.cpp (~5.7k; needs a dependency MAP first — mouse/keyboard interleaved with rendering, no established pattern), EngineMupdf.cpp (~6.7k, decouple rendering helpers per format).
- [ ] **16D**: typed `gGlobalPrefs` accessors (152 accesses in SumatraPDF.cpp) — only when tests exist.
- [ ] TC2 leftovers: uia `PageProvider` + legacy tile math not routed virtual→physical (non-cache path diverts; MVP-acceptable); favorites saved under v2 store VIRTUAL pageNo — normalize to physical at creation (same class as the FileState fix).

## Bugs open

- [x] **NEW 2026-10-10** Saving the document DEACTIVATES invert + contrast — FIXED s34-b: ReloadDocument now carries invertColors/contrastEnabled/contrastOpacity into the fresh FileState (E2E s34-reload-colors PASS; post-reload canvas byte-identical).
- [x] **NEW 2026-10-10** Color inversion of IMAGES is very bad — FIXED s34-b (user decision: true negative): session invert now per-pixel 255-v via InvertPixmap/InvertBitmapColors instead of the theme-color ramp + link-substitution (both corrupted images). Live-visual confirm pending.
- [ ] **NEW 2026-10-10** Ctrl+click to edit a highlight is SLOW — instrumented s34-b with `[perf] ShowEditAnnotationsWindow` logs; root cost candidate: first click builds window + EngineMupdfGetAnnotations (per-page load, whole doc, FC-H2 class). Fix direction: async/lazy list build — needs `[perf]` numbers from a real book first.
- [ ] **NEW 2026-10-10** Ctrl+click on a FLASHCARD highlight opens the flashcard List — IMPLEMENTED s34-b (Canvas ctrl+click detects /T author TumatraPDF-Flashcard → FlashcardSidebarShow; turns flashcard mode on if off). USER live-test pending (needs a book with flashcard highlights).
- [x] **NEW 2026-10-10** Flashcard hover label shows "testa" — RESOLVED 2026-10-10: "testa" is the annotation /T author (defaultAuthor → `%USERNAME%`, Annotation.cpp GetUserTemp :1384). User reviewed and decided to keep author behavior as-is (it only persists in the file on File > Save; `(none)` in `annotations.defaultAuthor` suppresses it).
- [ ] **BUG-3** right-click image context menu: only Copy-to-clipboard confirmed working; Save-as/Copy-link etc. dead. Investigate before fixing (may never have been implemented upstream).
- [ ] **BUG-4** Contrast × Trim Config ONE-SHOT vanish (first trim-config open after launch). Fix v2 (`EnsureContrastOverlayState` invariant + self-healing) deployed 2026-09-22 — awaiting user confirmation of the exact flow; `[contrast]` breadcrumbs in debug build.
- [ ] **Image editing functions (user 2026-10-10)** — Replace/Paint/Crop/Resize all FIXED s34 (E2E pass: `tests/tmp/s34-{replace,paint,crop}-e2e.ts`); root cause was a no-op image-submenu pruning bug (base `MenuRemove`/`MenuSetEnabled` don't descend into submenus) + WStrL/order bugs. Leftovers: modularize image cmds into Commands_Image.cpp (still in SumatraPDF.cpp/Menu.cpp); Convert button stays grayed by user decision.
- [ ] **Edit mode polish** (s34-b, user 2026-10-10): "+Text" only forwards to CmdCreateAnnotFreeText (cursor-pos heuristic + opens annotation editor) — needs dedicated click-to-place flow like +Img; Esc should cancel an armed +Img placement; placement preview bbox at cursor before click; stamp move/resize UX after placement; "+Img from file" picker; scale policy beyond the 90%-page clamp.

## Two Columns v2 — pending USER validations (code complete)

- [ ] Visual: Back/Forward after v2 toggle; page edit `3R`; reopen doc saved with v2.
- [ ] Visual: `TwoColumn2 + Trim` combo; `TwoColumn2 + R2L` (manga) — first virtual column must show the RIGHT column.
- [ ] Autoscroll + TC2 acceptance (continuous 1L→1R→2L→2R, no jump) — code-verified; user confirm on real book.

## Flashcards — open

- [ ] **FC-M1** tests: SM-2 (intervals/ease cap/relearn), study-JSON roundtrip, due filter — after Image Occlusion lands.
- [ ] **FC-M5** rate keys 1-4 have no visual hint (no rate buttons/tooltips on toolbar).
- [ ] **FC-H2** full-document annot scan under `docLock` on Toggle ON — UI freeze on huge PDFs; pre-check /Annots per page before `fz_load_page`.
- [ ] **FC-H3-left** study JSON: prune states of deleted cards (save only entries whose key matches an existing card). Atomic write + locale already fixed (s19).
- [ ] **FC-M4-left** filter expression compares PHYSICAL pageNo while TC2 shows virtual "3L" labels — decide dialog hint or virtual-input mapping.
- [ ] User live tests (queue): Filter window + global session (cross-tab jumping, per-book saves); List panel visual on real books; Settings window (folder picker + Drive backup, clear-selected, dark theme); bookmark-mirror on real TOC books (test PDF has none — automated test skipped the mirror).

## Image Occlusion cards — PLANNED (awaiting user decisions)

Boxes over images/diagrams as cards — same Highlight-annot + mask/reveal/rate pipeline as text clozes; only the DRAWING MODE at creation is new. 4 steps (IO-1 state `boxMode`+`pendingBoxes`; IO-2 canvas rubber-band before selection branches; IO-3 screen→page conversion like cloze rects; IO-4 `S` creates ONE Highlight with N quads, Esc/Box-off discards).

- [ ] **IO-D1** grouping: boxes session → 1 grouped card (recommended, matches Alt+multi-select) vs 1 card/box (Anki default)?
- [ ] **IO-D2** mode entry: toolbar "Box" button only? key `B`? both?
- [ ] **IO-D3** scope: boxes cover image/diagram regions only (no text selection) — confirm.

## s32-B — image batch live-debug (user authorized)

Replace Image / Edit With Paint / top-level Copy Image shipped s31 — s34 E2E now covers the round-trip: Replace picker→swap→writeback (s34-replace), Paint save→re-import (s34-paint), Crop drag→Save→ReplaceImageByIdx writeback (s34-crop). REMAINING (user-side): manual File > Save after an in-memory writeback to confirm on-disk persistence on a real book.

## Design rules — implementation pending

- [ ] Buttons/inputs must center content (text/icons) horizontally AND vertically (ToolbarLayout controls, timer/speed inputs, new UI elements). Rule defined 2026-09-08.

## Research (low priority)

- [ ] Image tools: right-click → 'OCR image' — find a lightweight, near-zero-dependency OCR route. Low priority.
