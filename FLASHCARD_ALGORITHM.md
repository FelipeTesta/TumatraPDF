# FLASHCARD_ALGORITHM.md

Official SuperMemo/SM-2 specification (VERIFIED, sources [S1]-[S8]) + TumatraPDF current scheduler map (CODE-VERIFIED, file:line refs) + completion plan (PROPOSAL, nothing implemented).

Status per part:
- Official spec: VERIFIED against canonical sources
- Current implementation: CODE-VERIFIED against `sumatrapdf-src/src/Flashcard.cpp/h` and related files
- Plan: PROPOSAL only — no code changes made

---

## 1. Official SM-2 (canonical, Wozniak 1990) [S1]

### 1.1 Grades 0..5

| Grade | Meaning |
|-------|---------|
| 5 | Perfect response |
| 4 | Correct response after hesitation |
| 3 | Correct response recalled with serious difficulty |
| 2 | Incorrect response, correct one seemed easy to recall |
| 1 | Incorrect response, correct one remembered |
| 0 | Complete blackout |

### 1.2 E-Factor update

```
EF' := EF + (0.1 - (5 - q) * (0.08 + (5 - q) * 0.02))
```
Equivalent polynomial form:
```
EF' := EF - 0.8 + 0.28*q - 0.02*q*q
```

### 1.2.1 Derived EF delta table (DERIVED from [S1])

| q | EF delta | Note |
|---|----------|------|
| 0 | -0.80 | Complete blackout |
| 1 | -0.54 | Incorrect, correct remembered |
| 2 | -0.32 | Incorrect, correct seemed easy |
| 3 | -0.14 | Correct with serious difficulty |
| 4 | 0.00 | Correct after hesitation |
| 5 | +0.10 | Perfect response |

- If EF < 1.3 → set EF = 1.3 [S1]
- For q = 4 the EF does not change
- Initial EF = 2.5 for every item
- Original range 1.1..2.5; floor raised to 1.3 after observing items with EF < 1.3 repeated annoyingly often and usually violated minimum information principle [S1]

### 1.3 Intervals (days)

```
I(1) := 1
I(2) := 6
I(n) := I(n-1) * EF   for n > 2
```
Fractional interval rounded UP to next integer [S1].

### 1.4 Failure reset

"If the quality response was lower than 3 then start repetitions for the item from the beginning without changing the E-Factor (i.e. use intervals I(1), I(2) etc. as if the item was memorized anew)." [S1]
→ Grades 0/1/2 reset repetition count to 0, next interval = 1 day; EF unchanged.

### 1.5 Same-day re-repeat

"After each repetition session of a given day repeat again all items that scored below four in the quality assessment. Continue the repetitions until all of these items score at least four." [S1]

### 1.6 EF floor rationale

Items with EF < 1.3 repeated too often, typically violated minimum information principle (Rule 4 [S6]).

---

## 2. Official refinements — NOT SPECIFIED in official docs

| Refinement | Official status |
|------------|-----------------|
| Learning steps (e.g. 1m 10m) | Not specified. Blog only: "With small steps, cards are shown as soon as the delay has passed... if step crosses day boundary, delay converted to days" |
| Graduating interval | Not specified (later scheduler concept) |
| Easy interval | Not specified (SM-2 has no Easy button) |
| Maximum interval | Not found in any official source |
| Hard/easy multipliers (1.2x / 1.3x) | Not in canonical SM-2 (Anki innovations) |
| Relearning steps on lapse | Only in community mirror of blocked help.supermemo.org: "if you leave relearning steps blank, the card will skip relearning and gets a new interval of 1 day by default" [S5] |
| Burying / sibling burying | Only in blocked official help (new/review/interday learning siblings) |
| Interval fuzz / randomization | Not in canonical SM-2 (Anki-only) |
| Card states new→learning→review | Blog uses them; exact transitions not formalized in official source |
| Forgetting index | Blog "spaced-repetition-in-learning": "Set at a level of 90%, is the default moment in SuperMemo for optimised repetition" [S2]; "general-principles-of-supermemo": forgetting index "can vary from 3% ... to 20%" [S3] |
| Forgetting curve formula | Community mirror of official formula: `R = exp(-t/S)` (retrievability, stability, time) [S4] |

