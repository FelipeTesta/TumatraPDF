# TODO
1. Open work ONLY.
2. Completed features/fixes live in [LOG.md](LOG.md) (per-session) + [CHANGELOG.md](CHANGELOG.md) (per-release).
3. User manual-test items = items only the user can validate.

## Housekeeping (quick)

- [ ] Old stray local branches (`master`, `feature/*`) vs git-flow conventions — prune after confirming nothing unmerged.
- [ ] TODO BUG-3 verify: right-click image menu flagged "most functions broken" once (may be unimplemented upstream) — live-check whether Crop/Resize/Replace/Paint forward path works; BUG-3 + s32-B can be tested together.

## Refactoring backlog (from 2026-09-21 tree-sitter audit — see LOG entries for completed R1/R2/R4, 16A-D, C2-C5/F6, SelectionState)

## Render performance — optimization points (tree-sitter audit 2026-10-10, s34-c; P1 DONE 2026-10-11)

- [x] **R-P1 `BlitPixmapRegion` HALFTONE** (DONE s34-c 2026-10-11): stretch mode now only set when actually stretching (isStretch flag); 1:1 BitBlt tiles skip SetStretchBltMode entirely. Verified: s34-crop-e2e PASS.
- [x] **R-P1 arch-tools GDI+ churn (fork)** (DONE s34-c 2026-10-11): early-out (nothing to draw on page → skip Graphics + all ctors) + shared static Gdiplus Pen/Brush/Font objects (app never calls GdiplusShutdown → statics safe); dashed in-progress line preserved. Verified: s33-arch-repro ALL PASS.
- [x] **R-P1 `DrawDocument` O(N) loop** (DONE s34-c 2026-10-11): binary-searched [firstPage,lastPage] window over monotonic pageOnScreen.y (continuous modes, nPages > 64) + row-mate walk-back + ±2 slack; in-loop visibleRatio/isShown guards unchanged; non-continuous/degenerate geometry = full loop (safe degradation). Verified: crop + arch E2E (page-1 render + pan to page 3).
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


## Two Columns v2 — pending USER validations (code complete)

- [ ] Visual: Back/Forward after v2 toggle; page edit `3R`; reopen doc saved with v2.
- [ ] Visual: `TwoColumn2 + Trim` combo; `TwoColumn2 + R2L` (manga) — first virtual column must show the RIGHT column.
- [x] Autoscroll + TC2 acceptance (continuous 1L→1R→2L→2R, no jump) — code-verified; user confirm on real book.
- [x] **TC2 stall on mouse-hover (s35, 2026-10-12):** flaky stall with TC2+autoscroll+trim+contrast+invert — root: layout rebuild mid-scroll makes `IsAtDocumentEnd()` spike true (transient canvas geometry) → the tick KILLED the autoscroll timer permanently. Fix: skip the tick when GetCanvasSize() invalid (mid-rebuild). Probe `tests/tmp/s35-tc2-auto.ts` 3/3 SCROLLING after fix (was 1/2 stall before). User confirm on real book.
- [x] **TC2 drag-slow (s35):** `RecalcVisibleParts` rebuilt a full DocumentLayout + copied per page on EVERY scroll tick (2× cost under TC2 virtual pages). Now recomputed directly on pagesInfo (allocation/copy round-trip gone). User confirm.
- [x] **TC2 OCR/text selection broken (s35):** `DisplayModel::IsOverText` passed the VIRTUAL page into the physical-page TextSelection store → no hit anywhere virtual≠physical (right column or page≥2) → drag panned instead of selecting. Fixed with `VirtualToPhysical` (one-line, convention from the other callers). E2E `tests/tmp/s35-tc2-sel.ts`: right-column drag now selects (254 chars) + left/whole-page unchanged.

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

## Test harnesses (tests/tmp/ — scratch, E2E-class até virarem ad-hoc)

- s34-replace-e2e.ts, s34-paint-e2e.ts, s34-crop-e2e.ts — Image replace/paint/crop+menu-roundtrip.
- s34-edit-e2e.ts (10 checks), s34-reload-colors.ts, s34c-inpage-e2e.ts (7 checks, draw-rect move path).
- s34e-doubletoggle.ts — toolbar rebar heights across 2 on/off cycles (layout regression probe).
- s34g-findcrash.ts — Ctrl+F typing repro on a book (outro uso: qualquer crash repro parametrizado por livro/query).
- s34c-tb-probe.ts, s34c-tb-probe2.ts — sondas de rebar/toolbar decoupling.
- s35-tc2-auto.ts — autoscroll stall probe (idle/storm/post-storm pixel-hash; flaky repro).
- s35-tc2-sel.ts — TC2 text selection por coluna (drag + clipboard readback).
- s33-arch-repro.ts — arch tools flow completo (scale/measure/pan/gates) — registrado em before-release? → virar ad-hoc se não estiver.
- Util: tests/winapi.ts, tests/win-automation.ts; DPI traps e postMessage traps documentados no lemma (m9fa780f3cf0b, m5317f7f3eb3d).

