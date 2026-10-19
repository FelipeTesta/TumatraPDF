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

After ~3-4 min the workflow publishes BOTH assets — `TumatraPDF-portable-win64.exe` + `TumatraPDF-install-win64.exe` (same exe; the `install` in the name flips it to installer mode) — to GitHub Releases and auto-commits the `update.txt` bump (`Latest: 1.0.8`) to `main` — so `git pull` BEFORE your next push, and running apps v1.0.x get the update dialog within a day (daily check; `update.txt` Latest is compared against the exe's `CURR_VERSION` via `CompareProgramVersion` — bump BuildConfig.h in the release commit or the prompt loops forever).

Gotchas:

- **Never delete/retag `v*`** — tags are protected (ruleset, admin-bypass only). Bump to the next patch instead.
- CI needs: NASM step, WebView2 nuget restore, tracked `sumatrapdf-src/bin/` tools — all already in `.github/workflows/release.yml`; don't remove.
- The workflow runs `gh`-style commit+push with GITHUB_TOKEN (`permissions: contents: write`) — repo default token is `read`; only this workflow may write.

### How the system works

- **Pipeline**: `.github/workflows/release.yml` — push tag `v*` → windows-2022 runner (setup-bun + setup-msbuild + setup-nasm + nuget WebView2) → Release x64 → copies exe as `TumatraPDF-portable-win64.exe` + `TumatraPDF-install-win64.exe` → bumps + commits repo-root `update.txt` (PortableExe64 + Installer64 keys) → GitHub Release published. Local equivalent: `bun cmd/build-installer.ts` (also copies both flavors to ../Compiled).
- **Installer mechanics**: no separate installer build. Same dll-mode exe; `install` in the filename (or `-install` flag) runs installer mode. Payload `out/<cfg>/InstallerData.dat` (libsumatrapdf.dll/PdfFilter.dll/PdfPreview.dll/sumatrapdf-tool.exe) is packed by the vcxproj prebuild (MakeLZSA, only when missing; libsumatrapdf postbuild deletes it to force repack) and embedded via `INSTALL_PAYLOAD_ZIP`. Install dir `%LOCALAPPDATA%\TumatraPDF` (or Program Files `-all-users`), registry keys all derive from `kAppName="TumatraPDF"` — zero overlap with upstream SumatraPDF. Installed exe = `TumatraPDF.exe` (`kExeName`, SumatraConfig.h).
- **Version source**: `src/BuildConfig.h` (`CURR_VERSION`). update.txt `Latest:` must be numerically > the running app's version for the prompt to fire.
- **Update flow in-app**: UpdateCheck.cpp pulls `update.txt` daily (setting `CheckForUpdates`), downloads the `Installer64` URL (both flavors read that key — `IsDllBuild()` is always true), validates MZ header + ≥1 MiB (unsigned, no cert). **Portable copy** → `SelfUpdateViaBatch`: detached cmd batch (ping-sleep, no `timeout` — fails console-less) waits for exit → `move /Y` over running exe → relaunch. **Installed copy** → runs the downloaded installer with `-install` (reuses registry-remembered install dir, replaces files in place). E2E test: `tests/ad-hoc-update.ts` (registered in `before-release.ts`).
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

## Semantic navigation (clangd / Serena)

clangd needs a compilation database to parse the tree with the real flags. MSBuild/premake
cannot emit one, so `cmd/gen-compile-commands.ts` writes it from the shared flag list.

```powershell
cd sumatrapdf-src
bun cmd/gen-compile-commands.ts          # -> compile_commands.json (gitignored, ~282 TUs)
clangd --check=src/wingui/WebView.cpp    # parse ONE TU, print diagnostics + check results
```

- Flags live in `cmd/clang-flags.ts`, shared with `cmd/clang-tidy.ts` (same include dirs /
  defines, so lint and navigation agree). The WebView2 include dir is resolved at runtime
  from `packages/`; `-std=c++23` matches `<LanguageStandard>stdcpplatest</LanguageStandard>`.
- Regenerate after adding/removing a `.cpp` under `src/`, or after a WebView2 NuGet bump.
- The DB MUST sit in `sumatrapdf-src/`: clangd walks up from the file and the
  `compile_flags.txt` in that directory shadows a DB placed at the repo root.
  `compile_flags.txt` is only a stale-by-design fallback (hardcoded WebView2 version) —
  run the generator instead. `.clangd`'s `-std` must match the projects: `CompileFlags.Add`
  is applied AFTER the database flags, so an older standard there silently downgrades parsing.
- Not covered on purpose: `src/mac/**` (Objective-C++), `src/testcode`, `src/tools`,
  `src/regress`.
- Serena MCP (clangd backend) is wired in `%APPDATA%\..\.config\opencode\opencode.json`
  (`mcp.servers.serena`, `--project` = this `sumatrapdf-src` dir) — semantic
  symbol/reference lookup from the agent, replacing syntactic greps for those questions.
  Server logs: `C:\Users\Testa\.serena\logs\<date>\mcp_*.txt`.
  Serena's bundled clangd 19 runs with `--background-index`: the FIRST symbol query starts a
  full project index and can exceed Serena's 240 s tool timeout while it works (one-time;
  shards persist in `%LOCALAPPDATA%\clangd\index`, so later queries are instant). Give it a
  few minutes, then retry. Wait until the shard count in that folder stops growing.

