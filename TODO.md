# TODO ['N'/'N Total']
1. Open work ONLY.
2. Completed features/fixes live in [LOG.md](LOG.md) (per-session) + [CHANGELOG.md](CHANGELOG.md) (per-release).
3. User manual-test items = items only the user can validate.
4. Housekeeping (2026-10-20, s36-ae): all `[x]` blocks archived — details live in LOG per session entry; this file lists open work only.

## Housekeeping (quick)

- [x] ~~Old stray local branches~~ — verified s36-ah: only `main` exists (tracks `origin/main`), no `master`/`feature/*` to prune.

## Dev tooling / analysis — open items (s36-q roadmap; DONE rows archived to LOG s36-q..s36-ad)

- [ ] **A4** clangd one-time index build (shards in `%LOCALAPPDATA%\clangd\index`) — WIP; when finished, project-wide `find_symbol` stops hitting the 240 s MCP timeout on this tree (file-scoped queries already instant, see Serena recipe in AGENTS.md §6b).
- [ ] **B4** Property/invariant tests for the coordinate + pageNo namespaces: `CvtToScreen`/`CvtFromScreen` round-trip, VirtWnd local↔root bridge (`pt + bounds.{x,y}`). Kills the most repeated bug class in this repo at test level (mixed virtual/physical pageNo, screen vs page coords, hit-test spaces).
- [ ] **B6** clang-tidy sweep triage - WIP s36-r; REMAINING: `bugprone-unhandled-code-paths` 102 (backlog: needs a per-function sensible default value, not a mechanical break), roundings/unchecked-conv deferred (behavior-visible) + the 8 `clang-diagnostic-error` TUs (flag/std gap vs MSVC; platform-suffix files excluded). switch-missing-default-case 128 → 0 in wave 4a (s36-ag); narrowing 81 → 4 vendored (s36-ad); analyzer findings 42 → 32, all triaged. The ~24k stylistic bulk (signed-bitwise, macro-to-enum, named-parameter, designated-initializers, redundant-parentheses, concise-preprocessor-directives, explicit-constructor) is a separate policy decision, not a defect list. Baseline: `out/lint/clang-tidy-summary.txt`. Waves 1-4a + analyzer triage: LOG s36-r / s36-ac / s36-ad / s36-ag.

## Dependency updates — open items (F1-F5 DONE & archived: LOG s36-z + s36-aa; commits f9ab93e, feb5748, 1d30da4, 9827242, ee8a160)

- [ ] **Suite baseline (pre-existing, NOT caused by the dependency batch)** — 17 of the entries after `issue-5734` fail on this fork: `exit 5` shutdown crash (issue-5529, issue-2629, issue-4684, issue-5922, issue-5924, issue-5926, find-results-sorted, issue-5874, issue-2252), `-unit-tests exit 1` (issue-5840, issue-5842, issue-5846, parse-tip-brackets), UI/timing (issue-1914, issue-5866, issue-5869, issue-5792). Exit code **5 == 0xC0000005 access violation**, not "graceful quit" as `cmd/control.ts` claims. Reproduces only in the **debug** build (Release x64 exits 0) ⇒ debug-only shutdown defect, triage separately.
- [ ] If a real dependency regression appears: bisect by patch group (the 27 patches in `ext/patches/` are the only local delta over pristine mupdf 1.28.5; reverse-apply state: 22 applied / 19 absent / 6 conflict).
- [ ] Bun `1.3.14 → 1.4.3` (build/test scripts only) after re-running the suite.
- [ ] **F6 — explicitly deferred**: harfbuzz 13.0.1 → 14.6.0 and freetype 2.14.3 → 2.15.0 only when mupdf bumps its pins (mupdf 1.28.5 pins exactly our versions — harfbuzz `c28ba35`, freetype `0a0221a`; upgrading independently diverges from the combination Artifex tests). synctex 1.16 / lzma 9.22b frozen upstream, no action. UnRAR metadata drift: `ext/versions.txt` says 7.1.6 but `ext/unrar/version.hpp` reports RARVER 7.11.0 (2025-03-20) — verify against rarlab before any change.

## Refactoring backlog (tree-sitter audits; completed R1/R2/R4, 16A-D, C2-C5/F6, R-P1..R-P3 → LOG)