## Research (low priority)

- [ ] Image tools: right-click → 'OCR image' — find a lightweight, near-zero-dependency OCR route. Low priority.

## Bookmarks Improve
1. 'Reorder' on/off: permite click:hold e reposicionar a ordem dos bookmarks
    - Soltar o bookmark sobre outro, o inclui como 1 nível indent, soltar no extremo esquerdo retira por fora do nível;
    - Exibir uma linha horizontal dinâmica que representa onde o bookmark vai entrar ao soltar, dependendo da posição do cursor
2. Rename: Click-Right (CR) → Rename
3. Delete: CR
4. 'Add': Cria um bookmark no final da lista com a página ATUAL
5. Update: CR → Altera o bookmark clicado para a página atual
Todas as alterações: Salvar no documento para persistir.

## Page Counting Format
Similar ao sistema adobe, queremos uma ferramenta para mudar o formato de contabilizar alguns intervalos de página, especialmente para início de documentos.

Exemplo: página 1-50 contabilizar em romano, da página 51 em dia, numérico → A página 51 se torna a nova página 1. Digitar 1 no campo de páginas, cai na página 51. Dessa forma o match das páginas dos livros fica exata.
Podemos criar uma função página absoluta, caso queremos ignorar as contagens consméticas; nesse exemplo: pág 1 → pag 51; pág (1) → pág 1.

# Bugs open

- [x] **FIXED 2026-10-11: CRASH on search (Ctrl+F)** — dump 34131b symbolized (dbg exe has symbols): `DisplayModel::PhysicalToVirtualForRect` wrote `*rectOut` unconditionally, and the FindThread `searchVisible` probe (SearchAndDDE.cpp:1357) was the ONLY caller passing nullptr → null-write on every search that reached that branch. Guard added on both branches (null rectOut ⇒ identity rect, return unmapped page). Real-doc E2E (ACM query + 2× Enter on the CAD esquadrias PDF) now crash-free.
- [x] **s34-g SHIPPED (2026-10-11, built+deployed, not committed):** resize enlarges BEYOND the original image rect (content-stream cm rewrite `SetImageDrawRect` + data swap; fallback = clamped padded swap on rotated/nested pages); flicker-free bbox drag (region-only invalidate); scale label text → white; Arch **Measure Radius** (3 clicks, r= label) + **Ref Line** (2 clicks, subtle full-page gray) — commands 523/524, persisted via measurements `kind`; Enter/numpad-Enter applies from the scale numeric field (edit subclass). USER validation pending: radius/refline UX, enlarge on a real book, flicker gone.

- [x] **NEW 2026-10-10** Saving the document DEACTIVATES invert + contrast — FIXED s34-b: ReloadDocument now carries invertColors/contrastEnabled/contrastOpacity into the fresh FileState (E2E s34-reload-colors PASS; post-reload canvas byte-identical).
- [x] **NEW 2026-10-10** Color inversion of IMAGES is very bad — FIXED s34-b (user decision: true negative): session invert now per-pixel 255-v via InvertPixmap/InvertBitmapColors instead of the theme-color ramp + link-substitution (both corrupted images). Live-visual confirm pending.
- [ ] **NEW 2026-10-10** Ctrl+click to edit a highlight is SLOW — instrumented s34-b with `[perf] ShowEditAnnotationsWindow` logs; root cost candidate: first click builds window + EngineMupdfGetAnnotations (per-page load, whole doc, FC-H2 class). Fix direction: async/lazy list build — needs `[perf]` numbers from a real book first.
- [ ] **NEW 2026-10-10** Ctrl+click on a FLASHCARD highlight opens the flashcard List — IMPLEMENTED s34-b (Canvas ctrl+click detects /T author TumatraPDF-Flashcard → FlashcardSidebarShow; turns flashcard mode on if off). USER live-test pending (needs a book with flashcard highlights).
- [x] **NEW 2026-10-10** Flashcard hover label shows "testa" — RESOLVED 2026-10-10: "testa" is the annotation /T author (defaultAuthor → `%USERNAME%`, Annotation.cpp GetUserTemp :1384). User reviewed and decided to keep author behavior as-is (it only persists in the file on File > Save; `(none)` in `annotations.defaultAuthor` suppresses it).
- [ ] **BUG-3** right-click image context menu: s34 E2E confirmed Replace/Paint/Crop work + submenu visibility fixed; REMAINING live-check: top-level Save-as path (CmdSaveImage → ShowImageEditWindow Save mode) and the non-image items (Copy-link etc.) reported dead. May be unimplemented upstream — investigate before fixing.
- [ ] **BUG-4** Contrast × Trim Config ONE-SHOT vanish (first trim-config open after launch). Fix v2 (`EnsureContrastOverlayState` invariant + self-healing) deployed 2026-09-22 — awaiting user confirmation of the exact flow; `[contrast]` breadcrumbs in debug build.
- [x] **Image editing functions (user 2026-10-10)** — Replace/Paint/Crop/Resize all FIXED s34 (E2E pass: `tests/tmp/s34-{replace,paint,crop}-e2e.ts`); root cause was a no-op image-submenu pruning bug (base `MenuRemove`/`MenuSetEnabled` don't descend into submenus) + WStrL/order bugs. Modularization into `Commands_Image.{h,cpp}` DONE s34-c 2026-10-11 (Menu.cpp dispatch stays — it's menu plumbing, not image logic). Convert button stays grayed by user decision.
- [x] **Edit mode polish (s34-d/e/f, 2026-10-11):** in-page bbox editing SHIPPED (click image → bbox over page, border/edge/inside drag; E2E 7/7). s34-e: round resize/square crop handles; live preview; fc/edit 2nd-toolbar double-toggle dead space fixed (uiState.layout force-clear + rebar pre-size). s34-f: ALL edits = pure image-data swap (stamp path REMOVED per user — footprint fixed, enlarge limited to the original image area); bbox tracks the applied region (`PageImageEdit.content`); 🔴 marker via ToolbarUpdateStateForWindow. Remaining polish: "+Text" click-to-place flow; Esc cancels selection; preview bbox for armed +Img; stamp move/resize UX after placement; "+Img from file" picker.

