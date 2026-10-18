# 0. Non-negotiables

- NEVER use PowerShell `Set-Content` / `WriteAllText` for file content — corrupts UTF-8 accented characters. Use Edit tool.
- Byte-safe text surgery if ever forced through shell: a `bun` script (`readFileSync`/`writeFileSync` in utf8) or the Edit tool — never PS text cmdlets. After ANY scripted write, verify with `git diff` that only the intended lines changed.
- Mojibake scan after bulk edits: `git ls-files` + check for U+FFFD (0xEF 0xBF 0xBD) — historical damage lived in LOG.md/Version-history.md (all fixed s29b; remaining "?" chars like "saves to gGlobalPrefs" are pre-existing ANSI-era losses, not recoverable from git).
- NEVER use `2>nul` (creates undeletable `nul` file on NTFS). Use `2>/dev/null`.
- NEVER use `&&` in shell — PowerShell 5.1 doesn't support it. Use `;` or `cmd1; if ($?) { cmd2 }`.
- NEVER hardcode command IDs — use `cmdId("CmdName")` from `tests/util.ts`. IDs shift when commands are added/removed.
- NEVER commit automatically — wait for explicit command.
- NEVER include `base/Base.h` in `.h` files — causes 300+ redefinition errors.
- NEVER put prose comments in `.h` files — put them on the definition in `.cpp`.
- Keep the opencode `TODO` lateral panel updated while working for the user to follow the process.
- ALWAYS use `Tree-Sitter` tools for mapping.
- ALWAYS use `#ifndef`/`#define` guards, NOT `#pragma once`.
- ALWAYS run `clang-format` on `.cpp`/`.c`/`.h` files after edits (before build).
- ALWAYS pass `-for-testing` when launching for ad-hoc testing.

# 0b. USE tree-sitter for exploration & debugging (MANDATORY)

The **tree-sitter MCP** tools (`tree-sitter_register_project_tool`, `find_usage`,
`find_text`, `run_query`, `get_symbols`, `analyze_project`) are the project's
primary way to explore code, trace symbol references, and debug. **ALWAYS reach
for tree-sitter BEFORE grep/read-walking.** It indexes the whole tree once
(`name "tumatrapdf"`, root = repo) and answers symbol/call-site questions in one
call, saving huge context and avoiding manual greps.

Rules:
- Register the project once per session: `tree-sitter_register_project_tool(name="tumatrapdf", path="<repo>")`. If `find_usage`/`find_text` return empty or stale, `tree-sitter_clear_cache` + re-`register_project_tool` (scans may be cached from a prior session).
- To map who calls a function / what a symbol reaches: `tree-sitter_find_usage(symbol, language="cpp")`.
- To find a code pattern with context: `tree-sitter_find_text(pattern, context_lines=N)`.
- **Trace call-sites of shared/ambiguous types (page numbers, coordinates, engine handles) with tree-sitter, not manual greps** — this repo has pervasive dual namespaces (virtual vs physical pageNo, screen vs page coords) that silently break features when mixed (see Two Columns v2).
- The user has repeatedly had to insist that agents use tree-sitter — **do not skip it**. If a task involves "map", "trace", "where is X called", "audit call-sites", use tree-sitter FIRST.
- Project must be registered before use; the server forgets the project between sessions (empty `list_projects_tool`).

Recommended flow: register → `analyze_project` → `get_symbols` / `get_ast` / `get_dependencies` → `find_usage` → `run_query` (advanced). Semantic questions tree-sitter cannot answer (which overload/namespace a name resolves to, every call-site of a shared type across files, compiler diagnostics for one TU): use the **Serena MCP** (clangd backend) — see §6b.
Warning: `tree-sitter_find_text` AVOID repo-wide on this large C++ tree with vendored subfolders (`mupdf/`, `ext/`) — risk MCP timeout (-32001). Scope with `file_pattern` or use targeted grep.

# 1. Overview

TumatraPDF is a fork of SumatraPDF (upstream baseline ~912ecf2, Aug 2026) — Windows C++ PDF reader, Win32 API. All fork features = direct patches in `sumatrapdf-src/src/` (`src/features/` aspirational, never existed). Own string/container types in `sumatrapdf-src/src/base/` (no STL).