---

## 3. Forgetting index / forgetting curve

- Default forgetting index = 10% (retrievability 90%) [S2]
- Adjustable range 3%–20% [S3]
- Formula: `R = exp(-t/S)` where R = retrievability, S = stability, t = time [S4]
- Implication for fixed-interval scheduler (SM-2): the intervals approximate the forgetting curve but do not adapt S per item; EF acts as a coarse stability proxy.

---

## 4. Twenty Rules of Formulating Knowledge [S6] — summary

| Rule | Title | Relevance to TumatraPDF |
|------|-------|------------------------|
| 4 | Stick to minimum information principle | Split complex items; our cards are masks over source text |
| 5 | Cloze deletion is easy and effective | Core of incremental reading; **aligns with our model** |
| 8 | Graphic deletion as good as cloze | Occlusion tests; **maps to our Image-Occlusion cards** |
| 11 | Combat interference | **Single greatest cause of forgetting**; our risk: user recalls via page context, not masked item |
| 12 | Optimize wording | — |
| 14 | Personalize and provide examples | — |
| 19 | Provide date stamping | — |
| 20 | Prioritize (sources → extraction → transfer → formulation → forgetting index) | — |

### Analysis note (NOT official)
Our cards are masks over source text (answer IN the document). Rule 5 aligns with our model; Rule 8 maps to our occlusion/Image-Occlusion cards; the real risk is Rule 11 (interference from surrounding context: user may recall via page context, not the masked item).

---

## 5. Other implementations (NON-OFFICIAL, for contrast)

### 5.1 Anki SM-2 variant [S7]
- Learning steps: `1m 10m`
- Graduating interval: 1 day
- Easy interval: 4 days
- Relearning steps: `10m`
- Minimum interval: 1 day
- Easy bonus: 1.3x
- Hard multiplier: 1.2x
- New interval after lapse: 0%
- Maximum interval: 36500 days
- Fuzz: 0.5% (learning), 15% (young), 85% (mature)
- Burying: new/review/interday-learning siblings
- Leech threshold: 8 lapses

### 5.2 FSRS (Free Spaced Repetition Scheduler) [S8]
- D/S/R model: `R = exp(-t/S)`
- 17 learned parameters
- Desired retention default: 90%
- No grades, no hard-coded intervals

---

## 6. Our implementation today (CODE-VERIFIED)

### 6.1 State (`Flashcard.h:42-60`, `Flashcard.cpp:1360-1369`)

```cpp
struct FlashcardStudyState {
    int rating;           // 0 = new, 1=Again, 2=Hard, 3=Good, 4=Easy
    int interval;         // days
    double easeFactor;    // EF
    int64_t lastReviewedAt;  // ms since epoch
    int64_t nextReviewAt;    // ms since epoch
    int reviewCount;      // total reviews
};
struct FlashcardStudyDoc::StateEntry {
    Str key;              // card identifier
    FlashcardStudyState state;
};
```
Missing vs official: `repetitions` (n in I(n)), `lapses`, learning step index, due-date field separate from `nextReviewAt`.

### 6.2 Algorithm (`Flashcard.cpp:1736-1772`, `FlashcardSm2Update`)

```pseudocode
function FlashcardSm2Update(state, rating, now_ms):
    // rating: 1=Again, 2=Hard, 3=Good, 4=Easy
    if rating == 1:           // Again
        state.interval = 0
        state.easeFactor -= 0.20
    else if rating == 2:      // Hard
        state.interval = max(1, state.interval * 1.2)
        state.easeFactor -= 0.15
    else if rating == 3:      // Good
        if state.interval == 0:
            state.interval = 1
        else:
            state.interval = max(1, state.interval * state.easeFactor)
        // EF unchanged
    else if rating == 4:      // Easy
        if state.interval == 0:
            state.interval = 1
        else:
            state.interval = max(1, state.interval * state.easeFactor * 1.3)
        state.easeFactor += 0.15

    state.easeFactor = clamp(state.easeFactor, 1.3, 2.5)
    state.nextReviewAt = now_ms + state.interval * 86400000
    state.lastReviewedAt = now_ms
    state.rating = rating
    state.reviewCount++
    return state
```
`now_ms = (int64)time(nullptr) * 1000` at `Flashcard.cpp:1738`.

