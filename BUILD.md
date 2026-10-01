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

## Release + auto-update (s26/s29)

### How to ship a new release (copy-paste)

```powershell
# 0. sanity: worktree clean-ish, latest main pulled
git pull --rebase origin main

# 1. bump the version baked into the exe
#    edit sumatrapdf-src/src/BuildConfig.h: CURR_VERSION 1.0 -> next version (keep it lower-then-tag equal: tag matches)
# 2. build + test release locally (recommended)
cd sumatrapdf-src
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" vs2022\TumatraPDF.sln /t:TumatraPDF /p:Configuration=Release /p:Platform=x64 /m
cd ..

# 3. commit, tag, push tag (tag push triggers the workflow)
git commit -am "release v1.0.8"
git push origin HEAD:main
git tag v1.0.8
git push origin refs/tags/v1.0.8

# 4. watch it
gh run watch --exit-status
```

After ~3-4 min the workflow publishes `TumatraPDF-portable-win64.exe` to GitHub Releases and auto-commits the `update.txt` bump (`Latest: 1.0.8`) to `main` — so `git pull` BEFORE your next push, and running apps v1.0.x get the update dialog within a day (daily check; `update.txt` is checked against app version via `CompareProgramVersion`).

Gotchas:
- **Never delete/retag `v*`** — tags are protected (ruleset, admin-bypass only). Bump to the next patch instead.
- CI needs: NASM step, WebView2 nuget restore, tracked `sumatrapdf-src/bin/` tools — all already in `.github/workflows/release.yml`; don't remove.
- The workflow runs `gh`-style commit+push with GITHUB_TOKEN (`permissions: contents: write`) — repo default token is `read`; only this workflow may write.

### How the system works
- **Pipeline**: `.github/workflows/release.yml` — push tag `v*` → windows-2022 runner (setup-bun + setup-msbuild + setup-nasm + nuget WebView2) → Release x64 → renames exe to `TumatraPDF-portable-win64.exe` → bumps + commits repo-root `update.txt` → GitHub Release published.
- **Version source**: `src/BuildConfig.h` (`CURR_VERSION`). update.txt `Latest:` must be numerically > the running app's version for the prompt to fire.
- **Update flow in-app**: UpdateCheck.cpp pulls `update.txt` daily (setting `CheckForUpdates`), downloads `releases/latest/download/TumatraPDF-portable-win64.exe`, validates MZ header + ≥1 MiB (unsigned, no cert), then `SelfUpdateViaBatch`: detached cmd batch (ping-sleep, no `timeout` — fails console-less) waits for exit → `move /Y` over running exe → relaunch. E2E test: `tests/ad-hoc-update.ts` (registered in `before-release.ts`).
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
