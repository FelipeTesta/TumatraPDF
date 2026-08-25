# MERGE.md — TumatraPDF Fork Merge Strategy

## Overview

TumatraPDF is a direct fork of SumatraPDF (upstream baseline ~912ecf2, August 2026). All fork changes are implemented directly as patches within `sumatrapdf-src/src/`. There are no `src/features/` or `src/hooks/` directories; `sumatrapdf-src/` is directly modified. Baseline copy preserved in `.tmp_compare/upstream/`.

## Fork Features (one-liner)

- Toolbar toggles (AutoScroll, Contrast, ViewportCrop, MarginTrim) with active underline and theme-agnostic styling
- Tabs underline and Alt+Click close support
- Autoscroll ETA display and per-doc speed memory
- Invert and contrast state persistence per document
- TrimConfig dialog window and ContrastOverlay module

## Merge Process

1. Check for upstream updates: `.\scripts\check-updates.ps1`
2. Backup current state: `git checkout -b merge/vX.Y.Z && git push -u origin merge/vX.Y.Z`
3. Clone new upstream version, replace `sumatrapdf-src/`
4. Diff against backup: `git diff --stat sumatrapdf-src.backup sumatrapdf-src`
5. Focus on areas affecting fork features:
   - `src/Theme.cpp` / `src/Theme.h` — color pipeline
   - `src/SumatraPDF.cpp` — main window, commands
   - `src/Canvas.cpp` — rendering
   - `src/DisplayModel.cpp` — display model
   - `src/AppSettings.cpp` — settings
   - `premake5.lua` / `premake5.files.lua` — build system
6. Adapt fork patches to upstream changes
7. Build and test: `bun cmd/build.ts` and `bun cmd/run-unit-tests.ts -dbg`
8. Update docs: `README.md`, `LOG.md`, `FLOW/tumatrapdf.dot`
9. Commit and merge to master

## Useful Commands

```powershell
# Build (Debug x64)
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" sumatrapdf-src\vs2022\TumatraPDF.sln /t:TumatraPDF /p:Configuration=Debug /p:Platform=x64 /m

# Run for testing
.\sumatrapdf-src\out\dbg64\TumatraPDF.exe -for-testing

# Clean build
Remove-Item -Recurse sumatrapdf-src\out\ -ErrorAction SilentlyContinue
```