### 6.3 State writers (12 total, all preserve sorted-by-key invariant)

| Writer | File:line | What changes | Preserves invariant? |
|--------|-----------|--------------|---------------------|
| `HandleFlashcardRate` | `Commands_Flashcard.cpp:1195-1218` | Rating update, reinsert at session end | Yes |
| `FlashcardStudyPruneStates` | `Flashcard.cpp:908-962` | Remove stale entries | Yes |
| `FlashcardStudySave` | `Flashcard.cpp:971-1032` | Write sidecar JSON | Yes |
| `FlashcardStudySaveAsync` | `Flashcard.cpp:1040-1044` | Deferred save (worker thread) | Yes |
| `CmdFlashcardDeleteActive` | `Commands_Flashcard.cpp:1820-1827` | Delete current card state | Yes |
| `FlashcardStudyLoad` | `Flashcard.cpp:1430-1489` | Load + sort | Yes |
| `FlashcardStudyResync` | `Flashcard.cpp:1615-1634` | Resync after external change | Yes |
| `FlashcardStudyAdoptByName` | `Flashcard.cpp:1544-1608` | Adopt moved file states | Yes |
| `ProfileDataUpdateStudyDoc` | `ProfileData.cpp` | Profile store upsert | Yes |
| `ProfileDataMergeStudyDoc` | `ProfileData.cpp` | Merge newest-wins on `lastReviewedAt` | Yes |
| `FlashcardDeleteAllStudyFiles` | `Flashcard.cpp:1055-1076` | Purge all study data | Yes |
| `FlashcardStudyBackupRefreshSlot/RecoverBackup` | `Flashcard.cpp:797-831` | Backup slot rotate/recover | Yes |

Invariant enforced at 4 boundaries: load, parse, prune, insert; debug gate `FlashcardStudyVerifySorted`.

### 6.4 New vs review vs due

- New card: `rating == 0` (4 consistent sites: `Commands_Flashcard.cpp:299`, `Flashcard.cpp:1669`, `ProfileData.cpp:2355`, `FlashcardToolbar.cpp:644`)
- Due test: `rating != 0 && nextReviewAt <= now`
- "Again" card re-inserted at END of current session queue (`Commands_Flashcard.cpp:1203-1206`) + `FlashcardPrewarmInvalidate()`

### 6.5 Queue (`Flashcard.cpp:1650-1730`, `FlashcardStudyListDocs`)

- New cards and due cards collected separately
- Combined per `NewCardsPosition` setting (0=new first, 1=new last, 2=mixed — default)
- `RandomOrder` (default true) shuffles WITHIN each group
- Cross-document session covers all tabs of THIS window

### 6.6 Rating UI

- Keys 1-4 (and numpad) → `CmdFlashcardRate1..4` → `HandleFlashcardRate`
- Toolbar buttons Again/Hard/Good/Easy with tooltips
- **NO feedback after rating** (no interval preview, no next-date preview)
- Lista panel: masked text, page, due countdown computed AT DRAW time (`FlashcardSidebar.cpp:111-113`), status colour (new=blue, due=orange, learning=purple, ok=green)

### 6.7 Settings (`cmd/gen-settings.ts:867-876`)

EXACTLY 3 fields in `flashcard` group:
- `RandomOrder` (Bool, true)
- `NewCardsPosition` (Int, 2)
- `FlashcardMarkerAlpha` (Int, 10)

Generated struct `FlashcardSettings { bool RandomOrder; int NewCardsPosition; int FlashcardMarkerAlpha; }` in `src/Settings.h`.

**VERDICT: There is NOT ONE scheduler knob today** (no starting EF, no max interval, no learning steps, no graduating/easy interval, no fuzz, no bury, no per-day limits).

To add a knob: add `field(...)` in `cmd/gen-settings.ts` → `bun cmd/gen-code.ts` → read as `gGlobalPrefs->flashcard<Name>`.

### 6.8 Persistence

