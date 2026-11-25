## 2026-11-25 (s84) — Update dialog: paint + layout + probe FAIL->PASS (true root causes)

### Fixed
- Update dialog ("New version available") showed the question but NO BUTTONS (user could not answer).
  - TRUE ROOT CAUSE 1 (paint): raw `TUMATRA_UPDATE_DIALOG` never handled `WM_DRAWITEM`. Owner-drawn buttons (BS_OWNERDRAW) require parent to paint them via `WM_DRAWITEM` -> `Button::PaintOwnerDrawn`. Missing case in `UpdateDialogWndProc` (UpdateCheck.cpp:690) -> buttons never painted -> user sees question with no buttons.
  - TRUE ROOT CAUSE 2 (layout): buttons laid out against computed `totalClientH` but `AdjustWindowRectEx` + DWM frame made real client area shorter -> buttons clipped at bottom. Fix: after `SetWindowPos`, re-read REAL client rect (`GetClientRect`), if shorter than needed, grow window (`SetWindowPos` again), then place buttons BOTTOM-ALIGNED in real client rect (`btnY = clientH - pad - btnH`).
  - TRUE ROOT CAUSE 3 (click IDs): `CreateButton` calls lacked `ctrlId` (100/101) -> `WM_COMMAND` ignored clicks (fell through to IDCANCEL=100 "Don't install").
  - Double-launch regression avoided: dialog stays PURE (returns 100/101 only); caller (`NotifyUserOfUpdate`) launches ONCE; on launch fail shows theme-safe `MessageBoxWarning` with error and returns without exiting.
  - Install failure feedback: caller shows `MessageBoxWarning` ("The update could not start. The downloaded file may be blocked by your antivirus - unblock it and try again.") instead of silent fail.

