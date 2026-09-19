# 0. Non-negotiables

- NEVER use PowerShell `Set-Content` / `WriteAllText` for file content — corrupts UTF-8 accented characters. Use Edit tool.
- NEVER use `2>nul` (creates undeletable `nul` file on NTFS). Use `2>/dev/null`.
- NEVER use `&&` in shell — PowerShell 5.1 doesn't support it. Use `;` or `cmd1; if ($?) { cmd2 }`.
- NEVER hardcode command IDs — use `cmdId("CmdName")` from `tests/util.ts`. IDs shift when commands are added/removed.
- NEVER commit automatically — wait for explicit command.
- NEVER include `base/Base.h` in `.h` files — causes 300+ redefinition errors.
- NEVER put prose comments in `.h` files — put them on the definition in `.cpp`.
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

# 1. Overview

TumatraPDF is a fork of SumatraPDF — a Windows C++ PDF reader using Win32 API. Own string/container types in `src/base/` (no STL). Early macOS app in `src/mac/` (not in scope). External deps in `ext/` and `mupdf/`.

Use `tree-sitter` tools to understand project structure.

# 2. Setup / Build / Test / Debug

```
Build:              bun ./cmd/build.ts                    → ./out/dbg64/SumatraPDF.exe
Test build:         bun cmd/build-test.ts
Unit tests:         bun cmd/run-unit-tests.ts -dbg        (or -rel / -asan)
Debug:              windbgx -Q -o -g ./out/dbg64/SumatraPDF.exe
```

VS command-line tools (`cl.exe`, `msbuild.exe`) must be in PATH.

# 3. Project Layout

- `src/` — all source code
- `src/base/` — custom string/helper/container types (Str, Vec, StrVec, etc.)
- `ext/`, `mupdf/` — external dependencies (don't clang-format)
- `cmd/` — TypeScript build/codegen scripts (run with `bun`)
- `tests/` — TypeScript tests
- `.work/` — build artifacts, embedded data sources
- `docs/md/` — documentation

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

LOG.md uses [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) format.

- Header: `## [YYYY-MM-DD] — Short Title`
- Categories: Added, Changed, Fixed, Removed, Deprecated, Decisions, Notes
- `[Unreleased]` at top for WIP
- Concise bullets, describe WHY, reference commits when relevant
- Update at end of sessions, before checkpoints, when starting/stopping feature branches

# 12. Project Learnings

_Living document — append patterns/gotchas discovered during development._

- **2026-09-14**: `#include "DisplayModel.h"` alone causes 49 cascade errors. Use `#include "DocController.h"` + `win->ctrl->CurrentPageNo()` when only pageNo is needed.
- **2026-09-14**: Use `str::Builder sb;` (not `StrBuilder`), `sb.AppendChar(';')` member function, `sb.TakeStr()` returns Str.
- **2026-09-14**: Nested structs need full qualification: `MainWindow::ArchToolsState::ArchScaleState`.
- **2026-09-12**: `Commands_Flashcard.cpp` requires exact include order: Settings.h → DocController.h → DocProperties.h → TreeModel.h → EngineBase.h → DisplayModel.h → EngineMupdf.h. Missing TreeModel.h before EngineBase.h causes 30+ cascade errors.
- **2026-09-12**: `AddUniquePageNo` is static in SumatraPDF.cpp — must reimplement locally if needed.