- Per-book sidecar JSON: `<ProfileDataDir>/study/<md5(filePath)>.json` (version 1, `docName`, `docPath`, `states`)
- Single-file profile store: `tumatraPDF-data_1.json` (same states per book, merged newest-wins on `lastReviewedAt`)
- Legacy fallback: `%LOCALAPPDATA%\SumatraPDF\FlashcardStudy\<md5>.json`
- `version` field exists but **NO migration logic yet**

### 6.9 Adding a state field = 6 steps

1. Struct (`Flashcard.h:42-50`)
2. Serialize (`Flashcard.cpp:660-671`)
3. Parse (`Flashcard.cpp:1360-1369`)
4. Default value
5. `FlashcardStudyDoc::version` bump
6. Migration path for existing v1 files (profile store picks up field automatically)

### 6.10 Perf constraints (plan must respect)

| Path | Work | Logged as |
|------|------|-----------|
| Rating critical path (UI thread) | Study-state scan O(log S) + SM-2 update O(1) + state write-back O(S) + stats O(1) + toolbar O(1) + nav/prefetch/prewarm/rerender | `[fc][perf] rate: scan=%dms stats=%dms toolbar=%dms nav=%dms rerender=%dms total=%dms` |
| Deferred work (≥50ms, study tick) | Prune O(C log C + S) + per-book save (worker thread, BELOW_NORMAL, shared JSON writer) + sidebar (visibility gate + incremental update) | — |
| Profile upsert | ON UI thread (`gProfile` has no locks) | — |
| Paint | Only visible pages via `cardIndicesByPage` | — |

Crash-loss window for SM-2 state ~60s (deferred save); flush-before-prompt ordering mitigates.

### 6.11 Test hooks (`cmd/control.ts`)

- `TestRateFlashcard` = 87 (cardIdx, rating)
- `TestCreateFlashcard` = 88 (pageNo[, rating])
- `TestFlushFlashcardBookkeeping` = 89
- `TestFlashcardFlush` = 90 (0=full, 1=save-only)
- Probes: `tests/ad-hoc-flashcard-study-freeze.ts`, `ad-hoc-flashcard-nav-stutter.ts` (registered in `tests/before-release.ts`)
- Rule: new command id on BOTH sides (client enum in `cmd/control.ts` + server enum in `src/SumatraControl.cpp`); never hardcode in probes (use `cmdId("CmdName")`)

### 6.12 CLOCK SEAM MISSING (P0 prerequisite)

`time(nullptr)` called directly in:
- `FlashcardSm2Update` (`Flashcard.cpp:1738`)
- `FlashcardStudyListDocs` (`Flashcard.cpp:1650`)

No injection point → deterministic scheduler regression test IMPOSSIBLE today.

### 6.13 NOT VERIFIED (from code audit)

- Actual P50/P95/P99 perf budgets (no fresh `[fc][perf]` capture)
- Cross-window session isolation under stress
- Backup-recovery concurrency
- Profile merge race between two machines
- `FlashcardStudyAdoptByName` (book moved on disk)
- R7 items: ~60s crash-loss window vs flush on WM_CLOSE, missing JSON migration path, adopt-by-name path, O(C log C + S) / O(log S) cost claims (audit confirmed against code)

---

## 7. Gap table