### Added
- `WM_DRAWITEM` case in `UpdateDialogWndProc` (UpdateCheck.cpp:690) dispatching to `btnDontInstall->PaintOwnerDrawn` / `btnInstall->PaintOwnerDrawn` by `CtlID` (100/101).
- Layout fix: after `SetWindowPos`, re-read client rect, grow if needed, place buttons bottom-aligned (`btnY = clientH - pad - btnH`).
- `CreateButton` signature + calls: `ctrlId` param (100 = Don't install, 101 = Install).
- Layout reorder: create buttons first -> compute `totalClientW` -> measure text with real static width (`totalClientW - 2*pad`).
- `WM_SETFONT` to buttons with `GetAppFont()`.
- Caller-side launch failure UI: `MessageBoxWarning` with error msg (theme-safe, no hardcoded RGB).
- Probe `ad-hoc-update-dialog.ts`: layout verification (buttons fully inside client rect in CLIENT coords), BM_CLICK on Install, asserts exit 101. Pixel paint check removed (PrintWindow doesn't capture owner-drawn reliably); layout check catches clip bug.
- Probe runs BOTH variants: installerExists=0 ("Download update") + installerExists=1 ("Install and relaunch").
- Probe timeouts: bounded `Promise.race` on control pipe (15s) and quit (5s) -> no 21-min hangs.

### Changed
- `CreateButton` signature in `wingui/WinGui.h:276` + `wingui/Button.cpp:198` -> `ctrlId` param (default 0).
- `UpdateCheck.cpp`: WM_CREATE reorder, text measurement with real static width, bottom-aligned button layout, WM_DRAWITEM dispatch, WM_SETFONT to buttons.
- `NotifyUserOfUpdate`: on `StartInstallerAutoUpgrade` fail -> `MessageBoxWarning` + return (no exit).

### Verified
- Probe `ad-hoc-update-dialog.ts`: 3/3 runs PASS (was FAIL on original code - buttons unpainted + clipped). Per run ~4.2s (was 21-min hang).
- Layout assertions: buttons fully inside client rect in CLIENT coords (`clientRect.bottom <= clientH`, `clientRect.top >= 0`).
- PNG captures: `tests/tmp/update-dialog-probe-0.png` (Download update) + `update-dialog-probe-1.png` (Install and relaunch) show both buttons COMPLETE, not clipped.
- Build: clean (0 errors) debug x64; `out/dbg64/TumatraPDF.exe`.
- `bun scripts/flow-sync.ts` passes (35 checked, 21 with known debt).
- Graphviz renders `FLOW/DEBUG/update-dialog.dot` (exit 0).
- WM_DRAWITEM sweep: checked all raw WndProcs hosting repo Buttons -> only UpdateCheck.cpp was missing the case (others: FlashcardToolbar, FlashcardSidebar, SumatraDialogs, SumatraPDF, Wnd framework).

### Notes
- Only allowed files touched: UpdateCheck.cpp/h, WinGui.h, Button.cpp, ad-hoc-update-dialog.ts, before-release.ts, LOG.md, FLOW/DEBUG/update-dialog.dot.
- No CHANGELOG/README/update.txt/FLASHCARD_ALGORITHM.md changes.
- Commit 1: fix + probe + map. Commit 2: LOG.md.

## 2026-11-25 (s83) — SM-2 spec doc added; scheduler gap map; FLOW maps re-verified + re-stamped; flashcard maps now state SM-2 is lite variant

### Added
- Root doc `FLASHCARD_ALGORITHM.md` = official SuperMemo/SM-2 spec (sources: canonical official article, official blogs, twenty rules) + our scheduler map (file:line) + completion plan P0-P6 (PROPOSAL, nothing implemented) + 8 owner decisions; registered in AGENTS.md hierarchy + caveman list.
- Code audit of our scheduler (12 state writers, all preserving sorted-by-key invariant; settings surface = 3 fields and NOT ONE scheduler knob; missing deterministic clock seam -> reproducible scheduler test impossible until P0).
- Key spec findings vs ours (EF formula: `EF' = EF + 0.1 - (5-q)*(0.08+(5-q)*0.02)`, I(1)=1 / I(2)=6 / I(n)=I(n-1)*EF rounded up, failure reset MUST NOT change EF, same-day re-repeat until q>=4, rating mapping: Again->q=1/0, Hard->q=3, Good->q=4, Easy->q=5).

### Changed
- `TODO.md` re-synced after v1.0.22 (open-work list only; SM-2 item now points at doc + real phases; flashcard bookmark-mirror covered by `ad-hoc-flashcard-filter-bookmarks.ts` probe).
- FLOW maps re-verified + re-stamped (14 stale stamps) + flashcard maps now state SM-2 is lite variant with reference to `FLASHCARD_ALGORITHM.md`.

### Verified
- `bun scripts/flow-sync.ts` passes (35 checked, 21 with known debt) + Graphviz exit 0 on all 15 touched maps; baseline 21 maps / 75 refs.
- Doc corrections verified (7): citation traceability, rating mapping, repetition off-by-one, deterministic migration rule, EF formula delta table, failure reset EF change, round-up interval.

### Notes
- SM-2 work NOT started (stays in TODO.md); first blocker is clock seam (P0); 8 owner decisions listed in doc section 9; user does visual validation.

## 2026-11-25 (s82) — Flashcard study freeze fixed; Home responsive + heatmap readable; Filter bookmarks first open; Auto-update install button fixed; Windows/AV compatibility; Ctrl+Alt separate rects

### Fixed
- Flashcard study freeze: render cache queue used `PopAt(0)` (memmove on every tile) -> O(N^2) repaint cost on every rating. Paint now reads with index + sorts unprocessed part only (same order). Rating 13-46 ms instead of UI thread freeze.
- Page navigation stutter with many cards: flashcard masks now paint only visible pages; speculative pre-render capped at 2 concurrent requests (no longer delays visible page). Avg nav ~32 ms (was ~65 ms).
- Home "last read" card: now shows ACTIVE TAB book (was book with most accumulated time when two books read in same second).
- Flashcards > Filter: bookmark (chapter) list populated on FIRST open (no Bookmarks panel prerequisite); each bookmark listed once (old code walked tree twice -> per-sibling dupes, exploded on deep outlines; also broke study+filter together).
- Auto-update "Install" button disappearance: silent install failure (unsaved changes, installer not downloaded, launch fail) now falls back to confirmation dialog instead of no-op; silent install relaunches app; self-update no longer needs helper .bat (unsigned portable replaces itself directly).
- Windows/AV compatibility: app no longer deletes own Mark-of-the-Web at startup; no longer spawns cmd.exe for self-update (triggered Bitdefender ATC.Malicious, SuspiciousBehavior.54B279F15D24AA5B). README (EN+ES) + SECURITY.md document false-positive, SHA-256 check, signing options.
- Ctrl+Alt accumulation: drawing several rectangles now paints as separate rects (were merged into one union).

### Added
- Home responsive layout: wide window -> study history beside heatmap; below half-screen -> stacks (study history -> heatmap -> recent books); study history never collapses/disappears.
- Home heatmap readable: months alternate peru/silver in FILL (was border only); week spanning two months no longer mis-colors 36 days (border follows real month per day); cells square with more spacing; TODAY always white; narrow window shows most RECENT weeks (not 53 cols squeezed to 5px).
- Home hover highlight fades 0.5 s after mouse leaves (was permanent); no flicker while moving inside field.
- Flashcard study perf branch: per-book study file written on worker thread; bookkeeping deferred out of rating path; study-state index binary-searched; paint touches only visible pages; Lista panel updates incrementally.

### Changed
- Autoscroll: button stays usable while flashcard toolbar open (was greyed); study mode stops autoscroll + disables button until study exit.
- Build/CI: clean Release build script (`cmd/build-release.ts`); guard against tests polluting tracked repo files; GitHub Actions checkout bump to v7; flashcard/perf FLOW map updates; subagent handoff contract (R1..R7).

### Verified
- Clean Release build x64 (the release binary): 0 errors; `out/rel64/TumatraPDF.exe` -> 11,276,288 bytes, sha256 `43B9FC13C42F3AAD50E9EDCB9C38D86E0D0AEBC2BE7D049D5A0B3DAFA6C9E4A8`, ProductVersion 1.0.22.
- The 1.0.22 binary starts, runs 12 s, Bitdefender quarantine delta 0, Windows Error Reporting crash events delta 0, no Zone.Identifier stream.
- The sha256 above is the LOCAL build; the published hashes are in the release notes (CI builds its own binary).
- Probe `ad-hoc-flashcard-study-freeze`: 3/3 runs pass (agent ran 5/5), 10 cards rated, 13-46 ms/rating, ping 5-22 ms.
- Probe `ad-hoc-flashcard-nav-stutter`: avg 32.5 ms and 31.6 ms over 400 navigations.
- Probe `ad-hoc-home-geometry`: 8 widths pass; `ad-hoc-home-verify`: pass; `ad-hoc-flashcard-filter-bookmarks`: 5 bookmarks, 0 dupes, 3 open/close cycles stable, study+filter alive.
- `bun scripts/flow-sync.ts` passes; graphviz renders flashcard maps.
- User validated visual result on portable build.

### Notes
- Code signing documented in SECURITY.md and DEFERRED (SignPath Community = free path but needs application).
- Release build MUST use `bun cmd/build-release.ts` (stale incremental build can crash at startup).
- Debug build can carry stale MSVC PCH after header change (delete `out/<cfg>/obj/**/*.pch`).

## 2026-11-24 (s81) — Autoscroll stays usable with the flashcard toolbar; study mode stops it; CI action bump

### Fixed
- Toolbar.cpp IsCmdEnabled: split the shared `case CmdViewportCropToggle / CmdAutoScrollToggle` so the two commands have independent conditions. CmdViewportCropToggle keeps clashing with arch tools AND flashcards (unchanged). CmdAutoScrollToggle now clashes only with arch tools; flashcard toolbar open no longer disables the autoscroll button/command; STUDY mode (`win->flashcard.studyMode`) disables it.
- Commands_Flashcard.cpp CmdFlashcardStudy: entering study mode stops a running autoscroll (`if (win->autoScroll.active) AutoScrollToggle(win)` — guarded so a stopped autoscroll is not started); leaving study mode makes the command/button available again.
- Commands_Flashcard.cpp CmdFlashcardStudy: calls `ToolbarUpdateStateForWindow(win, true)` so the main toolbar autoscroll button enable state refreshes immediately on study toggle.
- Audit: CmdFlashcardToggle does NOT stop/pause autoscroll (no AutoScrollSetHoldActive / AutoScrollPauseForFocus / direct stop) — autoscroll already usable with the flashcard toolbar open; nothing removed. HOLD interaction (`AutoScrollSetHoldActive`) untouched.
- Exposed `IsCmdEnabled` (Toolbar.h/Toolbar.cpp) and added `TestCommandEnabled <cmdId>` control-pipe command (cmd/control.ts + src/SumatraControl.cpp, enum 84 both sides, no orphan value) to report a command's enabled state.

### Changed
- .github/workflows/{readme-sync,codeql,static-analysis,fuzz,release,tests}.yml: `actions/checkout@v4` -> `actions/checkout@v7` (node24 runtime, clears the deprecation annotation). oven-sh/setup-bun@v2 untouched. flow-sync.yml not present on main (6 files bumped).

### Verified
- `bun cmd/build.ts` green.
- Control-pipe: flashcard toolbar ON -> autoscroll command ENABLED; study mode ON -> DISABLED + autoscroll NOT running; study mode OFF -> ENABLED again (before/after outputs in the session).
- All workflow YAML parse cleanly.

### Notes
- CI annotation disappearance verified by the architect after the push (workflows run on GitHub, not locally).
- HOLD behaviour untouched.

## 2026-11-23 (s80) — Flashcard study perf: save worker, flush ordering, hermetic invariant test

### Added
- Async per-book study JSON write: worker thread `FlashcardSaveWorker` (`THREAD_PRIORITY_BELOW_NORMAL`) following prewarm pattern. UI thread resolves path, snapshots states vec, enqueues under `gFcSaveLock`; coalescing per path keeps newest snapshot. PROFILE write stays on UI thread: `gProfile` (ProfileData.cpp) has no locking (no CRITICAL_SECTION/mutex in that file) — mutated and written directly.
- `TestFlashcardFlush` control command (id 88) to force bookkeeping flush.
- `FlashcardStudyVerifySorted` runtime gate (debug builds only) at 4 boundaries of sorted-state invariant (load, parse, prune, insert).
- Hermetic E2E `tests/ad-hoc-fc-invariant-hermetic.ts`: copies fixture into `tests/tmp/`, kills app instead of graceful quit, asserts study JSON on disk (one entry per rated key, rating/interval match, keys ascending, no dupes) plus fixture unchanged.

### Fixed
- Close tab losing pending SM-2 state: `CloseTab` flushed AFTER save prompt; declining save dropped it (study state lives in per-book JSON, not PDF). Save-only flush (no prune) runs BEFORE `MaybeSaveAnnotations`; full flush (prune+save+sidebar) only after tab may close; same split in `CloseAllDocumentTabsForUnsync` and `CloseWindow`. Prune must not run when save declined (deleted-but-restored card would lose state).
- Sorted-by-key invariant broken on load path: `FlashcardStudyLoad` sorted at end but EARLY RETURN for profile-only adopt (history exists only in profile store — cross-machine sync case) -> unsorted doc -> binary search missed states -> SM-2 progress looked "new" and duplicate key could be written into JSON. Every producer now sorts.
- Async write wrote EMPTY `docName`/`docPath` (commit comment claimed unneeded — false): Config window lists books by name, moved/renamed sync markers read path, `FlashcardStudyAdoptByName` matches by base name so moved book's history could never be adopted. Snapshot now carries both (resolved on UI thread) and ONE shared writer `FlashcardStudyWriteJsonToFile` serves sync and async path so format cannot drift.
- Worker exit path broken: join enqueued empty sentinel job (worker wrote it as `fopen(".tmp")`, stray file) and never set stop flag -> worker never exited, join ALWAYS timed out after 3 s. Join now sets stop flag under lock, signals, worker drains queue before breaking.
- "will retry on next flush" log was false: UI cleared `kFcDirtySave` right after enqueueing, failed write dropped silently. Worker now retries up to 3 times with 100 ms backoff and logs final give-up.
- String ownership: job path released with `::free`; `str::Dup(Str)` released with `str::Free`.

### Changed
- Render queue priority: `PageRenderRequest::speculative` (default false = visible). `GetNextRequest` serves visible request before speculative one (queue LIFO, prewarm tile could be served before tile of card on screen); full queue evicts speculative entry first.
- Lista panel updates ONE row on rating (`FlashcardSidebarUpdateIncremental`) instead of rebuilding list; falls back to full rebuild (logged) when row cannot be identified.
- Per-page text cache for list labels lives one populate only; incremental update does not extract text.

### Verified
- Keypress->paint probe measures exactly one paint per rating (arm/consume): old gate used counter reset inside unrelated paint-cost log branch, fired every 240th paint, never saw first paint after rating. Probe also uses `PhysicalToVirtualForRect` (Navigate helper) — `PhysicalToVirtualPage` picks wrong page for card in RIGHT column under Two Columns v2.
- Sync prefetch has own count (floor 1, max tier cap) so no longer collapses to 0 with prewarm budget — budget 0 when nav EWMA > 400 ms or tier cap 0, used to gate BOTH sync prefetch and async prewarm (mechanism disabled itself exactly when needed). EWMA now measures critical path.
- Dead drain loop in `FlashcardPrewarmInvalidate` removed (condition false as soon as deadline passed); epoch filter already discards stale jobs.
- Complexity claims checked vs code: state lookup O(log S) (sorted vec + binary search), prune O(C log C + S) (merge pass over sorted card keys, repo `VecSort`), sidebar and toolbar count O(C log S) per card.

### Notes
- Branch not merged into main: user validates card-to-card jump with `[fc][perf] rate: ...` / `[fc][perf] paint: keypress->paint=...` logs first.
- Sync `FlashcardStudySave` still used by delete and adopt/rename paths (they write synchronously).

### Added (paint perf phase)
- Per-tab page index `FlashcardTabState::cardIndicesByPage` (physical page -> card indices, primary + extraPages) built by `FlashcardBuildPageIndex()` (Flashcard.cpp:305).
- `PaintFlashcardMasks` (Canvas.cpp:4163) computes visible PHYSICAL pages from `dm->pagesInfo` (visibleRatio > 0), includes active card and Lista-selected card pages even when off-screen, iterates only indexed cards, falls back to full iteration when index missing.
- Pure helper `FlashcardVisibleCardIndices()` (Flashcard.cpp:350) shares logic for testing and paint path.
- Index rebuild hooked at 6 card-assignment sites: Commands_Flashcard.cpp:185/216/1662, FlashcardToolbar.cpp:1690/1992, SumatraControl.cpp:1919.
- Control command `TestFlashcardVisibleCardIndices` (id 89) runs hermetic index tests via control pipe.

### Fixed
- Cross-page cloze on-screen extraPage painted when primary page off-screen (index maps extraPages).
- Active study card and Lista-selected card painted even when page outside viewport.
- Two Columns v2 right-column cards: visible test uses `dm->pagesInfo` virtual pages (each has physicalPageNo + cropColumn) -> physical page marked visible if ANY of its virtual pages has visibleRatio > 0.

### Verified
- Build green (0 errors, 0 warnings).
- Control test: `bun cmd/control.ts out\dbg64\TumatraPDF.exe test-flashcard-visible-card-indices` -> "OK all FlashcardBuildPageIndex tests passed".
- Unit tests pass (`bun cmd/run-unit-tests.ts -dbg`).
- FAIL->PASS regression check: temporarily disabled `FlashcardBuildPageIndex` -> test FAILs ("FAIL test1: expected index size 51, got 0"), restored -> PASS.

### Complexity before -> after
- Before: `PaintFlashcardMasks` iterated ALL cards O(C x Q) where C = total cards, Q = avg quads/card (every rect converted + viewport cull).
- After: candidate set = cards whose physical page has a visible virtual page. Index lookup O(1) per visible page, paint O(cards_on_visible_pages x Q). Worst case (all pages visible) = O(C x Q), typical autoscroll = O(cards_on_visible_pages x Q). Index build O(C) amortized over loads.
- Real complexity: O(cards whose physical page has a visible virtual page). NOT O(1).

### NOT VERIFIED
- Before/after paint-cost numbers from `[fc] paint: %d cards, %d rects drawn, %.3f ms/paint avg` instrumentation (Canvas.cpp:4246) — user manual run required.
- TC2 right-column case verified by code inspection (visibleRatio per virtual page); not validated on real Two Columns v2 document.
- Active-card-outside-viewport case verified by code (explicit inclusion); not validated on real scroll-away-then-rate flow.
- Paint path optimization verified by index helper test; no test directly asserts `PaintFlashcardMasks` draws identical rects before/after.

## 2026-11-23 (s79) — Ctrl+Alt rectangle accumulation + test fixture guard

### Fixed
- Ctrl+Alt+drag painted ONE union rectangle instead of the accumulated rectangles: `PaintSelection` (Selection.cpp) took the union branch (`GetRectangularSelectionScreenRect` -> `bounds.Union(r)`) whenever `IsRectangularSelection(win)` was true, and that predicate is true for a rectangle selection (no text glyphs -> `textSelection->result.len == 0`). The code comment already described the intended behaviour ("for accumulation, keep the per-page rects") — the code did the opposite.
- `IsRectangularSelection()` now returns false while a Ctrl+Alt accumulation is active -> the per-rect paint path is used and the phantom move/resize handles and the resize cursor disappear (they used the same union). An explicit `isAccum` flag keeps the accumulated rectangles painted with sharp borders (after the drag the live selection is rebuilt from the accumulation, so `paintingAccum` alone was false). Plain Ctrl+drag unchanged.
- The flashcard itself was already correct (N rects -> N quads via `CmdFlashcardAdd` -> `FlashcardCreateHighlightAnnot` -> `SetQuadPointsAsRect`); only the on-screen overlay and the handles were wrong.
- The guard's CI check steps were broken twice and both are fixed: the guard step invoked `bun scripts/test-fixture-guard.ts` with `working-directory: sumatrapdf-src` while the script lives at the repo root (bun failed, `continue-on-error` swallowed it, no sentinel, the job passed silently) -> now `../scripts/test-fixture-guard.ts`; the check steps were bash (`[ -f`, `grep -q`) on a windows-2022 runner whose default shell is pwsh -> now PowerShell (`Test-Path` / `Select-String` on `$env:RUNNER_TEMP`).

### Added
- `sumatrapdf-src/tests/util.ts`: `copyFixtureToTmp()` (copies a tracked fixture into `tests/tmp/`), `trackedFileFingerprint()`, `assertTrackedFileUnchanged()`.
- `scripts/test-fixture-guard.ts`: runs a command, snapshots the tracked state before/after (status AND the `git diff HEAD` text, so a further edit to an already-modified file is caught), exits 1 with the `FIXTURE-GUARD-POLLUTION` marker plus optional `--sentinel`/`--log`; `--self-test` runs 5 cases (5 passed).
- `.github/workflows/tests.yml`: runs the unit suite through the guard on windows-2022; two `if: always()` steps fail the job on real pollution only.
- `AGENTS.md` §6: three rules — a test never opens a tracked repo file for writing; an annotation-creating test copies the fixture and never depends on a graceful quit (the modal "save changes?" prompt blocks the message loop); the fixture guard must pass.

### Changed
- 5 tests that create annotations now copy the fixture (`ad-hoc-flashcard-{cloze,crosspage,filter,list,multiselect}.ts`); the read-only tests were left alone.

### Verified
- The first CI run of the guard (run 45382257747) FAILED, and the cause was NOT pollution: the log-marker check step had `-and` inside the `Test-Path` argument list, which PowerShell parses as a parameter ("A parameter cannot be found that matches parameter name 'and'"). The guard step itself passed (script path fix works, the suite ran through the guard) and the sentinel check passed (no pollution in the unit suite). The check now wraps each cmdlet call in its own parentheses; verified by running the EXACT step body under pwsh: clean -> exit 0 (silent), polluted -> `::error::` + exit 1, log-without-marker -> exit 0.
- Root cause of the class: a test that modifies a document can write into the repo — the flashcard E2E opened the tracked fixture `sumatrapdf-src/ext/a-zlib/zlib.3.pdf` in place and the save prompt wrote the test card into it (28222 -> 34803 bytes, `/Subtype/Highlight` + `TumatraPDF-Flashcard`). The tracked file was restored (the polluted copy is kept in `git stash@{0}`); no study data had leaked (the study dir was empty).

### Notes
- The Ctrl+Alt fix has NO automated fail-then-pass check (a real Ctrl+Alt drag cannot be synthesised through the control pipe) — pending the user's visual validation.
- 48+ test references pointed at tracked `ext/` paths; only the 5 annotation-creating tests were migrated in this round, the read-only ones are safe by construction.
- Lesson recorded: verifying the pieces of an expression is not verifying the expression — run the exact string the runner executes, in the runner's shell.

## 2026-11-23 (s78) — Auto-update setting + silent install and restart

### Added
- `AutoUpdate` advanced setting (default ON) in `cmd/gen-settings.ts`, `src/Settings.h`, `docs/md/Advanced-options-settings.md`.
- Silent auto-install path in `src/UpdateCheck.cpp`: when `AutoUpdate=ON` and an automatic daily check finds an update, the app downloads it and installs silently with `-silent -fast-install` flags, then restarts — no dialog shown.
- Unsaved-work guard: `AnyOpenDocumentHasUnsavedChanges()` checks all open tabs via `EngineHasUnsavedAnnotations` (EngineCreate.cpp:353) -> `EngineMupdfHasUnsavedAnnotations` (EngineMupdf.cpp:7951); if any doc has unsaved annotations/outline/page-geometry edits, falls back to the confirmation dialog.

### Changed
- `StartInstallerAutoUpgrade()` now takes `silent` param; for installed unsigned builds it runs `"installer.exe -install -silent -fast-install"`; for portable it uses existing `SelfUpdateViaBatch` (already silent + self-restart).
- Automatic check path (`NotifyUserOfUpdate`) now installs silently when `gGlobalPrefs->autoUpdate && updateCheckType==Automatic` and no unsaved changes; otherwise shows themed dialog (existing behavior).
- Pre-release "Download and update" notification still uses `gUpdateAutoInstall` (non-silent).
- Moved "You're not expected to change those manually" comment in `cmd/gen-settings.ts` to after `AutoUpdate` so the setting appears in the Advanced Settings dialog (was hidden by blanket `internalRest` flag).

### Fixed
- Silent installed-app path now includes `-fast-install` so installer auto-starts install, relaunches app, and exits without showing any window (Installer.cpp:1543-1546, 1997, 1721).

### Verified
- `bun cmd/gen-code.ts` regenerates `src/Settings.h` with `bool autoUpdate` (line 1083) and names blob entry `AutoUpdate` (line 2010); FieldInfo has 3 elements (no `internal=true`), so setting appears in Advanced Settings dialog.
- `docs/md/Advanced-options-settings.md` contains `AutoUpdate = true` (line 1149).
- Silent installed-app command line: before `installer.exe -install -silent` -> after `installer.exe -install -silent -fast-install` (UpdateCheck.cpp:304-307).
- No-window verification: with `-silent -fast-install`, installer creates no HWND (Installer.cpp:2367-2370 runs `InstallerThread` directly), install starts automatically (Installer.cpp:1997), no "Install" button (Installer.cpp:1721), on success `StartSumatra()` + `ExitProcess(0)` (Installer.cpp:1543-1546).
- UAC caveat: per-machine install under Program Files triggers UAC prompt (Installer.cpp:2337-2348 `requiresSilentElevation = silent || fastInstall || runInstallNow`); this is a Windows requirement, not a bug.
- Failure-path behaviour: silent install failure returns error code from `RunInstaller()`; installer logs failure to `%TEMP%\sumatra-install-log.txt`; app has already exited (no notification possible) — minimal acceptable behaviour documented.
- Automatic vs user-initiated distinction survives async download: `updateCheckType` threaded through `DownloadUpdateAsyncData` (UpdateCheck.cpp:756, 760, 772, 1129) -> `NotifyUserOfUpdate`; user-initiated checks still show themed dialog (never silent install); `gUpdateAutoInstall` pre-release path unchanged (non-silent, UpdateCheck.cpp:408-418).
- Setting default-true proof: FieldInfo value=`true` (Settings.h:1986), deserialization uses `field.value != 0` (SettingsUtil.cpp:322) -> missing key deserializes to `true`.
- Setting UI surface: Advanced Settings dialog only (Settings > Advanced); not in main Options dialog.
- Build: 0 errors, 83 s, ProductVersion 1.0.21.

### Notes
- Portable self-update (`SelfUpdateViaBatch`) already restarts the app — no change needed there.
- The `-silent` flag is parsed in `Flags.cpp` and propagated in `Installer.cpp` (lines 1293-1294, 2205, 2287, 2339) for UAC elevation and installer UI suppression.
- 50 ms caveat: unsaved-work check iterates all tabs; negligible overhead.
- Migration: new global-prefs key appended at end of struct + names blob; deserialization fills default `true` automatically — no manual migration step needed.

## 2026-11-23 (s77) — Windows 11 compatibility: SmartScreen warning, self-unblock, release checksums

### Added
- `README.md` + `README.es.md`: "Windows may ask before running" subsection (unsigned build + Mark of the Web + SmartScreen; More info -> Run anyway / Properties -> Unblock / `Unblock-File`; installed warns once, portable from the second launch).
- `SECURITY.md`: "Windows warnings and unsigned builds" section (unsigned builds, public source, SHA-256 in the release notes, what to do on a Defender quarantine).
- release.yml: CI step appends the SHA-256 of both assets to the release notes (append, never replace; `gh release edit` with the GITHUB_TOKEN; step cannot fail the release).

### Changed
- Startup: the portable removes the Zone.Identifier of its own executable (best-effort, user-writable locations only, silent) -> SmartScreen stops warning on every launch.
- Update flow: the downloaded installer has its Mark of the Web removed before it is launched (defensive; the download path does not add it today).

### Verified
- Published v1.0.21 assets are `NotSigned` (Get-AuthenticodeSignature) -> the warning is expected for an unsigned build, not a malware verdict.
- The app makes exactly one network request of its own (the update check against raw.githubusercontent.com); no crash upload, no analytics.

### Notes
- Signing the binaries with a code-signing certificate would remove the SmartScreen warning entirely; that needs a certificate decision (and its cost) — not done.
- Runtime/visual validation of the startup self-unblock pending user manual test.

## 2026-11-23 (s76) — Flashcard study perf: phase timing + deferred bookkeeping + state index + prune merge + flush coverage + P1 invariant fix + render-queue priority + incremental sidebar update

### Added
- Per-phase timing of every rating (`[fc][perf] rate: scan=.. stats=.. toolbar=.. sidebarFlag=.. nav=.. rerender=.. total=..` plus cards/states counts) — critical-path only. Deferred work logged on its own line: `[fc][perf] deferred: prune=..ms save=..ms sidebar=..ms tabs=.. total=..ms` — shows what was moved out of the critical path and its real cost.
- `pendingSaveTabs` vector in `FlashcardState` — remembers the owning tab of each rated card explicitly, avoiding data loss in global sessions (cross-document) and when the user presses Next between ratings. Duplicate consecutive ratings in the same tab are suppressed (one entry per tab per flush cycle).
- Deferred bookkeeping: prune + per-book JSON/profile save + sidebar rebuild moved out of the critical path into a coalesced block (>= 50 ms) serviced by the existing study timer; explicit flush points (session stop, study toggle off, session timer expiry, tab switch, tab close, app exit) prevent losing pending work.
- Per-tab prune/save in deferred block: iterates `pendingSaveTabs`, verifies each tab is still in this window, runs prune-before-save per tab. Sidebar stays window-level.
- Keypress->paint probe in `PaintFlashcardMasks` (Canvas.cpp:4252-4281): arm/consume contract — `rateProbePending` armed in `HandleFlashcardRate` and `CmdFlashcardDeleteActive`; probe fires ONCE on the first paint after a rating where the active card's page is served from a cached tile (using `PhysicalToVirtualForRect` for correct Two Columns v2 column). Logs `[fc][perf] paint: keypress->paint=%dms card=%d page=%d cached=%d` or `OUTLIER` if delta > 5000 ms; flag consumed immediately.
- `FlashcardPrefetchCount(win)` (Commands_Flashcard.cpp:497-507): sync page-info prefetch gets its own count (1..tierCap, max 2) that never collapses to 0 — independent of EWMA/memory pressure. The tile prewarm still uses `FlashcardPrewarmBudget` (can be 0).
- Unsynced/tab-close data-integrity gap: `CloseAllDocumentTabsForUnsync` (SumatraPDF.cpp:5119) and `MaybeMigrateTab` (Tabs.cpp:244) now call `FlashcardFlushBookkeeping(win)` BEFORE any tab is torn down. Audit of all tab-removal paths:
  - CloseTab (SumatraPDF.cpp:5041) ✓ flush
  - CloseWindow (SumatraPDF.cpp:5216) ✓ flush
  - CloseAllDocumentTabsForUnsync (SumatraPDF.cpp:5119) ✓ flush (ADDED)
  - MaybeMigrateTab / tab drag-out (Tabs.cpp:266) ✓ flush (ADDED)
  - CloseTabsToTheRight/Left (SumatraPDF.cpp:10767) → CloseTab ✓
  - CommandPalette/CloseTab (CommandPalette.cpp:272) → CloseTab ✓
  - Favorites/CloseTab (Favorites.cpp:1083,1498) → CloseTab ✓
  - NavFilesInFolder/CloseTab (NavFilesInFolder.cpp:476) → CloseTab ✓
  - Menu/CloseTab (Menu.cpp:2040) → CloseTab ✓
  - TabGroupsManage/CloseTab (TabGroupsManage.cpp:696) → CloseTab ✓
  - TabsCtrl/CloseTab (TabsCtrl.cpp:571,736,796) → CloseTab ✓
  - RemoveTab (Tabs.cpp:159) called AFTER flush by callers; no direct external callers.
- Sorted `Vec<StateEntry>` by key for binary search lookups (repo-native, no STL). Invariant: states sorted after `FlashcardStudyLoad`, after every insert/update in `HandleFlashcardRate`, and after `FlashcardStudyPruneStates` (merge pass keeps order). Duplicates: never created (insert at sorted position), first wins if JSON has dupes.
- `FlashcardStateFindIdx` helper (Flashcard.cpp:94) — binary search returns index or insert position.
- `FlashcardStudySortStates` (Flashcard.cpp:1207) — insertion sort after JSON parse (typical < 500 entries, fast).
- `FlashcardStudyPruneStates` rewritten as merge pass O(S + C log C): builds sorted card-keys copy, single linear pass with two pointers (states sorted, cardKeys sorted) → keeps matching states in place.
- Config window flush: `FlashcardConfigDialog` (FlashcardToolbar.cpp:1564) calls `FlashcardFlushBookkeeping(win)` before `ConfigReloadList` so per-book counts are fresh.
- **P1 data-loss bug**: `FlashcardStudyLoad` early-return path (profile-only adopt at :1276) returned WITHOUT sorting → binary search missed states → SM-2 progress lost + duplicate keys written. FIX: `FlashcardStudySortStates(doc.states)` now called on ALL return paths (file, legacy fallback, adopt-by-name, profile-only).
- **Duplicate-key rule**: `FlashcardStudySortStates` (VecSort + dedup) keeps the entry with the newest `lastReviewedAt`; older duplicate dropped silently. Applied at load, parse, and prune.
- **Profile side comment**: `ProfileData.cpp:2008` — profile states stored in insertion order (never binary-searched); invariant enforced at consumer (`FlashcardStudyLoad`), not in profile.
- `FlashcardU64Cmp` / `FlashcardStudyStateKeyCmp` — file-scope comparators for `VecSort` (repo-native qsort).
- `FlashcardStudySortStates` now uses `VecSort` (O(N log N)) + adjacent dedup by `lastReviewedAt` desc for equal keys.
- `FlashcardStudyPruneStates` card-keys sort uses `VecSort` (O(C log C)) instead of insertion sort (O(C^2)).
- `FlashcardStudyParseJsonInto` ensures sorted invariant after parse (covers Config recover-backup, import, resync paths).
- `Commands_Flashcard.cpp:289` linear scan → `FlashcardStateFindIdx` binary search.
- `PageRenderRequest::speculative` field (RenderCache.h:96) — bool flag default false (visible). Existing callers keep visible priority by default.
- `RenderCache::RequestRenderingInternal` (RenderCache.cpp:629) — unified implementation with `speculative` parameter.
- `RenderCache::RequestRenderingSpeculative` (RenderCache.h:235, RenderCache.cpp:624) — public API for speculative callers (flashcard prewarm/prefetch).
- `GetNextRequest` priority logic (RenderCache.cpp:863-880) — scans from tail for first NON-speculative request; if none, takes speculative tail. Logs `[fc][perf] renderq: serving visible over %d speculative` when a visible request jumps ahead.
- Queue-full eviction rule (RenderCache.cpp:802-812) — evicts a SPECULATIVE request if any exists (first speculative from head); only if all are visible, drops oldest (legacy behavior). Logs `[fc][perf] renderq: queue full, evicted idx=%d (speculative=%d)`.
- Duplicate-request rule (RenderCache.cpp:670-707) — if existing is visible and incoming speculative, keep visible at its priority (no move to tail, no flag overwrite). If existing speculative, incoming visible overwrites flag to visible. If both same class, move to tail (newest wins).
- Queue-state logging (RenderCache.cpp:819-824) — logs `[fc][perf] renderq: queued spec=%d vis=%d (pageNo=%d spec=%d)` on each enqueue (cheap, only on state change).
- Two speculative call sites marked (Commands_Flashcard.cpp:544, 738):
  - Prewarm worker tile request (`FlashcardPrewarmWorker` → `RequestRenderingSpeculative`)
  - Prefetch render upcoming (`FlashcardPrefetchRenderUpcoming` → `RequestRenderingSpeculative`)
- `FlashcardState::listaIncrementalCardIdx` / `listaIncrementalCardTab` (MainWindow.h:609-610) — tracks which card's state changed (rating), set by `HandleFlashcardRate` at curIdx before advance, cleared on deferred flush.
- `FlashcardSidebarUpdateIncremental` (FlashcardSidebar.h:17, FlashcardSidebar.cpp:732-812) — updates ONE row in owner-drawn listbox (`LBS_OWNERDRAWFIXED`) without `LB_RESETCONTENT` or text re-extraction. Updates `listaRows[rowIdx]` (rating/interval/nextReviewAt) from study state, invalidates only that row's rect via `InvalidateRect`.
- Incremental preconditions (checked in order, bail to full rebuild if any fails):
  1. Panel exists + visible (`hwndListaView` + `uiState.fcListVisible`)
  2. Rated card's tab == current tab (Lista only shows current tab)
  3. studyOrderIdx valid + maps to card in current tab
  4. `listaRows` count == listbox item count (no structural add/remove)
  5. Listbox item data at rowIdx matches cardIdx (index alignment)
- Static label (masked text) NOT re-extracted on incremental update — the label string lives in the listbox item and is unchanged when only Due/Status change. PageText cache used only in full rebuild (`FlashcardSidebarPopulate`).
- Fallback to `FlashcardSidebarPopulate` when preconditions fail (logs `[fc][perf] sidebar: incremental skipped (reason)` + `[fc][perf] sidebar: falling back to full rebuild`).
- Deferred flush integration (Commands_Flashcard.cpp:1092-1103) — attempts incremental first via tracked `listaIncrementalCardIdx/Tab`; clears tracking after use (whether incremental or full).

### Changed
- `HandleFlashcardRate` order: state update + SM-2 + advance + prefetch + navigate + prewarm + stats + toolbar count + repaint happen first; prune -> save -> sidebar follow asynchronously.
- Nav phase now correctly measures prefetch + navigate + prewarm (previously measured ~0 ms).
- `FlashcardRunDeferredBookkeeping` now iterates `pendingSaveTabs` instead of deriving the tab from `currentCardIdx - 1` (which was wrong for global sessions and Next-between-ratings).
- `FlashcardFlushBookkeeping` no longer derives the tab; runs the deferred block directly with remembered tabs.
- `FlashcardStudyTick` periodic flush calls the new `FlashcardRunDeferredBookkeeping` without a derived tab.
- Removed all "thinking-out-loud" comments from `HandleFlashcardRate`; replaced with one factual comment.
- Sync prefetch calls (4 sites: FlashcardApplyStudyOrder, HandleFlashcardRate, CmdFlashcardStudy ON, CmdFlashcardDeleteActive) now use `FlashcardPrefetchCount` instead of `FlashcardPrewarmBudget` — at least the immediate next card is always warmed.
- EWMA (`cardSwitchMsEwma`) now measures the CRITICAL path duration (`totalMs` from the rate: log) instead of just `FlashcardNavigateToEntryInner` time. Updated in `HandleFlashcardRate` (Commands_Flashcard.cpp:1027-1032) and `CmdFlashcardDeleteActive` (Commands_Flashcard.cpp:1548-1552). Smoothing 0.7/0.3 and 2000 ms clamp preserved.
- `HandleFlashcardRate` (Commands_Flashcard.cpp:909): state lookup + write-back via binary search (was 2 linear scans O(S)). Insert at sorted position maintains invariant.
- `FlashcardSidebarPopulate` (FlashcardSidebar.cpp:687): state lookup via binary search O(log S) per card (was O(S) linear).
- `FlashcardToolbarUpdateCount` (FlashcardToolbar.cpp:639): state lookup via binary search O(log S) per card.
- `HandleFlashcardRate` (Commands_Flashcard.cpp:963-967) — sets `listaIncrementalCardIdx = curIdx`, `listaIncrementalCardTab = e.tab` before marking `kFcDirtySidebar`.
- `CmdFlashcardDeleteActive` (Commands_Flashcard.cpp:1531-1534) — marks `kFcDirtySidebar` without setting incremental tracker (deletion shifts indices → full rebuild required).
- `FlashcardRunDeferredBookkeeping` (Commands_Flashcard.cpp:1090-1103) — calls `FlashcardSidebarUpdateIncremental` with tracked indices; falls back to full populate on failure.

### Fixed
- Dead drain loop in `FlashcardPrewarmInvalidate` (Commands_Flashcard.cpp:549-556) removed — the condition `(int)(GetTickCount() - deadline) < 0` was false immediately (deadline = now+50). The epoch filter already discards stale jobs; no wait needed. Comment corrected to match actual behavior.

### Verified
- Build green (0 errors, 0 warnings, clang-formatted).
- SM-2 content, 60 s per-card cap, owning-tab attribution and prune-before-save order unchanged (read back the diff).
- Two new log lines per rating: critical-path line + deferred line (when deferred work runs).
- Keypress->paint probe logs when cached tile paints the new card.
- Prefetch floor ensures at least 1 card parsed before navigation even on weak machines.
- EWMA now reacts to real user-visible cost.
- `MAX_PAGE_REQUESTS` (8) never exceeded: eviction shifts + decrements `requestCount`; duplicate handling swaps in place without growing array.
- `ReportIf(requestCount > MAX_PAGE_REQUESTS)` not fired (verified by inspection of all modification sites).
- Incremental path preserves `LB_SETITEMDATA` mapping: rowIdx → cardIdx unchanged; owner-draw reads fresh `listaRows[rowIdx]` at paint time (Due countdown computed at draw).
- Both paths produce identical per-row state: incremental writes `rating/interval/nextReviewAt` to same `listaRows` slot that full rebuild would write; item data and count unchanged.
- JSON format unchanged (keys written in sorted order; object order irrelevant).
- Per-book counts in Config window now include pending ratings.
- All tab-removal paths audited; no path with MainWindow* available misses a flush.
- `FlashcardStudyPruneStates` returns dropped count; states remain sorted after merge pass.
- All producers sorted; profile side never binary-searched (comment added at ProfileData.cpp:2008).
- FlashcardPrewarmInvalidate drain loop dead: Flashcard.cpp:550 condition false immediately — no actual wait.
- Stale claims corrected: prewarmer implemented (not planned), tick uses wall-clock GetTickCount64 (not tick count), credits owning tab (not focused tab).
- Improvement options (O1..O7) await user review after external model analysis.
- External models' claims validated line by line: 12 confirmed / 3 refuted / 2 partial.
- Graphviz validation: both .dot files pass `dot -Tcanon` (exit 0) and `dot -Tsvg` (exit 0).

### Notes
- Up to 50 ms of study-state persistence can be lost if the process dies in that window; study-time credit is NOT deferred (it stays synchronous), so the statistics survive.
- The true keypress->paint latency probe is now IMPLEMENTED (was out of scope previous round).
- Budget split: sync prefetch floor=1..tierCap, async tile prewarm still gated by `FlashcardPrewarmBudget` (can be 0).
- Follow-ups (not implemented): key->state index (no STL: sorted Vec + binary search), async save worker, sidebar incremental update, render-queue priority.
- Config window study-doc read flush: lives in FlashcardToolbar.cpp (not allowed) — follow-up.
- Rate-limit logging: queue-state logs only on enqueue (state change), not per-frame. Serve-visible log only when visible jumps speculative.
- Incremental update avoids page text extraction entirely — critical for perf (page text extraction was O(P) per card per rating).
- Deletion uses full rebuild (indices shift); can optimize later if needed.

### Complexity before → after
- State lookup (rate): O(S) → O(log S)
- State insert (rate): O(S) append → O(S) shift at sorted pos (dominated by shift, but search is O(log S))
- Prune: O(S x C) → O(C log C + S) (merge pass)
- Sidebar populate: O(C x S) → O(C log S)
- Toolbar count: O(C x S) → O(C log S)

### True complexity (corrected):
- State lookup: O(S) → O(log S)
- State insert (rate): O(S) append → O(S) shift (search O(log S))
- **Prune: O(S × C) → O(C log C + S)** (VecSort + merge pass)
- Sidebar: O(C × S) → O(C log S)
- Toolbar count: O(C × S) → O(C log S)

### Binary-search edge-case table (FlashcardStateFindIdx):
| Input | Expected result |
|---|---|
| empty vec | -1, insert=0 |
| single element (match) | 0 |
| single element (miss, key < element) | -1, insert=0 |
| single element (miss, key > element) | -1, insert=1 |
| key smaller than all | -1, insert=0 |
| key larger than all | -1, insert=len |
| key in middle (found) | index |
| key in middle (miss) | -1, insert=position |

### Producer audit (every `tab->flashcard.studyDoc = ...` assignment):
| Producer | File:line | Sorted? | Fix |
|---|---|---|---|
| `FlashcardStudyLoad` (file/legacy/adopt) | Flashcard.cpp:1277 | ✓ | sort on all returns |
| `FlashcardStudyLoad` (profile-only) | Flashcard.cpp:1276 | ✓ | sort added |
| `FlashcardStudyParseJsonInto` | Flashcard.cpp:1202 | ✓ | sort after parse |
| `FlashcardStudyRecoverBackup` → `FlashcardStudyLoad` | FlashcardToolbar.cpp:1152 | ✓ | via FlashcardStudyLoad |
| `ProfileDataGetStudyDoc` → `FlashcardStudyLoad` | Flashcard.cpp:1276 | ✓ | sort in caller |
| `FlashcardStudyLoad` (adopt-by-name) | Flashcard.cpp:1277 | ✓ | sort after merge |

### NOT VERIFIED
- Before/after paint-cost numbers from `[fc] paint: %d cards, %d rects drawn, %.3f ms/paint avg` instrumentation (Canvas.cpp:4246) — user manual run required.
- TC2 right-column case verified by code inspection (visibleRatio per virtual page); not validated on real Two Columns v2 document.
- Active-card-outside-viewport case verified by code (explicit inclusion); not validated on real scroll-away-then-rate flow.
- Paint path optimization verified by index helper test; no test directly asserts `PaintFlashcardMasks` draws identical rects before/after.