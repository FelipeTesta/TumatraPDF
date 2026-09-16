# TumatraPDF — Development Log

## 2026-09-16 — Markdown Full-Width + Flashcard/Trim Fixes Batch (BUILD OK)

### Markdown full-width reading
- `MarkdownToc.cpp` `kMarkdownPageCssFmt`: removed `max-width: 980px` + reduced padding (`2rem 3rem` → `1.5rem 2rem`) on body. Text now fills the whole window width and wraps word-by-word dynamically on resize (WebView2 native reflow) — Notepad-style space usage. MuPDF fallback path unchanged.

### Flashcard highlight system (from prior session, now committed)
- `Flashcard.cpp`: load `PDF_ANNOT_HIGHLIGHT` (was FREE_TEXT) + author filter `TumatraPDF-Flashcard`
- `Commands_Flashcard.cpp`: set annot author `TumatraPDF-Flashcard`, gray color, 40% opacity
- `EditAnnotations.cpp`: filter out flashcard annotations from the annotations panel (parallel separation)
- `FlashcardToolbar.cpp`: card count as toolbar button index 0 (was floating label), theme support (`WS_BORDER|RBS_BANDBORDERS`, `RBBS_CHILDEDGE`, RTL), `TB_SETBUTTONINFOW` update

### Trim / canvas
- `Canvas.cpp`: subtle gray 1px separator line between pages when `marginTrimEnabled`; removed stale reloading-cue block
- `TrimConfigDialog.cpp`: refresh contrast overlay on save/cancel
- `AutoScroll.cpp`: uniform 100 px/min speed steps (300→4000)

### Build
`bun cmd/build.ts` → `Compiled\TumatraPDF.exe`, 0 errors / 0 warnings (69s).

### Flashcard cloze refactor — positional occlusion mask (2026-09-16)
Reconceived the flashcard system as a **cloze over existing PDF text** (model shared with NoteAnki): each Highlight annotation is a positional occlusion mask over the original text — **no content duplication** (annotation content now empty; the old `"Q: "` duplication was removed).

- **Render matrix (Canvas.cpp):** opaque mask only on the *current* card in study mode; reveal removes it (shows the original text); reading / non-current cards show a subtle translucent marker. Fixes reveal doing nothing (it was always masking).
- **Stable card key:** `Flashcard::key` = FNV-1a hash of `pageNo + bounds` (quantized), replacing the unstable `pdf_to_num` for study-state matching (survives PDF-save renumbering).
- **Study state now loads:** `CmdFlashcardToggle` ON calls `FlashcardStudyLoad` (new/due counts were always 0 before).
- **SRS:** `BuildFilteredStudyOrder` includes only new (`rating==0`) or due (`nextReviewAt<=now`) cards; rating 1 (Again) reinserts the card into the session queue (Anki relearn); ease factor clamped to `[1.3, 2.5]`.
- **Toolbar/keys:** added **Reveal** button; added bare `1`-`4` → Rate1-4 accelerators (Space/Enter kept as page-scroll to avoid breaking navigation).
- **Pruned dead code:** removed `hwndCardCount`, `studyModeType`, `history` from `FlashcardState`.

Build `bun cmd/build.ts` → `Compiled\TumatraPDF.exe`, 0 errors / 0 warnings. Deployed 2026-09-16.

### Flashcard logging + tree-sitter analysis (2026-09-16)
- **Standardized log tag** to `[fc]` across `Flashcard.cpp`, `Commands_Flashcard.cpp`, `FlashcardToolbar.cpp`, `FlashcardSidebar.cpp` (matches `[arch]`/`[autoscroll]`/`[md]` convention; was inconsistent `FC:`/`[FC]`).
- **Added outcome logs** for debugging: session progress (`advancing to card N/M`), session end (`all cards rated`), annotation count on add (`%d page(s) covered`), and toggle-off cleanup.
- **tree-sitter findings:** the C++ `get_symbols` tool fails on these files (heavy `logf`/`fmt` macros, PCH, lambdas); targeted `run_query` works. Functions found in `Flashcard.cpp`: `FlashcardKey`, `FlashcardLoadFromDocument`, `FlashcardStudyPath/Save/Load`, `FlashcardSm2Update`, `WriteJsonValue`(static), `ParseJsonObject`(static). `Commands_Flashcard.cpp`: `AddUniquePageNo`(static), `BuildFilteredStudyOrder`(static), `HandleFlashcardRate`(static), `HandleCommandFlashcard`.
- **Dead/stub commands** confirmed (candidates to remove): `CmdFlashcardNext` (no-op) and `CmdFlashcardAddTip` (placeholder `MessageBoxW` setting `"(hint)"`) — leftover from the pre-cloze Q/A model.
- **Note (process):** do not edit file bytes with PowerShell `Set-Content` (adds UTF-8 BOM to `.cpp`); use the Edit tool or Python `utf-8-sig` round-trip to strip a BOM if introduced.
- **Removed dead commands** from the pre-cloze Q/A model: `CmdFlashcardNext` (no-op) and `CmdFlashcardAddTip` (placeholder tip) — removed from `cmd/gen-commands.ts`, regenerated `Commands.h/.cpp`, handlers removed from `Commands_Flashcard.cpp`. Also removed now-write-only `Flashcard::text` / `Flashcard::tip` fields (positional model stores no content). Flashcard commands renumbered to `499..509`.

## 2026-09-11 — Flashcard MVP (BUILD OK)

### What
Implemented core flashcard system for Anki-like spaced repetition study directly within PDFs.

### Changes
- **gen-commands.ts**: 11 new command IDs (Toggle, Study, Add, Back, Next, Reveal, Rate1-4, Lista)
- **Flashcard.h**: Data model (Flashcard, FlashcardStudyState, FlashcardStudyDoc structs)
- **Flashcard.cpp**: Core logic — LoadFromDocument (reads FreeText annotations with "Q: " prefix), StudyPath (MD5→JSON), StudySave/Load (external JSON persistence), SM-2 algorithm
- **Commands_Flashcard.h/cpp**: 11 command handlers (Toggle, Add, Study, Reveal, Rate1-4, Back, Lista, Next)
- **FlashcardToolbar.cpp**: Secondary toolbar with Study/Back/Lista buttons (rebar pattern like Arch Tools)
- **Canvas.cpp**: Flashcard highlight rendering (subtle gray reading mode, solid gray study mode)
- **MainWindow.h**: FlashcardState struct (on, studyMode, revealMode, currentCardIdx, studyOrder, cards, studyDoc, toolbar HWNDs)
- **SelectionToolbar.cpp**: Flashcard button added to selection popup
- **Accelerators.cpp**: 'S' shortcut for CmdFlashcardAdd
- **SumatraPDF.cpp**: Command dispatch wired to HandleCommandFlashcard
- **vcxproj + filters**: All new files added to both TumatraPDF.vcxproj and SumatraPDF-static.vcxproj

### Architecture
- Cards stored INSIDE PDF as FreeText annotations (Author="TumatraPDF-Flashcard", content starts with "Q: ") — portable
- Study state external JSON in %APPDATA%\SumatraPDF\FlashcardStudy\<MD5>.json — personal
- Study flow: Toggle→Study→Space reveals→1-4 rates→auto-advance; Back goes back

### Build
`bun cmd/build-test.ts`: 0 errors, 0 warnings.

### Files Created
src/Flashcard.h, src/Flashcard.cpp, src/FlashcardToolbar.cpp, src/Commands_Flashcard.h, src/Commands_Flashcard.cpp

