# Build — TumatraPDF

Build guide. **Read before attempting any build.**

## Correct method (CRITICAL)

Build MUST run from INSIDE `sumatrapdf-src`. NOT from repo root (`TumatraPDF\`).

```powershell
cd sumatrapdf-src
bun cmd/build.ts
```

Output: `sumatrapdf-src\out\dbg64\TumatraPDF.exe`

### Why

`cmd/build.ts` (line 37) uses `vs2022\TumatraPDF.sln` (committed/correct version). Path
relative to cwd. Running from `sumatrapdf-src` → `sumatrapdf-src\vs2022\TumatraPDF.sln` ✓.

An agent regressed it to `..\vs2022\TumatraPDF.sln` (cwd-relative: parent
of sumatrapdf-src = missing file → MSB1009). Reverted to `vs2022\TumatraPDF.sln`.

Running `bun cmd/build.ts` from repo root (`TumatraPDF\`):
`bun : error: Module not found "cmd/build.ts"` (script lives in `sumatrapdf-src\cmd\`, not `TumatraPDF\cmd\`).

Build auto-copies exe to `..\Compiled\TumatraPDF.exe` (finalExeDir = join("..","Compiled")).
Manual deploy step below redundant but safe.

Subagents fail builds from root or by timeout. Architect (bash) always `cd sumatrapdf-src` first.

## Deploy (copy to Compiled)

```powershell
Copy-Item -Path sumatrapdf-src\out\dbg64\TumatraPDF.exe -Destination Compiled\TumatraPDF.exe -Force
```

## Smoke test

```powershell
Compiled\TumatraPDF.exe -for-testing -console
```

Close window → clean exit, no crash dump under `%LOCALAPPDATA%\SumatraPDF-data\<hash>\crashinfo`.

## Fallback: direct MSBuild

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" sumatrapdf-src\vs2022\TumatraPDF.sln /t:TumatraPDF /p:Configuration=Debug /p:Platform=x64 /m
```

## Release + auto-update (s26)

- **Pipeline**: `.github/workflows/release.yml` — push tag `v*` → windows-2022 runner (setup-bun + setup-msbuild) → Release x64 → bumps + commits repo-root `update.txt` → GitHub Release with `TumatraPDF.exe`.
- **Version source**: `src/BuildConfig.h` (`CURR_VERSION 1.0`). Tag version (e.g. `v1.0.1`) must be > running app's `Latest`/`CURR_VERSION` semantics: update.txt `Latest: 1.0.1` > app `1.0` triggers prompt.
- **Update flow in-app**: UpdateCheck.cpp pulls `update.txt` daily (setting `CheckForUpdates`), downloads `releases/latest/download/TumatraPDF-portable-win64.exe`, validates MZ header + ≥1 MiB (unsigned, no cert), then `SelfUpdateViaBatch`: detached cmd batch waits for exit → `move /Y` over running exe → relaunch.
- **Telemetry stance**: zero phone-home except the update-check GET against this repo. No crash upload (local dumps only).

## Why the exe shrank 22 MB → 11.7 MB

The two sizes are the SAME code — Debug vs Release configuration:

- **Debug (22 MB)**: `/Od` (no optimization), full runtime checks (`/RTC1`), `_ITERATOR_DEBUG_LEVEL=2` (extra STL debug locking), PDB-path + `__FUNCTION__` strings embedded, all asserts + `ReportIf` + [fc]/[arch] log strings live, no dead-code elimination.
- **Release (11.7 MB)**: `/O2`, checks compiled out, PLUS linker dead-code stripping (`/OPT:REF` — drops unreferenced functions/data) + identical-COMDAT folding (`/OPT:ICF` — merges identical code), and mupdf/SumatraPDF build most string tables + logging out.
- Debug info itself (.pdb ~100+ MB) is NOT in the exe — the size gap is purely codegen + stripping.
- Rule: `Compiled\TumatraPDF.exe` = Release build (`out\rel64`), `TumatraPDF-debug.exe` = Debug (`out\dbg64`). `cmd/build.ts` only builds Debug x64 (and overwrites `Compiled\TumatraPDF.exe` with it — re-copy `out\rel64\TumatraPDF.exe` after a debug build if both are deployed).

## New files in src/

Adding .cpp/.h → run `bun cmd/premake.ts` first (regenerates `vs2022/*.vcxproj`).

## Generated commands/settings

New command ID: edit `cmd/gen-commands.ts`, run `bun cmd/gen-commands.ts`
(regenerates `src/Commands.h`). New setting: edit `cmd/gen-settings.ts`, run
`bun cmd/gen-settings.ts` (regenerates `src/Settings.h`). NEVER hand-edit `@gen` files.

## Known issues

- **LNK1201 (PDB lock)**: `libsumatrapdf.pdb` locked by VS Code/OneDrive. Rename PDB, rebuild.
- **build-asan.ts / .vscode F5 broken**: reference old `SumatraPDF.sln`. Do not use.
- **build_log.txt**: `Module not found` error = wrong cwd (repo root). Use `cd sumatrapdf-src`.
- **MSB1009 (sln not found)**: `build.ts:37` with `..\vs2022` is a regression. Must read `vs2022\TumatraPDF.sln`. Revert on sight.

## Docs rule

Informativos stay English + caveman (AGENTS.md §13).