- [ ] **R3-left**: unify TrimRect math (TrimRectPage/Inverse) — sites semantics differ (mediabox shrink vs area/pt shift); forcing a helper = render-path risk. Only with a rendering regression test.
- [ ] **R5-left**: remove v1 crop stack (`DisplayModel.h` viewportCrop v1 fields) — BLOCKED: v1 button (Scan View) still live. Do when/if v1 is retired.
- [ ] **R6-left**: `Commands_AutoScroll.cpp:35-38` reads toolbar checkbox state directly (`IsDlgButtonChecked`) — route via `SetToolbarButtonCheckedState` idiom.
- [ ] **R7 / 16C-next**: continue god-file extraction — SumatraPDF.cpp (~14.2k lines, target ~10k), then Canvas.cpp (~5.7k; needs a dependency MAP first — mouse/keyboard interleaved with rendering, no established pattern), EngineMupdf.cpp (~6.7k, decouple rendering helpers per format).
- [ ] **16D**: typed `gGlobalPrefs` accessors (152 accesses in SumatraPDF.cpp) — only when tests exist.
- [ ] **TC2 leftovers**: uia `PageProvider` + legacy tile math not routed virtual→physical (non-cache path diverts; MVP-acceptable); favorites saved under v2 store VIRTUAL pageNo — normalize to physical at creation (same class as the FileState fix).

## Two Columns v2 — pending USER live-confirmations (code complete, all E2E-verified)

- [ ] User confirm on a real book: autoscroll acceptance over TC2 (1L→1R→2L→2R, no jump), drag speed, OCR text selection per column. The three s35 fixes (stall-on-hover, drag-slow, per-column selection) are code-verified + E2E PASS — see LOG s35/s36-l.

## Flashcards — open

- [ ] **FC-X1 live test** (shipped s36-m): cross-page cloze — user test on a real book (2-page passage).
- [ ] **FC-M1** tests: SM-2 (intervals/ease cap/relearn), study-JSON roundtrip, due filter — after Image Occlusion lands.
- [ ] **FC-M5** rate keys 1-4 have no visual hint (no rate buttons/tooltips on toolbar).
- [ ] **FC-H3-left** study JSON: prune states of deleted cards (save only entries whose key matches an existing card). Atomic write + locale already fixed (s19).
- [ ] **FC-M4-left** filter expression compares PHYSICAL pageNo while TC2 shows virtual "3L" labels — decide dialog hint or virtual-input mapping.
- [ ] User live tests (queue): Filter window + global session (cross-tab jumping, per-book saves); List panel visual on real books; Settings window (folder picker + Drive backup, clear-selected, dark theme); bookmark-mirror on real TOC books (test PDF has none — automated test skipped the mirror).

## Image Occlusion cards — PLANNED (awaiting user decisions)

Boxes over images/diagrams as cards — same Highlight-annot + mask/reveal/rate pipeline as text clozes; only the DRAWING MODE at creation is new. 4 steps (IO-1 state `boxMode`+`pendingBoxes`; IO-2 canvas rubber-band before selection branches; IO-3 screen→page conversion like cloze rects; IO-4 `S` creates ONE Highlight with N quads, Esc/Box-off discards).

- [ ] **IO-D1** grouping: boxes session → 1 grouped card (recommended, matches Alt+multi-select) vs 1 card/box (Anki default)? — NOTE (s36-m): the cross-page cloze already ships this mechanism for multi-page (`FlashcardGroupToken` + `Flashcard::extraPages`); reuse it for box groups (same card, several rect groups on one page) instead of a second scheme.
- [ ] **IO-D2** mode entry: toolbar "Box" button only? key `B`? both?
- [ ] **IO-D3** scope: boxes cover image/diagram regions only (no text selection) — confirm.

## s32-B — image batch live-debug (user authorized)

Replace / Edit With Paint / top-level Copy Image shipped s31; s34 E2E covers the round-trips (replace, paint, crop → writeback). REMAINING (user-side): manual File > Save after an in-memory writeback to confirm on-disk persistence on a real book.

## Design rules — implementation pending

- [ ] Buttons/inputs must center content (text/icons) horizontally AND vertically (ToolbarLayout controls, timer/speed inputs, new UI elements). Rule defined 2026-09-08.

## Test harnesses (tests/tmp/ — scratch, E2E-class until promoted to ad-hoc)