## Informativo hierarchy (single source of truth)

| File | Role | Audience |
|---|---|---|
| `AGENTS.md` (root) | ONLY governing doc — rules for agents editing code. Wins ALL conflicts | agents |
| `README.md` (root) | ONLY showcase — TumatraPDF features vs forked SumatraPDF | users/kjk |
| `BUILD.md` / `MERGE.md` | working docs: build, upstream sync | agents |
| `LOG.md` / `TODO.md` | changelog (append-only, never rewrite) / active backlog | agents |
| `CHANGELOG.md` (root) | per-version public changelog (v1.0.x releases, Keep a Changelog style, release checklist) | agents/users |
| `LICENSE` / `SECURITY.md` / `CONTRIBUTING.md` (root) | GitHub community-health files: GPL-3.0 (verbatim text), vuln reporting, contributor ground rules | users |
| `update.txt` (root) | update metadata served to running apps (`Latest:` + download URLs) — bumped by the release workflow, do NOT hand-edit except the URL keys | agents |
| `Design_Guidelines.md` (root) | conventions + known traps distilled from sessions. **§6 / §6.1 = UI rules for Win32 dialogs, panels, toolbar, overlays — read before ANY visible UI change; §6.1 ends with the mandatory screenshot verification loop (dark + light)** | agents |
| `FLOW/*.dot` | process maps only (never code architecture — §0b) | agents |
| `TEMP/` | deprecated informativos (gitignored) | — |
| `sumatrapdf-src/agents.md`, `sumatrapdf-src/docs/**` | upstream SumatraPDF reference, verbatim | upstream |

Any other `.md` in subfolders = deprecated (→ `TEMP/`) or upstream reference. Root `AGENTS.md` overrides anything found in subfolders.

Use `tree-sitter` tools (§0b) to understand project structure.

# 2. Setup / Build / Test / Debug

Repo root has NO `cmd/` — all build/test commands run with workdir = `sumatrapdf-src/`. Details: BUILD.md.

```
Build:              bun cmd/build.ts                      → out/dbg64/TumatraPDF.exe (auto-deploys to ../Compiled/)
Test build:         bun cmd/build-test.ts
Unit tests:         bun cmd/run-unit-tests.ts -dbg        (or -rel / -asan)
Debug:              windbgx -Q -o -g out/dbg64/TumatraPDF.exe
```

VS command-line tools (`cl.exe`, `msbuild.exe`) must be in PATH. Fallback: `MSBuild.exe sumatrapdf-src\vs2022\TumatraPDF.sln /t:TumatraPDF /p:Configuration=Debug /p:Platform=x64 /m`.

# 3. Project Layout

```
TumatraPDF/                   # repo root
├── sumatrapdf-src/           # forked upstream tree
│   ├── src/                  # all source code (fork patches live here)
│   ├── src/base/             # custom string/helper/container types (Str, Vec, StrVec)
│   ├── ext/, mupdf/          # vendored deps (no clang-format)
│   ├── cmd/                  # bun build/codegen scripts
│   ├── tests/                # bun tests
│   └── docs/md/              # upstream documentation
├── Compiled/                 # deployed TumatraPDF.exe (gitignored)
├── FLOW/                     # process .dot maps
├── TEMP/                     # deprecated informativos (gitignored)
├── .tmp_compare/             # upstream baseline for merge diffs (gitignored)
└── *.md                      # root informativos (hierarchy §1)
```

# 4. C/C++ Code Conventions

## Include order in `.cpp` files (CRITICAL — wrong order = 300+ cascade errors)

1. `#include "base/Base.h"` — ALWAYS first (PCH)
2. Other `base/` headers (Crypto.h, File.h, etc.)
3. `wingui/` headers (UIModels.h, Layout.h, WinGui.h, etc.)
4. Project headers in dependency order: `Settings.h` → `DisplayMode.h` → `DocController.h` → `DocProperties.h` → `TreeModel.h` → `EngineBase.h` → `DisplayModel.h` → `mupdf/pdf.h` → `EngineMupdf.h`
5. Other project headers (SumatraPDF.h, MainWindow.h, WindowTab.h, etc.)
6. **The file's own header — ALWAYS LAST**

