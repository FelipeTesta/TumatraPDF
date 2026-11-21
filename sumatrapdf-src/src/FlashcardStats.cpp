/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

// Flashcard study-time statistics: seconds spent studying flashcards with
// study mode active, per document (FileState.flashcardStudyTimeSec, persisted with
// the normal file-history settings) and per calendar day (sidecar file
// "flashcard-stats.txt" under the data root's stats\ subdir — s64-R9o unified
// storage; one "YYYYMMDD <sec> <cards>" line per day, deliberately plain text,
// no JSON dependency). Cards rated are
// counted alongside the seconds (FileState.flashcardCardsDone per document,
// third sidecar column per day; 2-column lines from before the count existed
// parse with cards=0).

#include "base/Base.h"
#include "base/File.h"

#include "Settings.h" // FileState (FileHistory.h relies on it being declared)
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "DocController.h" // MarkdownModel fwd (WindowTab.h relies on it)
#include "FileHistory.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "TabGroupsManage.h"
#include "ProfileData.h"
#include "FlashcardStats.h"

struct DailyEntry {
    int dateKey; // YYYYMMDD
    int sec;
    int cards; // cards rated that day (0 for entries written before the count)
};

// dual-write gate: when false, skip writing the legacy sidecar file but keep
// streaming day rows into the single-file profile store (unconditional)
static bool kProfDualWriteSidecars = true;

static struct {
    Vec<DailyEntry> daily; // kept sorted by dateKey
    int dirtySec = 0;      // unsaved seconds (write threshold)
    int dirtyCards = 0;    // unsaved rated cards (secondary write threshold)
    bool loaded = false;   // sidecar parsed
} gFlashcardStats;

// YYYYMMDD for "today" in local time (bucket key; matches the heatmap layout)
static int DateKeyToday() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return (int)st.wYear * 10000 + (int)st.wMonth * 100 + (int)st.wDay;
}

// canonical sidecar location: <data-root>\stats\flashcard-stats.txt (s64-R9o)
static TempStr FlashcardStatsPathTemp() {
    TempStr root = ProfileDataGetDataRootTemp();
    TempStr statsDir = path::JoinTemp(root, StrL("stats"));
    return path::JoinTemp(statsDir, StrL("flashcard-stats.txt"));
}

// legacy sidecar lived next to the settings file — kept readable as fallback
// and as the copy source for the one-time migration; never deleted
static TempStr FlashcardStatsLegacyPathTemp() {
    TempStr settingsPath = GetSettingsPathTemp();
    TempStr dir = path::GetDirTemp(Str(settingsPath));
    return path::JoinTemp(dir, StrL("flashcard-stats.txt"));
}

// copy-only migration (s64-R9o): on load, if the new sidecar is missing but
// the legacy one exists, COPY it under <data-root>\stats. Never moves/deletes
// the legacy file. Best-effort: a failed copy logs and the legacy file is
// still read via the fallback path.
static void FlashcardStatsMigrateIfNeeded() {
    TempStr p = FlashcardStatsPathTemp();
    if (file::Exists(p)) {
        return;
    }
    TempStr legacy = FlashcardStatsLegacyPathTemp();
    if (!file::Exists(legacy)) {
        return;
    }
    TempStr root = ProfileDataGetDataRootTemp();
    TempStr statsDir = path::JoinTemp(root, StrL("stats"));
    if (!dir::CreateAll(Str(statsDir))) {
        logf("[stats] could not create '%s', keeping legacy sidecar\n", Str(statsDir));
        return;
    }
    if (file::Copy(Str(p), Str(legacy), true)) {
        logf("[stats] sidecar migrated to data-root: '%s' -> '%s'\n", Str(legacy), Str(p));
    } else {
        logf("[stats] sidecar migration FAILED (legacy kept): '%s'\n", Str(legacy));
    }
}

