/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

// Reading-time statistics (FASE B "Stats"): seconds spent reading with
// auto-scroll active, per document (FileState.readingTimeSec, persisted with
// the normal file-history settings) and per calendar day (sidecar file
// "reading-stats.txt" under the data root's stats\ subdir — s64-R9o unified
// storage; one "YYYYMMDD <sec>" line per day, deliberately plain text, no JSON
// dependency).

#include "base/Base.h"
#include "base/File.h"

#include "Settings.h" // FileState (FileHistory.h relies on it being declared)
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "DocController.h" // MarkdownModel fwd (WindowTab.h relies on it)
#include "FileHistory.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "ProfileData.h"
#include "ReadingStats.h"

struct DailyEntry {
    int dateKey; // YYYYMMDD
    int sec;
};

// dual-write gate: when false, skip writing the legacy sidecar file but keep
// streaming day rows into the single-file profile store (unconditional)
static bool kProfDualWriteSidecars = true;

static struct {
    Vec<DailyEntry> daily; // kept sorted by dateKey
    u64 lastTick = 0;      // wall-clock reference for the webview path
    float accumSec = 0.0f; // sub-second carry: ticks arrive ~64Hz with ~0.01s
    int dirtySec = 0;      // unsaved seconds (write threshold)
    bool loaded = false;   // sidecar parsed
} gReadingStats;

// YYYYMMDD for "today" in local time (bucket key; matches the heatmap layout)
static int DateKeyToday() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return (int)st.wYear * 10000 + (int)st.wMonth * 100 + (int)st.wDay;
}

// canonical sidecar location: <data-root>\stats\reading-stats.txt (s64-R9o)
static TempStr ReadingStatsPathTemp() {
    TempStr root = ProfileDataGetDataRootTemp();
    TempStr statsDir = path::JoinTemp(root, StrL("stats"));
    return path::JoinTemp(statsDir, StrL("reading-stats.txt"));
}

// legacy sidecar lived next to the settings file — kept readable as fallback
// and as the copy source for the one-time migration; never deleted
static TempStr ReadingStatsLegacyPathTemp() {
    TempStr settingsPath = GetSettingsPathTemp();
    TempStr dir = path::GetDirTemp(Str(settingsPath));
    return path::JoinTemp(dir, StrL("reading-stats.txt"));
}

// copy-only migration (s64-R9o): on load, if the new sidecar is missing but
// the legacy one exists, COPY it under <data-root>\stats. Never moves/deletes
// the legacy file. Best-effort: a failed copy logs and the legacy file is
// still read via the fallback path.
static void ReadingStatsMigrateIfNeeded() {
    TempStr p = ReadingStatsPathTemp();
    if (file::Exists(p)) {
        return;
    }
    TempStr legacy = ReadingStatsLegacyPathTemp();
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

static void ReadingStatsLoadIfNeeded() {
    if (gReadingStats.loaded) {
        return;
    }
    gReadingStats.loaded = true;
    ReadingStatsMigrateIfNeeded();
    TempStr p = ReadingStatsPathTemp();
    if (!file::Exists(p)) {
        // fallback read: legacy sidecar next to the settings file
        p = ReadingStatsLegacyPathTemp();
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
        int n = 0;
        if (p1 != buf) {
            n = 1;
            char* p2 = p1;
            secL = strtol(p2, &p2, 10);
            if (p2 != p1) {
                n = 2;
            }
        }
        if (n == 2 && dateKeyL > 19000101 && secL >= 0) {
            DailyEntry e{(int)dateKeyL, (int)secL};
            gReadingStats.daily.Append(e);
        }
    }
    // keep sorted by dateKey (simple insertion since the file is small)
    for (int i = 1; i < len(gReadingStats.daily); i++) {
        DailyEntry e = gReadingStats.daily[i];
        int j = i - 1;
        while (j >= 0 && gReadingStats.daily[j].dateKey > e.dateKey) {
            gReadingStats.daily[j + 1] = gReadingStats.daily[j];
            j--;
        }
        gReadingStats.daily[j + 1] = e;
    }
}

static void ReadingStatsAddDaily(float sec) {
    ReadingStatsLoadIfNeeded();
    int key = DateKeyToday();
    for (int i = 0; i < len(gReadingStats.daily); i++) {
        if (gReadingStats.daily[i].dateKey == key) {
            gReadingStats.daily[i].sec += (int)sec;
            return;
        }
    }
    DailyEntry e{key, (int)sec};
    gReadingStats.daily.Append(e);
}

void ReadingStatsAddSeconds(MainWindow* win, float sec) {
    if (!win || sec <= 0.0f) {
        return;
    }
    // ticks arrive at ~64Hz with ~0.01s each — accumulate the fractional part
    // and only credit whole seconds (the stores hold Int seconds)
    gReadingStats.accumSec += sec;
    int whole = (int)gReadingStats.accumSec;
    if (whole <= 0) {
        return;
    }
    gReadingStats.accumSec -= (float)whole;

    {
        Str group = gGlobalPrefs->activeTabGroup;
        if (group.s && *group.s) {
            ProfileDataAddSessionReadingSec(group, whole);
        }
    }

    // per-document time (FileState is the same store the open-count uses)
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->filePath) {
        FileState* fs = gFileHistory.FindByPath(tab->filePath);
        if (fs) {
            fs->readingTimeSec += whole;
            ProfileDataAddReadingSec(tab->filePath.s, whole);
        }
    }
    ReadingStatsAddDaily((float)whole);
    gReadingStats.dirtySec += whole;
    if (gReadingStats.dirtySec >= 60) {
        ReadingStatsFlush(false);
    }
}