| Requirement (official, citation) | Ours (file:line) | Gap | User impact | Effort |
|----------------------------------|------------------|-----|-------------|--------|
| EF formula: `EF' = EF + 0.1 - (5-q)*(0.08+(5-q)*0.02)` [S1] | `Flashcard.cpp:1736-1772` uses hardcoded deltas (-0.20, -0.15, +0.15) | Formula not implemented; deltas approximate only q=1,2,4; q=3 no EF change correct | Wrong EF evolution for all grades; q=5 not representable | M |
| EF floor 1.3 [S1] | `Flashcard.cpp:1768` clamp to [1.3, 2.5] | Floor correct | — | — |
| Initial EF = 2.5 [S1] | `Flashcard.cpp:1360-1369` parse default; new card gets EF=2.5 via `FlashcardStudyState{}` zero-init then first rating sets | Implicit via first rating path; no explicit initial EF field | New card first interval uses EF=2.5 de facto | S |
| I(1)=1, I(2)=6, I(n)=I(n-1)*EF [S1] | `Flashcard.cpp:1736-1772` uses `interval*EF` for Good, `interval*EF*1.3` for Easy, `interval*1.2` for Hard | No I(1)=1 / I(2)=6 distinction; no repetition counter `n`; Easy/Hard multipliers are Anki not SM-2 | Intervals diverge from SM-2 from 3rd repetition | M |
| Round UP fractional interval [S1] | `Flashcard.cpp:1736-1772` uses `max(1, ...)` on double → implicit trunc on int assign | No explicit ceil; `interval` is `int` so fractional part lost (truncates down, not up) | Intervals slightly shorter than spec | S |
| Failure reset: grades 0/1/2 → repetition=0, interval=1, EF unchanged [S1] | `Flashcard.cpp:1740-1745` (rating=1): interval=0, EF-=0.20 | **EF changed** (violates spec); interval=0 not 1; no repetition counter to reset | "Again" cards get easier over time (EF drops), next interval 0 → shown again immediately | M |
| Same-day re-repeat until q≥4 [S1] | Not implemented | Entire feature missing | Items scored 1-3 not re-reviewed same session | M |
| Repetitions counter `n` for I(n) [S1] | Missing from `FlashcardStudyState` | No `n` → cannot compute I(1)/I(2)/I(n) correctly | Core SM-2 logic unimplementable | M |
| Learning steps (not specified in the official docs; blog [S2] only describes step behaviour; 1m 10m is Anki [S7]) | Not implemented | No learning phase; new card goes straight to review intervals | No gradual learning; first interval = 1 day (Good) or 0 (Again) | M |
| Graduating / Easy interval (not specified in the official docs; SM-2 has no Easy button; Anki values [S7]) | Not implemented | No concept | — | M |
| Max interval (not found in any official source; Anki 36500 days [S7]) | Not implemented | Unbounded growth | Very old cards get huge intervals | S |
| Fuzz / randomization (not in canonical SM-2; Anki fuzz values [S7] (NON-OFFICIAL)) | Not implemented | Predictable schedule | — | S |
| Bury / sibling bury (only in community mirror of blocked official help [S5]; Anki sibling rules [S7] (NON-OFFICIAL)) | Not implemented | Related cards may appear together | Interference (Rule 11 [S6]) | M |
| Per-day new/review limits (not specified in the official docs; Anki defaults [S7] (NON-OFFICIAL)) | Not implemented | Unbounded sessions | Overwhelm | S |
| Lapses / leech [S5] | Missing `lapses` field | No leech detection | Stuck cards never flagged | M |
| Post-rating feedback (interval, next date) | `FlashcardSidebar.cpp:111-113` draw-time only | No immediate preview after rating | User blind to scheduling effect | S |
| Deterministic clock seam | `Flashcard.cpp:1738,1650` direct `time(nullptr)` | No injection point | No reproducible scheduler tests | S |
| JSON migration path | `version` field exists, no migration code | v1 files stuck on old schema | Future schema changes break load | M |

**Citation convention**: [S1] = the canonical official article; "not specified in the official docs" = no official source states it (no number invented); [S5]/[S7] = community/non-official references, for contrast only.

---

## 8. Phased plan (PROPOSAL)

### P0: Test harness + clock seam + pure scheduler function
**Goal**: Deterministic, testable SM-2 core.
**Changes**:
- `Flashcard.h`: Add `FlashcardSchedulerInput`, `FlashcardSchedulerOutput` structs; pure function declaration `FlashcardSm2Compute(const FlashcardSchedulerInput&, int64_t now_ms) -> FlashcardSchedulerOutput`
- `Flashcard.cpp:1736-1772`: Extract current logic into `FlashcardSm2Compute`; add `now_ms` parameter (default `time(nullptr)*1000` for backward compat)
- `Flashcard.cpp:1650`: Thread `now_ms` through `FlashcardStudyListDocs`
- `tests/ad-hoc-flashcard-sm2-core.ts`: New probe (registered in `tests/before-release.ts`) exercising `FlashcardSm2Compute` with fixed `now_ms` against known vectors (grade 0..5, various EF/interval/repetition states)
- `cmd/control.ts` + `src/SumatraControl.cpp`: Add `TestFlashcardSm2Compute` command (both enums)
**Acceptance**: `bun tests/ad-hoc-flashcard-sm2-core.ts` passes; deterministic output for same input.
**Risks**: None (pure refactor, old path preserved via default arg).
**Rollback**: Revert the two files.

