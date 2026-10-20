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
8. Update docs: `README.md`, `LOG.md`, `FLOW/tumatrapdf.dot` (English + caveman, AGENTS.md §13)
9. Commit and merge to master

## Dependency upgrades — vendored tree rules

mupdf is the one dependency whose delta is a patch stack, so it has a fixed
procedure (used in s36-z for 1.28.0-rc2 → 1.28.5):

1. Fetch the upstream SumatraPDF commit that bumps mupdf (`git fetch --depth 1
   <full-SHA>` in a sparse clone of `ext/mupdf`) — its tree IS the target state.
2. Replace `sumatrapdf-src/mupdf/` wholesale (CRLF-normalized; repo stores it
   with CRLF under `.gitattributes` `* -text`). Never hand-edit that tree.
3. Whole-file additions upstream keeps OUTSIDE the vendored tree (e.g.
   `pkcs7-windows.c`) live in `sumatrapdf-src/src/mupdf/` and are wired into
   `premake5.files.lua` + `includedirs`.
4. Patches ride along in `sumatrapdf-src/ext/patches/` (copy of upstream
   `ext/patches/`); verify with
   `git apply --check -R --ignore-whitespace -p1 <patch>` (repo CRLF vs patch LF
   ⇒ `--ignore-whitespace` is mandatory, otherwise every patch falsely conflicts).
5. `bin/premake5.exe vs2022` (fork sources are protected by
   `premake5.files.lua`), delete stale `out/<cfg>/obj/**/*.pch`, then
   `bun cmd/build.ts`.

Other vendored deps (libarchive, libjpeg-turbo, mujs, decoders) are curated flat
trees: sync only the files present on both sides, keep the hand-maintained build
config (`config_windows.h`, `jconfig*.h`, ...), then update `ext/versions.txt`.


## Useful Commands

```powershell
# Build (Debug x64)
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" sumatrapdf-src\vs2022\TumatraPDF.sln /t:TumatraPDF /p:Configuration=Debug /p:Platform=x64 /m

# Run for testing
.\sumatrapdf-src\out\dbg64\TumatraPDF.exe -for-testing

# Clean build
Remove-Item -Recurse sumatrapdf-src\out\ -ErrorAction SilentlyContinue
```