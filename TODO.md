# TODO
1. Open work ONLY.
2. Completed features/fixes live in [LOG.md](LOG.md) (per-session) + [CHANGELOG.md](CHANGELOG.md) (per-release).
3. User manual-test items = items only the user can validate.

## Housekeeping (quick)

- [ ] Old stray local branches (`master`, `feature/*`) vs git-flow conventions — prune after confirming nothing unmerged.
- [x] BUG-3/s32-B menu gating live-checked (s36-l, image-XObject fixture + vision): image items appear ONLY over an image — PASS. Remaining sub-items live under **BUG-3** (Save-as path + non-image items).

## Dev tooling / analysis roadmap (s36-q — from 2026-10-18 C/C++ tech survey)

Status: `DONE` = shipped + verified, `WIP` = in progress, `TODO` = pending. Effort: S/M/L. Order = recommended sequence, not a contract.

| Fase | ID | Task | Effort | Status | Expected improvement |
| --- | --- | --- | --- | --- | --- |
| A | A1 | Compilation database: `cmd/gen-compile-commands.ts` + shared flag list `cmd/clang-flags.ts` → `compile_commands.json`, 282 TUs. Must live in `sumatrapdf-src/` (`compile_flags.txt` there shadows a root-level DB) | S | DONE s36-q | clangd parses every TU with real build flags — `src/wingui/WebView.cpp` went from `WebView2EnvironmentOptions.h not found` + `fatal_too_many_errors` to 0 real diagnostics; unblocks A2/B3/B4 |
| A | A2 | Serena MCP (clangd LSP backend) in global `opencode.json`; vendored trees excluded via `.serena/project.local.yml` | S | DONE s36-q | Agent gets semantic queries (`find_symbol`, `find_referencing_symbols`, `get_diagnostics_for_file`, 28 tools) → real name resolution instead of greps for the recurring dual-namespace bug family (virtual vs physical pageNo, screen vs page coords) |
| A | A3 | `cmd/clang-tidy.ts` reuses the shared flag list; std `c++20` → `c++23` (matches `stdcpplatest`) | S | DONE s36-q | Lint and navigation agree; no false findings from a wrong language standard |
| A | A4 | One-time clangd index build (shards in `%LOCALAPPDATA%\clangd\index`) | — | WIP | Once finished, project-wide symbol/reference queries answer in seconds; today they can hit Serena's 240 s timeout (per-file queries already instant) |
| B | B1 | CodeQL on `push` + `pull_request` (paths `sumatrapdf-src/src/**`, `sumatrapdf-src/cmd/**`, `.github/workflows/**`) beside the weekly cron; advisory first (upload sarif, no fail) | S | DONE s36-r | Findings land on the change that caused them (today: up to 7 days late) + fork patches get a quality gate |
| B | B2 | MSVC `/analyze` (+ `/sdl`), Release x64: local script + CI job, documented warning baseline + suppression list (baseline: `out/analyze/warnings-summary.txt` — 120 fork warnings, 17 rules, 2026-10-18) | M | DONE s36-r | Catches MSVC/Win32-specific classes (handle leaks, buffer overruns, null-deref paths) that clang-tidy and CodeQL miss |
| B | B3 | clang-tidy sweep over all 282 TUs (only possible now that the DB exists) → triage fix / suppress / backlog | S | DONE s36-r | Replaces ad-hoc per-file lint with one prioritized defect backlog (`bugprone-*`, `performance-*`). Driver hardened (`-sweep` continues past failures + per-check aggregation) and now prefers the **standalone LLVM clang-tidy 23.1.1** — the MSVC-bundled `VC\Tools\Llvm` 19.1.5 crashes `0xC0000005` (exit 5) on every TU (override with `CLANG_TIDY` env var); full 275-TU sweep done at `-jobs 6` - 29,922 diagnostics / 95 checks / 8 errors in `out/lint/clang-tidy-summary.txt` (re-derivable with `-parse-only`); triage split out into B6 |
| B | B4 | Property/invariant tests for coordinate + pageNo namespaces: `CvtToScreen`/`CvtFromScreen` round-trip, VirtWnd local↔root bridge (`pt + bounds.{x,y}`) | M | TODO | Kills the most repeated bug class in this repo at test level (mixed virtual/physical pageNo, screen vs page coords, hit-test spaces) |
| B | B5 | Triage + fix the 120 MSVC `/analyze` fork findings in the B2 baseline (`out/analyze/warnings-summary.txt`, 17 rules): C26495 member-init (57), C28159 `GetTickCount`→`GetTickCount64` (21), C6387/C6385/C6386 buffer sizes (13), C6011/C6001/C28182/C28183 null paths (16), C6031 unchecked returns (4), C6287 dead code (3), C6262 stack (2), C26110/C26454/C33005/C26813/C26819 (8) → fix or suppress with documented rationale | M | DONE s36-t | Turns the advisory baseline into real fixes; kills real null-deref / uninitialized-member / buffer-size classes on the fork's own TUs. s36-t: 120→17 / 0 errors (wave 1: C28159 GetTickCount64+u64 sweep 21→0, C26495 in-class member init on 22 structs 57→0; wave 2: C6287 redundancy, C26813 flags-enum bit test + operator&, C26454 literal, C6031 consumed returns, C6387 hwnd guards, C6011/C28183 Canvas guards, C33005 VARIANT zero-init, C6385 encoder size check, C26819 [[fallthrough]] — see LOG s36-t). Final 17 = 5 in user-modified files + 12 documented FP/accepted verdicts (LOG Notes) |
| B | B6 | Triage the clang-tidy baseline `out/lint/clang-tidy-summary.txt` (29,922 diagnostics / 95 checks over 267 TUs). Order: (1) the 42 `clang-analyzer-*` path findings - core.CallAndMessage 9, security.ArrayBound 8, core.NullDereference 7, deadcode.DeadStores 6, cplusplus.NewDeleteLeaks 4, unix.Malloc 3, core.NonNullParamChecker 2, core.StackAddressEscape 1, core.NullPointerArithm 1, unix.cstring.NullArg 1; (2) real `bugprone-*` classes - switch-missing-default-case 128, unhandled-code-paths 102, narrowing-conversions 48, undefined-memory-manipulation 24, bitwise-pointer-cast 22, integer-division 19, derived-method-shadowing-base-method 19, incorrect-roundings 17; (3) the 8 `clang-diagnostic-error` TUs (flag/std gap vs MSVC). The ~24k stylistic bulk (signed-bitwise, macro-to-enum, named-parameter, designated-initializers, redundant-parentheses, concise-preprocessor-directives, explicit-constructor) is a separate policy decision, not a defect list | M | WIP s36-r | Converts the sweep from a curiosity into fixes on fork-only code (`FlashcardSidebar`, `FlashcardToolbar`, `EditAnnotations`, `Canvas`, `PdfDarkModeImageBgBlend`, `ScreenshotCapture`, `Toolbar`, `HomePage`). s36-r wave 1: 11 real fixes (TOC, Commands_Edit/Image, Str.cpp, StrFormatParse, PdfSync, PdfDarkModeImageBgBlend, Canvas ×3, SumatraStartup — see LOG) + FP verdicts for ~19 findings; remaining: `EngineDump.cpp:365`, `base/Base.h:743` (2-args-called-with-1), real `bugprone-*` classes, the 8 error TUs (excluded: platform-suffix files); wave 2a: +31 fixed (OverlayScrollbar float centers, PngOptimizer row addressing/size_t, Installer/SumatraLog/UpdateCheck i64 consts, HomePage/InstallerCommon); remaining: switch-missing-default 128, unhandled-code-paths 102, narrowing 48, roundings/unchecked-conv deferred |
| C | C1 | Repair `cmd/build-asan.ts` (was unrunnable: VS-2026 detection, premake trap, wrong solution/target; repaired s36-r) → working `/fsanitize=address` build + `cmd/run-unit-tests.ts -asan` (verified s36-r: `out/dbg64_asan/TumatraPDF.exe` + `clang_rt.asan_dynamic-x86_64.dll`, 0 errors, 277 s; `EnableASAN=true` on x64_asan; ASan unit tests exit 0) | M | DONE s36-r | Memory-safety defects (use-after-free, out-of-bounds) caught by unit tests; today zero ASan coverage on Windows |
| C | C2 | Time Travel Debugging recipe (`tttracer.exe` present): record a hard repro, replay in WinDbgX, document in BUILD.md | M | DONE s36-ab | Deterministic replay of timing/interaction bugs (toolbar drag-collapse class) instead of "cannot reproduce". s36-ab: pipeline verified end-to-end (record via `TTD.exe -launch` elevated + `echo Y|` EULA; replay via `cdbX64.exe -z trace.run -c "!tt ..."`, headless) - recipe + traps in BUILD.md; traces land as `<App>NN.run` next to the recorder cwd, ~20 MB/s of GUI activity |
| C | C3 | Application Verifier over `TumatraPDF.exe` + unit tests (handles/heaps/locks), triage findings | M | DONE s36-ac | Runtime detection of handle/heap/lock misuse, esp. Win32 GDI/HWND lifetimes in the new panels. s36-ac: Heaps+Handles+Locks - test_util 102 783 tests PASS exit 0; TumatraPDF render/extract-text runs exit 0, no verifier stops (logs in AppVerifierLogs; verifier disabled after; batches in %TEMP%\c3-appverif\) |
| C | C4 | Install WinDbg (`winget install Microsoft.WinDbg`) | S | DONE s36-y | Symbolicated crash-dump analysis + TTD replay (C2 depends on it); WinDbg 1.2610.1001.0 installed via winget (verified: "Instalado com êxito") |
| D | D1 | libFuzzer/ASan harnesses on entry points we own: mupdf xref/object parsing, `-dump-exif`, cmark-gfm, image decoders through our wrappers (jxl/heic/dav1d) | L | WIP s36-u | Malformed PDF/EPUB/image crashes found by machine before users hit them — highest bug yield for a PDF reader (and the same class as most past CVEs). s36-u slice 1: `cmd/fuzz.ts` app-level ASan fuzzer (headless `-render`/`-extract-text`/`-dump-exif` entries, byte mutator, 23 seeds, 300 execs / 0 crashes baseline) + fixed the ASan app exe startup (`DEPENDENTLOADFLAG` 0x800 to 0xA00, LOG s36-u); s36-x slice 2: native libFuzzer harness for base parsers (`cmd/build-fuzz-harness.ts` -> `out/fuzz/fuzz-parsers.exe`, 13.6k execs/s, LOG s36-x); s36-ab: mupdf harness LINKED + re-verified on the F1/F4/F5 dependency batch (886-975 execs/s, 0 crashes, LOG s36-ab); remaining: exif seeds, cmark-gfm/image-decoder harnesses (same recipe as s36-ab) |
| D | D2 | Seed corpus from `ext/*.pdf` + `tests/` regression files, bounded local runs first | S | DONE s36-u | Fuzzer reaches deep parser paths in minutes instead of days of blind mutation (corpus auto-gathered recursively by `cmd/fuzz.ts` from `ext/` + `tests/`; bounded runs via `-n`/`-seconds`) |
| D | D3 | Nightly time-boxed fuzz job (`daily.yml` / `linux-daily.yml`) + crash artifact upload | S | DONE s36-ab | Continuous parser regression coverage; every crash lands with its repro input attached. s36-ab: `.github/workflows/fuzz.yml` - nightly native-harness campaigns (base parsers 10 min + mupdf 15 min, advisory, crash artifacts 14d); app-level ASan driver stays local (CI cost not justified yet) |
| E | E1 | MSBuild binlog → top-20 slowest TUs → sccache/ccache + PCH scope review; measure build wall time before/after | M | DONE s36-v | Shorter edit→build→test loop; less idle waiting per iteration (measurable delta, not a guess). s36-v DONE: measured, NOT needed - compile is 36 s / 238 TUs (slowest 2.14 s, PCH effective), edit-build-test loop ~10 s; sccache/PCH changes would not pay. Tool: `cmd/build-times.ts` (`out/build-times.txt`), see LOG s36-v |
| E | E2 | Permanent premake fix: fork-only sources missing from `premake5.lua` (AGENTS.md §6b "premake TRAP"); MERGE conflict surface — isolated commit | S | DONE s36-w | Removes the LNK1120 landmine: a `premake5 vs2022` run silently strips 10 fork sources and the link failure surfaces much later. s36-w DONE: 13 sources added to `sumatrapdf_files()` + DEPENDENTLOADFLAG asan exception (0xA00) with positive filters in both exe projects (beta8 ignores `not` filters); regen verified incl. fork files + 0xA00, build green (LOG s36-w) |