### P1: State schema v2 + v1→v2 migration
**Goal**: Add `repetitions`, `lapses`, `learningStep` fields; migrate existing files.
**Changes**:
- `Flashcard.h:42-50`: Extend `FlashcardStudyState` with `int repetitions; int lapses; int learningStep;`
- `Flashcard.cpp:660-671`: Serialize new fields
- `Flashcard.cpp:1360-1369`: Parse new fields with defaults (0)
- `Flashcard.cpp`: `FlashcardStudyDoc::version` = 2
- `Flashcard.cpp`: Migration in `FlashcardStudyLoad` — if `version < 2`, derive `repetitions` deterministically from stored `interval` (interval == 0 → n=0 new/relearning; interval == 1 → n=1; interval >= 6 → n=2, the first value reproducing I(n)=I(n-1)*EF; anything else → n=0 treat as fresh card), `lapses=0`, `learningStep=0`; this is an approximation (old algorithm never stored n) so pre-existing card schedules shift slightly once — P2 ships behind opt-in flag with legacy path intact
- `tests/ad-hoc-flashcard-migration.ts`: Probe loading v1 JSON, verify v2 fields populated per rule above
**Acceptance**: Old sidecar loads, new fields present per deterministic rule, `FlashcardStudyVerifySorted` passes.
**Risks**: Approximate migration shifts old card schedules slightly; flag-gated so users opt in.
**Rollback**: Version stays 1; new fields ignored by old code.

### P2: Faithful SM-2 core behind settings
**Goal**: Implement canonical EF formula, I(1)/I(2)/I(n), round-up, failure reset without EF change, repetitions counter.
**Changes**:
- `cmd/gen-settings.ts`: Add `FlashcardUseOfficialSm2` (Bool, false), `FlashcardStartingEase` (Double, 2.5), `FlashcardMinEase` (Double, 1.3)
- `bun cmd/gen-code.ts` → regenerates `Settings.h/cpp`
- `Flashcard.cpp`: New `FlashcardSm2OfficialCompute` implementing:
  - EF formula from [S1] polynomial
  - `repetitions` counter: interval is computed from the count BEFORE incrementing (n = number of successful repetitions completed before this review): n=0 → I(1)=1 day, n=1 → I(2)=6 days, n>=2 → I(n)=I(n-1)*EF (rounded UP); then on q≥3 increment n, on q<3 reset n=0
  - Failure reset: q<3 → `repetitions=0`, `interval=1`, EF unchanged
  - Same-day re-repeat: collect cards with q<3, re-queue until all q≥4 (session-local)
  - Test vectors must pin this: a card rated Good with n=0 must get 1 day, its second Good must get 6 days (not 1*EF)
- `FlashcardSm2Update`: If `gGlobalPrefs->flashcardUseOfficialSm2`, delegate to official compute
- `tests/ad-hoc-flashcard-sm2-official.ts`: Probe verifying official vectors
**Acceptance**: With flag ON, official vectors match [S1]; with flag OFF, current behaviour preserved.
**Risks**: Session re-repeat changes queue dynamics; test thoroughly.
**Rollback**: Flag OFF restores old behaviour.

### P3: Modern refinements as OPTIONAL advanced settings
**Goal**: Learning steps, graduating/easy interval, max interval, fuzz, lapse relearning, bury, daily limits.
**Changes** (each behind its own flag, safe defaults = OFF/0):
- `cmd/gen-settings.ts`: Add fields:
  - `FlashcardLearningSteps` (Str, "") — space-separated minutes (e.g. "1 10")
  - `FlashcardGraduatingInterval` (Int, 1)
  - `FlashcardEasyInterval` (Int, 4)
  - `FlashcardMaxInterval` (Int, 36500)
  - `FlashcardFuzzPct` (Int, 0) — 0=off, else percent
  - `FlashcardRelearningSteps` (Str, "")
  - `FlashcardBuryNew` / `FlashcardBuryReview` / `FlashcardBuryInterday` (Bool, false)
  - `FlashcardDailyNewLimit` / `FlashcardDailyReviewLimit` (Int, 0=unlimited)
  - `FlashcardLeechThreshold` (Int, 8)