## Headers are NOT self-sufficient

- Headers rely on `.cpp` providing types via PCH
- `Commands.h` has NO include guard (generated raw `enum {}`) — NEVER include from another header

## String conventions

- `Str`/`WStr` = `{char*/wchar_t*, int len}` — not NUL-terminated by default
- String literals: use `StrL("...")`/`WStrL(L"...")` (compile-time length), not `Str("...")` (runtime strlen)
- `fmt("format", ...)` — type-safe formatter, returns `TempStr`. Pass `Str` objects for `%s`, not `.s`
- `logf`/`logfa` — format via `fmt()` then route through `log()`/`loga()`
- For C/Win32 APIs: `CStrTemp(Str)` → `char*`, `CWStrTemp(WStr)` → `WCHAR*` (NUL-terminated copy in temp arena)
- Prefer `len(x)` over `x.len` (uniform, works for all containers)

## Comments

- Prose function comments go in `.cpp` only, not `.h`
- Struct/class/enum/macro/constant comments stay in `.h`
- Example: declaration in `Foo.h` (no comment), definition in `Foo.cpp` (comment above)

# 5. Adding New Things

## New command

1. Add to `cmd/gen-commands.ts` (at end, before `CmdNone`)
2. `bun cmd/gen-code.ts` → regenerates `src/Commands.h` and `src/Commands.cpp`
3. Document in `docs/md/Commands.md`
4. Add to **New commands** list in next version section of `docs/md/Version-history.md`

## New cmd-line flag

1. Add to `cmd/gen-flags.ts`
2. `bun cmd/gen-code.ts` → regenerates `src/Flags.cpp`
3. Implement handling in `Flags.cpp`
4. Document in `docs/md/Command-line-arguments.md`
5. Add to **New command-line arguments** list in `docs/md/Version-history.md`

## New advanced setting

1. Add definition in `cmd/gen-settings.ts`
2. `bun cmd/gen-code.ts` → regenerates `src/Settings.h` and `src/Settings.cpp`

## Generated code warnings

- DocProp name maps (`// @gen-start docprop-*`), `gVirtKeysNum` (`// @gen-start virt-keys-num`): **don't hand-edit**. Edit data in `cmd/gen-code.ts`, run `bun cmd/gen-code.ts`.
- Embedded data: `.work/embedded.dat` packed by `bun cmd/pack-embedded.ts`. After editing manual sources, run `bun cmd/gen-docs.ts` before testing.

# 6. Testing

## Which tests to run

- Named issue fix → `bun tests/issue-<number>.ts`
- Change to tested code → grep `tests/` for related test, run that
- base/test_util work → `bun cmd/run-unit-tests.ts -dbg`
- Nothing covers it → manual check (`-for-testing`, screenshot). Don't run full suites.
- NEVER edit `tests/all.ts` to skip tests.

## Test file structure

- Script: `tests/issue-<number>.ts` (or `tests/issue-<number>-data/` for dirs)
- Export `async function testit(): Promise<void>` — THROWS on failure
- Standalone runner at end: `if (import.meta.main) { await runStandalone(testit); }`
- Shared helpers in `tests/util.ts` (`EXE`, `buildApp`, `runStandalone`, `cmdId`)
- Register in `tests/all.ts` (import `testit`, add to `tests` array)

## Ad-hoc tests

- Named `tests/ad-hoc-<name>.ts`, same structure as regular tests
- Do NOT register in `tests/all.ts`; register in `tests/before-release.ts`
- Run directly: `bun tests/ad-hoc-<name>.ts`

## Test guidelines