## Dependency updates — PROGRESSIVE PLAN (audit 2026-10-20)

Baseline: fork ships **mupdf 1.28.0-rc2**; upstream SumatraPDF master runs **1.28.5** and records its local mupdf edits as **47 patch files in `ext/patches/`**. Execute top-to-bottom; each phase must end green (build 0 err/0 warn + listed gates) before starting the next. Effort labels: S = < 1 h, M = 1–3 h, L = > 3 h.

### Measured diff (snapshot before any work)

- **mupdf 1.28.0 → 1.28.5**: 182 commits, 152 files touched — `source/fitz` (46), `source/pdf` (26), `include/mupdf` (12), `source/html` (10), `docs/reference` (13), `source/tools` (8), rest platform/wrap. Content is overwhelmingly **security/crash hardening**: OOB read/write in TTF subsetting (709729/709735/709736/709737), stack overflow + OOB in pdf recolor (709742/709743), OOB write in `mutool audit` (709753), OOB in seps pixmap conversion (708749), SEGV on early metadata (709734), plus shade clipping / pattern rendering / js `Rect` fixes. Patch-level release ⇒ no API break.
- **Patch carriage**: of upstream's 47 patches, **11 reverse-apply exactly** on our tree (0002, 0004, 0005, 0006, 0007, 0008, 0009, 0012, 0013, 0014, 0019), **22 apply cleanly forward = absent today**, **14 conflict = context drifted**. A wholesale tree replacement resyncs all 47 in one shot.
- **Integration surface** (upstream commit `585ce546`, 161 files): `ext/mupdf/**` (158) + `ext/versions.txt` + `premake5.files.lua` (+`crypt-chacha20.c`, +`regexp.c`) + `vs2022/mupdf.vcxproj`/`.filters` (+`regexp.h`, +`fitz-imp.h`, +`crypt-chacha20.c`, +`regexp.c`) + `src/SumatraPDF.cpp` (hardcoded `FZ_VERSION` string) + `cmd/deps-build-defs.ts` (upstream-only build system — **N/A for the fork**, which uses premake). **Zero other `src/` changes ⇒ no API adaptation required.**
- **Fork carries no local mupdf edits**: no commit touched `sumatrapdf-src/mupdf/` after the import commit `fd7e625`, and the working tree is clean ⇒ replacing the tree loses nothing.

### F1 — mupdf 1.28.5 (P1, M) — DONE s36-z