[] Quando Two columns=on & Autoscroll=on & Trim=on & Contrast=on & Invert=on, ao mover o mouse por cima da tela, o autoscroll para. Avaliar. Pode estar relacionado com o peso do arquivo.

Quando Twocolumns=on, clicar e arrastar a página está lento/travando;

Quando Twoxolumns=on, não consigo selecionar o texto OCR para anotações ou flashcards;
### 2026-10-15
[x] autoscroll:on & twocolumn:on → shift:hold → não consigo arrastar a página com o touchpad do notebook — FIXED s35-b (2026-10-15): causa = WM_MOUSEWHEEL (swipe vertical do touchpad) chega com MK_SHIFT (Shift é o modificador do quick view) e era remapeado como scroll HORIZONTAL (Canvas.cpp CanvasOnMouseWheel hScroll check) — sem espaço horizontal no quick view = nada rolava. Quick view ON agora força scroll vertical. E2E: tests/tmp/s35b-quickview.ts (wheel MK_SHIFT scrollY 3→23 OK + drag pan OK).
[x] autoscroll:on & twocolumn:on → estou lendo o lado DIREITO da página → shift:hold → shift:release volta para o lado ESQUERDO da página — FIXED s35-b (2026-10-15): QuickToggleViewportCropV2 restaurava sempre `physAnchor*2-1` (coluna ESQUERDA). Agora captura a coluna na ENTRADA do quick view (`viewportCropV2SavedColumn`, ColumnOfVirtual) e restaura nela na saída (RestoreViewportAfterV2Toggle ganhou param `column`, default 0 = toggle regular inalterado). E2E: página virtual 2 antes → 2 depois do release (coluna direita mantida).

## Função Stats
A ideia dessa função é incluir estatísticas de leitura na metade superior do "home".
### Como?
Sempre que usarmos a função "Autoscroll", vamos capturar e somar o tempo de leitura ao usuário.
Depois queremos visualizar o tempo de leitura TOTAL, e tempo de leitura por cada livro/documento e o tempo de leitura Diário.
Precisamos desenvolver uma página agradável visualmente e dinâmica.

|Tempo de Leitura Total: 00h 00m |
|Livros mais lidos:|
|Livro1 - 00h 00m|Livro4 - 00h 00m|
|Livro2 - 00h 00m|Livro5 - 00h 00m|
|Livro3 - 00h 00m|Livro6 - 00h 00m|
|Calendário:|Grade de 365 Quadrados pequenos ⬜⬛(1 por dia do ano, incluir setas '←' 2026 '→' para avançar ou retornar), cada um representa um dia, quando mouse:hover exibe as horas lidas aquele dia ('1h 00m'); usar gradiente de cores para os dias com maior tempo de leitura, deixar sem cor quando 0 (Ver review heatmap do ANKI)
|[M]x54|
|[T]x54|
|[W]x54|
|[T]x54|
|[F]x54|
|[S]x54|
|[S]x54|
|Gráfico anual (Dias x Horas Lidas)|

### Relacionados
Ao minimizar a página, ou mudar de janela windows, parar autoscroll automaticamente.