- Build same way as `cmd/build.ts` does (via `tests/util.ts`)
- Missing external tool (e.g. MiKTeX) → print message, skip, return normally
- Good test fails when fix is reverted
- GUI automation in Bun TypeScript (`tests/winapi.ts`, `tests/win-automation.ts`), not PowerShell
- Use `cmdId("CmdName")`, never hardcode numeric IDs
- Prefer `-dbg-control <named-pipe>` + `cmd/control.ts` over GUI automation
- Scratch files → `tests/tmp/` (gitignored) or `os.tmpdir()`
- Binary fixtures from source → commit source alongside (with regeneration instructions)
- Bug repro files → `C:\Users\kjk\OneDrive\!sumatra\bugs\bug-<no>*`

# 6b. Debug tooling inventory

(moved from README 2026-09-23; research 2026-08-16, paths relative to `sumatrapdf-src/`)

- **Build/run:** `cmd/build.ts` (vs2022\TumatraPDF.sln /t:TumatraPDF → `out/dbg64/TumatraPDF.exe`), `cmd/run.ts` (build + launch)
- **Runtime control:** `-dbg-control <pipe>` + `cmd/control.ts` — Test* commands (Ping/Quit/TestSearch/TestToc/TestToolbarButtons/TestAIChat)
- **UI automation:** `tests/winapi.ts`, `tests/win-automation.ts` — FFI win32, postMessage cross-process, `captureWindowToPng` (works occluded)
- **Flags:** `-for-testing -console -log -stress-test -bench -render -extract-text -set-color-range`
- **Static analysis:** `cmd/clang-tidy.ts`, `cmd/cppcheck.ts`, `cmd/analyze.ts` (MSVC `/analyze` sweep → `out/analyze/{msvc-analyze.log,warnings-summary.txt}`). CI: root `.github/workflows/codeql.yml` (CodeQL c-cpp, push/PR/weekly, advisory) + `static-analysis.yml` (weekly MSVC `/analyze`, artifacts only, never fails). The workflows under `sumatrapdf-src/.github/` are INERT — repo root is TumatraPDF. Details: BUILD.md § Static analysis
- **Semantic navigation:** `bun cmd/gen-compile-commands.ts` → `compile_commands.json` (gitignored; MUST live in `sumatrapdf-src/` — `compile_flags.txt` there shadows a root-level DB), then `clangd --check=<file>` for diagnostics, or the **Serena MCP** (clangd backend, `--project` = `sumatrapdf-src`; tools `find_symbol`, `find_referencing_symbols`, `get_symbols_overview`, `get_diagnostics_for_file`). Use for semantic questions tree-sitter can't answer (which overload/namespace, all call-sites of a shared type); keep tree-sitter for structure/AST. Details: BUILD.md § Semantic navigation
- **Serena on THIS tree (s36-s recipe — verified working)**: unscoped `find_symbol` parses all 282 TUs → 240s MCP timeout. ALWAYS pass `relative_path` (scoped search); warm up with `get_symbols_overview` on one file first. `.serena/project.yml` is tuned: `ls_workspace_folders: ["./src"]` + `ignored_paths` for `mupdf/ext/out/.vs/packages/tests-tmp/.work` (clangd still reads vendored headers via `-I`). Working calls: overview / scoped find_symbol / find_referencing_symbols (cross-file) / diagnostics (`{}` = clean parse).
- **Logging:** `SumatraLog.h/.cpp` auto-log → `%LOCALAPPDATA%\SumatraPDF\<hash>\sumatra-log.txt`, rotation >5MB; tags `[fc]` `[arch]` `[autoscroll]` `[md]` `[webview]`

**Known issues (fix when touched):**
- `cmd/build-asan.ts` references `SumatraPDF.sln`/`SumatraPDF-static` — ASan pipeline broken
- `.vscode/launch.json`/`tasks.json` target `SumatraPDF-dll.exe`/`SumatraPDF.sln` — no F5 debug in VS Code
- `-dbg-control` lacks commands for custom feature state (crop rect, trim, autoscroll ETA, contrast/invert)