- [x] No branch/tag: the shared tree has a second agent on `main`, so the swap was done in place with explicit-path tracking (upstream baseline, not a fork branch).
- [x] Fetched upstream's vendored tree at the integration commit `585ce54655e8ccf2b82200aa2b31176cc2ebef6c` (sparse clone → `tests/tmp/sp-1285/`) and **replaced** `sumatrapdf-src/mupdf/` wholesale (`tests/tmp/mupdf-sync.ts`, CRLF-normalized, binary-safe): 1410 files, +7 / −4.
- [x] Wire the new sources: `premake5.files.lua` `mupdf_files()` → `+crypt-chacha20.c`, `+load-webp.c`, `+regexp.c`; `mupdf/source/helpers/pkcs7/pkcs7-windows.c` → `src/mupdf/pkcs7-windows.c` (upstream moved that helper out of the vendored tree — ported with `src/mupdf/README.md`; `premake5.lua` mupdf project gained a `src/mupdf` include dir because `murun.c`/`pdfsign.c` include it bare).
- [x] Regenerated `vs2022/mupdf.vcxproj` + `.filters` via `bin/premake5.exe vs2022` (regen is safe since s36-w); only those two files changed, fork sources intact. Stale PCHs deleted (2).
- [x] `ext/versions.txt` mupdf → `1.28.5` (2026-09-24).
- [x] `FZ_VERSION`: not needed — the fork defines no such string (upstream's is tied to their own `src/SumatraPDF.cpp` mutool block) and the build is green without it.
- Correction vs the audit snapshot: the 1.28.5 commit vendored **27 patch files** (not 47 — the 47 was later master) and **22 of them are already applied** in that tree; the fork carried only 11 ⇒ the swap is a superset, no patch behavior lost. All 27 + README now live in `sumatrapdf-src/ext/patches/` (F3 provenance).

### F2 — Gates for F1 (S) — IN PROGRESS

- [x] `bun cmd/build.ts` → 0 err / 0 warn → `Compiled/` (87.8 s full mupdf recompile).
- [x] `bun cmd/run-unit-tests.ts -dbg` → PASS.
- [x] `bun tests/all.ts` once — 2 **pre-existing** fork bugs fixed on the way: 23 tests wrote `SumatraPDF-settings.txt` (fork reads `TumatraPDF-settings.txt`) and `lint-mingw-sources` was missing 17 fork sources; `issue-906` spawned the exe via `cmd.exe /c` (breaks on the space in this repo path) → spawn argv directly.
- [x] Release rebuild so `out/rel64/obj-s/mupdf.lib` matches 1.28.5 (the fuzz harness links it) — `MSBuild vs2022\TumatraPDF.sln /t:TumatraPDF /p:Configuration=Release /p:Platform=x64`. NOTE: `cmd/build.ts` has **no** `-release` for x64 (only `-rel-32`).
- [x] Targeted E2E: `tests/ad-hoc-flashcard-cloze.ts` PASS, `tests/tmp/s36i-roundtrip.ts` PASS. Manual render smoke (`tests/tmp/rel-smoke.ts`, canvas pixel capture — release + debug): text PDF 255 colours / lumVar 5487, JPEG 35 507 colours (libjpeg 3.2.0 SIMD path), EPUB 523 colours / lumVar 8011 (mupdf HTML layout), markdown 1530 colours ⇒ no blank/errored render. Annot add/edit is covered by the cloze harness (2-page selection + spanning group).
- [ ] **Suite baseline (pre-existing, NOT caused by F1)** — 17 of the entries after `issue-5734` fail on this fork: `exit 5` shutdown crash (issue-5529, issue-2629, issue-4684, issue-5922, issue-5924, issue-5926, find-results-sorted, issue-5874, issue-2252), `-unit-tests exit 1` (issue-5840, issue-5842, issue-5846, parse-tip-brackets), UI/timing (issue-1914, issue-5866, issue-5869, issue-5792). Exit code **5 == 0xC0000005 access violation**, not "graceful quit" as `cmd/control.ts` claims. Reproduces only in the **debug** build (Release x64 on the same mupdf 1.28.5 exits 0; image-resize flow with no document also exits 0) ⇒ debug-only shutdown defect, triage separately.
- [ ] If a real regression appears: bisect by patch group (the 27 patches are the only local delta over pristine 1.28.5).

### F3 — Adopt the `ext/patches/` discipline (S) — DONE s36-z

- [x] Copied upstream's `ext/patches/` (27 `.patch` + `README.md`, the set carried by the 1.28.5 integration commit) into `sumatrapdf-src/ext/patches/`.
- [x] Documented the procedure in `MERGE.md` § "Dependency upgrades — vendored tree rules" (fetch upstream commit → replace tree wholesale → `ext/patches/` replay → regen premake → build).
- [x] Reverse-apply sweep verified (`git apply --check -R --ignore-whitespace -p1`): 22 applied / 19 absent / 6 conflict; the fork's pre-swap tree had 11 applied / 22 absent / 14 conflict ⇒ superset, no patch behavior lost. **Trap**: without `--ignore-whitespace` all 27 report a false conflict (repo CRLF vs patch LF).

### F4 — Small independent updates (P2, S each) — DONE s36-aa

- [x] `libarchive 3.8.8 → 3.8.10` (security release; 7z Delta/RISCV/ARMThumb/IA64 filters). 116 files synced + `archive_crc32.c` / `archive_umask.c` / `archive_umask_private.h` / `archive_xml_private.h` added, `archive_crc32.h` removed; premake list updated; `config_windows.h` needed `#define ARCHIVE_CRYPTO_WIN 1` (3.8.10 guards the WinCNG digest init with it) and `archive_read_support_format_cab.c` needed `(size_t)1 << w_bits` (C4334). Gates: `issue-1678` PASS, `issue-2199` PASS, unit tests PASS.
- [x] kjk decoders → current HEADs: `chmdec` `19d91e5` (LZX read-before-window fix, `chm.c` +411 B), `jxldec` `d71b246` (`jxl.c` +849 lines); `heicdec` (`8846fe9`) and `djvudec` (`cfd3251`) were already byte-identical to HEAD ⇒ metadata bump only.
- [x] `mujs 1.3.9 → 1.3.11` via `bun cmd/a-mujs.ts` (needs `VsDevCmd.bat` for `cl.exe`; `cmd/util.ts` `detectVisualStudio2026()` now falls back to VS 2022). Generated `regexp.h`/`COPYING` arrive CRLF from the clone and were normalized to LF.
- [x] `liblzma 5.8.3 → 5.8.4`: 18 files synced; the 5 local-only `common/tuklib_*.h` copies must stay (upstream moved them to its own `src/common/`, which the local build lacks). No `ext/versions.txt` entry exists for liblzma.
- [ ] `darkmodelib` → moved to F5 (renamed API, not a security fix).

### F5 — Medium/optional (M)

- [x] `libjpeg-turbo 3.1.4.1 → 3.2.0` (s36-aa). Re-vendored 145 files. 3.2.0 restructures: `jsimd.c` + `jsimd*.h` moved to `simd/` (one dispatcher, arch chosen by `SIMD_ARCHITECTURE` — added to the hand-maintained `jconfigint.h`); `jversion.h` now exposes only `JCOPYRIGHT` (`jerror.h` uses it); **all precision wrappers are mandatory** — `jcinit.c`/`jdmaster.c` dispatch on `cinfo->data_precision` at runtime and reference `j12init_*`/`j16init_*` unconditionally, so the build compiles `src/wrapper/*.c` (excluding `template.c` and the `rdpng-*`/`wrpng-*` PNG wrappers, which need the external `spng`). premake: asm lists mirror `SIMD_SOURCES` in `simd/CMakeLists.txt` (the `*-ext.asm` fragments are `%include`-only — never assemble them alone). Gates: build 0/0, `issue-3731` (JPEG render) PASS, `issue-2199` PASS, `issue-1678` PASS, flashcard cloze PASS, unit tests PASS.
- [x] Removed dead `ext/zlib-ng` (104 files) + its `premake5.files.lua` block, `premake5.lua` stale link line and `ext/versions.txt` entry. Zero references left.
- [x] `darkmodelib` upgraded to `e8ec95ba63` (2026-10-10; repo renamed `ozone10/win32-darkmodelib`, no tags). Renames: `DarkModeSubclass.{h,cpp}` → `Darkmodelib.h` + `src/Darkmodelib.cpp` (+ `MemoryHelper*`, new `DmlibIni.cpp`); **namespace `DarkMode` → `dmlib`** (23 files / 94 call sites rewritten; the function names themselves are unchanged; `UseDarkModeLib()` is the fork's own helper and stays). premake + `cmd/build-lib-defs.ts` updated. Verified: build 0/0, toolbar round-trip PASS, dark/light/flashcard-sidebar screenshots **pixel-equivalent to the pre-upgrade Release baseline** (chrome/page means within ±1/255; toggle round-trips exactly). Pre-existing oddity noted separately: dark-mode chrome paints a saturated yellow (≈ rgb 245,237,19) on this machine in BOTH builds — not caused by the upgrade; investigate separately (accent/theme preset).
- [ ] Bun `1.3.14 → 1.4.3` (build/test scripts only) after re-running the suite.

### F6 — Explicitly deferred (documented)

- [ ] **harfbuzz 13.0.1 → 14.6.0 and freetype 2.14.3 → 2.15.0**: both are pins of mupdf's thirdparty set, and mupdf **1.28.5 still pins exactly our versions** (harfbuzz `c28ba35`, freetype `0a0221a`). Upgrading independently diverges from the combination Artifex tests; revisit only when mupdf bumps its pins (then the delta is one tree swap).
- [ ] **synctex 1.16 / lzma 9.22b**: frozen upstream projects, no action.
- [ ] **UnRAR metadata drift**: `ext/versions.txt` says `7.1.6` while `ext/unrar/version.hpp` reports `RARVER 7.11.0` (2025-03-20) — verify against rarlab before any change.
- [ ] **OCR**: upstream vendors `tesseract 5.5.2` (Artifex fork) for `mutool draw -F ocr.*`; that is the cheapest path for the "OCR image" TODO item — decide separately (binary size cost).

## Refactoring backlog (from 2026-09-21 tree-sitter audit — see LOG entries for completed R1/R2/R4, 16A-D, C2-C5/F6, SelectionState)

## Render performance — optimization points (tree-sitter audit 2026-10-10, s34-c; P1 DONE 2026-10-11)

- [x] **R-P1 `BlitPixmapRegion` HALFTONE** (DONE s34-c 2026-10-11): stretch mode now only set when actually stretching (isStretch flag); 1:1 BitBlt tiles skip SetStretchBltMode entirely. Verified: s34-crop-e2e PASS.
- [x] **R-P1 arch-tools GDI+ churn (fork)** (DONE s34-c 2026-10-11): early-out (nothing to draw on page → skip Graphics + all ctors) + shared static Gdiplus Pen/Brush/Font objects (app never calls GdiplusShutdown → statics safe); dashed in-progress line preserved. Verified: s33-arch-repro ALL PASS.
- [x] **R-P1 `DrawDocument` O(N) loop** (DONE s34-c 2026-10-11): binary-searched [firstPage,lastPage] window over monotonic pageOnScreen.y (continuous modes, nPages > 64) + row-mate walk-back + ±2 slack; in-loop visibleRatio/isShown guards unchanged; non-continuous/degenerate geometry = full loop (safe degradation). Verified: crop + arch E2E (page-1 render + pan to page 3).
- [x] **R-P2 `PaintTile` linear scans** (DONE s36-m 2026-10-18, 80/20): Find + GetRenderDelay now scan NEWEST-FIRST — the hot hit is almost always the most recently added entry (render thread appends to cache[cacheCount]; requests appended + popped from tail), so the common case is O(1) instead of a full walk. Full hash keyed (dm,pageNo,rot,zoom,tile) CONSIDERED + REJECTED: no STL in base (would need a hand-rolled map surviving DropCacheEntry compaction + darkModeEpoch invalidation) for ~0.5-1ms/frame worst-case scan cost — bad cost/benefit for a core cache.
- [x] **R-P2 `RecalcVisibleParts` full-layout copy per scroll tick** — DONE s35 (TODO line 39: RecalcVisibleParts recomputes directly on pagesInfo; DocumentLayout::Reset has no per-tick caller anymore; verified by grep 2026-10-18).
- [x] **R-P2 hbmp blit DC churn** — STALE (verified s36-m 2026-10-18): render tiles are MALLOC pixmaps (EngineMupdf.cpp:1642 `AllocPixmap` sets no hbmp) so BlitPixmapRegion takes the StretchDIBits path — no CreateCompatibleDC per tile. The hbmp DC path exists only for DIB-backed pixmaps (icons, thumbnails).
- [x] **R-P2 `HdcCreateSimpleFont` per paint** — STALE (verified s36-m 2026-10-18): HdcCreateSimpleFont already caches via FindCreatedFont/RememberCreatedFont (base/Win.cpp:2155) — per-paint calls are a lookup, and no caller DeleteObjects the shared font.
- [x] **R-P3 annotations full-doc scan (s36-o)**: `EngineMupdfGetAnnotations` called `GetFzPageInfo(i, false)` = fz_load_page + annots + **full stext extraction** per never-visited page (the real killer). Now `loadQuick=true` (annots only); new `EngineGetPageAnnotations(engine, pageNo, out)` for single-page callers (EditAnnotations focus-lookup + `FindMatchingAnnotation` — both only need one page). Dialog list (EditAnnotations:1305) keeps the full scan, now without stext.
- Fine as-is: InvertPixmap (255-v) ≤ old ramp cost; debug-info Record/UpdateRenderInfo no-op when windows closed; FreeIfFull/FreeNotVisible bounded rare scans.

- [ ] **R3-left**: unify TrimRect math (TrimRectPage/Inverse) — sites semantics differ (mediabox shrink vs area/pt shift); forcing a helper = render-path risk. Only with a rendering regression test.
- [ ] **R5-left**: remove v1 crop stack (`DisplayModel.h` viewportCrop v1 fields) — BLOCKED: v1 button (Scan View) still live. Do when/if v1 is retired.
- [ ] **R6-left**: `Commands_AutoScroll.cpp:35-38` reads toolbar checkbox state directly (`IsDlgButtonChecked`) — route via `SetToolbarButtonCheckedState` idiom.
- [ ] **R7 / 16C-next**: continue god-file extraction — SumatraPDF.cpp (~14.2k lines, target ~10k), then Canvas.cpp (~5.7k; needs a dependency MAP first — mouse/keyboard interleaved with rendering, no established pattern), EngineMupdf.cpp (~6.7k, decouple rendering helpers per format).
- [ ] **16D**: typed `gGlobalPrefs` accessors (152 accesses in SumatraPDF.cpp) — only when tests exist.
- [ ] TC2 leftovers: uia `PageProvider` + legacy tile math not routed virtual→physical (non-cache path diverts; MVP-acceptable); favorites saved under v2 store VIRTUAL pageNo — normalize to physical at creation (same class as the FileState fix).


## Two Columns v2 — pending USER validations (code complete)

- [x] Visual (s36-l): Back/Forward after v2 toggle PASS (1→2→1→2); page edit `3R` graceful no-op (out-of-range, no crash) PASS; reopen doc saved with v2 = MANUAL ONLY (`-for-testing` never saves settings; no `-settings` flag).
- [x] Visual (s36-l): `TwoColumn2 + Trim` renders sane PASS; `TwoColumn2 + R2L` (manga) mirrors columns — leftmost = the RIGHT half PASS. Harness `tests/tmp/s36l-tc2.ts` + openrouter-VL vision review.
- [x] Autoscroll + TC2 acceptance (continuous 1L→1R→2L→2R, no jump) — code-verified; user confirm on real book.
- [x] **TC2 stall on mouse-hover (s35, 2026-10-12):** flaky stall with TC2+autoscroll+trim+contrast+invert — root: layout rebuild mid-scroll makes `IsAtDocumentEnd()` spike true (transient canvas geometry) → the tick KILLED the autoscroll timer permanently. Fix: skip the tick when GetCanvasSize() invalid (mid-rebuild). Probe `tests/tmp/s35-tc2-auto.ts` 3/3 SCROLLING after fix (was 1/2 stall before). User confirm on real book.
- [x] **TC2 drag-slow (s35):** `RecalcVisibleParts` rebuilt a full DocumentLayout + copied per page on EVERY scroll tick (2× cost under TC2 virtual pages). Now recomputed directly on pagesInfo (allocation/copy round-trip gone). User confirm.
- [x] **TC2 OCR/text selection broken (s35):** `DisplayModel::IsOverText` passed the VIRTUAL page into the physical-page TextSelection store → no hit anywhere virtual≠physical (right column or page≥2) → drag panned instead of selecting. Fixed with `VirtualToPhysical` (one-line, convention from the other callers). E2E `tests/tmp/s35-tc2-sel.ts`: right-column drag now selects (254 chars) + left/whole-page unchanged.

## Flashcards — open

- [x] **FC-X1 cross-page cloze (s36-m, 2026-10-18)**: selection spanning a page break = ONE card now. Author group token `#%08X` (`FlashcardGroupToken`) on every page's annot → loader merges into one card (`extraPages`); mask paints all pages; Page column shows range. Trap: exact `Author() == "TumatraPDF-Flashcard"` comparisons (2 sites) → `StartsWith`. E2E `tests/ad-hoc-flashcard-crosspage.ts`. PENDING: user live test on a real book (2-page passage).
- [ ] **FC-M1** tests: SM-2 (intervals/ease cap/relearn), study-JSON roundtrip, due filter — after Image Occlusion lands.
- [ ] **FC-M5** rate keys 1-4 have no visual hint (no rate buttons/tooltips on toolbar).
- [x] **FC-H2 CLOSED (s36-s, commit 85c02e7)** — `FlashcardLoadFromDocument` got the fast **/Annots pre-filter** (`pdf_lookup_page_obj` + `pdf_dict_get`; annot-free pages skip `fz_load_page` entirely — seconds→ms on big books; subagent plan (a); (b) docLock→pagesLock order REJECTED as deadlock-prone, (c) async / (d) partial lists rejected on complexity). Whole GetAnnotations class fixed: `loadQuick=true` + per-page API (s36-o), navigation delay SOLVED by `FlashcardPrefetchUpcoming` (s36-p). Gates: cloze e2e + s36i PASS; committed 85c02e7 (with s36-n/o/p batch) + 83a1393 (tidy leftovers).
- [ ] **FC-H3-left** study JSON: prune states of deleted cards (save only entries whose key matches an existing card). Atomic write + locale already fixed (s19).
- [ ] **FC-M4-left** filter expression compares PHYSICAL pageNo while TC2 shows virtual "3L" labels — decide dialog hint or virtual-input mapping.
- [ ] User live tests (queue): Filter window + global session (cross-tab jumping, per-book saves); List panel visual on real books; Settings window (folder picker + Drive backup, clear-selected, dark theme); bookmark-mirror on real TOC books (test PDF has none — automated test skipped the mirror).

## Image Occlusion cards — PLANNED (awaiting user decisions)

Boxes over images/diagrams as cards — same Highlight-annot + mask/reveal/rate pipeline as text clozes; only the DRAWING MODE at creation is new. 4 steps (IO-1 state `boxMode`+`pendingBoxes`; IO-2 canvas rubber-band before selection branches; IO-3 screen→page conversion like cloze rects; IO-4 `S` creates ONE Highlight with N quads, Esc/Box-off discards).

- [ ] **IO-D1** grouping: boxes session → 1 grouped card (recommended, matches Alt+multi-select) vs 1 card/box (Anki default)? — NOTE (s36-m 2026-10-18): the cross-page cloze already ships this exact mechanism for multi-page: `FlashcardGroupToken` (shared token written into the annot author) + `Flashcard::extraPages`. Reuse it for box groups (same card, several rect groups on one page) instead of inventing a second scheme.
- [ ] **IO-D2** mode entry: toolbar "Box" button only? key `B`? both?
- [ ] **IO-D3** scope: boxes cover image/diagram regions only (no text selection) — confirm.

## s32-B — image batch live-debug (user authorized)

Replace Image / Edit With Paint / top-level Copy Image shipped s31 — s34 E2E now covers the round-trip: Replace picker→swap→writeback (s34-replace), Paint save→re-import (s34-paint), Crop drag→Save→ReplaceImageByIdx writeback (s34-crop). REMAINING (user-side): manual File > Save after an in-memory writeback to confirm on-disk persistence on a real book.

## Design rules — implementation pending

- [ ] Buttons/inputs must center content (text/icons) horizontally AND vertically (ToolbarLayout controls, timer/speed inputs, new UI elements). Rule defined 2026-09-08.

## Test harnesses (tests/tmp/ — scratch, E2E-class até virarem ad-hoc)

- s34-replace-e2e.ts, s34-paint-e2e.ts, s34-crop-e2e.ts — Image replace/paint/crop+menu-roundtrip.
- s34-edit-e2e.ts (10 checks), s34-reload-colors.ts, s34c-inpage-e2e.ts (7 checks, draw-rect move path).
- s34e-doubletoggle.ts — toolbar rebar heights across 2 on/off cycles (layout regression probe).
- s34g-findcrash.ts — Ctrl+F typing repro on a book (outro uso: qualquer crash repro parametrizado por livro/query).
- s34c-tb-probe.ts, s34c-tb-probe2.ts — sondas de rebar/toolbar decoupling.
- s35-tc2-auto.ts — autoscroll stall probe (idle/storm/post-storm pixel-hash; flaky repro).
- s35-tc2-sel.ts — TC2 text selection por coluna (drag + clipboard readback).
- s33-arch-repro.ts — arch tools flow completo (scale/measure/pan/gates) — registrado em before-release? → virar ad-hoc se não estiver.
- Util: tests/winapi.ts, tests/win-automation.ts; DPI traps e postMessage traps documentados no lemma (m9fa780f3cf0b, m5317f7f3eb3d).

## Research (low priority)

- [ ] Image tools: right-click → 'OCR image' — find a lightweight, near-zero-dependency OCR route. Low priority.

## Bookmarks Improve
1. 'Reorder' on/off: permite click:hold e reposicionar a ordem dos bookmarks
    - Soltar o bookmark sobre outro, o inclui como 1 nível indent, soltar no extremo esquerdo retira por fora do nível;
    - Exibir uma linha horizontal dinâmica que representa onde o bookmark vai entrar ao soltar, dependendo da posição do cursor
2. Rename: Click-Right (CR) → Rename
3. Delete: CR
4. 'Add': Cria um bookmark no final da lista com a página ATUAL
5. Update: CR → Altera o bookmark clicado para a página atual
Todas as alterações: Salvar no documento para persistir.

## Page Counting Format
O sistema já exibe páginas em outros tipos de contagem, precisamos de uma ferramenta para alterar os pdfs, fazendo com que os números do arquivo batam com os números reais do livro.

Similar ao sistema adobe, queremos uma ferramenta para mudar o formato de contabilizar alguns intervalos de página, especialmente para início de documentos.

Exemplo: página 1-50 contabilizar em romano, da página 51 em dia, numérico → A página 51 se torna a nova página 1. Digitar 1 no campo de páginas, cai na página 51. Dessa forma o match das páginas dos livros fica exata.
Podemos criar uma função página absoluta, caso queremos ignorar as contagens consméticas; nesse exemplo: pág 1 → pag 51; pág (1) → pág 1.

## Ferramenta de captura
shift+s
desenha um retangulo que captura a imagem dentro do retangulo com o DPI correspondente ao documento → copy to clipboard

## Stats
para cada livro lido no stats, vamos incluir do lado direito `book_stats` (escolher um emoji), esse botão vai abrir uma janela de informações sobre o livro;
dentro dessa janela, vamos criar um gráfico de calor de leitura:
### Reading heatmap
A ideia é criar um gráfico grid de quadrados (similar ao do stats), onde cada quadrado representa 1 página do livro;
quando autoscroll=on, toda vez que o app entrar em uma nova página E sair para a próxima página, deve ++1 nessa página;
### `book_stats`
|*Nome do livro*|
|Reading: 00h 25m |
| 7/15 Pages | 46% |
|Flashcards: `info` |
|Reading Heatmap:|
|⬛⬛⬛⬛⬜|
|⬜⬜⬜⬜⬛|
|⬜⬜⬛⬛⬛|

# Bugs open

### Old
- [x] **FIXED 2026-10-11: CRASH on search (Ctrl+F)** — dump 34131b symbolized (dbg exe has symbols): `DisplayModel::PhysicalToVirtualForRect` wrote `*rectOut` unconditionally, and the FindThread `searchVisible` probe (SearchAndDDE.cpp:1357) was the ONLY caller passing nullptr → null-write on every search that reached that branch. Guard added on both branches (null rectOut ⇒ identity rect, return unmapped page). Real-doc E2E (ACM query + 2× Enter on the CAD esquadrias PDF) now crash-free.
- [x] **s34-g SHIPPED (2026-10-11, built+deployed, not committed):** resize enlarges BEYOND the original image rect (content-stream cm rewrite `SetImageDrawRect` + data swap; fallback = clamped padded swap on rotated/nested pages); flicker-free bbox drag (region-only invalidate); scale label text → white; Arch **Measure Radius** (3 clicks, r= label) + **Ref Line** (2 clicks, subtle full-page gray) — commands 523/524, persisted via measurements `kind`; Enter/numpad-Enter applies from the scale numeric field (edit subclass). USER validation pending: radius/refline UX, enlarge on a real book, flicker gone.

- [x] **NEW 2026-10-10** Saving the document DEACTIVATES invert + contrast — FIXED s34-b: ReloadDocument now carries invertColors/contrastEnabled/contrastOpacity into the fresh FileState (E2E s34-reload-colors PASS; post-reload canvas byte-identical).
- [x] **NEW 2026-10-10** Color inversion of IMAGES is very bad — FIXED s34-b (user decision: true negative): session invert now per-pixel 255-v via InvertPixmap/InvertBitmapColors instead of the theme-color ramp + link-substitution (both corrupted images). Live-visual confirm pending.
- [x] **NEW 2026-10-10** Ctrl+click to edit a highlight is SLOW — FIXED s36-o: `FindMatchingAnnotation` (SumatraPDF.cpp) filtered on `a->pageNo == key.pageNo` yet scanned the WHOLE document (fz_load_page + stext per page); now `EngineGetPageAnnotations(engine, key.pageNo)` = one page, no stext.
- [ ] **NEW 2026-10-10** Ctrl+click on a FLASHCARD highlight opens the flashcard List — IMPLEMENTED s34-b (Canvas ctrl+click detects /T author TumatraPDF-Flashcard → FlashcardSidebarShow; turns flashcard mode on if off). USER live-test pending (needs a book with flashcard highlights).
- [x] **NEW 2026-10-10** Flashcard hover label shows "testa" — RESOLVED 2026-10-10: "testa" is the annotation /T author (defaultAuthor → `%USERNAME%`, Annotation.cpp GetUserTemp :1384). User reviewed and decided to keep author behavior as-is (it only persists in the file on File > Save; `(none)` in `annotations.defaultAuthor` suppresses it).
- [ ] **BUG-3** right-click image context menu: s34 E2E confirmed Replace/Paint/Crop work + submenu visibility fixed; REMAINING live-check: top-level Save-as path (CmdSaveImage → ShowImageEditWindow Save mode) and the non-image items (Copy-link etc.) reported dead. May be unimplemented upstream — investigate before fixing.
- [ ] **BUG-4** Contrast × Trim Config ONE-SHOT vanish (first trim-config open after launch). Fix v2 (`EnsureContrastOverlayState` invariant + self-healing) deployed 2026-09-22 — awaiting user confirmation of the exact flow; `[contrast]` breadcrumbs in debug build.
- [x] **Image editing functions (user 2026-10-10)** — Replace/Paint/Crop/Resize all FIXED s34 (E2E pass: `tests/tmp/s34-{replace,paint,crop}-e2e.ts`); root cause was a no-op image-submenu pruning bug (base `MenuRemove`/`MenuSetEnabled` don't descend into submenus) + WStrL/order bugs. Modularization into `Commands_Image.{h,cpp}` DONE s34-c 2026-10-11 (Menu.cpp dispatch stays — it's menu plumbing, not image logic). Convert button stays grayed by user decision.
- [x] **Edit mode polish (s34-d/e/f, 2026-10-11):** in-page bbox editing SHIPPED (click image → bbox over page, border/edge/inside drag; E2E 7/7). s34-e: round resize/square crop handles; live preview; fc/edit 2nd-toolbar double-toggle dead space fixed (uiState.layout force-clear + rebar pre-size). s34-f: ALL edits = pure image-data swap (stamp path REMOVED per user — footprint fixed, enlarge limited to the original image area); bbox tracks the applied region (`PageImageEdit.content`); 🔴 marker via ToolbarUpdateStateForWindow. Remaining polish: "+Text" click-to-place flow; Esc cancels selection; preview bbox for armed +Img; stamp move/resize UX after placement; "+Img from file" picker.

### USER
função twocolumn sempre abrir 'off' em novos arquivos; a função não está fechando corretamente quando trim=on;

precisamos armazenar POR ARQUIVO, pelo menos para os últimos 10 arquivos:
- última velocidade de autoscroll
- configuração de 'trim config'
Para arquivos nunca abertos antes, usar config 'padrão'

[] Quando Two columns=on & Autoscroll=on & Trim=on & Contrast=on & Invert=on, ao mover o mouse por cima da tela, o autoscroll para. Avaliar. Pode estar relacionado com o peso do arquivo.

Quando Twocolumns=on, clicar e arrastar a página está lento/travando;

Quando Twoxolumns=on, não consigo selecionar o texto OCR para anotações ou flashcards;

[ ] melhorar o passo do zoom

Quando 'Trim Config'=on, pausar o autoscroll

precisamos ter um "MASTER" de tempo de leitura (backup) independente dos livros para caso o tempo de leitura não se perca quando algum livro seja renomeado, deletado ou movido; em um primeiro momento vai estar relacionado ao livro, mas depois continua armazenado mesmo sem o livro em disco. isso será importante especialmente quando a lista de documentos lidos ficar grande demais e tivermos que limitar.

No home, o tempo de leitura deve ficar: Today 'Nm' | Total: 'Nh NNm'; está aparecendo '7d', nao sei o que seria isso; também melhorar o contraste das cores, os tempos estão muito escuros, ficam pouco legível.

'Ctrl+shift+t' → reabrir sessão fechada com todos os livros (pode ser a ultima janela fechada com todos os livros)

quando Twocolumn=on, clicar em algum bookmark, leva o usuário para a página Left do número de página registrado no bookmark (capítulo), atualmente está indo pra página errada;

### 2026-10-15
[x] autoscroll:on & twocolumn:on → shift:hold → não consigo arrastar a página com o touchpad do notebook — FIXED s35-b (2026-10-15): causa = WM_MOUSEWHEEL (swipe vertical do touchpad) chega com MK_SHIFT (Shift é o modificador do quick view) e era remapeado como scroll HORIZONTAL (Canvas.cpp CanvasOnMouseWheel hScroll check) — sem espaço horizontal no quick view = nada rolava. Quick view ON agora força scroll vertical. E2E: tests/tmp/s35b-quickview.ts (wheel MK_SHIFT scrollY 3→23 OK + drag pan OK).
[x] autoscroll:on & twocolumn:on → estou lendo o lado DIREITO da página → shift:hold → shift:release volta para o lado ESQUERDO da página — FIXED s35-b (2026-10-15): QuickToggleViewportCropV2 restaurava sempre `physAnchor*2-1` (coluna ESQUERDA). Agora captura a coluna na ENTRADA do quick view (`viewportCropV2SavedColumn`, ColumnOfVirtual) e restaura nela na saída (RestoreViewportAfterV2Toggle ganhou param `column`, default 0 = toggle regular inalterado). E2E: página virtual 2 antes → 2 depois do release (coluna direita mantida).

## Função Stats — SHIPPED s35-c (2026-10-16); design round s35-d IN PROGRESS (user feedback @1131)
- [x] **B1 Reading-time capture**: seconds credited while autoscroll scrolls (fixed-page: per-tick elapsed; markdown/webview: wall-clock dt between guardian ticks). Per-doc `FileState.readingTimeSec` (gen-settings; persisted with file history) + per-day sidecar `<settingsDir>\reading-stats.txt` (plain `YYYYMMDD <sec>` lines). Flush at ≥60 dirty sec, toggle-off, doc end, timer done, SaveSettings. E2E s35c-stats PASS (9s credited over ~10s scroll).
- [x] **B2 Auto-pause**: frame deactivate (minimize / focus loss) → autoscroll pauses (timer killed, toolbar unchecked, `[autoscroll] paused` log); re-activate → resumes (fresh time base, ETA recomputed). Manual toggle always overrides. E2E: pause window credited ~0 (delta 9s vs ~13s without pause).
- [x] **B3 Home stats band** v1 (top half of Home): most-read ranking, totals, heatmap, annual chart. E2E s35d-homestats PASS.
- [x] **D1 Layout restructure** (s35-f): Stats section on TOP ("Stats" title); recently-opened header + search moved DOWN into the books section (chrome->booksHdr); gray separator at the band bottom. E2E PASS.
- [x] **D2 Heatmap hover offset BUG** (s35-f FIXED + styled): ev.pt LOCAL vs WINDOW-absolute rects → handlers convert pt+bounds. Empty = black w/ gray outline; filled = graded silver; hover = silver; TODAY = orange outline. E2E PASS (silver 10135 / orange 78).
- [x] **D8 Coordinate-mismatch family audit** (DONE 2026-10-16): convention documented in AGENTS.md §12 (VirtWnd pt spaces: ev.pt/OnSetCursor/GetTooltipTemp = LOCAL to bounds; ctx.bounds/VirtWnd::bounds = ROOT-space; bridge = pt+bounds.{x,y}; Cvt* = page↔canvas-client; trim precedent was the same class, R4 unified the math). tree-sitter inventory: CvtToScreen ~19 files (Canvas 30, DisplayModel 14), CvtFromScreen ~12 files, ptLocal family in VirtWnd.cpp 28/HomePage.cpp 12/TipText/TabsCtrl. No further duplicated conversion code found (R4 already deduplicated trim; the HomeStatsWnd 3-handler bridge is the single local→root bridge).
- [x] **D3 Most-read as proportional bars** (s35-f): rows = name + time + silver bar ∝ hours (max = top reader); internal WHEEL scroll (OnMouseWheel over rcRank consumes; home scroll elsewhere); "N books - scroll" hint.
- [x] **D4 Design system** — F1 SHIPPED s36 (2026-10-16, built+deployed, not committed): HomeStats palette extracted to shared `DsAccents` design tokens (`ToolbarLayout.{h,cpp}`, theme-aware resolved per paint; fonts `DsFontFamily/DsFontSizeLabel/DsFontSizeTitle`); dark pixel-identical (`s35d-homestats` PASS: 10135/953813/78). USER visual review PENDING: `tests/tmp/s36-f1-grid.png` + `s36-f1-graph.png` (dark). LIGHT pair values added but UNVERIFIED (user: dark first, light later). §6.1 verdict still stands for the rest — toolbar grouping weak → F5/D9. CONTAINMENT (user ask s36): cellDx floor 4→2 + `s36b-responsive.ts` probe — band content proven inside the band at 600-1400px windows, narrow windows degrade via design guards (see LOG s36 Notes).
- [x] **D5 Home button state** (s35-f; page-controls leak FIXED s36 2026-10-16): overlay ON → only Home visible + checked/pressed; OFF → restore. E2E s35d-d5: 25→1→25. s36 follow-up (user: "pq continuam aparecendo botões no home=on?"): the 4 floating page controls (label/bg/edit/total) stayed visible parked top-left — `UpdateToolbarPageText` anchored on CmdPrint/CmdOpenFile which hides with home=on; now gated on `win->showHomeOverlay` BEFORE TbGetRect (TB_GETRECT fails on hidden buttons — also a latent crash on resize-while-home=on, closed). E2E s36e-d5: home=on → 1 button + 0 visible toolbar child windows; off → 25 + 4 restored. Round 2 (s36f, user 2026-10-16): timer trio hidden the same way (gate BEFORE TbGetRect in PositionToolbarChildWindows + LayoutToolbarChildWindows on the toggle); timer slot now reserves the MEASURED trio width (MeasureTimerTrioDx) so it can never overlap the previous group; contrast veil SUSPENDS on home=on and resumes on home=off (EnsureContrastOverlayState show/hide; create/destroy still bound to contrastEnabled — BUG-4 invariant intact); Esc-close now runs the full restore sequence (used to leave ALL buttons hidden). E2E s36f-homefocus PASS (real doc: 25 btns + 7 children → 1 + 0 + veil suspended → 25 + 7 + veil resumed; Esc restores). Vision review (gemini subagent): PASS/PASS.
- [x] **D6 Responsiveness** (F4 SHIPPED s36, 2026-10-16, built+deployed, not committed): TOOLBAR side FIXED — sliver + gap had two chained root causes: (1) `SetToolbarBandToRowHeight` used `rows × first-button-height`, wrong for MIXED rows (icon rows 30px vs BTNS_SHOWTEXT rows 38px → 60 instead of 68 → last row clipped); now measures the real extent (maxBottom + top margin) and self-corrects against the rebar's child-edge inset (RB_SETBANDINFO → re-measure → grow loop, ≤3 passes); (2) `RelayoutFrame` re-applied the rebar's own measured height, so the rebar WINDOW never grew with the band and clipped it from outside; now sized from Σ band cyChild + window chrome (RB_GETBANDCOUNT/GETBANDINFO). E2E `s36g-d6.ts` PASS at 1400/900/700/640 (rows 1/2/2/2: band==content 30/68, toolbar window 34/94, rebar win/client 30/30 + 68/68, zero canvas overlap — truth source = new `d6` header in TestToolbarButtons, measured inside the app); regressions s36e-d5 + s35d-homestats (pixel-identical 7437/956999/78) + s35d-d5 PASS. Stats-band side CLOSED earlier s36b (probe 600→1400 all contained; narrow windows drop/blank the band by design: layout reserve gate + b.dx<320; clamp cellDx 4→2 defense-in-depth). Vision review of `s36g-d6-{wide,narrow-2row}.png` re-dispatched (first gemini attempt returned NO_VISION).
- [x] **D6b drag-collapse SOLVED (s36-k round 2, 2026-10-17)** — user: shrink fixed, now fails on EXPAND ("só uma toolbar vazia"). TWO chained writers: (1) band cx feedback loop (RB_GETBANDINFO→SETBANDINFO write-back glued transient widths) — Design B: only the rebuild writes cx, height passes write RBBIM_CHILDSIZE only, client<=0 recovery; (2) RBBS_FIXEDSIZE on the main band pinned the child to a transient width → REMOVED (Toolbar.cpp ~2528, fStyle=RBBS_VARIABLEHEIGHT only; no gripper on single-band rebar). Plus `ToolbarRewrapAfterSizing(force)` on WM_EXITSIZEMOVE (click-release reset). Gate 3/3 PASS (s36k-dragsim both directions + s36i + s36e-d5) + vision 6/6. Details: LOG.md s36-j "Fixed (s36-k round 2)"; lemma m9e3d3feb1ff6.
- [x] **D6c wrap por blocos (s36-k round 3, 2026-10-17)** — user: "vários botões descem todos de uma vez... revisar os blocos" (page nav devia ficar com os números de página). `PackToolbarGroupsByWidth` (greedy por grupos nos separadores) substitui o balanceamento do `TB_SETROWS`; breaks = `TBSTATE_WRAP` embutido no re-add; `TBSTYLE_WRAPABLE` removido (comctl recomputava os states); pack mede a largura do REBAR + autocura no rewrap. Narrow = 3 linhas coerentes (nav+página+zoom / trio do timer / tools fork). Traps: `wrapAfter[-1]` stack corruption (grupos all-hidden), mark em sep (hidden → wrap perdido). LOG s36-j "Changed (s36-k round 3)"; lemma mf7383013304c.
- [x] **D7 Calendar grid↔graph mode toggle** (s35-f; grid REDESIGNED s36 2026-10-16 per user spec): Graph = line chart day(x)×hours(y), silver points + DrawCurve, orange TODAY cross-hair; TestStatsToggleGraph=46 pipe E2E round-trip PASS. Grid = month-separated heatmap: inter-month gap dpi(6), weekday letters ALL 7 rows vertically centered (M T W T F S S), 3-letter month names (Jan..Dec) BELOW each block centered over its columns, year nav clamped ≥2026 (no data before the feature shipped). Graph x axis: `|` tick at each month start + 3-letter name centered in its segment. Calendar verified against the official one (JS Date) — E2E s36d-grid PASS (13 seeded cells at exact (row,col), month gap 8px). User visual review PENDING: tests/tmp/s36-f1-grid.png + s36-f1-graph.png (+ s36d-grid/graph PNGs).
- [x] **D4a Design-canon audit + `Design_Guidelines.md` §6.1** (2026-10-16): new subsection **§6.1 "Visual system for fork-authored UI"** = DPI space scale (`DpiScale(hwnd, n)`, n ∈ {4,8,12,16,24,32}), theme colors only via `Theme.h` helpers, repaint on theme switch (never a baked class brush), contrast floor (text 4.5:1 / large+UI 3:1 in BOTH themes), toolbar grouping (`DpiScale(hwnd,1)` separator, gap 8 inner / 16 cross, centered), icon-vs-text (monochrome icon first, no emoji in product chrome), dialog anatomy, and the MANDATORY screenshot verification loop (dark + light, via `scripts/capture-window.ps1`). Baseline evidence + audit: `temp/design-2026-10-16/` (`20-tuma-home.png`, `21-tuma-reader.png`). Verdict of the visual review: fork UI reads as "features bolted on" — toolbar mixes text/emoji/icons with random spacing, home stats band has weak hierarchy.
- [x] **D4b Timer-done overlay (AutoScroll.cpp) theme + DPI**: hardcoded `RGB(30,30,30)` class brush + `RGB(255,165,0)` text → `ThemeWindowBackgroundColor()` painted in `WM_ERASEBKGND` (honours a live theme switch) + `ThemeWindowTextColor()`; box 200x80, offset 40 and font 52 now `DpiScale()`d. Visible defect only in light/System themes (this install is all-Dark).
- [x] **D9 Toolbar (autoscroll / contrast / flashcard controls)** — SHIPPED s36 (F5): duplicate `kCtrlGapX/kCtrlH/kLabelW/kTimerSlotW` locals unified into `gToolbarTokens` (one source of truth); double separator before the autoscroll group removed (one thin separator per group boundary); Home = ICON-ONLY button (new `TbIcon::Home`, Tabler-style house SVG — replaces the "⌂ Home" glyph-text button, §6.1 no-glyph rule); every custom control already centered (`DT_CENTER|DT_VCENTER|DT_SINGLELINE` in the CDDS_ITEMPOSTPAINT draw, shipped earlier); text labels kept on the remaining fork buttons on purpose (ambiguous without text — full iconization deferred to user taste). E2E s36h-d9 PASS (35 buttons, 0 consecutive separators, Home rect 32px icon-sized); regressions s36e-d5/s35d-d5/s36g-d6/s35d-homestats all PASS.
- [x] **D10 Remaining hardcoded colors in fork files** — CLOSED s36 (F6+F7 audit + surgical tokens): `FlashcardToolbar.cpp` 15 duplicated raw RGB sites → named semantic state tokens (kFcSync*/kFcHold*/kFcCheckedAccent, values identical); ETA overlay magenta colorkey + black pill documented as INTENTIONAL (floats over page content — theme colors would blend into the page); audit-closed with no change: `Canvas.cpp` (debug visualizers + already-paired LoadErrorTextColor), `FlashcardSidebar` (named status set + dark/light pairs), `HomePage` (kCol1..5 upstream brand bar, kAboutBorderCol upstream heritage, kHomeSelectionColor named semantic accent, help "?" black-on-own-white-disc affordance).
- [x] **D11 Hardcoded spacing in fork files** — F2 SHIPPED s36 (2026-10-16, built+deployed, not committed): raw constants wrapped in `DpiScale` at use — `kOpenDocumentYShift` ×2 + paired `+3`s (books header), `kThumbsMiddleMargin` (thumbsBottomY), HomeStats breakpoints `b.dx < 320/520` (DIP vs already-scaled px; no-op at 1.25 because band widths are column-quantized 212/471/720…), `FlashcardSidebar` `kFcSplitterDx`. AUDIT: the rest was already compliant — `kAbout*`, `kInnerPadding`, `kSearchEditDy`, `kHeaderSearchGapY`, `kSearchThumbnailsGapY` (all DpiScale'd at call sites), `kAIChatLabel*` (AIChatCommon), `kFcListMinDx` (FlashcardSidebar). Heatmap `cellGap/rowGap=2` deliberately raw (micro-gap, GitHub-heatmap convention; scaling would change the approved dark look). Regression: homestats pixel-identical (7707/956498/78).
- [x] **D12 Toolbar text buttons ellipsize ("Auto-s...")** (user 2026-10-16: *"não quero que corte o texto do nome dos botões (Auto-s...), deixar o nome inteiro"*) — F3 SHIPPED s36 (2026-10-16, built+deployed, not committed): `EnforceToolbarTextButtonWidths` (Toolbar.cpp) after both button (re-)add sites (creation + rebuild, before the wrap loop): measures each `BTNS_SHOWTEXT` label with the toolbar font (`HwndMeasureText`) and enforces `labelDx + DpiScale(12)` min width via `TbSetButtonDx` — only widens, never shrinks, labels never shortened. Root cause: comctl `BTNS_AUTOSIZE` measures the label once at insert (stale/default font) → truncates at e.g. 100% DPI. At 1.25 the widths already fit (dump `tests/tmp/s36c-d12-{before,after}.txt` identical — no-op here, guarantee elsewhere). Labels drawn centered by the existing CDDS_ITEMPOSTPAINT (NO DT_END_ELLIPSIS there — the earlier Button.cpp `kBtnTextFmt` lead was for dialog buttons, not toolbar). `s35d-d5` PASS post-change (25→1→25). Related open: **D6** (toolbar sliver/row gap — F4), **D9** (groups — F5).

## Vertical_Toolbar (NAO FAZER)
em outra sessão eu pedi para o agente mapear simbolos para associar a cada botão novo; vamos implementar um sistema, quando a janela estiver menor do que 1/3 da largura da tela, vamos adaptar a barra de ferramentas na vertical esquerda, com os boões de cima pra baixo; vamos continuar com uma barra de ferramentas horizontal superior onde vamos deixar os campos com input numérico: page e timer (exemplo) e os botões que;
os botões que passarem para a vertical_toolbar se converteram em símbolos (nome do botão em mouse:hover); dessa forma ficará mais bonito;
quando home=on, continuar ocultando todos os botões da mesa