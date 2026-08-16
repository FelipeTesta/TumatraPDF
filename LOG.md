# TumatraPDF — Development Log

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