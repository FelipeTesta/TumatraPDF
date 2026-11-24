# FLOW map repair plan

Status: 2026-11-23 (s71) · enforcement: `scripts/flow-sync.ts` + `.github/workflows/flow-sync.yml` · debt: **79 dangling refs across 22 maps**

Self-contained document. Share it as-is: it contains map names, reference counts and symbol names only — **no implementation code**.

---

## 1. What a FLOW map is

`FLOW/*.dot` are Graphviz **process** maps for this repo (a Windows C++ PDF reader, native Win32, ~550 source files). They answer "how does this mechanism work / how do I do this task", for agents and humans. They are **not** architecture diagrams — those come from tree-sitter on demand.

`FLOW/DEBUG/*.dot` are one-off investigation maps and are deliberately outside this plan and outside the check.

Map rules that already hold:

- one file per mechanism or workflow, plain Graphviz `digraph`, no rendering config;
- node labels carry the durable facts (who calls whom, timers, gotchas, invariants), colored by role (green = entry/user action, blue = mechanism, purple = render/output, yellow = warning, red = gotcha/failure, grey = config/deploy);
- **no line numbers** (444 `Foo.cpp:338` references were stripped in s71 — they rot in weeks);
- every edit re-stamps the map: `// dot-sync: verified=<date> refs=<hash of the referenced code>`.

## 2. How staleness is detected

For each map the checker extracts:

- **file references** — `Something.cpp` / `.h` / `.c` tokens;
- **symbol references** — CamelCase identifiers with 3+ humps, minus the map's own node ids and minus Win32 control class names.

Each is verified against the code index (`src/`, vendored `mupdf/` + `ext/`, and the `cmd/gen-*.ts` generators, because setting/command/flag names are declared there, not in the C++ tree). Tokens that legitimately belong to someone else (Win32, WebView2 COM, external tools) live in `FLOW/.sync-allow.txt`.

Result is a **ratchet**: `FLOW/.sync-baseline.json` records the debt that already existed, CI fails only on NEW staleness, and `--baseline` refuses to grow an entry. Debt can only shrink.

## 3. Debt snapshot

| | |
|---|---|
| Maps checked | 35 (`FLOW/DEBUG/` excluded) |
| Maps with debt | 22 |
| Dangling refs | 79 (75 symbols, 4 file attributions) |
| Already resolved (s71 instant batch) | setting-name casing: `AutoScrollSpeedMultiplier` (correct name — now indexed from the generators), `MarginTrimEnabled`, `ViewportCropV2Enabled` ×2, `TocBoxSubclassId` |

"Churn" below = commits in the last 60 days touching the source files that the map references. High churn + stale map = an agent reading it is actively misled.

## 4. Priority queue

Priority = (is the map read before acting) × (churn) × (debt). P0 = misleading today, P3 = cheap cleanup.