**premake TRAP:** `bin/premake5.exe vs2022` regenerates `vs2022/*.vcxproj` from upstream `premake5.lua`, which does NOT know the 10 fork-added source files (Flashcard*.cpp, Commands_{Flashcard,AutoScroll,ArchTools,View,File}.cpp, ToolbarLayout.cpp, MainWindowCreate.cpp) — every run silently strips their ClCompile entries → app release link fails LATER with LNK1120 unresolved externals. `cmd/run-unit-tests.ts` no longer runs premake (fixed 2026-09-23). If premake must run: `rtk git stash push -- sumatrapdf-src/vs2022/` restores the tree afterwards (checkout/restore blocked by rtk safety net). Permanent fix: add fork sources to premake5.lua (MERGE conflict surface — defer).

# 7. Windows Shell Safety

The Bash tool runs under Git Bash (MSYS2), **not** cmd.exe:

- NEVER: `2>nul` → `2>/dev/null`
- NEVER: `rmdir /s /q` → `rm -rf`
- NEVER: `del` → `rm`
- NEVER: `dir` → `ls`
- NEVER: `&&` → `;` or `cmd1; if ($?) { cmd2 }`
- Windows-native commands: wrap in `cmd /c "..."`
- Always use Unix-style commands and paths

# 8. File Editing & Formatting

- C/C++ files under `src/`: run `clang-format` after edits, before build
- Do NOT clang-format vendored code (`mupdf/`, `ext/`)
- TS/JS/JSON/MD files: `bunx prettier --write <files>`
  - Settings: `.prettierrc.json` (printWidth 120, endOfLine lf)
  - Format only files you touched (most `cmd/`/`tests/` predate config)

# 9. Commit & PR Conventions

- Issue fix commits: first line ends with `(fixes #<issue-no>)`
- AI-assisted commits: append `prompt: <concise user prompt>` at very end
- Version history (`docs/md/Version-history.md`):
  - Features/behavior changes in prose (mention menus, shortcuts, user-visible effects)
  - NO bug fixes (commit message + GitHub issue records them)
  - Consolidated **New commands** and **New command-line arguments** lists at end of version section

# 10. Safety & Permissions

- Auto-build on edit: YES (use `cmd/build-test.ts` or `cmd/build.ts`)
- Auto-test after fix: YES (run related test only, not full suite)
- Auto-commit: NEVER
- Auto-push: NEVER
- Edit vendored code (`ext/`, `mupdf/`): minimal, match local style, no clang-format
- Edit generated code (`Commands.h`, `Settings.h`, `Flags.cpp`): NO — edit gen scripts and re-run
- Edit `.work/` artifacts: only via build scripts
- Git checkpoint before major changes: YES

# 11. LOG.md Conventions