Scratch E2E harnesses live in `tests/tmp/` (gitignored): s34-replace/paint/crop/edit-e2e, s34-reload-colors, s34c-inpage-e2e, s34e-doubletoggle (toolbar rebar heights across on/off cycles), s34g-findcrash (parametric crash repro: any book/query), s34c-tb-probe, s35-tc2-auto (autoscroll stall probe: idle/storm/post-storm pixel-hash), s35-tc2-sel (per-column text selection), s33-arch-repro (arch tools flow — promote to ad-hoc if not registered in before-release). Reusable helpers: `tests/winapi.ts`, `tests/win-automation.ts`; DPI + postMessage traps documented in lemma (m9fa780f3cf0b, m5317f7f3eb3d).

## Research (low priority)

- [ ] **OCR image** (right-click on image → OCR text): upstream vendors tesseract 5.5.2 (Artifex fork) for `mutool draw -F ocr.*` — cheapest path for the fork; decide separately (binary size cost). Lightweight near-zero-dependency alternatives also acceptable.

## Bookmarks Improve

1. 'Reorder' on/off: allows click:hold and repositioning bookmark order.
   - Dropping a bookmark onto another includes it as 1 indent level; dropping at the far left removes it from the level;
   - Show a dynamic horizontal line representing where the bookmark will land on drop, depending on cursor position.
2. 'Rename': Right-Click → Rename.
3. 'Delete': Right-Click.
4. 'Add': creates a bookmark at the end of the list for the CURRENT page.
5. 'Update': Right-Click → changes the clicked bookmark to the current page.
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

- [ ] **BUG-3** right-click image context menu: s34 E2E confirmed Replace/Paint/Crop work + submenu visibility fixed; REMAINING live-check: top-level Save-as path (CmdSaveImage → ShowImageEditWindow Save mode) and the non-image items (Copy-link etc.) reported dead. May be unimplemented upstream — investigate before fixing.
- [ ] **BUG-4** Contrast × Trim Config ONE-SHOT vanish (first trim-config open after launch). Fix v2 (`EnsureContrastOverlayState` invariant + self-healing) deployed 2026-09-22 — awaiting user confirmation of the exact flow; `[contrast]` breadcrumbs in debug build.
- [ ] **Ctrl+click flashcard highlight opens List** — implemented s34-b (Canvas ctrl+click detects /T author TumatraPDF-Flashcard → FlashcardSidebarShow; turns flashcard mode on if off). USER live-test pending (needs a book with flashcard highlights).
- [ ] **s34-g user validation pending**: Arch Measure Radius + Ref Line UX, resize-enlarge on a real book, flicker-free drag confirm (shipped + E2E 2026-10-11).
- [ ] **Dark-mode chrome yellow** (from s36-aa darkmodelib verification, NOT a regression): dark-theme chrome paints a saturated yellow (≈ rgb 245,237,19) on this machine in BOTH old and new darkmodelib builds; PrintWindow and BitBlt agree. Investigate source (accent color / theme preset).

## USER reports — open

Shipped in **s36-ah** (details + verification in LOG): twocolumn/trim now per file and OFF for new files; per-file trim config + autoscroll speed; Trim Config pauses autoscroll; master reading time (daily buckets, survives rename/delete/move); Home `Today | Total` + contrast; Ctrl+Shift+T restores the whole closed window; TC2 bookmark lands on the LEFT page.

- [ ] melhorar o passo do zoom — **implemented, awaiting your verdict** (s36-ah): zoom levels refined to a ~1.33 ratio (added 10/18/250/500/1000/2000/4000 %). One-line revert if you dislike it.
- [ ] Checkbox do Timer ainda sai da estrutura do botão cobrindo outros botões, investigar a fundo como fixar a posição e corrigir — **OPEN with evidence** (s36-ah): the trio is anchored to `TbGetRect(CmdAutoScrollToggle)` measured BEFORE `EnforceToolbarTextButtonWidths` widens that button by the trio width; after a 2-3 row wrap it bleeds 4 px into the row above, and the position is never recomputed once the wrap settles. Fix belongs in `ToolbarLayout.cpp PositionToolbarChildWindows` (clamp to the anchor + re-run after wrap) and needs visual verification.

## Vertical_Toolbar (PARKED — DO NOT DO)

Idea frozen by the user ("NAO FAZER") — kept for reference only: when the window is narrower than 1/3 of the screen, adapt the toolbar to a left vertical bar (buttons top-to-bottom, converted to symbols with name on hover), keeping a top horizontal bar only for numeric inputs (page, timer) and remaining buttons; when home=on keep hiding all buttons.