| # | Map | Prio | Stale | Churn 60d | Probable class (confirm) | Action |
|---|---|---|---|---|---|---|
| 1 | `command-system.dot` | P0 | 13 (4F + 9S) | 163 | B + C | Re-derive command dispatch: handler table now split across `Commands_*.cpp`; `MenuEnabler.cpp`/`ReadAloud.cpp`/`Commands_Nav.cpp`/`Commands_Crop.cpp` no longer exist; verify each `Cmd*` exists before citing |
| 2 | `tumatrapdf.dot` | P0 | 1 | 236 | D | Top-level entry map every session reads first. Confirm whether `FitSinglePage` still exists; delete the node or rename |
| 3 | `document-load-flow.dot` | P0 | 5 (1F + 4S) | 156 | B + C | `Settings.cpp` -> current owner (`AppSettings.cpp` / generated `Settings.h`); `MigrateGlobalPrefs`, `UpdateFileStateForTab` renamed |
| 4 | `settings-system.dot` | P0 | 4 (2F + 2S) | 84 | B + C | `Settings.cpp`, `AdvancedOptions.cpp` -> `AppSettings.cpp`, `AdvancedSettingsDialog.cpp`; same two renamed symbols |
| 5 | `tab-window-management.dot` | P1 | 8 | 95 | C | `TabGroupsSave/Load`, `AddTabToGroup`, `MoveGroupTabs`, `RestoreTabsFromHistory`, `FrameWndProc` — `TabGroupsManage.cpp` exists, so these were renamed, not deleted |
| 6 | `annotation-edit-flow.dot` | P1 | 7 | 138 | C + D | `SetAnnotationContents/Author/Color`, `GetAnnotationAtPoint`, `SkipAnnotationValidation`, `FzFindImageAtPos`, `UpdateFileStateForTab` |
| 7 | `toolbar-function-callgraph.dot` | P1 | 7 | 138 | C | `ToolbarSubclassSecondaryToolbar`, `ToggleMainToolbar`, `FrameWndProc`, `MenuBarWndProc`, `FlashcardWndProc`, `CmdShowToolbar`, `HandleCmdFlashcardToggle` |
| 8 | `search-dde-flow.dot` | P1 | 5 (1F + 4S) | 137 | B + C | `ReadAloud.cpp` -> `ReadAloudHighlight.cpp` / `ReadAloudPlaybackBar.cpp`; `CmdFindWindow`, `HitTestLink`, `QueuedOpenCmd`, `UpdateNavHistory` |
| 9 | `homepage.dot` | P1 | 4 | 115 | C | `HomePageWnd`, `CollectHomePageFiles`, `HomeLayoutCacheMatches`, `HomeLayoutCacheFilesMatch` — `HomePage.cpp` exists |
| 10 | `toolbar-system.dot` | P1 | 3 | 122 | C | `ToolbarSubclassSecondaryToolbar`, `ToolbarPackStateIndex`, `ThemeHighlightColor` |
| 11 | `theme-darkmode.dot` | P2 | 3 | 63 | C + D | `ThemeHighlightColor`, `ToolbarSubclassSecondaryToolbar`; decide whether `OnEraseBkgnd` is a name or a convention (allowlist if the latter) |
| 12 | `toolbar-user-interactions.dot` | P2 | 2 | 84 | C | `TbDeleteButton`, `ToolbarSubclassSecondaryToolbar` |
| 13 | `toolbar-critical-ordering.dot` | P2 | 1 | 99 | C | `ToolbarSubclassSecondaryToolbar` |
| 14 | `tab-system.dot` | P2 | 1 | 62 | D | `OpenNewWindow` |
| 15 | `toolbar-per-toolbar-state.dot` | P2 | 2 | 31 | C | `ToolbarPackStateIndex`, `TbDeleteButton` |
| 16 | `toolbar-debug-log-analysis.dot` | P2 | 1 | 31 | C | `ToolbarSubclassSecondaryToolbar` |
| 17 | `print-export.dot` | P3 | 7 (1F + 6S) | 14 | B + C + D | `PrintDialog.cpp` -> `Print.cpp`; `CmdPrintSetup`, `CmdExtractText/Images`, `HandleCmdExtractText`, `PageLayoutMargin`, `PrintToPdf` |
| 18 | `resource-management.dot` | P3 | 1 | 23 | C | `UpdateFileStateForTab` |
| 19 | `stress-testing.dot` | P3 | 1 | 27 | C | `StressTestRunner` |
| 20 | `UX.dot` | P3 | 2 | 12 | C | `CmdArchApply`, `CmdGoToNextPrevFavorite` |
| 21 | `profile-portable-drive.dot` | P3 | 1 | 10 | C | `DriveSyncFiles` |
| 22 | `webview-markdown.dot` | P3 | 1 | 52 | C | `PrintToPdf` |

Fast path for P2/P3: most have 1–3 symbols. Fixing 10–11, 12–16, 18–22 is a single short session and removes 19 of the 22 debt entries.

## 5. Failure classes seen so far

