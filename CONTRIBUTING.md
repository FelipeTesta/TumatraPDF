# Contributing to TumatraPDF

Thanks for your interest! TumatraPDF is a personal fork of
[SumatraPDF](https://github.com/sumatrapdfreader/sumatrapdf) with custom
features (flashcards / cloze SRS, two-column crop view, arch tools, autoscroll
ETA, contrast overlay...). Issues and pull requests are welcome.

## Ground rules

1. **Read `AGENTS.md` first** — it's the governing doc for this repo (for humans
   and AI agents alike). Key non-negotiables:
   - Never `Set-Content`/`WriteAllText` file content from PowerShell (destroys
     UTF-8; use an editor).
   - Never hardcode command IDs (use `cmdId("CmdName")` in tests).
   - Run `clang-format` on C++ files you touched before building.
2. **Build from inside `sumatrapdf-src/`** (see `BUILD.md`):
   `bun cmd/build.ts` → `out/dbg64/TumatraPDF.exe` (auto-deploys to `../Compiled/`).
3. **Tests:** change-tested code only — find the related `sumatrapdf-src/tests/`
   script; don't run full suites for a 10-line fix.
4. **Docs:** all project docs are English-only (README/LOG/TODO/BUILD/MERGE/
   RELEASE/AGENTS/Design_Guidelines, `FLOW/*.dot`).
5. **Commits:** first line ends with `(fixes #N)` when fixing an issue;
   describe features (not fixes) in `CHANGELOG.md` when readying a release.
6. **License:** by contributing you license your changes under GPLv3 (the
   project's license, inherited from SumatraPDF).

## Where things live

- `AGENTS.md` — repo rules + project map (start here)
- `BUILD.md` — build/release (incl. the copy-paste release procedure)
- `MERGE.md` — syncing with upstream SumatraPDF
- `LOG.md` / `TODO.md` / `CHANGELOG.md` — session log / active backlog /
  public per-version changelog
- `Design_Guidelines.md` — conventions + known traps distilled from sessions
- `FLOW/*.dot` — process diagrams
- `sumatrapdf-src/` — the actual source (forked upstream tree)