static void FlashcardStatsLoadIfNeeded() {
    if (gFlashcardStats.loaded) {
        return;
    }
    gFlashcardStats.loaded = true;
    FlashcardStatsMigrateIfNeeded();
    TempStr p = FlashcardStatsPathTemp();
    if (!file::Exists(p)) {
        // fallback read: legacy sidecar next to the settings file
        p = FlashcardStatsLegacyPathTemp();
        if (!file::Exists(p)) {
            return;
        }
        logf("[stats] reading legacy sidecar (new absent): '%s'\n", Str(p));
    }
    Str d = file::ReadFile(p);
    StrVec lines;
    Split(&lines, d, StrL("\n"), true);
    for (Str line : lines) {
        char buf[64]{};
        int lenCopy = std::min(len(line), (int)(dimof(buf) - 1));
        memcpy(buf, line.s, (size_t)lenCopy);
        char* p1 = nullptr;
        long dateKeyL = strtol(buf, &p1, 10);
        long secL = 0;
        long cardsL = 0;
        int n = 0;
        if (p1 != buf) {
            n = 1;
            char* p2 = p1;
            secL = strtol(p2, &p2, 10);
            if (p2 != p1) {
                n = 2;
                char* p3 = p2;
                cardsL = strtol(p3, &p3, 10);
                if (p3 != p2) {
                    n = 3;
                }
            }
        }
        if (n >= 2 && dateKeyL > 19000101 && secL >= 0) {
            DailyEntry e{(int)dateKeyL, (int)secL, (int)(n == 3 ? cardsL : 0)};
            gFlashcardStats.daily.Append(e);
        }
    }
    // keep sorted by dateKey (simple insertion since the file is small)
    for (int i = 1; i < len(gFlashcardStats.daily); i++) {
        DailyEntry e = gFlashcardStats.daily[i];
        int j = i - 1;
        while (j >= 0 && gFlashcardStats.daily[j].dateKey > e.dateKey) {
            gFlashcardStats.daily[j + 1] = gFlashcardStats.daily[j];
            j--;
        }
        gFlashcardStats.daily[j + 1] = e;
    }
}

static void FlashcardStatsAddDaily(float sec) {
    FlashcardStatsLoadIfNeeded();
    int key = DateKeyToday();
    for (int i = 0; i < len(gFlashcardStats.daily); i++) {
        if (gFlashcardStats.daily[i].dateKey == key) {
            gFlashcardStats.daily[i].sec += (int)sec;
            return;
        }
    }
    DailyEntry e{key, (int)sec, 0};
    gFlashcardStats.daily.Append(e);
}

// count one rated (studied) card: +1 to the rating card's owning document's
// FileState.flashcardCardsDone and to today's sidecar bucket.
// Accumulator spaces: `tab` = per-document (FileState/profile perDoc);
// gFlashcardStats.daily / dirtyCards = window/machine-level sidecar (not
// per-doc). The daily bucket is credited even when tab is null.
void FlashcardStatsAddCard(WindowTab* tab) {
    if (tab && tab->filePath) {
        FileState* fs = gFileHistory.FindByPath(tab->filePath);
        if (fs) {
            fs->flashcardCardsDone += 1;
            ProfileDataAddFlashcardCard(tab->filePath.s);
        }
    }
    FlashcardStatsLoadIfNeeded();
    int key = DateKeyToday();
    bool foundToday = false;
    for (int i = 0; i < len(gFlashcardStats.daily); i++) {
        if (gFlashcardStats.daily[i].dateKey == key) {
            gFlashcardStats.daily[i].cards += 1;
            foundToday = true;
            break;
        }
    }
    if (!foundToday) {
        // fresh day: the seconds tick may not have credited a whole second
        // yet, so the entry can legitimately not exist — create it with the
        // card count only
        DailyEntry e{key, 0, 1};
        gFlashcardStats.daily.Append(e);
    }
    // the flush also persists the per-document FileState (settings write is
    // driven by the normal save cadence)
    gFlashcardStats.dirtyCards += 1;
    if (gFlashcardStats.dirtyCards >= 20) {
        FlashcardStatsFlush(true);
    }

    {
        Str group = GetWindowActiveTabGroup(tab ? tab->win : nullptr);
        if (group.s && *group.s) {
            ProfileDataAddSessionCard(group);
        }
    }
}