- `bun cmd/gen-code.ts`
- `Flashcard.cpp`: Implement each as opt-in branch in scheduler
- `FlashcardStudyState`: Add `learningStep` usage, `lapses` increment on lapse
- `tests/ad-hoc-flashcard-refinements.ts`: Probe each setting
**Acceptance**: Each flag toggles behaviour; defaults match current (no change).
**Risks**: Settings explosion; keep behind `FlashcardUseOfficialSm2` gate initially.
**Rollback**: All flags OFF.

### P4: Session/queue integration
**Goal**: Learning vs review buckets, same-day re-repeat until q≥4, due ordering.
**Changes**:
- `Flashcard.cpp:1650-1730` (`FlashcardStudyListDocs`): Separate learning cards (in learning steps) from review cards; learning cards due when step elapsed
- Queue order: learning (by due) → review (by due) → new (per `NewCardsPosition`)
- Same-day re-repeat: after session, re-queue cards with q<3 at front of learning queue
- `FlashcardToolbar.cpp`: Update counters (learning/review/new/due)
**Acceptance**: Manual session: learning cards appear at step times; failed cards repeat same session.
**Risks**: Queue logic complex; off-by-one in step indexing.
**Rollback**: Revert `FlashcardStudyListDocs`.

### P5: UI feedback + theme-sensitive rules
**Goal**: Interval preview after rating, Lista countdown, new/review/due counters.
**Changes**:
- `Commands_Flashcard.cpp:1195-1218` (`HandleFlashcardRate`): After update, compute next interval/date, show toast/status (reuse `FlashcardToolbar` status area)
- `FlashcardSidebar.cpp:111-113`: Precompute countdown at list build (not draw time), show "1d 3h" format
- `FlashcardToolbar.cpp`: Add counters: New / Learning / Review / Due
- Theme: Per §0/§13/Design_Guidelines §6.2 — theme tokens only, no hardcoded colours
**Acceptance**: Theme-sensitive implementation (theme tokens only, no hardcoded colours — Design_Guidelines §6) and USER's live validation on a real build in dark and light; agent side asserts geometry/feedback through control pipe.
**Risks**: Toast may interfere with reading; make dismissible.
**Rollback**: Remove toast, revert sidebar.

### P6: Rollout
**Goal**: Opt-in flag, telemetry, migration, rollback.
**Changes**:
- `cmd/gen-settings.ts`: `FlashcardSchedulerVersion` (Int, 1=legacy, 2=official) — replaces `FlashcardUseOfficialSm2`
- `Flashcard.cpp`: On first run with v2, migrate all books (P1 logic)
- `[fc][perf]` telemetry: Log scheduler version, per-rating latency, queue sizes
- `TODO.md`: Add rollback procedure (set `FlashcardSchedulerVersion=1`, delete sidecars, restart)
- `CHANGELOG.md`: Document new scheduler as opt-in feature
**Acceptance**: Clean upgrade/downgrade cycle; `[fc][perf]` shows no regression.
**Risks**: Migration may miss edge cases; test with real user data.
**Rollback**: Version=1 + delete sidecars.

---

## 9. Owner decisions (policy calls, not code)

| Decision | Options | Recommendation | Consequence |
|----------|---------|----------------|-------------|
| Rating scale | Keep 4 buttons (Again/Hard/Good/Easy) vs adopt 6 grades (0..5) | Keep 4; map: Again→1/0 (collapse q=0 blackout and q=2 incorrect-but-almost), Hard→3 (correct with serious difficulty), Good→4 (correct after hesitation), Easy→5 (perfect) | Collapsing q=0 and q=2 into one button loses the official distinction between "complete blackout" and "incorrect but almost remembered" — which is exactly what the EF formula distinguishes; simpler UI but information loss |
| Learning step durations | Anki `1m 10m` / custom / none | Configurable `FlashcardLearningSteps` default `"1 10"` | Users can tune; default matches Anki familiarity |
| Max interval | 36500 (Anki) / 7300 (20yr) / none | `FlashcardMaxInterval` default 36500 | Prevents absurd intervals |
| Fuzz | Off / On (seeded RNG for reproducibility) | Off by default (`FlashcardFuzzPct=0`); if ON, seed from card key + day | Reproducible tests; no surprise scheduling |
| Per-day limits | Unlimited / Configurable | `FlashcardDailyNewLimit=20`, `FlashcardDailyReviewLimit=200` defaults | Prevents overwhelm; power users raise |
| Relearning semantics | Our "Again=interval 0 + reinsert end of session" vs official "reset to I(1), EF unchanged" | Official semantics behind `FlashcardUseOfficialSm2`; legacy default | Official: failed cards get 1-day interval, not immediate re-show |
| EF clamp [1.3, 2.5] | Keep / widen / narrow | Keep [1.3, 2.5] | Matches official floor rationale |
| Leech threshold | 8 (Anki) / configurable / none | `FlashcardLeechThreshold=8` default, flag to disable | Flags stuck cards; user decides action |

