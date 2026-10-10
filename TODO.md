# TODO

Open work ONLY. Completed features/fixes live in [LOG.md](LOG.md) (per-session) + [CHANGELOG.md](CHANGELOG.md) (per-release). User manual-test items = items only the user can validate.

## Housekeeping (quick)

- [ ] Old stray local branches (`master`, `feature/*`) vs git-flow conventions — prune after confirming nothing unmerged.
- [ ] TODO BUG-3 verify: right-click image menu flagged "most functions broken" once (may be unimplemented upstream) — live-check whether Crop/Resize/Replace/Paint forward path works; BUG-3 + s32-B can be tested together.

## Refactoring backlog (from 2026-09-21 tree-sitter audit — see LOG entries for completed R1/R2/R4, 16A-D, C2-C5/F6, SelectionState)

- [ ] **R3-left**: unify TrimRect math (TrimRectPage/Inverse) — sites semantics differ (mediabox shrink vs area/pt shift); forcing a helper = render-path risk. Only with a rendering regression test.
- [ ] **R5-left**: remove v1 crop stack (`DisplayModel.h` viewportCrop v1 fields) — BLOCKED: v1 button (Scan View) still live. Do when/if v1 is retired.
- [ ] **R6-left**: `Commands_AutoScroll.cpp:35-38` reads toolbar checkbox state directly (`IsDlgButtonChecked`) — route via `SetToolbarButtonCheckedState` idiom.
- [ ] **R7 / 16C-next**: continue god-file extraction — SumatraPDF.cpp (~14.2k lines, target ~10k), then Canvas.cpp (~5.7k; needs a dependency MAP first — mouse/keyboard interleaved with rendering, no established pattern), EngineMupdf.cpp (~6.7k, decouple rendering helpers per format).
- [ ] **16D**: typed `gGlobalPrefs` accessors (152 accesses in SumatraPDF.cpp) — only when tests exist.
- [ ] TC2 leftovers: uia `PageProvider` + legacy tile math not routed virtual→physical (non-cache path diverts; MVP-acceptable); favorites saved under v2 store VIRTUAL pageNo — normalize to physical at creation (same class as the FileState fix).

## Bugs open

- [ ] **BUG-3** right-click image context menu: only Copy-to-clipboard confirmed working; Save-as/Copy-link etc. dead. Investigate before fixing (may never have been implemented upstream).
- [ ] **BUG-4** Contrast × Trim Config ONE-SHOT vanish (first trim-config open after launch). Fix v2 (`EnsureContrastOverlayState` invariant + self-healing) deployed 2026-09-22 — awaiting user confirmation of the exact flow; `[contrast]` breadcrumbs in debug build.

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

Replace Image / Edit With Paint / top-level Copy Image shipped s31, live-tested only partially — verify round-trip: Replace Image picker→swap→save; Paint save→re-import; manual PDF save after (XObject swap via ReplaceImageWithFile/ReplaceImageByIdx + GetImageElementCoords).

## Design rules — implementation pending

- [ ] Buttons/inputs must center content (text/icons) horizontally AND vertically (ToolbarLayout controls, timer/speed inputs, new UI elements). Rule defined 2026-09-08.

## Research (low priority)

- [ ] Image tools: right-click → 'OCR image' — find a lightweight, near-zero-dependency OCR route. Low priority.