## Static analysis / CI workflows (CodeQL, MSVC /analyze, clang-tidy)

GitHub runs ONLY the workflows at the repo root `.github/workflows/`. The files under
`sumatrapdf-src/.github/workflows/` are upstream leftovers and NEVER execute (repo root
is `TumatraPDF/`, build tree is `sumatrapdf-src/`). Live: `release.yml`, `codeql.yml`,
`static-analysis.yml`.

### CodeQL (`codeql.yml`)

- c-cpp, `build-mode: manual` → `bun ./cmd/build-codeql.ts` (Release x64 through
  `vs2022\TumatraPDF.sln`, regenerates `.work/embedded.dat` first).
- Triggers: push/PR to `main` on `sumatrapdf-src/src|cmd|vs2022/**`, weekly cron,
  `workflow_dispatch`. Advisory: SARIF filtered to `sumatrapdf-src/src/**` and uploaded;
  CodeQL steps are `continue-on-error` (fork PRs get a read-only token).

### MSVC /analyze (`cmd/analyze.ts` → `static-analysis.yml`)

```powershell
cd sumatrapdf-src
bun cmd/analyze.ts                          # all fork TUs; deps must be prebuilt (Release x64)
bun cmd/analyze.ts -files src\Toolbar.cpp   # touch + rebuild only the owning project
bun cmd/analyze.ts -with-deps               # full solution cascade (slow, vendored noise)
```

- Builds the PROJECT (`vs2022\TumatraPDF.vcxproj`), never the solution: the solution
  folder drags `PdfFilter` / `PdfPreview` / `test_util` along. `/t:Rebuild` is required
  (Build alone finds every TU up to date and analyzes nothing);
  `BuildProjectReferences=false` keeps the vendored projects out of the run, so a prior
  normal Release build must exist.
- Output: `out/analyze/msvc-analyze.log` + `out/analyze/warnings-summary.txt` (per-rule
  counts, fork sources separated from vendored). Diagnostics are un-prefixed from the
  MSBuild node id (`10>`) and de-duplicated (MSBuild repeats each one in its summary).
  `VSLANG=1033` forces English diagnostics for parsing.
- **TRAP**: `SelectedFiles` is a GLOBAL MSBuild property. Passing it on the command line
  filters every other project's items as well — `ResourceCompile` goes empty and the RC
  task dies with `MSB4044`. Worse, the filtered build then rewrote `base.lib` with a
  single member (`BasePch.obj`), so the next link failed with **781 `LNK2001`/`LNK1120`
  unresolved externals** for `src/base` symbols. Verify with
  `dumpbin /archivemembers out/rel64/obj-s/base.lib` (a healthy lib lists ~49 objects) and
  repair with `msbuild vs2022\base.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64`.
  `cmd/analyze.ts` therefore TOUCHES the listed sources to defeat the incremental check
  instead, and prints this hint when a link fails.
- CI: weekly cron + `workflow_dispatch`; plain Release build first, then the sweep;
  artifact `msvc-analyze` (log + summary, 30 days). Findings never fail the job; only a
  real MSBuild failure does.

### App-level fuzzing (ASan) — `cmd/fuzz.ts`

Feeds byte-mutated inputs to `out/dbg64_asan/TumatraPDF.exe` through its headless entries
(`-render <file> 1`, `-extract-text <file> 1`, `-dump-exif <file>` — all run before window
creation and exit) and records crashes with the offending input in `tests/tmp/fuzz-crashes/`.
Requires the ASan exe (`bun cmd/build-asan.ts`). Deterministic: rerun with the same
`-seed-offset` to reproduce. `-stress-test`/`-engine-dump` are NOT headless (GUI) — don't
use them as fuzz entries.

    bun cmd/fuzz.ts -n 300 -seconds 600          # render mode over ext/ + tests/ seeds
    bun cmd/fuzz.ts -mode exif -n 200            # EXIF parser only

Known trap: the four `x64_asan` configs link with `/DEPENDENTLOADFLAG` — it must be
`0xA00` (SYSTEM32 + APPLICATION_DIR), not `0x800`: with `0x800` the loader cannot find
`clang_rt.asan_dynamic-x86_64.dll` in the app dir and the exe exits 53 silently before
main (fixed s36-u, LOG).

## Known issues

- **LNK1201 (PDB lock)**: `libsumatrapdf.pdb` locked by VS Code/OneDrive. Rename PDB, rebuild.
- **`.vscode` F5 broken**: `launch.json`/`tasks.json` still target `SumatraPDF-dll.exe` / `SumatraPDF.sln` — no F5 in VS Code. **ASan builds work again** (s36-r): `bun cmd/build-asan.ts [-rel] [-clean]` → `out/dbg64_asan|rel64_asan/TumatraPDF.exe`, and `bun cmd/run-unit-tests.ts -asan` runs the base unit tests under ASan.
- **build_log.txt**: `Module not found` error = wrong cwd (repo root). Use `cd sumatrapdf-src`.
- **MSB1009 (sln not found)**: `build.ts:37` with `..\vs2022` is a regression. Must read `vs2022\TumatraPDF.sln`. Revert on sight.

## Docs rule

Informativos stay English + caveman (AGENTS.md §13).