---

## 10. NOT VERIFIED block

From research digest (2.1):
- Official blog pages for learning steps, graduating interval, easy interval, max interval, fuzz, bury, relearning, card-state transitions — not formalized in accessible official source
- Community mirror of blocked `help.supermemo.org` for relearning steps default
- Tool log (one line per attempt):
  - `webfetch https://supermemo.com/en/archives1990-2015/english/ol/sm2` → SUCCESS [S1]
  - `webfetch https://supermemo.com/en/blog/spaced-repetition-in-learning` → SUCCESS [S2]
  - `webfetch https://supermemo.com/en/blog/general-principles-of-supermemo` → SUCCESS [S3]
  - `webfetch https://supermemo.guru/wiki/Forgetting_curve` → SUCCESS [S4]
  - `webfetch https://supermemo.guru/wiki/Leech` → SUCCESS [S5]
  - `webfetch https://supermemo.com/en/blog/twenty-rules-of-formulating-knowledge` → SUCCESS [S6]
  - `webfetch https://docs.ankiweb.net/deck-options.html` → SUCCESS [S7]
  - `webfetch https://github.com/open-spaced-repetition/fsrs4anki/wiki` → SUCCESS [S8]
  - `webfetch https://supermemo.com/en/archives1990-2015/english/ol/twenty-rules` → 404
  - `webfetch https://help.supermemo.org/wiki/Algorithm` → 403 Cloudflare
  - `webfetch https://help.supermemo.org/wiki/SM-2` → 403 Cloudflare
  - `webfetch https://help.supermemo.org/wiki/Spaced_repetition` → 403 Cloudflare
  - `webfetch https://lite.duckduckgo.com/lite/?q=supermemo+sm2` → CAPTCHA
  - `webfetch https://search.marginalia.nu/search?q=supermemo+sm2` → rate-limited

From code audit (2.2):
- Actual P50/P95/P99 perf budgets (no fresh `[fc][perf]` capture)
- Cross-window session isolation under stress
- Backup-recovery concurrency
- Profile merge race between two machines
- `FlashcardStudyAdoptByName` (book moved on disk)
- R7 items: ~60s crash-loss window vs flush on WM_CLOSE, missing JSON migration path, adopt-by-name path, O(C log C + S) / O(log S) cost claims (audit confirmed against code)

---

## 11. Sources

[S1] https://supermemo.com/en/archives1990-2015/english/ol/sm2 — OFFICIAL canonical SM-2 article (grades, EF formula, intervals, failure reset, same-day re-repeat, EF floor rationale)
[S2] https://supermemo.com/en/blog/spaced-repetition-in-learning — OFFICIAL blog, 90% retrievability default
[S3] https://supermemo.com/en/blog/general-principles-of-supermemo — OFFICIAL blog, forgetting index 3-20%
[S4] https://supermemo.guru/wiki/Forgetting_curve — Community mirror, formula `R=exp(-t/S)`
[S5] https://supermemo.guru/wiki/Leech — Community mirror, leech + relearning steps default
[S6] https://supermemo.com/en/blog/twenty-rules-of-formulating-knowledge — OFFICIAL blog, 20 rules
[S7] https://docs.ankiweb.net/deck-options.html — Anki manual, NON-OFFICIAL
[S8] https://github.com/open-spaced-repetition/fsrs4anki/wiki — FSRS wiki, NON-OFFICIAL