void ReadingStatsTickWebview(MainWindow* win) {
    u64 now = GetTickCount64();
    if (gReadingStats.lastTick == 0) {
        gReadingStats.lastTick = now;
        return;
    }
    // the rAF loop scrolls continuously; credit the real elapsed wall-clock,
    // capped so a laptop-sleep gap never inflates a reading session
    float dt = (float)(now - gReadingStats.lastTick) / 1000.0f;
    gReadingStats.lastTick = now;
    if (dt > 0.0f && dt <= 1.5f) {
        ReadingStatsAddSeconds(win, dt);
    }
}

void ReadingStatsResetTick() {
    gReadingStats.lastTick = 0;
}

void ReadingStatsFlush(bool force) {
    ProfileDataFlushSessionsIfNeeded();
    if (!force && gReadingStats.dirtySec == 0) {
        return;
    }
    ReadingStatsLoadIfNeeded();
    str::Builder sb;
    for (int i = 0; i < len(gReadingStats.daily); i++) {
        DailyEntry& e = gReadingStats.daily[i];
        sb.Append(fmt("%d %d\r\n", e.dateKey, e.sec));
    }
    Str data = sb.TakeStr();
    if (kProfDualWriteSidecars) {
        TempStr root = ProfileDataGetDataRootTemp();
        TempStr statsDir = path::JoinTemp(root, StrL("stats"));
        dir::CreateAll(Str(statsDir));
        file::WriteFile(ReadingStatsPathTemp(), data);
    }
    str::Free(data);
    gReadingStats.dirtySec = 0;
    // stream the day rows into the profile store too (own-machine rows); the
    // profile save flushes AFTER, in the same cadence
    for (int i = 0; i < len(gReadingStats.daily); i++) {
        ProfileDataUpsertReadingDay(gReadingStats.daily[i].dateKey, gReadingStats.daily[i].sec);
    }
    ProfileDataSaveNow();
}

int ReadingStatsGetDaySec(int dateKey) {
    ReadingStatsLoadIfNeeded();
    for (int i = 0; i < len(gReadingStats.daily); i++) {
        if (gReadingStats.daily[i].dateKey == dateKey) {
            return gReadingStats.daily[i].sec;
        }
    }
    return 0;
}

// all-time reading time, independent of the per-document FileState entries:
// summing the daily buckets means a renamed/deleted/moved document cannot lose
// the time it contributed (the sidecar is keyed by date, not by path)
int ReadingStatsGetTotalSec() {
    ReadingStatsLoadIfNeeded();
    int total = 0;
    for (int i = 0; i < len(gReadingStats.daily); i++) {
        total += gReadingStats.daily[i].sec;
    }
    return total;
}