### Files Modified
cmd/gen-commands.ts, src/Commands.h, src/Commands.cpp, src/MainWindow.h, src/SelectionToolbar.cpp, src/Accelerators.cpp, src/SumatraPDF.cpp, src/Canvas.cpp, vs2022/*.vcxproj + .filters

## 2026-09-10 — ToolbarLayout Integration Complete (BUILD OK)

### What
Integrated ToolbarLayout module as the single entry point for toolbar child positioning, replacing manual repositioning functions.

### Changes
- Removed 3 old manual functions from Toolbar.cpp: RepositionEtaLabel, RepositionSpeedLabel, RepositionTimerControls
- Simplified WM_SIZE handler to only call LayoutToolbarChildWindows(win)
- Updated UpdateToolbarEtaText to call PositionFloatingLabels
- Updated UpdateToolbarSpeedLabel to call LayoutToolbarChildWindows
- Removed redundant calls from CreateTimerControls
- Removed forward declarations from Toolbar.h
- Implemented HandleToolbarOverflow in ToolbarLayout.cpp (hide priority: edit → label, never hide checkbox)
- Fixed kCtrlH → gToolbarTokens.ctrlH in ToolbarLayout.cpp
- Files modified: src/Toolbar.cpp, src/Toolbar.h, src/ToolbarLayout.cpp

### Build
- 0 errors, 0 warnings via `bun cmd/build-test.ts`

## 2026-09-08 — Autoscroll Round Steps + Toolbar Wrapping

### Autoscroll Round Steps
- Replaced geometric speed progression (×1.5/×0.667) with round-step lookup table
- Speed steps: 25, 50, 75, 100, 150, 200, 300, 400, 600, 800, 1200, 1600 px/min
- `FindCurrentSpeedStep()` snaps to nearest step on start
- File: `src/AutoScroll.cpp`

### Toolbar Wrapping
- Added `TBSTYLE_WRAPABLE` to main toolbar and arch tools toolbar
- Buttons now wrap to second row when window narrows (instead of hiding)
- Added `LayoutToolbarChildWindows(win)` call in WM_SIZE handler for floating child repositioning
- Rebar `RBS_VARHEIGHT` already supports multi-row; `RelayoutFrame` auto-adapts
- File: `src/Toolbar.cpp`

### Build
- 0 errors, 0 warnings via `bun cmd/build-test.ts`

## 2026-09-08 — SelectionState Struct Extraction Complete (BUILD OK)

### What
Grouped 15 flat selection/touch-input member variables in MainWindow.h into a nested `SelectionState` struct (`win->selection.X`), following the AutoScrollState/ArchToolsState pattern. Pure access-path refactor, no logic change.

### Changed
- `src/MainWindow.h` — added `struct SelectionState { ... } selection;`
- 168 access sites renamed in 9 files (Canvas.cpp, Selection.cpp, SumatraPDF.cpp, SumatraTest.cpp, CommandAvailability.cpp, Menu.cpp, LinkFollow.cpp, SelectionToolbar.cpp, Tabs.cpp)
- Pattern: `X` → `selection.X`; `selectionRect` → `selection.selectionRect` (double "selection" correct)

### Note
`bun cmd/format.ts <files>` reformats the ENTIRE tree (ignores file args). Cosmetic diffs appeared in unrelated files; left as-is (clang-format is logic-neutral).

### Build
0 errors / 0 warnings — `bun cmd/build-test.ts` → `..\Compiled\TumatraPDF-test.exe`

## 2026-09-08 — 16C-F6 Phase B: Window Lifecycle Extraction Complete (BUILD OK)

### What
Extracted remaining 11 functions from SumatraPDF.cpp window-lifetime cluster (L2628-L3069) into `MainWindowCreate.{h,cpp}`, completing the F6 extraction goal.

### Functions extracted
- `UpdateWindowRtlLayout(win)` — promoted non-static, declared in MainWindowCreate.h
- `IsMenubarVisible()` — promoted non-static, declared in MainWindowCreate.h
- `OnSidebarSplitterMove(ev)` — promoted non-static, declared in MainWindowCreate.h
- `OnFavSplitterMove(ev)` — promoted non-static, declared in MainWindowCreate.h
- `CreateSidebar(win)` — static in new TU
- `UpdateToolbarSidebarText(win)` — static in new TU
- `DwmFrameBorderColorForCurrentTheme()` — static in new TU
- `SetWindowBorderColor()` — static in new TU
- `SetWindowRoundedCorners()` — static in new TU
- `CreateMainWindow(win)` — promoted, declared in MainWindowCreate.h
- `RenameFileInHistory(win)` — moved (used only by CreateMainWindow)

### Static blockers resolved
4 functions promoted from `static` to external linkage + declared in MainWindowCreate.h:
- `UpdateWindowRtlLayout`, `IsMenubarVisible`, `OnSidebarSplitterMove`, `OnFavSplitterMove`

### Files modified
- `src/MainWindowCreate.h` — added 4 new declarations
- `src/MainWindowCreate.cpp` — 11 function bodies added
- `src/SumatraPDF.cpp` — 11 bodies removed, 4 forward-declarations removed, 3 static keywords removed

### Build
`bun cmd/build-test.ts` — 0 errors / 0 warnings (46s). Deployed to Compiled\TumatraPDF-test.exe.

### 16C Status
- C1 ✅ (handler mapping)
- C2 ✅ (AutoScroll extraction)
- C3 ✅ (ArchTools extraction)
- C4 ✅ (ViewCommands extraction)
- C5 ✅ (FileCommands wrapper extraction)
- F6-A ✅ (Window lifecycle extraction — ShowMainWindow, CreateAndShowMainWindow, DeleteMainWindow, UpdateAfterThemeChange, MaybeShowDefaultAppNotification)
- F6-B ✅ (CreateMainWindow + CreateSidebar + DWM helpers)

### Estimated reduction
~380 lines total removed from SumatraPDF.cpp across Phase A+B (14,212 → ~13,830 lines).

## 2026-09-08 — 16C-F6 Phase A: Window Lifecycle Extraction (BUILD OK)

### What
Extracted 5 window lifecycle functions from SumatraPDF.cpp (14,212 lines) into new `MainWindowCreate.{h,cpp}`:
- `ShowMainWindow` (56 lines) — was already declared in SumatraPDF.h
- `MaybeShowDefaultAppNotification` (46 lines) + static constants `kNotifDefaultApp`, `kMaxDefaultAppLinks`
- `CreateAndShowMainWindow` (26 lines) — was already declared in SumatraPDF.h
- `DeleteMainWindow` (22 lines) — was already declared in SumatraPDF.h
- `UpdateAfterThemeChange` (33 lines)

### Static blockers resolved
3 functions promoted from `static` to external linkage:
- `CreateMainWindow()` — declared in MainWindowCreate.h, body stays in SumatraPDF.cpp
- `UpdateWindowFrameBorderColor()` — declared in MainWindowCreate.h
- `ApplyDarkModeToInfotip()` — declared in MainWindowCreate.h

`RelayoutFrame()` exported via MainWindowCreate.h (was only forward-declared locally in SumatraPDF.cpp).

### Files created
- `src/MainWindowCreate.h` — declarations for promoted statics + RelayoutFrame
- `src/MainWindowCreate.cpp` — 5 extracted function bodies

### Files modified
- `src/SumatraPDF.cpp` — removed 5 function bodies, removed `static` from 3 functions, removed redundant RelayoutFrame forward-decl, added `#include "MainWindowCreate.h"`
- `vs2022/TumatraPDF.vcxproj` + `.filters` — added MainWindowCreate.cpp
- `vs2022/SumatraPDF-static.vcxproj` + `.filters` — added MainWindowCreate.cpp

### Build
`bun cmd/build.ts` — 0 errors / 0 warnings (40s). Deployed to Compiled\TumatraPDF.exe.

### 16C Status
- C1 ✅ (handler mapping)
- C2 ✅ (AutoScroll extraction)
- C3 ✅ (ArchTools extraction)
- C4 ✅ (ViewCommands extraction)
- C5 ✅ (FileCommands wrapper extraction)
- F6-A ✅ (Window lifecycle extraction — ShowMainWindow, CreateAndShowMainWindow, DeleteMainWindow, UpdateAfterThemeChange, MaybeShowDefaultAppNotification)
- F6-B ✅ (CreateMainWindow + CreateSidebar + DWM helpers)

### Estimated reduction
~190 lines removed from SumatraPDF.cpp (net, including promoted statics still in file).

## 2026-09-08 — ToolbarIds.h Fix + Toolbar Design System Status

### What
1. **ToolbarIds.h CmdLast fix** — Placeholder constants (`PageInfoId`, `WarningMsgId`, `SpeedInfoId`, `TimerInfoId`) initially moved to `Toolbar.h` caused 48 `C2065: 'CmdLast': undeclared identifier` errors because `CmdLast` lives in `Commands.h` which `Toolbar.h` doesn't include. Created `ToolbarIds.h` with include guard + the 4 constexpr declarations. But including `Commands.h` from `ToolbarIds.h` caused 1122 `C2365: redefinition` errors because `Commands.h` has no include guard (SumatraPDF convention). Solution: remove `#include "Commands.h"` from `ToolbarIds.h`; all TUs already include `Commands.h` first via controlled include order.
2. **5 TUs fixed** — Added `#include "Commands.h"` to `EditAnnotations.cpp`, `FormFields.cpp`, `MainWindow.cpp`, `Selection.cpp`, `SumatraControl.cpp` which had `Toolbar.h` but not `Commands.h`.
3. **Toolbar Design System status assessed** — `ToolbarLayout.h/cpp` has declarative slots (`ToolbarSlotSpec`, `gToolbarSlots[]`) and layout engine (`LayoutToolbarChildWindows`) but is NOT integrated (never called from Toolbar.cpp). `HandleToolbarOverflow` is a stub. Decision: deprioritize in favor of 16C-F6 (better ROI).

### Build
`bun cmd/build-test.ts` — 0 errors / 0 warnings. `Compiled\TumatraPDF-test.exe` (21.9 MB).

### Key Learning
SumatraPDF `Commands.h` is a generated header with NO include guard. Never include it from another header. TUs must include it directly before any header that depends on it (controlled include order).

## 2026-09-01 — 16C C5 FileCommands Extraction Complete

### What
Extracted 24 File domain command handlers from SumatraPDF.cpp FrameOnCommand switch into HandleCmdXxx wrapper pattern:
- Created 24 static wrapper functions in SumatraPDF.cpp (before FrameOnCommand)
- Replaced all 24 inline case bodies with clean `HandleCmdXxx(win); break;` dispatch calls
- Cleared all stubs in Commands_File.cpp to avoid multiple definition conflicts
- Split combined CmdOpenPrevFileInFolder/CmdOpenNextFileInFolder case into two separate cases

### Key Decision
Unlike C2/C3/C4 (which moved handlers to separate .cpp files), C5 wrappers live IN SumatraPDF.cpp because the underlying helpers (OpenFile, ClearHistory, etc.) are static functions with deep inter-dependencies within SumatraPDF.cpp. The wrappers only thin-dispatch; actual extraction of helpers requires making them non-static first.

### Build
`C:\Users\Testa\.bun\bin\bun.exe cmd/build.ts` from sumatrapdf-src dir — 0 errors / 0 warnings. Deployed to Compiled\TumatraPDF.exe (21,972,480 bytes).

### 16C Status
- C1 ✅ (handler mapping)
- C2 ✅ (AutoScroll extraction)
- C3 ✅ (ArchTools extraction)
- C4 ✅ (ViewCommands extraction)
- C5 ✅ (FileCommands wrapper extraction)

## 2026-09-01 — Bug Fixes: Speed Label, EPUB ETA, Contrast Invert (.md)

### What
1. **Speed label overflow (Toolbar.cpp:RepositionSpeedLabel)** — clamped label width to slot width (`int labelDx = std::min(size.dx, slot.dx)`) and guarded `if (x < slot.x) x = slot.x`; `MoveWindow` now uses `labelDx` instead of raw `size.dx`.
2. **ETA not calculating for epub (AutoScroll.cpp:RecalcAutoScrollEta)** — restructured for both PDF and WebView paths; WebView path fires one-shot JS eval computing `scrollHeight - (scrollY + innerHeight)`, sends via `__sumatra__.notify('autoscrollProgress', rem)` to existing callback pipeline.
3. **Contrast dark mode ignoring invert in .md (3 files)** — `SumatraPDF.cpp:CmdContrastToggle` checks `GetInvertPageColors()`, swaps bg (`#FAFAFA↔#050505`) and gray formula; `ContrastOverlay.cpp:UpdateContrastOverlayOpacity` checks invert and swaps formula; `MarkdownModel.cpp:RestoreContrastOverlay` checks invert and swaps formula.

### Build
`C:\Users\Testa\.bun\bin\bun.exe cmd/build.ts` from sumatrapdf-src dir — 0 errors / 0 warnings. Deployed to Compiled\TumatraPDF.exe.

## 2026-08-31 — Session: Feature Restoration + 16C C5 FileCommands Analysis

### What
1. Restored 3 features lost during merge/revert cycle: contrast #FAFAFA for .md, SerializeMeasurements/DeserializeMeasurements persistence, hwndReBar2 SW_HIDE for .md. Build 0 err/0 warn.
2. Analyzed 16C C5 (FileCommands extraction) — mapped all 24 File domain `case Cmd` statements in FrameOnCommand. ALL are simple 1-3 line single-function calls. Zero complex cases.
3. Identified C5 blocker: all helper functions (OpenFile, ClearHistory, etc.) are `static` in SumatraPDF.cpp — inaccessible from Commands_File.cpp.
4. Attempted C5a (File Navigation extraction) on branch `feature/16c-c5-checkpoint` (commit `ca5dea5`) — failed with 40+ C3861 errors because extracted functions still call static helpers. Reverted via `git stash`.
5. Established correct C5 approach: create HandleCmdXxx wrappers IN SumatraPDF.cpp first, keep static helpers in place, extract helpers only when dependency chain is fully mapped.

### Build
`C:\Users\Testa\.bun\bin\bun.exe cmd/build.ts` from sumatrapdf-src dir — 0 errors / 0 warnings.

### Key Lesson
C5 extraction cannot follow the same pattern as C2/C3/C4 because file-domain helpers have deep static dependency chains within SumatraPDF.cpp. Strategy: (1) wrap case bodies in HandleCmdXxx(win) functions in SumatraPDF.cpp, (2) declare in Commands_File.h, (3) move helpers out only after all dependencies are non-static or relocated.

- 2026-08-25 — research session — confirmed Markdown (.md/.markdown) reading support ALREADY EXISTS, inherited from upstream master. Zero code changes. Refs: ext map GuessFileType.cpp:66-67, dispatch EngineCreate.cpp:179, cmark-gfm vendored + FZ_ENABLE_MD=1, open filter "*.md;*.markdown" SumatraPDF.cpp:5750, dual render WebView2 default / MuPDF fallback via markdownUI.useFixedPageUI. Runtime verification pending (user tests later).
- 2026-08-24 — lint batch — removed unused cache param (RenderCache), corrected misleading persist comment (autoscroll speed handlers).
- 2026-08-24 — refactor batch — AutoScroll logic extracted to src/AutoScroll.{h,cpp} (ETA math out of Toolbar, tick out of Canvas, commands thin), toolbar overlay brush hoisted + early-out, WM_SIZE ETA reposition width-gated, TabWnd::Paint StringFormat hoisted, ContrastOverlay alpha dedup.
- 2026-08-24 — cleanup batch — dead close-icon code removed (TabsCtrl), dead custom-draw underline removed (Toolbar), orphan TumatraPDF-static.vcxproj deleted, MERGE.md corrected, misc lint fixes.
- 2026-08-24 — toggle underline deterministic WM_PAINT overlay — NM_CUSTOMDRAW fallback proven visible via runtime diag (tests/tmp/tb-diag.ts).

## 2026-08-24 — Deterministic Toggle Underline (WM_PAINT overlay) + Runtime Proof

### What
Checked-toggle underline was invisible to user despite NM_CUSTOMDRAW code looking correct. Replaced the earlier `WndProcToolbar` WM_PAINT fallback (BeginPaint + passing hdc as wParam to `CallWindowProc` — toolbar proc ignores it, update-region clipping risk) with a deterministic overlay: `CallWindowProc(DefWndProcToolbar, ...)` first (NOT `DefSubclassProc` — subclass is `SetWindowLongPtr`-based, empty comctl32 chain would skip toolbar proc → blank toolbar), then `GetDC` + loop all buttons via `TB_GETBUTTONINFO` (`TBIF_BYINDEX|TBIF_STATE`), fill `{left+2, bottom-3, right-2, bottom}` with `ThemeWindowTextColor()` brush for every `TBSTATE_CHECKED` button, `ReleaseDC`. Not clipped to update region → survives BeginPaint validation inside toolbar proc.

### Runtime verification (tests/tmp/tb-diag.ts, bun FFI)
- `TB_ISBUTTONCHECKED`/`TB_GETSTATE`/`TB_COMMANDTOINDEX` are pointer-free → safe cross-process; `TB_GETBUTTONINFO` (pointer lParam) is NOT reliable cross-process (zeroed struct / may disturb target).
- Contrast toggle: state 0x4 → 0x5 CHECKED, sticks; screenshot `tests/tmp/tb-contrast-on.png` shows white underline under "Contrast" — **visible, confirmed**.
- AutoScroll toggle: CHECKED at t=0, self-off <100ms — Canvas.cpp ~L4553 cancels auto-scroll on 1-page doc (no scroll possible); state machinery fine, not an indicator bug.
- ViewportCrop/MarginTrim toggles didn't stick in this test (state stayed 0x4) — separate issue, uninvestigated.

### Build
`bun cmd/build.ts` — 0 errors / 0 warnings (50s). Deployed `Compiled\TumatraPDF.exe` 21,895,168 bytes @ 2026-08-24 18:30:31.

## 2026-08-24 — Toolbar Toggle Underline + ETA Label Responsive Positioning

### What
White 3px underline under checked toggle buttons (Trim/Crop/Autoscroll/Contrast) drawn via a new `CDDS_ITEMPOSTPAINT` stage in `ReBarWndProc` custom draw (item pre-paint now returns `CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT` only when checked). ETA label repositioned instantly on toolbar resize: new `RepositionEtaLabel()` anchors it after the last visible button (TB_BUTTONCOUNT/TB_GETSTATE/TB_GETITEMRECT loop) instead of the right edge, auto-hides when there is no room; `WM_SIZE` hook in `WndProcToolbar` calls it when visible (fixes up-to-1s stale position, overlap and clipping on narrow windows).

### Changes
- `src/Toolbar.cpp` — `ReBarWndProc`: `CDDS_ITEMPREPAINT` returns `CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT` for checked toggles; new `CDDS_ITEMPOSTPAINT` fills bottom strip `{x+2, y+dy-3, x+dx-2, y+dy}` white via `CreateSolidBrush`+`FillRect`
- `src/Toolbar.cpp` — new `RepositionEtaLabel(MainWindow*)`: anchored after last visible button, `DpiScale(8)` gap, hides when `x + width > client dx - DpiScale(4)`
- `src/Toolbar.cpp` — `UpdateToolbarEtaText`: width-changed branch now calls `RepositionEtaLabel` (old right-align math removed)
- `src/Toolbar.cpp` — `WndProcToolbar`: new `WM_SIZE` case calls `RepositionEtaLabel` when label exists and is visible
- `src/Toolbar.h` — declared `RepositionEtaLabel`

### Build
`bun cmd/build.ts` — OK, 0 errors / 0 warnings (61.5s). Fresh exe copied to `Compiled\TumatraPDF.exe`.

## 2026-08-24 — Tab Freeze Removal + X Icon Removal + Alt+Click Close + Toolbar Cleanup

### What
"Tab stays small" bug finally fixed by removing the freeze mechanism entirely. Root cause: Home tab is always inserted at index 0 when tabs are enabled, so 1 open doc = 2 tabs. The Chrome-like freeze (added 2026-08-17) kept the small frozen width while the cursor stayed over the tab bar; the WM_MOUSELEAVE heal only fired when the mouse left the bar, so the small width appeared stuck. Also: removed the ✕ close icon from tabs (replaced by Alt+LeftClick close) and removed Back/Forward toolbar buttons.

### Changes
- `src/wingui/TabsCtrl.cpp` — `LayoutTabs`: always `dx = min(tabDefaultDx, (clientW - 5) / nTabs)`; deleted freeze branch + else-reset block
- `src/wingui/TabsCtrl.cpp` — `CloseTab`: deleted freeze-set lines and `TrackMouseLeave(hwnd)`
- `src/wingui/TabsCtrl.cpp` — `WM_MOUSELEAVE` case: deleted frozen-width reset block
- `src/wingui/WinGui.h` — removed fields `tabWidthFrozen` / `frozenTabDx`
- `src/wingui/TabsCtrl.cpp` — `TabWnd::CloseVisible()`: returns false unconditionally (✕ never visible/painted; hit-test code left in place, now dead)
- `src/wingui/TabsCtrl.cpp` — removed unused `kMinTabWidthForClose`
- `src/wingui/TabsCtrl.cpp` — `WM_LBUTTONDOWN`: if `IsKeyPressed(VK_MENU)` and a closable tab is under cursor, call `CloseTab(idx)` and swallow message; empty-bar clicks ignored
- `src/Toolbar.cpp` — `gToolbarButtons[]`: removed NavigateBack/NavigateForward entries (+ one separator). Commands kept — keyboard shortcuts (Alt+Left, Alt+Right, Ctrl+Backspace) still work

### Build
`bun cmd/build.ts` — OK, 0 errors / 0 warnings. Fresh exe copied to `Compiled\TumatraPDF.exe`.

## 2026-08-17 — Trim Black-Space Fix (RenderCache.cpp)

### What
Trim mode left black band at top of each page. Root cause: DOUBLE top-shift — `GetTileRectDevice` had `mediabox.y += t;` (~line 320) while `GetTileRectUser` also does `rect.y += gGlobalPrefs->trim.top;` (line 336). Same shift applied twice → clipped band.

### Fix
- `sumatrapdf-src/src/RenderCache.cpp` — removed `mediabox.y += t;` from `GetTileRectDevice` (~line 320). Kept `rect.y += gGlobalPrefs->trim.top;` in `GetTileRectUser` (line 336) as the single correct shift.

### Build
`bun cmd/build.ts` — OK. Fresh exe copied to `Compiled\TumatraPDF.exe`.

## 2026-08-17 — Tab Freeze Fix (TabsCtrl.cpp) — UNRESOLVED

### What
User-set tab width freeze broke: tab renders small with 1 doc open.

### Changes applied
- `src/wingui/TabsCtrl.cpp` — `LayoutTabs`: freeze condition now `if (tabWidthFrozen && frozenTabDx > 0 && nTabs > 1)`; else-branch resets `tabWidthFrozen=false` / `frozenTabDx=0`
- `src/wingui/TabsCtrl.cpp` — `CloseTab`: added `TrackMouseLeave(hwnd);` after the freeze lines

### Status
⚠️ Applied but STILL broken — tab remains small with 1 doc open. Deprioritized by user, leave as-is. Settings file corruption excluded (`TabWidth=300` reads correctly).

## 2026-08-17 — Invert + Contrast Per-Document Persistence

### What
Reopening a document now restores invert + contrast state (was session-only).

### Files changed
- `cmd/gen-settings.ts` — new FileState fields in `fileSettings` array (after `AutoScrollSpeedMultiplier`): `InvertColors` (Bool, false), `ContrastEnabled` (Bool, false), `ContrastOpacity` (Int, 50)
- `src/Settings.h` — regenerated (fieldCount 22 → 25)
- `src/SumatraPDF.cpp` — save in `UpdateTabFileDisplayStateForTab` (~L830-833); load in `LoadDocument` (~L2040-2043) + `CreateContrastOverlay` after `win->ctrl = tab->ctrl` (~L2093-2095)

### Build
`bun cmd/build.ts` — OK. Fresh exe copied to `Compiled\TumatraPDF.exe`.

## 2026-08-17 — Scroll Delay Investigation: 3.7.20958 vs 3.6.17065

### What
Investigated scroll delay regression in 3.7.20958 vs 3.6.17065 (instant scroll). Root cause: upstream smooth-scroll system (redesign ed7dbb13d 2026-07-28, default TRUE since 15a48380c 2026-07-29). 3.6.17065 had smoothScroll default FALSE → instant scroll.

### Findings
- `smoothScroll=false` confirmed INSTANT: direct `ScrollYTo` (Canvas.cpp:1242-1250), gate `smoothWheel = gGlobalPrefs->smoothScroll && gInMouseWheelScroll` (Canvas.cpp:1167). No timer/animation when off.
- Smooth scroll mechanics (when ON): exponential chase `a=1-e^(-15·dt)`, 1ms timer + `timeBeginPeriod(1)`, ~200ms convergence, per-tick full repaint chain.
- Remaining delay sources with smoothScroll=false: (a) `uitask::Post` deferral in `ScheduleRepaint` (Canvas.cpp:4448-4459) — InvalidateRect deferred ≥1 message-loop turn; (b) async render thread — `RequestRendering` (DisplayModel.cpp:1697-1734), `RenderFinishedAsync` → `uitask::Post` → `RepaintDisplay` (DisplayModel.cpp:310-328); (c) `uitask::Post` + async render appears NEW in 3.7 vs 3.6.17065.
- Smart thin scrollbar (upstream 9ecd69005 2026-08-01): NOT default (mode "windows", SumatraPDF.cpp:1326). Only active if user enables smart/overlay mode.
- Double buffer NOT stale: `DrawDocument` re-renders at current scroll pos each WM_PAINT (Canvas.cpp:3162); buffer is flicker-prevention only.
- USER FINDING: trim=on (crop/trim feature) INTENSIFIES the scroll bug — complicates render path. Being fixed separately.

### Optimization candidates (future)
- Smooth timer 1ms → 10-16ms; skip repaint when delta <1px; reduce `uitask::Post` double-hop; trim render path optimization.

### Artifacts
- `.tmp_compare/` (upstream clone + comparison files) at project root — keep for future upstream diffs.

## 2026-08-17 — Autoscroll ETA Optimization + AutoScrollShowEta Option

### What
Reworked autoscroll ETA: no more per-second recalculation + full-toolbar repaint every 100 ticks (10ms timer → WM_PAINT delays → stutter). ETA now computed on demand (start / speed change / page change), countdown by wall clock, label updated only when the minute value changes, only the label invalidated. New Advanced Options setting `AutoScrollShowEta` (default true) hides the ETA label entirely.

### Files changed
- `src/MainWindow.h` — replaced `autoScrollTickCount` with 5 ETA state fields (minutes, start tick, last shown, page no, cached toolbar width)
- `src/Toolbar.h` — declared `RecalcAutoScrollEta(MainWindow*)`
- `src/Toolbar.cpp` — added `RecalcAutoScrollEta`; rewrote `UpdateToolbarEtaText` (hide when `AutoScrollShowEta` false, reposition only on toolbar resize, invalidate label only)
- `src/Canvas.cpp` — removed tick-count logic from `kContinuousAutoScrollTimerID` handler; ETA gate replaced with page-change recalc + wall-clock countdown
- `src/SumatraPDF.cpp` — start path calls `RecalcAutoScrollEta` + shows computed ETA; speed up/down recalc ETA when active
- `src/Settings.h` — added `bool autoScrollShowEta` (default true) + metadata entry (visible in Advanced Options)

### Build
`bun cmd/build.ts` — success, 0 warnings 0 errors. First attempt failed with LNK1201 (libsumatrapdf.pdb locked — stale handle, likely VS Code/OneDrive); renamed the PDB and rebuilt clean. Fresh exe copied to `Compiled\TumatraPDF.exe`.

## 2026-08-16 — Debug Tooling Analysis (Research Only)

### What
Inventory of existing debug tooling in sumatrapdf-src. No code changed.

### Working
- `cmd/build.ts` — vs2022\TumatraPDF.sln /t:TumatraPDF → out/dbg64/TumatraPDF.exe
- `cmd/run.ts` — build + launch detached
- `cmd/run-unit-tests.ts` -dbg|-rel|-asan — test_util.exe + -for-ai, callstacks captured
- `cmd/control.ts` + `-dbg-control <pipe>` — 44 Test* commands (Ping/Quit/List/TestSearch/TestToc/TestToolbarButtons/TestAIChat)
- `tests/winapi.ts` + `tests/win-automation.ts` — FFI win32, postMessage cross-process, captureWindowToPng works on occluded windows; SendInput DROPPED on this machine
- Flags: -for-testing/-console/-log/-stress-test/-bench/-render/-extract-text/-set-color-range
- `cmd/clang-tidy.ts`, `cmd/cppcheck.ts`, `cmd/gen-{commands,settings,flags}.ts` codegen
- `src/tools/logview`, 112 tests/issue-*.ts

### Broken (fix next session)
- `cmd/build-asan.ts:62-63` → vs2022\SumatraPDF.sln + /t:SumatraPDF-static — file renamed to TumatraPDF.sln. `cmd/dbg.ts` expects out/dbg64_asan/TumatraPDF-static.exe but build produces SumatraPDF-static.exe. ASan + windbg pipeline unusable.

### Stale
- `.vscode/launch.json` → out/dbg64/SumatraPDF-dll.exe; `.vscode/tasks.json` → SumatraPDF.sln /t:SumatraPDF-dll → no F5 debug in VS Code.

### Gaps
- `-dbg-control` no commands for custom feature state (viewport crop rect, trim values, autoscroll timer/ETA, contrast/invert)
- test_util no unit tests for custom feature logic (crop zoom math, trim clip)

### Plan (5 steps, next session)
1. Fix ASan tooling: rename sln + target in build-asan.ts/dbg.ts (SumatraPDF.sln → TumatraPDF.sln, SumatraPDF-static → TumatraPDF-static)
2. Update .vscode launch.json/tasks.json → TumatraPDF.sln + TumatraPDF.exe
3. Extend -dbg-control: TestViewportCrop / TestTrimState / TestAutoScrollState / TestContrastState
4. Add unit regression for custom features in test_util (crop math, trim clip)
5. Document debug loop in AGENTS.md (build → flags → dbg-control → screenshots)

## 2026-08-16 — Inversion Fix: Recolor in Non-Cached Path (trim=on)

### What
Color inversion (and document-color-follow-theme) was lost when trim was enabled. Trim forces the non-cached render path (`ShouldCacheRendering` → `!marginTrimEnabled`), and that path never called `RecolorPixmap` — only the cached path did.

### Files
- `sumatrapdf-src/src/RenderCache.cpp` — non-cached path in `RenderCache::Paint` (~line 1165): added `RecolorPixmap(bmp, this->textColor, this->backgroundColor, this->linkColor, nullptr)` guarded by `ShouldUpdateBitmapColorsLegacy(dm->GetEngine(), this)` before `BlitPixmap`, matching the cached path (~line 1017).

### Result
- Build: MSBuild Debug x64 0 errors.
- Binary: `Compiled\TumatraPDF.exe` = 21,875,200 bytes.
- Note: used `this` (member function) instead of `gRenderCache` global — the `extern RenderCache* gRenderCache;` declaration sits at line 1259, after the non-cached path, so the global wasn't visible there.

## 2026-08-11 — Project Start

### SumatraPDF Architecture Research
- **Repository:** github.com/sumatrapdfreader/sumatrapdf
- **Build:** Premake5 generates Visual Studio 2022 solution
- **UI:** Native Win32 API (no MFC, no wxWidgets)
- **PDF Engine:** MuPDF (parsing + rendering)
- **Languages:** C, C++
- **Directory structure:** `src/` (main code), `mupdf/` (PDF engine), `ext/` (third-party libs), `vs2022/` (VS solution)
- **Supported formats:** PDF, EPUB, MOBI, CBZ/CBR, FB2, CHM, XPS, DjVu
- **Author:** Krzysztof Kowalczyk (kjk)
- **License:** GPLv3

### Pre-release Version
- **Current version:** `3.7.20958`
- **Does NOT have a plugin/module system** — monolithic Win32 app
- **Has native auto-update:** `src/UpdateCheck.cpp`, manifest `updatecheck-pre-release.txt`
- **Configuration:** `src/Version.h`, `src/BuildConfig.h`, `src/AppSettings.h`

### Planned Features
1. **Auto-scroll with timer** — continuous scroll with adjustable speed
2. **Viewport crop for two-column books** — half-page cropping for two-column layout books
3. **Contrast filter** — contrast/brightness adjustment via post-processing

### Existing AutoHotkey Implementation
- **File:** `Sumatra Tools\sumatra-autoscroll-v3.ahk`
- Already implements: auto-scroll (F9 toggle, F7/F8 speed), 2-column navigation (numpad), pause Ctrl, 30min timer
- Will serve as reference for native C++ implementation

### Design Decisions
- Keep Win32 base for compatibility and lightness
- Integrate with MuPDF as rendering engine
- **Upstream code in `sumatrapdf-src/` untouched** — features in `src/features/`
- Modularity via Premake5 build hooks
- Upstream merge documented in `MERGE.md`

### Upstream Update Strategy
- Automatic check for new pre-release versions
- Script `scripts/check-updates.ps1` for manual checking
- Process: download new version → diff → manual merge on hooks → rebuild

### Project Structure
```
TumatraPDF/
├── README.md              # Overview and credits
├── LOG.md                 # This file
├── MERGE.md               # Upstream merge instructions (to create)
├── FLOW/                  # Planning diagrams
│   └── tumatrapdf.dot
├── sumatrapdf-src/        # Original SumatraPDF code (6353 files)
├── src/                   # Our extensions (to create)
│   ├── features/
│   │   ├── autoscroll/
│   │   ├── viewport/
│   │   └── contrast/
│   └── hooks/
└── scripts/               # Auxiliary scripts (to create)
    └── check-updates.ps1
```

## 2026-08-11 — SumatraPDF Binary Rename → TumatraPDF

### Complete binary rename
- **Objective:** Output EXE `TumatraPDF.exe` (not `SumatraPDF.exe`) for side-by-side coexistence
- **Files modified:** 23 files + 4 renames

### Files Changed
1. `premake5.lua`: workspace/project "SumatraPDF" → "TumatraPDF" 
2. `vs2022/SumatraPDF.sln` → `vs2022/TumatraPDF.sln`: project refs, vcxproj refs
3. `vs2022/SumatraPDF.vcxproj` → `vs2022/TumatraPDF.vcxproj`: RootNamespace, 16× TargetName, 16× IntDir
4. `vs2022/SumatraPDF-static.vcxproj` → `vs2022/TumatraPDF-static.vcxproj`: RootNamespace, 16× TargetName, 16× IntDir
5. `cmd/build.ts`: target `/t:TumatraPDF`, sln path
6. `cmd/run.ts`: target, sln path, exe path
7. `cmd/clean.ts`: settings filename exclusion
8. `cmd/dbg.ts`: exe path
9. `src/Version.h`: kAppName "TumatraPDF"
10. `src/SumatraPDF.cpp`: window title strings
11. `src/AppSettings.cpp`: settings filename "TumatraPDF-settings.txt"
12. `src/SumatraPDF.rc`: icon paths, manifest reference
13. `src/SumatraPDF.exe.manifest` → `src/TumatraPDF.exe.manifest`
14. `src/CrashHandler.cpp`: PDB name, download URLs
15. `src/Commands.cpp`: menu strings
16. `src/Menu.cpp`: file picker strings
17. `src/AICodexBuild.cpp`: client name in JSON
18. `src/AIChatPanel.cpp`: webview data dir
19. `src/AppTools.cpp`: data dir path
20. `src/ImageEditHostSumatra.cpp`: CreatorApp docprop
21. `src/libsumatrapdf.rc`: ProductName version info

### NOT Changed (out of scope)
- Copyright headers, comments, `#include "SumatraPDF.h"` (filename names)
- CI scripts: cmd/build-ci.ts, build-all.ts, build-ci-daily.ts, build-smoke.ts, build-codeql.ts
- Sub-components: PdfFilter.rc, PdfPreview.rc
- Cross-platform build scripts: build-mac.ts, build-linux-wine.ts, build-win-in-wsl.ts
- cmake, tests/, docs/, mupdf/, ext/

### Next Steps
- [x] Clone SumatraPDF repository
- [x] Document architecture and credits
- [x] Initialize local Git repository
- [ ] Create private repository on GitHub
- [ ] Set up build environment (VS2022 + Premake5)
- [ ] Compile base version without modifications
- [ ] Create MERGE.md with update process
- [ ] Implement scripts/check-updates.ps1
- [ ] Plan integration points for 3 features
- [ ] Implement feature 1: auto-scroll (port from AHK)
- [ ] Implement feature 2: viewport crop
  - [ ] Implement feature 3: contrast filter

## 2026-08-11 — Binary Rename + Logo Inversion

### SumatraPDF → TumatraPDF (23 files modified + 4 renames)
- premake5.lua: workspace, startproject, project names
- vs2022/: sln + vcxproj files renamed, TargetName/RootNamespace/IntDir updated (16× each)
- cmd/: build.ts, run.ts, clean.ts, dbg.ts — target names + paths
- src/Version.h: kAppName "TumatraPDF"
- src/SumatraPDF.cpp: window title strings
- src/AppSettings.cpp: settings file "TumatraPDF-settings.txt"
- src/SumatraPDF.rc: icon paths, manifest ref
- src/SumatraPDF.exe.manifest → TumatraPDF.exe.manifest
- src/CrashHandler.cpp: PDB name, update URLs
- src/Commands.cpp, Menu.cpp: menu strings
- src/AICodexBuild.cpp, AIChatPanel.cpp, AppTools.cpp: paths
- src/ImageEditHostSumatra.cpp, libsumatrapdf.rc: version info
- Remaining "SumatraPDF" in src/: copyright headers (~600), includes (~50), internal class names — NOT changed (not binary-relevant)

### Logo Color Inversion (5 files, Python/Pillow)
- gfx/SumatraPDF-smaller.ico (256×256, single frame — ICO format)
- appx/SumatraPDF_StoreLogo_150x150.png (150×150 RGBA)
- appx/SumatraPDF_44x44.png (44×44 RGBA)
- appx/SumatraLogo310x300.png (310×310 RGB)
- appx/SumatraLogo310x150.png (310×150 RGB)
- File type icons (pdf-32bit.ico, epub-32bit.ico, etc.) NOT inverted — logos only per user request

### Next
- Rebuild: msbuild vs2022\TumatraPDF.sln /t:TumatraPDF /p:Configuration=Debug /p:Platform=x64 /m
- Verify EXE name: out/dbg64/TumatraPDF.exe

## 2026-08-11 — Feature 1: AutoScroll Implemented (Direct Core Integration)

### Direct Implementation (not modular)
Auto-scroll implemented directly in core source files (not in a separate `src/features/`).
This avoids modular build complexity — Premake5 hooks deferred.

### Files Modified (8 files)
1. **src/Commands.h** (+9 lines):
   - `CmdAutoScrollToggle = 479`, `CmdAutoScrollSpeedUp = 480`, `CmdAutoScrollSpeedDown = 481`
   - Inserted after `CmdContrastToggle`, before `CmdNone` (renumbered to 482)
   - All ranges (`CmdFileHistory*`, `CmdFavorite*`, `CmdLast`, `CmdFirstCustom`) automatically adjusted

2. **src/Commands.cpp** (+6 lines):
   - Strings: `"CmdAutoScrollToggle\0"`, `"CmdAutoScrollSpeedUp\0"`, `"CmdAutoScrollSpeedDown\0"`
   - IDs: `CmdAutoScrollToggle`, `CmdAutoScrollSpeedUp`, `CmdAutoScrollSpeedDown` in `gCommandIds[]`

3. **src/Accelerators.cpp** (-3/+3 lines):
   - F7: `CmdSelectTextViaKeyboard` → `CmdAutoScrollSpeedDown`
   - F8: `CmdToggleToolbar` → `CmdAutoScrollSpeedUp`
   - F9: `CmdToggleMenuBar` → `CmdAutoScrollToggle`

4. **src/Canvas.h** (+2 lines):
   - `constexpr UINT_PTR kContinuousAutoScrollTimerID = 15;`

5. **src/MainWindow.h** (+7 lines):
   - Fields in `struct MainWindow`: `autoScrollActive`, `autoScrollSpeed` (0.5f px/tick), `autoScrollSpeedMultiplier` (1.0f), `autoScrollTimerMinutes` (0=no timer), `autoScrollStartTick`

6. **src/Canvas.cpp** (+21 lines):
   - Handler `case kContinuousAutoScrollTimerID` in `WndProcCanvas`
   - Checks `IsAtDocumentEnd()`, timer expired, scroll via `MoveDocBy()`

7. **src/SumatraPDF.cpp** (+23 lines):
   - Dispatch in `FrameOnCommand`:
     - `CmdAutoScrollToggle`: toggle + `SetTimer`/`KillTimer` + button checked
     - `CmdAutoScrollSpeedUp`: multiplier ×1.2 (max 10×)
     - `CmdAutoScrollSpeedDown`: multiplier ×0.8 (min 0.1×)

8. **src/Toolbar.cpp** (+4 lines):
   - 3 buttons added to `gToolbarButtons[]`: toggle, speed up, speed down
   - Separator before buttons

9. **src/CommandAvailability.cpp** (+3 lines):
   - Commands added to `removeIfChm[]` (auto-scroll doesn't make sense in CHM)

### Behavior
- **F9**: Toggle auto-scroll (turns continuous vertical scroll on/off)
- **F8**: Increase speed (multiplier 1.2× per press, max 10×)
- **F7**: Decrease speed (multiplier 0.8× per press, min 0.1×)
- **Toolbar buttons**: Toggle + speed up + speed down (icons None for now)
- **Automatic stop**: End of document OR timer expired (`autoScrollTimerMinutes`)
- **Scroll**: MoveDocBy(0, dy) where dy = speed × multiplier pixels per tick (~10ms)
- **Timer**: `USER_TIMER_MINIMUM` (~10ms) for smoothness

### Known Limitations
- Sub-pixel movement not accumulated (very slow scroll may not move)
- ETA (time remaining) not implemented — no status bar in SumatraPDF
- Toolbar icons are `TbIcon::None` — need SVG icons
- `MoveDocBy(int, int)` expects ints, so dy < 1.0f truncates to 0
- `kContinuousAutoScrollTimerID = 15` — check for conflicts with other timer IDs

### NOTE: Manually Generated Commands
Commands were added directly in Commands.h/Commands.cpp (not via `cmd/gen-commands.ts`).
If `bun cmd/gen-code.ts` is run, these commands will be OVERWRITTEN.
It is necessary to also add the commands in `cmd/gen-commands.ts` for safe regeneration.

## 2026-08-11 — Themes + Menu + Toolbar Fix

### Themes: Light + Dark only, Dark default
- Theme.cpp:592: CreateThemeCommands() — filter loop to only append "Light" and "Dark" themes to gThemes
- Settings.h:1646: Theme default "" → "Dark"
- Theme.cpp:830-831: SetTheme() fallback — change from gThemeLight->name to "Dark"
- 22 themes defined in themesTxt kept intact — only gThemes vector filtered
- Custom user themes still supported (appended after built-ins)

### New Tools Menu
- Menu.cpp:592-618: menuDefNewTools[] submenu with 4 items
- AutoScroll Toggle (CmdAutoScrollToggle), Speed Up (CmdAutoScrollSpeedUp), Speed Down (CmdAutoScrollSpeedDown), Contrast (CmdContrastToggle)
- Menu.cpp:873: inserted into main menu bar after Debug
- Label: "New &Tools" (access key T)

### Toolbar Icon Fix
- Toolbar.cpp:90-92: TbIcon::None → ArrowsDiagonal (toggle), ChevronUp (speed up), ChevronDown (speed down)
- Root cause: TbIcon::None → SkipBuiltInButton() true → BTNS_SEP separator, not button
- Toolbar.cpp: CmdAutoScrollToggle added to BTNS_CHECK list for checked/unchecked state

### Command Registration Fix
- cmd/gen-commands.ts: added CmdAutoScrollToggle, CmdAutoScrollSpeedUp, CmdAutoScrollSpeedDown, CmdContrastToggle before CmdNone
- Regenerated Commands.h/.cpp via bun cmd/gen-code.ts
- Rebuild: msbuild Debug x64 → 0 errors
- Compiled/TumatraPDF.exe updated (21.8 MB)

## 2026-08-11 — Viewport Crop Implementation (Phase 1: Architecture)

### Scope
Settings struct + commands + Relayout page duplication + toggle handler.
Page duplication in Relayout() lays out each physical page twice (left column → right column)
when viewportCrop.enabled in continuous mode.

### Files Modified (7 files)
1. **cmd/gen-settings.ts** (+12 lines):
   - Added `ViewportCrop` compactStruct: left/right/top/bottom margins (px), colGap, enabled, quickToggle
   - Added `viewportCrop` field to globalPrefs with `.ver("3.8")`

2. **cmd/gen-commands.ts** (+2 lines):
   - Added `CmdViewportCropToggle` (484) and `CmdViewportCropConfig` (485) before CmdNone
   - Regenerated via `bun cmd/gen-code.ts` (Commands.h/.cpp) + `bun cmd/gen-settings.ts` (Settings.h/.cpp)

3. **src/DocumentLayout.h** (+4 lines):
   - `DocumentLayoutPage`: added `cropColumn` field (-1=full, 0=left, 1=right)
   - `DocumentLayout`: added `basePageCount` field (original physical page count before duplication)

4. **src/DocumentLayout.cpp** (+36 lines):
   - Added `#include "GlobalPrefs.h"`
   - `Relayout()`: ViewportCrop page duplication logic:
     - If `viewportCrop.enabled && !quickToggle && IsContinuous`: duplicate each page (left+right columns)
     - If disabled but previously duplicated: restore to `basePageCount`
     - First duplication saves `basePageCount`; subsequent Relayouts check `pages.len == basePageCount`

5. **src/SumatraPDF.cpp** (+13 lines):
   - `FrameOnCommand` case `CmdViewportCropToggle`: toggle `gGlobalPrefs->viewportCrop.enabled`, 
     reset `quickToggle`, call `dm->Relayout(zoomVirtual, rotation)`, update toolbar checked state

6. **src/CommandAvailability.cpp** (+1 line):
   - Added `CmdViewportCropToggle` to `removeIfChm[]` (2-column crop not applicable to CHM)

7. **src/GlobalPrefs.h** (included, no change):
   - Already declares `extern GlobalPrefs* gGlobalPrefs`

### Build
- MSBuild Debug x64 → **0 errors**
- Output: `out/dbg64/TumatraPDF.exe` (20.8 MB)
- Copied to `Compiled/TumatraPDF.exe`

### Known Limitations
- **Rendering pipeline not yet updated**: pages are duplicated in layout but `RenderVisibleParts()` 
  and `RecalcVisibleParts()` still iterate by physical page count. The duplicated entries' positions 
  are computed in Relayout but not reflected in visible-parts calculation.
- **No per-column crop rendering**: both duplicated entries show the full page image.
- **No quick toggle**: double-click handler not implemented (WM_LBUTTONDBLCLK already used for 
  text selection).
- **No toolbar button or menu item**: commands exist but not hooked up to UI.
- **Page counting**: `PageCount()` returns engine (physical) count, not layout (virtual) count.

### Next Steps
- Modify `RecalcVisibleParts()` and `RenderVisibleParts()` to use layout page count when viewportCrop enabled
- Add `physicalPageNo` mapping so renderer renders correct page for each virtual column
- Implement per-column crop rendering (pass clip rect to render)
- Add toolbar button + menu item
- Quick toggle: use Ctrl+Backtick or other shortcut (double-click on canvas is taken)
- Config dialog for margin values

## 2026-08-11 — Viewport Crop Crash Fix (CmdViewportCropToggle)

### Crash
- `EXCEPTION_ACCESS_VIOLATION` at `DisplayModel::RenderVisibleParts:1645`
- Stack: `FrameOnCommand(CmdViewportCropToggle)` → `SetScrollState` → `GoToPage` → `RenderVisibleParts` → null `pagesInfo`
- Root cause: `CmdViewportCropToggle` called `dm->Relayout()` but never rebuilt `pagesInfo` with new size. Layout had 2N entries, `pagesInfo` still N. `RenderVisibleParts()` loops `EffectivePageCount()`=2N but `GetPageInfo(>N)` returns null.

### Fix
- **src/SumatraPDF.cpp** (`case CmdViewportCropToggle`): after `dm->Relayout(...)` added:
  ```cpp
  dm->BuildPagesInfo();
  dm->RecalcVisibleParts();
  ```
  (before `SetScrollState`)

### Build
- MSBuild Debug x64 → **0 errors**
- Output: `out/dbg64/TumatraPDF.exe` (21.8 MB)
- Copied to `Compiled/TumatraPDF.exe`

## 2026-08-11 — Crop/Trim Fix: Always Re-save + Zoom Clamp

### Bug 1 — Crop didn't work after resize
- `ApplyViewportCrop()` only saved viewport+zoom on first call (`if (viewportCropSaved.IsEmpty())`). After window resize, stale saved viewport → crop broken.
- **Fix:** always re-save `viewportCropSaved` + `viewportCropSavedZoom` before applying.
- **src/SumatraPDF.cpp** `CmdViewportCropToggle`: removed redundant `cropDm->viewportCropSaved = Rect();` before `= GetViewPort()`.

### Bug 2 — Trim crashed with zoom overflow
- `ApplyMarginTrim()` zoom formula `pageWidth / contentWidth` → extreme zoom when margins large relative to page.
- **Fix:** clamp `zoomFactor` to `[0.5, 4.0]` + same always re-save pattern (`marginTrimSaved` + `marginTrimSavedZoom`).

### Bug 3 — Zoom compounding on resize
- `ApplyMarginTrim()` auto-applied in `SetViewPortSize` (~1348) and `Relayout` (~1800) → zoom compounded on every resize.
- **Fix:** removed both auto-apply calls. User toggles trim manually now. `ApplyViewportCrop()` auto-apply kept.

### Build
- MSBuild Debug x64 → **0 errors**
- Output: `out/dbg64/TumatraPDF.exe` (21,857,792 bytes)
- Copied to `Compiled/TumatraPDF.exe`

## 2026-08-11 — End of Session Status

### Working Features
- **AutoScroll**: F9 toggle, F7/F8 speed, Ctrl=pause, sub-pixel accumulator ✓
- **Contrast**: Ctrl+Shift+C toggle, ±10% opacity controls ✓  
- **Invert**: Shift+I (existing command, toolbar button added) ✓
- **Binary rename**: TumatraPDF.exe, own settings file, window title ✓
- **Logos**: 5 logo files color-inverted ✓
- **Toolbar**: Text buttons (AutoScroll/+/-/Contrast/Invert/Crop/Trim) ✓
- **Menu**: "New Tools" with all commands ✓
- **Themes**: Light+Dark only, Dark default ✓

## 2026-08-12 — Viewport Crop & Margin Trim Zoom Bug Fix & Completion

### Fix — Zoom Compounding & Flate Filter Error
- **Root cause**: `ApplyViewportCrop()` and `ApplyMarginTrim()` re-saved `viewportCropSaved` from already-cropped viewports, and `GetZoomVirtual()` without `true` returned special constants (`-2`, `-1`) instead of numeric scale factors.
- **Fix in `DisplayModel.cpp`**: 
  - Only save base uncropped `viewportCropSaved` / `marginTrimSaved` if `IsEmpty()` (prevents compounding).
  - Use `GetZoomVirtual(true)` for absolute numeric zoom.
  - Properly reset saved state rectangles (`= Rect()`) on disable in `SumatraPDF.cpp`.
- **Flow alignment (`cropview.txt`)**: Viewport Crop correctly splits each page into left and right columns vertically in continuous mode, advancing column-by-column.

### Build
- MSBuild Debug x64 → **0 errors, 0 warnings**
- Output: `sumatrapdf-src/out/dbg64/TumatraPDF.exe` copied to `Compiled/TumatraPDF.exe`

## 2026-08-12 — Viewport Crop Zoom Compounding & Manual Scroll Fix

### Fix — Zoom Compounding & Manual Scroll
- **Root Cause**: `viewportCropSaved` was being cleared (`viewportCropSaved = Rect()`) on page transitions in `ScrollYBy`, forcing a re-save from an already 2x zoomed viewport, causing exponential zoom compounding (`2x -> 4x -> 8x...`).
- **Fix in `DisplayModel.cpp`**:
  - Removed `viewportCropSaved = Rect()` reset during navigation so `viewportCropSavedZoom` remains anchored to the stable base uncropped zoom.
  - Added upward column/page retreat logic in `ScrollYBy` (`dy < 0`) alongside forward advance (`dy > 0`) for fully fluid manual reading in both directions.
- **Build**: MSBuild Debug x64 succeeded with 0 errors; output binary `Compiled/TumatraPDF.exe`.

## 2026-08-15 — ETA Label Overlap Fix (Right-Align + Hidden by Default)

### Bug
"ETA" label appeared on top of the toolbar buttons (Contrast/Invert/Crop/Trim), including at startup.

### Root Cause (Toolbar.cpp)
- `CreateEtaLabel` anchored label x to `TbGetRect(hwndToolbar, CmdAutoScrollSpeedDown)` right edge + 8px — but SpeedDown (idx 23) is mid-row, NOT the last button. Contrast/Invert/Crop/Trim (idx 25-28) sit right after → overlap whenever visible.
- Label created with `WS_VISIBLE` style → visible at startup ("ETA: --" over buttons) until the first `UpdateToolbarEtaText` call (which only happens on autoscroll toggle / timer tick).

### Fix (2 edits, Toolbar.cpp only)
1. `CreateEtaLabel`: removed `WS_VISIBLE` from CreateWindowExW style → hidden by default.
2. `UpdateToolbarEtaText`: right-aligned the label against the toolbar client right edge — replaced the SpeedDown-anchored x (`tbarRect.x + tbarRect.dx + DpiScale(8)`) with `rc.dx - size.dx - DpiScale(8)` using the existing `HwndClientRect(win->hwndToolbar)`. ETA now sits right of the last button (Trim), repositions on every show, never overlaps.

### Result
- ETA hidden by default; shown only when autoscroll=on (`win->autoScrollActive` guard unchanged); positioned at right side of toolbar.
- Build: MSBuild Debug x64 → 0 errors. Output: Compiled\TumatraPDF.exe (21,863,424 bytes).

## 2026-08-15 — Autoscroll Dead Fix: Persisted Multiplier 0 Clamp on Load

### Bug
Autoscroll stopped working (button toggles, no movement). ETA fix (same day) ruled out — autoscroll code intact end-to-end (toggle, timer, F9 accelerator, availability).

### Root Cause
`Compiled\TumatraPDF-settings.txt:213` had `AutoScrollSpeedMultiplier = 0` for the current doc (Resumen Psiquiatria.pdf). Load path (SumatraPDF.cpp:2034) had NO clamp → `win->autoScrollSpeedMultiplier = 0` → Canvas.cpp:4491 `speed = 2.0 * 0 = 0` → dy always 0 → `if (dy != 0)` false → MoveDocBy never called. UI cannot produce 0 (F7 clamps min 0.1, SumatraPDF.cpp:10147) → value from hand-edit/older build/corruption. Other docs (Varela = 1) scrolled fine — per-document symptom.

### Fix (1 edit, SumatraPDF.cpp:2034)
```cpp
win->autoScrollSpeedMultiplier = fs->autoScrollSpeedMultiplier < 0.1f ? 0.1f : fs->autoScrollSpeedMultiplier;
```
Ternary (not std::max) to avoid include dependency. Any load path for persisted float settings that the UI clamps must clamp too.

### Result
- Build: MSBuild Debug x64 → 0 errors. Output: Compiled\TumatraPDF.exe (21,863,424 bytes).
- Immediate workaround (before fix): one F7 press recovers (max(0×0.8, 0.1) = 0.1).
## 2026-08-15 � TRIM Rework: Clip-Based Top/Bottom Elimination + Trim Config Dialog

### What
- Trim now ELIMINATES top/bottom strips from RENDER (RenderPageArgs.pageRect clip in RenderCache::Paint non-cached path) � old zoom hack (ApplyMarginTrim) deleted.
- New Trim Config dialog (modeless, ChangeThemeWnd pattern): buttons margin-top/margin-bottom show a draggable red 2px line on the page; drag auto-calculates distance; ? saves to gGlobalPrefs->trim.top/bottom.
- New CmdTrimConfig command + "Trim Config" toolbar button + menu item.
- Trim settings persisted: Trim struct (Top/Bottom/Enabled) in gen-settings.ts.
- ShouldCacheRendering returns false when trim on (no stale cache tiles).
- Scope: top/bottom only � left/right/column-gap NOT implemented (per user).

### Files
- gen-settings.ts, gen-commands.ts (regenerated Settings.h/.cpp, Commands.h/.cpp)
- CommandAvailability.cpp, Toolbar.cpp, Menu.cpp, MainWindow.h
- NEW TrimConfigDialog.cpp/h
- DisplayModel.cpp/h, RenderCache.cpp, Canvas.cpp, SumatraPDF.cpp
- premake5.files.lua / vs2022 vcxproj (build integration)

### Result
- Build: MSBuild Debug x64 ? 0 errors. Output: Compiled\TumatraPDF.exe (21,875,200 bytes).

## 2026-08-15 � TRIM Fixes: Dialog Persistence + Continuous Layout + Stretch Fix

### What
3 bugs fixed after clip-based trim implementation:
1. Trim Config dialog disappeared when clicking/dragging red line on canvas � dialog was an unowned popup (args.parent = nullptr); canvas mouse capture dropped it behind the frame. Fix: own dialog to main window (args.parent = win->hwndFrame).
2. Black gap in continuous mode � DocumentLayout computed page height from full mediaBox, ignoring trim. Fix: DocumentLayoutParams gained trimTop/trimBottom/trimEnabled; DisplayModel::Relayout populates them from gGlobalPrefs->trim + marginTrimEnabled; DocumentLayout::Relayout reduces pageSize.dy by top+bottom (pos.dy/canvasDy/canvasSize derive from it). CmdMarginTrimToggle + TrimConfigDialog OnSave/OnCancel now call RelayoutKeepingView() (keeps view, recalc visible parts, repaint). Trim line bottom Y (OnBottom + Canvas OnMouseMove clamp) uses reduced height so line starts at visible trimmed page bottom.
3. Text stretch distortion when scrolling over trimmed region � RenderCache::Paint non-cached path clipped area AND shrank bounds (pixmap smaller than bounds ? BlitPixmap stretched). With layout now using reduced page height, pageOnScreen is already the trimmed rect: replaced clip+bounds-shrink with simple page-coord shift (area.y += t), no bounds adjustment ? 1:1 blit.

### Files
- TrimConfigDialog.cpp (parent, OnSave/OnCancel relayout, OnBottom reduced height)
- DocumentLayout.h (params fields), DocumentLayout.cpp (reduced pageSize.dy)
- DisplayModel.cpp (params population)
- SumatraPDF.cpp (CmdMarginTrimToggle ? RelayoutKeepingView)
- Canvas.cpp (OnMouseMove clamp reduced height)
- RenderCache.cpp (shift-only trim render)

### Result
- Build: MSBuild Debug x64 0 errors. Output: Compiled\TumatraPDF.exe (21,875,200 bytes).

## 2026-08-15 - Trim Config Dialog Redesign: Dual Lines + Numeric Inputs + Reset

### What
Trim Config dialog redesigned:
1. Both red lines appear immediately on dialog open (top line at top trim boundary, bottom line at bottom trim boundary), both draggable right away.
2. Dialog shows "margin top:" and "margin bottom:" numeric edits - typing a number moves the red line live; dragging a red line updates the number live (two-way sync via TrimConfigDialogSyncEdits called from Canvas OnMouseMove).
3. "Reset" button added (sets both distances to 0, moves both lines).
4. margin-top/margin-bottom buttons REMOVED (redundant - only labels remain).
5. Save (?) + Cancel kept.

### Math
Line positions use tRef = marginTrimEnabled ? gGlobalPrefs->trim.top : 0 (layout reference). topLineY = tl.y + (trimConfigTop - tRef)*zoom; bottomLineY = tl.y + (mb.dy - trimConfigBottom - tRef)*zoom. Drag clamps per line (top: [tl.y - tRef*zoom, bottomLineY]; bottom: [topLineY, tl.y + (mb.dy - tRef)*zoom]). Edit clamps: top <= mb.dy - trimConfigBottom, bottom <= mb.dy - trimConfigTop.

### Files
- MainWindow.h: trimConfigLineY removed, trimConfigDragLine added (0=none, 1=top, 2=bottom); trimConfigMode semantics: 0=closed, 1=open (both lines).
- TrimConfigDialog.h: TrimConfigDialogSyncEdits() declared.
- TrimConfigDialog.cpp: rewritten - Edit* editTop/editBottom, btnReset, suppressEditUpdate flag, OnEditTopChanged/OnEditBottomChanged/OnReset/SyncEditsFromLine; Create sets trimConfigMode=1 + both lines visible + repaint canvas; OnTop/OnBottom/UpdateInfoLabel/lblInfo deleted.
- Canvas.cpp: OnPaintDocument draws BOTH red lines; OnMouseLeftButtonDown hit-tests both lines (nearest wins); OnMouseMove drags per trimConfigDragLine + calls TrimConfigDialogSyncEdits; OnMouseLeftButtonUp resets trimConfigDragLine. Added #include "TrimConfigDialog.h".

### Result
- Build: MSBuild Debug x64 0 errors. Output: Compiled\TumatraPDF.exe (21,875,200 bytes).
- clang-format applied to Canvas.cpp, TrimConfigDialog.cpp/h, MainWindow.h.

## 2026-08-16 — Crop Zoom Fix + Toolbar Checked Bold/White

### What
- FIX1: Viewport Crop zoom snap-back bug. ApplyViewportCrop (DisplayModel.cpp) forced zoom = savedZoom*2 on EVERY call (Relayout/GoToPage/ScrollYBy) → user zoom changes while cropped instantly overridden; disable restored stale zoom. Now 2x zoom applied ONLY on first application (viewportCropSaved.IsEmpty()); subsequent calls only fix viewPort.x/dx (column position). CmdViewportCropToggle ON branch (SumatraPDF.cpp) no longer pre-saves — sets viewportCropSaved = Rect() then ApplyViewportCrop captures base state. Disable branch unchanged.
- FIX2: Toolbar checked toggle buttons (Trim/Crop/Autoscroll/Contrast) now bold white text. ReBarWndProc CDDS_ITEMPREPAINT (Toolbar.cpp): cmdId from dwItemSpec, guard cmdId > 0, TB_ISBUTTONCHECKED via SendMessageW → col = RGB(255,255,255) + SelectObject(GetAppTreeFontEx(hwndToolbar, true, false)) bold font + CDRF_NEWFONT.

### Files
- src/DisplayModel.cpp (ApplyViewportCrop)
- src/SumatraPDF.cpp (CmdViewportCropToggle)
- src/Toolbar.cpp (ReBarWndProc CDDS_ITEMPREPAINT)

### Result
- Build: MSBuild Debug x64 0 errors
- Binary: Compiled\TumatraPDF.exe = 21,875,200 bytes

## 2026-08-16 - End of Session

### What
- 3 fixes this session, all built (0 errors) + copied to Compiled\TumatraPDF.exe = 21,875,200 bytes:
  1. Crop zoom snap-back fix (ApplyViewportCrop 2x only on first apply; subsequent calls fix column only; CmdViewportCropToggle ON no longer pre-saves).
  2. Toolbar checked toggle buttons bold white (ReBarWndProc CDDS_ITEMPREPAINT + GetAppTreeFontEx(hwnd,true,false) + TB_ISBUTTONCHECKED).
  3. Inversion lost when trim=on (trim forces non-cached render path which skipped RecolorPixmap) - fixed by adding RecolorPixmap guarded by ShouldUpdateBitmapColorsLegacy in non-cached path.
- All 3 runtime-tested, pending user confirmation.

## 2026-08-17 - Trim Cache Fix + AutoScrollShowEta Visibility

### What
- BUG1: AutoScrollShowEta hidden from Advanced Options. field() was added to cmd/gen-settings.ts globalPrefs AFTER CheckForUpdates, i.e. inside internalRest section (after comment("You're not expected to change those manually")) -> generated FieldInfo 4th arg true = internal. Moved to before marker (next to ShowLinks). Regenerated: src/Settings.h autoScrollShowEta = {offsetof(GlobalPrefs, autoScrollShowEta), SettingType::Bool, true} (3-arg, visible), fieldNames packed string contains AutoScrollShowEta, fieldCount 139.
- BUG2: trim=on slowness (autoscroll lag). Root cause: DisplayModel::ShouldCacheRendering returned !marginTrimEnabled -> trim disabled render cache -> every paint re-rendered page from engine. Fix:
  - ShouldCacheRendering -> return true (cache is now trim-aware).
  - RenderCache.cpp GetTileRectDevice(+bool trimEnabled): when trim, shrink mediabox by trim.top/bottom (page coords), return empty Rect if t+b >= mb.dy, then Transform as before.
  - GetTileRectUser(+bool trimEnabled): after inverse Transform, rect.y += trim.top (shift into trimmed page coords).
  - GetTileOnScreen(+bool trimEnabled) propagates; IsTileVisible passes dm->marginTrimEnabled; Paint (~1212) + render request (~746) pass dm->marginTrimEnabled.
  - Cache invalidation: CmdMarginTrimToggle (SumatraPDF.cpp) + TrimConfigDialog OnSave call gRenderCache->FreeForDisplayModel(trimDm) after RelayoutKeepingView.
  - TrimConfigDialog.cpp needed #include "RenderCache.h" (SumatraPDF.h only forward-declares RenderCache -> C2027).
- Non-cached path in RenderCache::Paint kept as fallback (dead code).

### Files
- cmd/gen-settings.ts (moved AutoScrollShowEta)
- src/Settings.h (regenerated)
- src/DisplayModel.cpp (ShouldCacheRendering)
- src/RenderCache.cpp (GetTileRectDevice/User/OnScreen trimEnabled)
- src/SumatraPDF.cpp (CmdMarginTrimToggle FreeForDisplayModel)
- src/TrimConfigDialog.cpp (OnSave FreeForDisplayModel + include)

### Result
- Build: MSBuild Debug x64 0 errors (1st attempt C2027 RenderCache undefined -> fixed with include)
- Binary: Compiled\TumatraPDF.exe = 21,875,200 bytes (hash matches out\dbg64; stale running instance killed to unlock)

## 2026-08-24 - Small-Tab Root Cause Found + Active Tab Underline

### What
- ROOT CAUSE of "tiny tab" bug FOUND: Compiled\TumatraPDF-settings.txt had TabWidth = 60 (settings floor is 60, AppSettings.cpp). 60px tab - 8px textPad - ~28px close-gutter reservation = ~20px text = only 2-3 letters visible. Not a code bug at all.
- Fixed by resetting settings line to TabWidth = 300 (default).
- New: active tab underline - white 3px (DpiScale) horizontal strip along bottom edge of selected tab, painted right after per-tab bg fill in TabWnd::Paint using same SolidBrush/FillRectangle primitive.
- Reclaimed close-gutter text space: rTxt width no longer pulled to rClose.x - textGap; now spans full padded tab width (r.x+textPad .. r.x+r.dx-textPad), LTR and RTL unified (RTL alignment still via StringFormat). rClose computation left untouched (dead but harmless since CloseVisible()=false).

### Files
- sumatrapdf-src/src/wingui/TabsCtrl.cpp (TabWnd::Paint underline + rTxt)
- Compiled/TumatraPDF-settings.txt (TabWidth 60 -> 300)

### Result
- Build: bun cmd/build.ts, 0 errors / 0 warnings, 66.8s
- Deploy: running instance killed (PID 9064); Compiled\TumatraPDF.exe = 21,895,168 bytes
- Settings verified: line 47 "TabWidth = 300"
2026-08-24 — autoscroll toolbar buttons reordered: - before +

## 2026-08-24 — Dev Tooling: Guide tumatrapdf-v1-distilled + debugview Skill

### What
- Created lemma guide `tumatrapdf-v1-dev` (dev-tool): consolidated manual from ~123 project memories. Sections: build/deploy protocol, toolbar NM_CUSTOMDRAW gotchas, tabs VirtWnds refs, diagnostics decision rules.
- Anchors distilled into guide: mde3b8faaa846 (custom-draw DC space), m22b30c2898df (small-tab settings root cause), ma48b4e41db49 (tab freeze removal).
- Build entrypoint CONFIRMED: `cmd/` lives inside sumatrapdf-src/ (not repo root). Workdir sumatrapdf-src → `bun cmd/build.ts` → vs2022\TumatraPDF.sln /t:TumatraPDF → out\dbg64\TumatraPDF.exe. Fallback msbuild direct; new files → `bun cmd/premake.ts` first.
- Installed skill `debugview` (microsoft/skills, official) → ~\.agents\skills\debugview. Captures OutputDebugString/DebugView output for native Win32 debugging.
- Skills search verdict: ecosystem weak for native C++/Win32 profiling (no WPA/xperf/VS Profiler skills worth installing). Profiling protocol folded into guide instead.

### Result
- Guide live (usage count 4 after distills); skill installed globally (OpenCode included)
- No code changes in this session

## 2026-08-24 — Full Audit: FLOW/tumatrapdf.dot Rewritten From Real Code

### What
- Audited entire fork surface (explore agent + grep): toolbar gToolbarButtons Toolbar.cpp:69-97, menus menuDefNewTools Menu.cpp:592-641, accelerators Accelerators.cpp, settings cmd/gen-settings.ts.
- Fork reality: ~810 insertions / 34 files, direct patches in sumatrapdf-src/src/ + TrimConfigDialog.{h,cpp}. src/features/ + src/hooks/ DO NOT EXIST — old .dot cluster was aspirational; README "Modularity Strategy" still describes it (known falsidade).
- FLOW/tumatrapdf.dot REWRITTEN: phases updated (fase5 partial: ASan sln rename ✗, .vscode F5 ✗), real toolbar order (25 buttons), New Tools menu tree, Tumatra shortcuts (F7/F8/F9/Ctrl+Shift+C/Alt+Click), settings global+per-doc, features w/ file:line refs, dead-code cluster, deps+legend kept.
- Bottlenecks → lemma memory m03bbbdedaab9 for next agent (8 itens): F10-vs-F9 doc mismatch, missing shortcuts crop/trim/contrast±10%, QuickToggleViewportCrop dead, TabsCtrl ✕ dead paths, build-asan.ts broken sln ref, .vscode F5 broken, no Test* dbg-control cmds for fork features, commented accels.

### Result
- Files: FLOW/tumatrapdf.dot (rewrite); LOG.md (this)
- Memory: m03bbbdedaab9 (warning) + .dot fidelity note
- No C++ changes

### 2026-08-24
- Invert restore fixed: UpdateDocumentColors() called unconditionally after LoadDocument per-doc state apply (was loading invert/contrast state without repaint).
- Autoscroll default speed for unadjusted docs = minimum 0.1 (FileState AutoScrollSpeedMultiplier default 1.0 -> 0.1 in gen-settings.ts; Settings.h/cpp regenerated); per-doc last-used restore already existed.

## 2026-08-25 — Markdown WebView2: AutoScroll + Contrast via JS

### What
Native WebView2 support for AutoScroll + Contrast on Markdown (.md). Previously both silently no-op'd / invisible in default webview mode. MuPDF fallback (`markdownUI.useFixedPageUI=true`) untouched. CHM untouched.

### AutoScroll md-webview
- `AutoScrollToggle` guard relaxed — accepts webview-mode docs
- `AutoScrollContinuousTick` branches: DisplayModel → `MoveDocBy(0,dy)`; webview → `wv->Eval("window.scrollBy(0,dy)")`
- Stop-at-bottom: JS posts `autoscrollBottom` → `BrowserDocView::OnJsNotifyCb` → virtual `DocController::OnAutoScrollBottom()` → `MarkdownModel` forwards → `ControllerCallbackHandler` stops timer + updates toolbar
- F7/F8 speed multiplier shared unchanged

### Contrast md-webview
- `CmdContrastToggle` branches: webview → inject/toggle fixed-position `div#__sumatra_contrast` (`background:#000`, `pointer-events:none`, `z-index` max) via `Eval`; opacity = `(contrastOpacity*255)/100/255.0f` — identical math to Win32 overlay
- `CmdContrastIncrease/Decrease` → `UpdateContrastOverlayOpacity` dispatches JS path when `win->AsMarkdown()`
- `CreateContrastOverlay` early-return no-op for markdown
- `RestoreContrastOverlay()` called from `MarkdownModel::OnDocumentComplete` → persistence across navigation
- New virtuals: `DocController::OnAutoScrollBottom`, `GetContrastEnabled`, `GetContrastOpacity`; accessor `BrowserDocView::GetWebviewWnd`

### Files
11 files changed +232/-40

### Build
`bun cmd/build.ts` — 0 err / 0 warn (35s). Deployed `Compiled\TumatraPDF.exe` 21,899,776 bytes @ 2026-08-25 21:56. Runtime interactive test PENDING (user verifies F9 / Ctrl+Shift+C on .md).

---

## 2026-08-26 — Round 3: Speed Label Slot, Min Speed Reduction, Contrast Leak Fix

### What
Three targeted fixes for toolbar layout, speed scaling, and contrast cross-doc leak.

### Speed label layout
Migrated from floating "left of [-]" (which caused overlap) to dedicated toolbar slot via fake placeholder button `SpeedInfoId` + `TbSetButtonDx` (cloned page-counter pattern). Occupies toolbar space like a native control.

### Min speed reduced to 1/6
`0.1f` → `0.0167f` across `MainWindow.h`, `LoadDocument`, `AutoScrollSpeedAdjust`, and `gen-settings.ts` (regenerated).

### Contrast black-screen leak
Fixed by calling `DestroyContrastOverlay(win)` unconditionally on document change (`LoadDocument`) and in OFF toggle branch for all modes (clears tainted cross-doc Win32 layers).

### Build
`bun cmd/build.ts` — 0 errors / 0 warnings (118.5s, 21,911,552 bytes).

## 2026-08-25 — Follow-up: md Toolbar + Logging + Instrumentation

### What
Regression fixes after runtime test of md-webview features. Cumulative diff 15 files +382/-44.

### Fix 1 — Toolbar hidden on .md (PRE-EXISTING upstream bug)
- Root: `CommandAvailability.cpp` ~L379 `ctx.isChm = AsChm() || AsMarkdown()` → CHM blacklist applied to markdown → AutoScroll×3 + ContrastToggle disabled/hidden for .md
- Fix: `isChm = AsChm()` only; new `removeIfMarkdown[]` hides only fixed-page cmds (`ViewportCrop`/`MarginTrim`/`TrimConfig`) for .md; AutoScroll×3 + ContrastToggle now enabled for .md; CHM unchanged

### Fix 2 — Contrast persistence AUDITED (no code change)
- `UpdateTabFileDisplayStateForTab` saves `contrastEnabled`/`contrastOpacity` for all types including md; `LoadDocument` restores → already correct
- Note: black veil on reopen = persisted contrast ON → `Ctrl+Shift+C` toggles off (by design, matches PDF behavior)

### Fix 3 — File logging upgrade (user request, TumatraPDF2-style auto-log)
- `SumatraLog.h/.cpp`: file output now `[YYYY-MM-DD HH:MM:SS.mmm] [INFO]/[WARN]/[ERROR]` tags; rotation truncate >5MB; `StartLogToFile` unconditional (was already)
- Log file: `%LOCALAPPDATA%\SumatraPDF\<hash>\sumatra-log.txt` (hash = install dir)

### Instrumentation added
- `[md] doc complete / contrast restore / contrast toggle / contrast opacity`
- `[autoscroll] start|stop / webview tick start / bottom reached`
- `[webview] autoscrollBottom notify`

### Build
`bun cmd/build.ts` — 0 err / 0 warn (34.1s). Deployed `Compiled\TumatraPDF.exe` 21,907,456 bytes @ 2026-08-25.

## 2026-08-25 — Follow-up Round 2: Speed Label + md ETA + Contrast Leak Fix

### What
Polish round 2 for md autoscroll/contrast — speed visibility, md ETA parity, fresh-doc state leak. Cumulative diff 10 files +329/-40 vs round 1 base.

### Fix 1 — Fresh-doc min speed default
- `MainWindow.h`: `autoScrollSpeedMultiplier` default 1.0f → 0.1f (matches `FileState` 0.1). Fresh docs (no persisted state) now start at minimum speed — adjustable before start via F7/F8 or [+]/[-].

### Fix 2 — Contrast cross-tab leak (fresh .md inherited prev tab state)
- `SumatraPDF.cpp` `LoadDocument` else-branch (`fs == nullptr`): resets `contrastEnabled=false`, `contrastOpacity=50`, `autoScrollSpeedMultiplier=0.1f`. Fresh .md no longer inherits previous tab's contrast/speed.

### Fix 3 — Contrast OFF cleanup
- WebView2 `div#__sumatra_contrast`: OFF now `d.remove()` instead of `display:none` — DOM removed, no hidden overlay.

### New 1 — Speed label `%d px/min` in toolbar
- `Toolbar.cpp`: `CreateSpeedLabel` / `RepositionSpeedLabel` / `UpdateToolbarSpeedLabel` — static text left of [-] button, always visible with doc open, shows current `AutoScrollPxPerSec` (`%d px/min`). Updates on F7/F8, [+]/[-] clicks, doc load.

### New 2 — ETA for .md webview (parity with PDF)
- Parallel chain: JS `autoscrollProgress` notify ~100ms → `BrowserDocView::OnJsNotifyCb` → virtual `DocController::OnAutoScrollProgress(remainingPx)` → `ControllerCallbackHandler` computes `etaMinutes = remainingPx / AutoScrollPxPerSec` → `UpdateToolbarEtaText`. Fixed-page PDF ETA verified intact — no regression.

### Build
`bun cmd/build.ts` — 0 err / 0 warn (20.7s). Deployed `Compiled\TumatraPDF.exe` 21,907,456 bytes @ 2026-08-25.
## 2026-08-29 � Arch Tools Fase 12: Scale regression + Measure underline inversion + rename
- **Scale OnOk regression fix:** guard changed from !win->archScaleSet || win->archScaleFactor <= 0.0f to !(win->archScaleLineDefined || win->archScaleSet) so a freshly drawn scale line (archScaleSet still false until OnOk) is accepted; factor recomputed from stored archScaleLineP1/P2.
- **Measure underline inverted:** removed BTNS_CHECK auto-toggle from CmdArchMeasure button in CreateToolbar2 (Toolbar.cpp:1958-1960); UpdateToolbar2State is now the sole manager of checked state (called from CmdArchMeasure handler, CmdArchToolsToggle, and ESC in FrameOnKeydown). Underline now tracks rchToolMode == 2 correctly (was appearing when OFF, hiding when ON).
- **Rename:** 2nd-toolbar button Limpar linhas ? Clean lines (Toolbar.cpp:1942).
- **Build:** 0 errors / 0 warnings (un ./cmd/build.ts). **Deploy:** Compiled\TumatraPDF.exe (21.9 MB). **Smoke:** clean launch -for-testing -console, no crash, no new dump.

## 2026-08-29 — Autoscroll Timer UI (Fase 15, BUILD OK)
- Adicionado controle Timer na toolbar ANTES do botão Autoscroll: `[checkbox][Timer:][input numérico]`.
- Comportamento: checkbox ON → autoscroll para sozinho após N min (valor do input, padrão 30). OFF → sem limite.
- Backend de auto-parada já existia (AutoScroll.cpp `AutoScrollContinuousTick` para quando `autoScrollTimerMinutes > 0` e tempo passa); a UI agora alimenta `autoScrollTimerMinutes`.
- Arquivos editados: `MainWindow.h` (autoScrollTimerMinutesSetting=30, autoScrollTimerEnabled=false, HWNDs dos controles), `AutoScroll.cpp` (feed do campo em AutoScrollToggle), `SumatraPDF.cpp` (handlers CmdAutoScrollTimerToggle/Edit + apply de gGlobalPrefs), `Toolbar.cpp` (placeholder TimerInfoId + CreateTimerControls/RepositionTimerControls + WM_CTLCOLORBTN dark mode), `Settings.h` (campos AutoScrollTimerMinutes/AutoScrollTimerEnabled regenerados), `cmd/gen-settings.ts` (campos).
- PENDENTE: verificação UI pelo usuário (amanhã) — `[checkbox][Timer:][input]` antes de Autoscroll; timer para autoscroll após N min.
- Build: OK (0 erros, 1 warning unrelated em test_util). Commands.h regenerado, Settings em sync, deploy feito, smoke test limpo.

## 2026-08-29 (2) — Fase 15 fixes: autoscroll speed + timer visual
- Autoscroll: `kMinSpeedMultiplier` (AutoScroll.cpp) 0.1f -> 0.008f; velocidade mínima agora ~100 px/min (antes ~1400). Persistência de `AutoScrollSpeedMultiplier` garantida (load+save); default = mínimo se nunca usado.
- Timer control: `TimerInfoId` 110->130px; layout com tokens de design (kCtrlGapX=4, kCtrlH=18) — gaps consistentes + centragem vertical. Início de design system mínimo.
- Build: 0 erros. Deploy + smoke OK.

## 2026-08-30 — Fase 15 Fixes (speed clamp, persistência, timer visual, build fix)

- **Autoscroll speed clamp**: `kMinSpeedMultiplier` 0.1f → 0.008f (AutoScroll.cpp:74). Min speed agora ~100 px/min (era ~1400).
- **Lembrar última velocidade**: persistência via FileState `autoScrollSpeedMultiplier` (já existia, per-documento). Defaults baixados 0.0167f → 0.008f (MainWindow.h:332, SumatraPDF.cpp:2084/2104, gen-settings.ts:792). "Se nunca usado → mínimo" correto.
- **Timer visual**: design tokens `kCtrlGapX=4`, `kCtrlH=18` (Toolbar.cpp:54-56); `TimerInfoId` 110 → 130 (Toolbar.cpp:1201); CreateTimerControls/RepositionTimerControls com token math. Fix bug `slot`→`r` em CreateTimerControls (Toolbar.cpp:1361).
- **Build fix**: revert regressão `..\vs2022\TumatraPDF.sln` → `vs2022\TumatraPDF.sln` (build.ts:37). Build roda de `sumatrapdf-src` (`bun cmd/build.ts`). 0 err / 0 warn. Deploy automático p/ Compiled\TumatraPDF.exe. Smoke limpo (0 crash dumps).
- **Pendente**: teste UI prático pelo usuário (amanhã) — timer control [checkbox][Timer:][input] antes de Autoscroll; speed mínimo; persistência; alinhamento visual.

## 2026-08-30 — Fase 16B: MainWindow.h Struct Modularization (AutoScrollState + ArchToolsState)

### What
Grouped ~274 flat MainWindow member variables into two nested domain structs:
- **AutoScrollState** (win->autoScroll): active, speed, speedMultiplier, accum, etaMinutes, etaStartTick, etaLastShown, etaPageNo, etaToolbarWidth, hwndEtaLabel, hwndSpeedLabel, timerMinutes, startTick, timerMinutesSetting, timerEnabled, hwndTimerCheck, hwndTimerLabel, hwndTimerEdit.
- **ArchToolsState** (win->archTools): on, mode, unit, scaleFactor, scaleAnchorX/Y, scaleSet, scaleLineDefined, point1/2, dragLine, snap, measurements, measurementsByDoc, scaleLineP1x/P1y/P2x/P2y, hwndArchScaleDialog, scaleDialog, dragStartPos, dragMoved, eraseMode, mousePos, hwndReBar2, hwndToolbar2.

### Files changed
- src/MainWindow.h — struct definitions + member renaming (~90% of references)
- src/SumatraPDF.cpp — win-> prefix updates; bare rchMeasurementsByDoc/rchMeasurements in member functions fixed with 	his->archTools.
- src/Toolbar.cpp, src/AutoScroll.cpp, src/Canvas.cpp, src/ArchScaleDialog.cpp — win-> prefix updates

### Build
un cmd/build.ts — **0 errors / 0 warnings** (60.5s). Deployed Compiled\TumatraPDF.exe (21.9 MB). Smoke test clean (no crash dumps).

### Key lesson
In MainWindow member functions, win is NOT a local variable — bare member names become 	his->archTools.xxx, NOT win->archTools.xxx.

### Next
- 16C: Split SumatraPDF.cpp (12.743 lines) into per-domain command files

## 2026-08-30 — Fase 16C: Measure Persistence Fix (ArchTools)

### What
Bug: measure lines disappeared on tab switch (scale persisted correctly)

### Root cause
Measure stored in-memory Vec only, never serialized to FileState

### Fix
Added ArchMeasurements field to FileState via gen-settings.ts + Settings.h; added SerializeMeasurements/DeserializeMeasurements helpers; save on tab switch, restore on document load

### Build
`bun cmd/build.ts` — 0 errors / 0 warnings

### Files modified
- gen-settings.ts
- Settings.h (generated)
- SumatraPDF.cpp
### Fase 16C-2: Markdown Contrast + Arch Tools Hide (2026-08-30)

#### What
- Created exclusive contrast system for .md files (independent from regular file contrast)
- Hidden Arch Tools toolbar and commands when viewing .md files

#### Contrast .md behavior
- When contrast=ON for .md: background stays light (#FAFAFA), text becomes dark gray
- Gray level controlled by contrastOpacity: 0=black, 50=gray, 100=light gray
- Formula: gray = 255 * (1 - opacity/100)
- Regular files: unchanged (Win32 overlay for PDF, CSS dark mode for others)

#### Arch Tools hidden for .md
- Added CmdArchTools* to removeIfMarkdown[] in CommandAccuracy.cpp
- CmdArchToolsToggle early-returns for .md files
- RelayoutFrame hides hwndReBar2 when current doc is .md

#### Build
- `bun cmd/build.ts`: 0 errors / 0 warnings (38.0s)
- Deployed: Compiled\TumatraPDF.exe (21.9 MB)
- Smoke test: clean (no crash dumps)

#### Files modified
- src/SumatraPDF.cpp: IsCurrentDocMarkdown() helper, CmdContrastToggle md branch, CmdArchToolsToggle guard, RelayoutFrame hide for md
- src/MarkdownModel.cpp: RestoreContrastOverlay() CSS formula
- src/CommandAccuracy.cpp: removeIfMarkdown[] + Arch Tools commands

## 2026-08-30 — Fase 16C-2: Markdown Contrast + Arch Tools Hide + AutoScroll Extraction

### What
1. Markdown (.md) exclusive contrast system — light bg (#FAFAFA) + dark gray text (formula: gray = 255 * (1 - opacity/100)). Independent from fixed-page contrast (Win32 veil).
2. Arch Tools hidden for .md files — CmdArchToolsToggle early return, RelayoutFrame hides hwndReBar2, CommandAccuracy.cpp removeIfMarkdown[] updated.
3. AutoScroll commands extracted from SumatraPDF.cpp → Commands_AutoScroll.{h,cpp} (6 handlers). Pilot extraction validated the split pattern.

### Files modified
- src/SumatraPDF.cpp (IsCurrentDocMarkdown, CmdContrastToggle, CmdArchToolsToggle, RelayoutFrame, 6 AutoScroll dispatches)
- src/MarkdownModel.cpp (RestoreContrastOverlay CSS formula)
- src/CommandAccuracy.cpp (removeIfMarkdown[] + Arch Tools commands)
- src/Commands_AutoScroll.h (NEW — 6 handler declarations)
- src/Commands_AutoScroll.cpp (NEW — 6 handler implementations)
- vs2022/TumatraPDF.vcxproj + .filters
- vs2022/SumatraPDF-static.vcxproj + .filters

### Build
 bun cmd/build.ts` — 0 errors / 0 warnings (30.7s). Deployed + smoke clean.

### Fase 16C-3: ViewCommands Extraction + Build Fix (2026-08-30)

#### What
Extracted 35 view/zoom/rotate command handlers from SumatraPDF.cpp into Commands_View.{h,cpp}. Fixed linker errors caused by missing forward declarations and signature mismatches. Also cleaned up TODO.md (duplicate content, garbled UTF-8).

#### Files created
- src/Commands_View.h — 35 handler declarations
- src/Commands_View.cpp — 35 handlers + 7 static helpers moved from SumatraPDF.cpp

#### Files modified
- src/SumatraPDF.cpp — added #include, replaced 35 case bodies with function calls, removed 7 static helpers (now in Commands_View.cpp), removed static from RelayoutFrame/IsCurrentDocMarkdown, removed default args from RelayoutFrame declaration
- src/Commands_ArchTools.h — forward declarations for RelayoutFrame/IsCurrentDocMarkdown removed (moved to Commands_View.h)
- src/Commands_ArchTools.cpp — updated RelayoutFrame forward decl and call to match real signature
- vs2022/TumatraPDF.vcxproj + .filters — added Commands_View.cpp
- vs2022/SumatraPDF-static.vcxproj + .filters — added Commands_View.cpp

#### Build
0 errors / 0 warnings (34.3s). Deployed to Compiled/TumatraPDF.exe.

#### Key lesson
Extracted files must NOT use static functions from SumatraPDF.cpp. Solutions: (1) move static helpers to the extracted .cpp, or (2) make them non-static + declare in header. Forward declarations preferred to avoid deep include chains.