void FlashcardStatsAddSeconds(MainWindow* win, WindowTab* tab, float sec) {
    if (sec <= 0.0f) {
        return;
    }
    // Accumulator spaces: win->flashcardStatsAccumSec is a per-window
    // sub-second carry (so two windows' ticks don't interleave into one
    // carry). Whole seconds are credited per-document via the OWNING tab
    // passed by the caller (studyOrder[curIdx].tab), never via
    // win->CurrentTab() — see FlashcardStats.h. The daily bucket + dirtySec
    // stay window/machine-level.
    win->flashcardStatsAccumSec += sec;
    int whole = (int)win->flashcardStatsAccumSec;
    if (whole <= 0) {
        return;
    }
    win->flashcardStatsAccumSec -= (float)whole;

    {
        MainWindow* ownerWin = tab ? tab->win : win;
        Str group = GetWindowActiveTabGroup(ownerWin);
        if (group.s && *group.s) {
            ProfileDataAddSessionStudySec(group, whole);
        }
    }

    // per-document time (FileState is the same store the open-count uses)
    if (tab && tab->filePath) {
        FileState* fs = gFileHistory.FindByPath(tab->filePath);
        if (fs) {
            fs->flashcardStudyTimeSec += whole;
            ProfileDataAddFlashcardSec(tab->filePath.s, whole);
        }
    }
    FlashcardStatsAddDaily((float)whole);
    gFlashcardStats.dirtySec += whole;
    if (gFlashcardStats.dirtySec >= 60) {
        FlashcardStatsFlush(false);
    }
}

void FlashcardStatsFlush(bool force) {
    ProfileDataFlushSessionsIfNeeded();
    if (!force && gFlashcardStats.dirtySec == 0 && gFlashcardStats.dirtyCards == 0) {
        return;
    }
    FlashcardStatsLoadIfNeeded();
    str::Builder sb;
    for (int i = 0; i < len(gFlashcardStats.daily); i++) {
        DailyEntry& e = gFlashcardStats.daily[i];
        sb.Append(fmt("%d %d %d\r\n", e.dateKey, e.sec, e.cards));
    }
    Str data = sb.TakeStr();
    if (kProfDualWriteSidecars) {
        TempStr root = ProfileDataGetDataRootTemp();
        TempStr statsDir = path::JoinTemp(root, StrL("stats"));
        dir::CreateAll(Str(statsDir));
        file::WriteFile(FlashcardStatsPathTemp(), data);
    }
    str::Free(data);
    gFlashcardStats.dirtySec = 0;
    gFlashcardStats.dirtyCards = 0;
    // stream the day rows into the profile store too (own-machine rows)
    for (int i = 0; i < len(gFlashcardStats.daily); i++) {
        ProfileDataUpsertStudyDay(gFlashcardStats.daily[i].dateKey, gFlashcardStats.daily[i].sec,
                                  gFlashcardStats.daily[i].cards);
    }
    ProfileDataSaveNow();
}

int FlashcardStatsGetDaySec(int dateKey) {
    FlashcardStatsLoadIfNeeded();
    for (int i = 0; i < len(gFlashcardStats.daily); i++) {
        if (gFlashcardStats.daily[i].dateKey == dateKey) {
            return gFlashcardStats.daily[i].sec;
        }
    }
    return 0;
}

// cards rated on the given day (YYYYMMDD); 0 when none
int FlashcardStatsGetDayCards(int dateKey) {
    FlashcardStatsLoadIfNeeded();
    for (int i = 0; i < len(gFlashcardStats.daily); i++) {
        if (gFlashcardStats.daily[i].dateKey == dateKey) {
            return gFlashcardStats.daily[i].cards;
        }
    }
    return 0;
}

// all-time flashcard study time, independent of the per-document FileState entries:
// summing the daily buckets means a renamed/deleted/moved document cannot lose
// the time it contributed (the sidecar is keyed by date, not by path)
int FlashcardStatsGetTotalSec() {
    FlashcardStatsLoadIfNeeded();
    int total = 0;
    for (int i = 0; i < len(gFlashcardStats.daily); i++) {
        total += gFlashcardStats.daily[i].sec;
    }
    return total;
}

// all-time count of rated cards (sum of every daily bucket — the master total,
// independent of any document's FileState, so renamed/deleted/moved books
// cannot lose the cards they contributed)
int FlashcardStatsGetTotalCards() {
    FlashcardStatsLoadIfNeeded();
    int total = 0;
    for (int i = 0; i < len(gFlashcardStats.daily); i++) {
        total += gFlashcardStats.daily[i].cards;
    }
    return total;
}

void FlashcardStatsOnSessionEnd() {
    FlashcardStatsFlush(true);
}