LOG.md uses [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format. Actual entry style in this repo: `## YYYY-MM-DD (sN) — Short Title` with `### Fixed/Added/Changed/Verified/Notes` (no `[Unreleased]` section — history only, newest first).

- Header: `## YYYY-MM-DD (sN) — Short Title`
- Categories: Fixed, Added, Changed, Verified, Removed, Notes
- Concise bullets, describe WHY, reference commits when relevant
- Update at end of sessions, before checkpoints, when starting/stopping feature branches
- Language: English + caveman (§13) — always

# 12. Project Learnings

_Living document — append patterns/gotchas discovered during development._

- **2026-09-14**: `#include "DisplayModel.h"` alone causes 49 cascade errors. Use `#include "DocController.h"` + `win->ctrl->CurrentPageNo()` when only pageNo is needed.
- **2026-09-14**: Use `str::Builder sb;` (not `StrBuilder`), `sb.AppendChar(';')` member function, `sb.TakeStr()` returns Str.
- **2026-09-14**: Nested structs need full qualification: `MainWindow::ArchToolsState::ArchScaleState`.
- **2026-09-12**: `Commands_Flashcard.cpp` requires exact include order: Settings.h → DocController.h → DocProperties.h → TreeModel.h → EngineBase.h → DisplayModel.h → EngineMupdf.h. Missing TreeModel.h before EngineBase.h causes 30+ cascade errors.
- **2026-09-12**: `AddUniquePageNo` is static in SumatraPDF.cpp — must reimplement locally if needed.
- **2026-10-09**: `Commands_ArchTools.cpp` needs the same chain as Commands_Flashcard.cpp (DocController → DocProperties → TreeModel → EngineBase → DisplayModel) to touch DisplayModel members (`viewportCropV2Enabled` etc.) — C2027 otherwise.
- **2026-10-09**: `tests/winapi.ts` `getClientRect` returns `{left,top,right,bottom}` — NOT dx/dy; `.dx` → NaN → `packCoords` masks to 0 → synthetic clicks land at (0,0).
- **2026-10-16 (D8 audit, recurring bug family)**: **coordinate-space mismatch** is a recurring bug class here. Convention — which space each API speaks:
  - `DisplayModel::CvtToScreen/CvtFromScreen`: PAGE coords ↔ CANVAS-CLIENT coords (includes scroll+zoom). Heaviest users: Canvas.cpp (~43 calls), DisplayModel.cpp (~26), Selection, Commands_Edit, ReadAloudHighlight, SearchAndDDE.
  - `VirtWnd` tree: `ev.pt` (after `BubbleMouse`, VirtWnd.cpp:595) and `OnSetCursor(Point)`/`GetTooltipTemp(Point)` args are **LOCAL to the target's bounds**; `ptWindow` and `ctx.bounds`/`VirtWnd::bounds` are **ROOT-relative**. Rects computed in `Paint` from `ctx.bounds` are root-space → hit-test with `pt + bounds.{x,y}` (the canonical local→root bridge; see HomeStatsWnd OnMouseUp/Move/SetCursor). Trim precedent: same class (page/viewport Y mixing), R4 unified 3 drag-line math copies → `TrimComputeLineInfoForPage`.
  - E2E posted coords may arrive DPI-virtualized ×1.25 (non-deterministic) — verify with page-top-border invariants, not assumed raw pixels.
  - RULE: before any hit-test, name the space of BOTH operands out loud (local / root / client / page). Mixed = bug.
- **2026-10-18 (flashcard multi-page grouping)**: a PDF annotation cannot live on two pages, so ANY "one logical thing spanning pages" must be stored as N annotations + an in-file grouping key. Convention used for cross-page clozes: a shared token in the annotation AUTHOR (`TumatraPDF-Flashcard#%08X`, via `pdf_set_annot_author` = MuPDF `/T`, which survives save; other keys are not guaranteed to be written back). Do NOT put such a token in the annotation CONTENT: `RebuildCommentsFromAnnotationsInner` (EngineMupdf.cpp) turns any annot with non-empty contents into a page tooltip/comment. **TRAP**: identity checks on our annotations must use `str::StartsWith(Author(a), StrL("TumatraPDF-Flashcard"))`, never `str::Eq` — exact equality silently drops tagged annots (hit Canvas.cpp ctrl+click→List panel and the EditAnnotations filter that hides flashcard annots). Grouped cards: `Flashcard::extraPages` (secondary pages); primary page fields (`pageNo/bounds/rects`) stay the lowest page so nav/key/sort keep working.

# 13. Informativos: English + Caveman (MANDATORY)

All project informativos (`README.md`, `LOG.md`, `TODO.md`, `BUILD.md`, `MERGE.md`, `FLOW/*.dot`) are written in **English, caveman-compressed — always**. No Portuguese in new content; translate PT on sight.

- Caveman: drop articles/filler/hedging, fragments OK. Preserve EXACTLY: code blocks, inline code, paths, commands, IDs, numbers, table layout, headings.
- TODO unfinished sections (`[ ]`, pending, awaiting-validation, future plans): translate only, NEVER compress. Done sections (`[x]`/`✅`/COMPLETE/resolved): translate + compress.
- Keep `FASE` (not PHASE) as phase identifier — matches LOG.md history.
- `FLOW/*.dot` = process maps only. Never hand-maintain code-architecture `.dot` — use tree-sitter (§0b).
- Deprecated informativos live in `TEMP/` (gitignored): `FLASHCARD_IMPLEMENTATION_PLAN.md`, `docs/TumatraPDF2/PLAN.md` (TumatraPDF2 discontinued), `sumatrapdf-src/LOG.md`, `sumatrapdf-src/readme.md`, `diff_sumatra.txt` — reference only, never extend.
