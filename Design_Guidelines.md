# Design Guidelines — TumatraPDF

Reference doc for agents/humans writing code in this repo. Distills C++ Core
Guidelines, cppbestpractices, refactoring.guru, MuPDF docs + 8 fork sessions of
real bugs (LOG s1-s23). **AGENTS.md wins all conflicts.** Language: EN + caveman.
Created 2026-09-30. Upstream-verbatim files (`sumatrapdf-src/AGENTS.md`,
`docs/md/`) untouched by this doc.

## Sources

- C++ Core Guidelines (isocpp, Jun 2026) — P, R, E, I, Per, A, NL sections
- cppbestpractices (J. Turner) — style chapter
- refactoring.guru — 22-pattern catalog + code smells
- MuPDF vendored docs (`sumatrapdf-src/mupdf/docs/`) — fz context, coordinates
- SumatraPDF upstream AGENTS — house conventions
- Project history: LOG.md s1-s23, lemma memories, FLOW/*.dot

## 1. Core principles (adapted from Core Guidelines P)

- **P.1/P.3 express intent**: name says what. `FlashcardEnsureTabCards` good.
  `f(win, -1, 0)` bad — bundle args into struct (I.23: <4 params or bundle).
- **P.4/P.5 type safety**: no `void*` APIs in new code. Str `{ptr,len}` carries
  size = bounds checkable (I.13). No `(ptr, count)` pairs — pass `Str`/`Vec`/span-like.
- **P.8 no leaks**: RAII for every Win32/mupdf resource (see §3).
- **P.9/P.10 no waste**: `const` by default; immutable data preferred.
- **P.11 encapsulate messy**: WndProc casts, GDI churn, mupdf calls stay inside
  wrapper functions — never leak into feature logic (I.30).
- **P.12/P.13 tools + libs**: clang-format mandatory (`sumatrapdf-src/.clang-format`);
  reuse `base/` (Str, Vec, StrVec, File, path) before writing new helpers.
- **A.1 stable vs unstable**: upstream code = stable, fork patches = unstable.
  Keep fork features in NEW files (`Commands_Flashcard.cpp`, `FlashcardToolbar.cpp`...)
  — eases upstream merge, isolates testing. Never rewrite upstream files when a
  new-file patch works (MERGE surface).
- **A.2 reusable parts as library**: shared helpers live in `Flashcard.h/.cpp`
  (e.g. `FlashcardDrawHoldButton` reused by 2 windows) — extract on 2nd use.
- **I.2/I.3 globals**: upstream uses globals (`gGlobalPrefs`) — follow upstream,
  don't fight it; new fork code prefers params + single owner structs.

## 2. Code conventions (upstream + Turner)

- Include order critical (AGENTS §4): `base/Base.h` PCH first → base → wingui →
  project deps (Settings→DisplayMode→DocController→DocProperties→TreeModel→
  EngineBase→DisplayModel→EngineMupdf) → others → **own header LAST**. Wrong order
  = 30-300+ cascade errors. Headers NOT self-sufficient (PCH pattern).
- `#ifndef`/`#define` guards only, never `#pragma once`.
- No prose comments in `.h` — comment lives on `.cpp` definition. Struct/enum/
  macro/constant comments stay in `.h`.
- Strings: `StrL("...")`/`WStrL(L"...")` literals (compile-time len), never
  `Str("...")`. `fmt()` type-safe — pass Str OBJECTS (`%s` takes `Str`, `.s` =
  compile error by design). `len(x)` over `x.len`.
- NUL-termination before C/Win32 APIs: `CStrTemp`/`CWStrTemp` or
  `ToWStrTemp`/`ToUtf8Temp` (objects, not `.s`).
- `nullptr` not `NULL`/`0`. `{}` init preferred (no narrowing). `explicit`
  single-arg ctors. No `using namespace` in headers.
- Member init list / default member init — never "forget" a field (uninit = UB).
- No STL: own containers. Ownership rule (I.11/R.3 adapted): `Vec<T>` OWNS, `Str`
  is a non-owning VIEW, `TempStr` = temp arena (reset per message — never stash).
  Who frees a raw pointer must be obvious from type or comment.
- Rule of zero adapted: don't hand-write copy/move/dtor unless novel ownership.
- Comments: `//` only. State WHY not WHAT (NL.2). Code says what, comment says
  contract/intent.

## 3. Resource & error handling (R + E mapping)

- **RAII everywhere**: acquire in ctor, release in dtor. Existing:
  `ScopedRecursiveMutex` (mupdf docLock), `ScopedGdi`, `ScopedFont`. New paired
  Win32 acquire/release → wrap in scoped struct, never manual `DeleteObject`
  across early returns.
- **mupdf**: every `fz_*` call that can throw → `fz_try/fz_catch` +
  `fz_report_error`. Markup annots: use `pdf_bound_annot`, never
  `pdf_annot_rect` (crash — LOG s8). Lock with `ScopedRecursiveMutex(&docLock)`.
- **Fail fast, no exceptions**: repo style = `CrashIf` (debug) + guard clauses
  (E.26/E.25). Handlers start with `if (!win || !tab) return;` — validate early,
  never deep-null-deref.
- **Destructors never fail** (E.16): Vec/Str dtors must tolerate partial state.
- **Atomic persistence**: write tmp + `MoveFileExW(REPLACE_EXISTING)` — pattern
  already in `FlashcardStudySave` (Flashcard.cpp:219), `File_win.cpp`. ALL JSON
  saves follow it. Never write directly over a file being read.
- **Error returns carry log**: on failure path `logf("[tag] ... ERROR: reason")`
  before return — every fork feature logs with a `[fc]`-style tag (user request:
  logs must identify bugs without debugger).
- No global error state (`errno`-style) (E.28). Return values + logs.

## 4. Patterns mapped to this codebase

| Pattern                 | Use here                                                    | Example                                                                          |
| ----------------------- | ----------------------------------------------------------- | -------------------------------------------------------------------------------- |
| Command                 | generated dispatch table, never hand-switch in new code     | `Commands.h`, `HandleCommandFlashcard`                                           |
| State                   | split SESSION state vs DOCUMENT state                       | `MainWindow::flashcard` (session) vs `WindowTab::flashcard` (per-book) — s22 fix |
| Strategy                | order settings picked at runtime                            | `NewCardsPosition` combine in `BuildFilteredStudyOrder`                          |
| Facade/lazy             | one entry point hides loading                               | `FlashcardEnsureTabCards`                                                        |
| Observer-lite           | explicit refresh hooks after mutation                       | `FlashcardSidebarPopulate` called by rate/clean/add                              |
| Template method         | paint hooks in big WndProc blocks                           | flashcard paint inside Canvas paint                                              |
| Owner-draw + data model | numeric state in `Vec<FcListRow>`, strings in listbox items | Lista panel (s23) — allocation-free draw                                         |

Code smells to avoid (refactoring.guru):

- Long method: giant WndProc → extract per-message handlers (paint block,
  hit-test block). Target: one screen per function.
- Duplicate code: 3 flashcard dialogs repeat window-class + modal-pump +
  parent-disable + DPI scaffolding (~150 lines each) — extract shared base
  (see §8 improvements #1) BEFORE writing dialog #4 (IO dialogs upcoming).
- Switch abuse: dispatch tables fine; feature-internal switch on state = use
  function table or split structs.
- Data clumps: (pageNo, rect, engine) traveling together → pass the owning
  struct (WindowTab/card), not loose args.

## 5. Performance (Per mapping)

- **Per.1/3/6**: don't optimize blind — measure. `-bench`, `-stress-test`,
  `[fc]` logs with counts. No perf claims without numbers.
- **Per.14/15**: minimize allocs in hot paths. Paint path (`WM_PAINT`, owner-draw
  item draw): ZERO heap allocs for state — numeric cache structs (`FcListRow`)
  drawn raw; strings precomputed. Temp arena OK (reset per message) but prefer
  precomputed data. Page-text extraction = cache per page (Lista pattern),
  never re-extract per draw.
- **Per.16/19**: compact linear data (`Vec<RectF> rects`), linear sweeps.
  Avoid pointer-chasing graphs for per-frame data.
- **Per.17**: hot fields first in time-critical structs.
- **Rescan vs incremental**: `FlashcardLoadFromDocument` = full annot scan; called
  on toggle/add/tab-change only (lazy `cardsLoaded` flag) — keep it that way.
  FC-H2 (big-doc scan cost) = known backlog; if touched, cache by mtime.
- **Cache invalidation rule**: every cache names its invalidator (doc reload,
  tab change, annot edit) in a comment. Stale cache = invisible bug.
- **Render invalidation**: `MainWindowRerender`/`InvalidateRect` after state
  changes — no manual paint outside WM_PAINT.

## 6. UI/UX — Win32 dialog + panel rules (from s15-s23, validated live)

- **DPI**: every metric through `DpiScale(hwnd, n)`. Paint helpers take HWND
  (bare `hwnd` out of scope = C2065). Window sizes `DpiScale`d at creation.
- **Keyboard-first**: Esc/X = cancel (WM_CLOSE), Enter = apply (EDIT subclass
  VK_RETURN), Space/Enter = reveal in study. All actions reachable without mouse.
- **Destructive actions**: hold-to-confirm (`FlashcardDrawHoldButton` —
  orange 2s minor, red 5s major). Minor clearing = shorter hold. Never a plain
  "Yes" click for deletions.
- **Instant apply**: option dialogs apply per click, no OK button (Order s20,
  Filter s22). Checkbox/bookmark toggles act immediately + keep window open.
- **State visible**: toolbar button CHECKED = feature active (Filter btn while
  filter on). Count labels scope-aware ("N cards" current-doc vs session).
- **Owner-draw listbox**: `LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|WS_VSCROLL`,
  item data = index into Vec model. TRAP: WM_MEASUREITEM fires at CreateWindowEx
  BEFORE subclass exists → measure after subclassing + force
  `LB_SETITEMHEIGHT`. Never wrap managed HWNDs in `wingui::Wnd` (subclasses on
  attach, destroys on dtor).
- **Modal pump**: dialog = own window class (TUMATRA_FLASHCARD_*), parent
  disabled, pump until close. `Foo dlg;` NEVER `Foo dlg{};` (Vec member → C2512).
- **Panels**: docked children of hwndFrame with own Splitter (isLive=false),
  manual WM_SIZE layout (no VBox for canvas-adjacent slots) — AI-chat pattern.
- **Colors**: state colors readable in dark+light (blue/orange/purple/green
  status set s23). Selected-row keeps readable status color.
- **Flicker**: targeted InvalidateRect; WS_EX_COMPOSITED for complex owner-draw
  children if flicker observed.
- **Feedback**: every mutation → log line + count update + (if list open)
  list refresh. Silent state change = bug factory.

## 7. Dual namespaces — HARD rules (project bug history)

- **virtual pageNo vs physical pageNo** (TC2 crop): features MUST route through
  `PhysicalToVirtualForRect` / `VirtualToPhysical...`; check
  `IsViewportCropV2Active` for layout truth. Name vars `vPage`/`pPage` — never
  bare `pageNo` in mixed code (TC2 FASE 5/6 bugs, cloze mapping s8).
- **screen coords vs page coords (PDF points)**: `CvtToScreen` returns
  viewport-relative (moves with scroll) — delta-scroll idiom, not absolute
  positioning. Card bounds = PAGE coords; masks painted via per-quad rects.
- Any new coordinate crossing = comment naming both spaces at the crossing.
  Trace call-sites with tree-sitter, not eyeball (pervasive mixing broke
  features twice).

## 8. High-return / low-risk improvements (backlog candidates)

| #   | Improvement                                                                                                            | Where                                  | Risk                         | Return                                   |
| --- | ---------------------------------------------------------------------------------------------------------------------- | -------------------------------------- | ---------------------------- | ---------------------------------------- |
| 1   | ✅ DONE s24 — Extract shared dialog scaffolding (class reg + create-center + modal pump) — used by Order/Filter/Config | FlashcardToolbar.cpp                   | low (new helper, mechanical) | high: kills 3x dup, next 4 dialogs cheap |
| 2   | vPage/pPage naming rule + coordinate helpers audit                                                                     | Canvas/DisplayModel call-sites         | none (naming)                | high: prevents recurring bug class       |
| 3   | ✅ DONE s24 — Refresh-hook audit: Clear-ALL now calls FlashcardSidebarPopulate                                         | Commands_Flashcard.cpp + Config window | low                          | high (systemic bug family)               |
| 4   | ✅ DONE s24 — Extract flashcard paint block from Canvas into `PaintFlashcardMasks()`                                   | Canvas.cpp                             | low (mechanical move)        | medium: readable paint chain             |
| 5   | ✅ DONE s24 — xorshift128+ shuffle (rand()%n biased, weak)                                                             | Commands_Flashcard.cpp                 | low                          | medium: quality random for study order   |
| 6   | Deferred — ExtractPageText page-cache generalize (no second consumer yet)                                              | FlashcardSidebar.cpp → EngineBase util | low                          | low now (speculative abstraction)        |
| 7   | ✅ DONE s24 — clang-format of modified files inside cmd/build.ts (vendored excluded)                                   | cmd/build.ts                           | low                          | medium: consistency enforcement          |
| 8   | Deferred — FC-H2: annot scan cache by mtime (big docs; dedicated session)                                              | Flashcard.cpp                          | medium (annot mutations)     | high for large books                     |

FC-M1 (SM-2 unit tests), IO-1..4 (image occlusion) already in TODO.md — not
duplicated here.

## 9. Known traps (project history — read before touching these areas)

- `Vec() = default` explicit → `Foo dlg;` not `Foo dlg{};` (C2512).
- `Fmt` args: Str objects, never `.s` (deleted ctor — compile error, by design).
- WStr has NO implicit `WCHAR*` conversion — DrawTextW/LB_ADDSTRING take `.s`.
- Headers not self-sufficient: include chain before WindowTab.h = Settings→
  DisplayMode→DocController→DocProperties→TreeModel→EngineBase→EngineMupdf→
  base/GuessFileType.h→EngineAll.h. Missing link = cascade errors.
- WM_MEASUREITEM ordering (see §6). `static` fn used before definition needs
  forward decl (FlashcardNavigateToCard ↔ FlashcardApplyStudyOrder).
- Nested struct full qualification: `MainWindow::ArchToolsState::ArchScaleState`.
- NEVER PowerShell `Set-Content`/`WriteAllText` for file content — corrupts
  UTF-8 (mojibake). Edit tool only, even for mechanical renames.
- NEVER `2>nul` (NTFS undeletable file) — `2>/dev/null`. No `&&` (PS 5.1) — `;`.
- premake5 regen strips fork vcxproj sources → LNK1120 later; stash-restore
  `vs2022/` after any premake run.
- Build: MSBuild only via hard path; `cmd/build.ts` overwrites
  `Compiled\TumatraPDF.exe` with DEBUG exe — re-copy `out\rel64` after final build.
- Tests: `cmdId("CmdName")` never hardcode ids (lint enforces); log-SLICE
  assertions (rotation-safe); GUI card-creation needs RETRY loop; tab navigation
  BY FRAME TITLE (About tab shifts indices); long repro fail → run minimal
  isolation test before touching code (s21 lesson).
- Posted WM_KEYDOWN does NOT update GetKeyState — synthetic-input tests use
  wParam flag fallback (MK_ALT pattern).
- Per-exe portable app-data: study JSONs keyed MD5(filePath) under
  `FlashcardStudy\` (or the configured study dir) — Config window's clear-selected keeps per-book isolation (s24).
- Hand-rolled JSON parsing: when searching a field NAME (`"rating"` without
  quotes), the match position + name length lands ON the key's CLOSING QUOTE —
  skip it before expecting the `:` (s24 parser bug: every StudyLoad returned
  all-zero states since s14; found via the Config window's stats).
- `compactStruct` settings accept Bool/Int/Float/Color ONLY — a Str field makes
  the startup validator `ReportIf(!IsCompactable(...))` fire (exit 105 "debug
  report, not crash" in sumlog). Str settings go in non-compact structs /
  top-level (pattern: `flashcardStudyDir` on GlobalPrefs).
- `gen-code.ts` needs `cl` on PATH (dies at genVirtKeys otherwise) — settings
  alone: `bun cmd/gen-settings.ts` runs standalone and writes both parts.