| Class | Meaning | Example | Fix |
|---|---|---|---|
| **A. Name drift** | officially correct name spelled differently in the map | `MarginTrimEnabled` vs field `marginTrimEnabled` | use the real name; setting/command/flag names resolve through the generators |
| **B. File attribution** | the cited `.cpp` was split or merged | `MenuEnabler.cpp` -> `Menu.cpp`, `PrintDialog.cpp` -> `Print.cpp`, `Settings.cpp` -> `AppSettings.cpp` | re-attribute the label; do not invent files |
| **C. Symbol renamed** | concept survived, identifier changed | `UpdateFileStateForTab`, `TabGroupsSave`, `HomePageWnd` | find the current identifier (tree-sitter `find_usage` / grep) and update |
| **D. Symbol deleted** | feature or helper genuinely removed | `FitSinglePage`, `PrintToPdf`, `OpenNewWindow`, `DriveSyncFiles` | delete the node and its edges; if the whole map is dead, delete the map |
| **E. Non-symbol text** | external API or prose placeholder | `GetGuiResources`, `HandleCmdXxx`, `OnNavigationStarting` | `FLOW/.sync-allow.txt` (scoped to one map when possible) |

## 6. Repair procedure (per map)

1. `bun scripts/flow-sync.ts` → read the map's entry in `FLOW/.sync-baseline.json`.
2. For each stale token: find the current owner — tree-sitter `find_usage` / `get_symbols` scoped to the module, or a search for the old name. Record the real name.
3. Rewrite labels. Keep node ids stable, keep the color role, keep the legend meaning. Labels say **what happens**, not where — cite one file or one symbol per fact, never a line number.
4. If the flow itself changed (steps merged, removed, added), redraw the edges; do not just rename the label on a stale edge.
5. `bun scripts/flow-sync.ts --stamp` (review date + hash of the referenced code), then `--baseline` (the entry for that map must shrink or disappear).
6. Render check: `dot -Tsvg FLOW/<map>.dot > NUL` — a malformed map must not be committed.
7. If the mechanism is gone, delete the map and note it in `LOG.md`.

## 7. Brief for external review (paste this part)

> **Context.** A Windows C++ desktop application (native Win32 UI) keeps ~35 Graphviz "process maps" that explain how mechanisms work and how tasks are done. They are read by AI agents at the start of a work session, so a stale map silently produces wrong actions. We now verify every file/symbol reference in the maps against the code and enforce a ratchet baseline (37 maps checked, 22 stale, 79 dangling references; ~550 source files; a 60-day churn metric per map). No source code is provided, only names and counts.
>
> **What we already did.** Removed all line-number references (they rot in weeks); added a per-map review stamp (date + hash of the referenced code) that warns when the referenced code changed since review; excluded one-off debug maps; allowlisted external API names; index setting/command names from the code generators rather than the C++ tree.
>
> **Questions.**
> 1. **Durable references.** Names still rotate (`UpdateFileStateForTab` -> something else), and line numbers are worse. Is there a notation for "this node is about this code" that survives refactors — e.g. cite only the invariant or entry command, or reference a symbol *role* rather than a symbol? What do you use in practice?
> 2. **Granularity.** These maps mix "how the mechanism works" (timers, caches, state machines) with "how to do a task" (build, release, debug). Should those be two document types with different lifetimes and review rules?
> 3. **Triage.** When a map has many dead references, how do you decide between (a) patch labels, (b) re-derive the flow from scratch, (c) delete the map? Any heuristic based on churn, age or coverage?
> 4. **Review cadence.** Today review is triggered by a warning when referenced code changes. Is event-triggered review the right model, or should maps expire (review-by date) regardless of code changes?
> 5. **Notation.** Suggested legend/color convention for node roles (entry point, mechanism, render/output, gotcha, external dependency) plus a convention for invariants and "common mistake" notes — keeping it readable at ~15 nodes.
> 6. **Priority model.** We rank repair by (map read before acting) × (60-day churn of referenced files) × (debt size). Is that the right weighting, and what factor is missing?
> 7. **Agent budget.** How much detail per map before it stops being useful to an LLM reader and starts consuming too much context? Any rules for splitting oversized maps?
>
> **What to return.** Conventions, checklists and heuristics — not code. Prefer concrete rules ("every node label must …", "a map is archived when …") over general advice.

## 8. Definition of done

- `bun scripts/flow-sync.ts` exits 0 with **no** entry for the map in `FLOW/.sync-baseline.json`.
- The map's stamp matches the current code (`--stamp` run after the edit).
- Every label cites an existing file or symbol, or an English description with no invented identifier.
- Diagram renders (`dot -Tsvg`), color roles unchanged, no line numbers.
- `LOG.md` notes what was rewritten per session; `TODO.md` debt count updated.
