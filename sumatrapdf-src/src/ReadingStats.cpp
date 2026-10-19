/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

// Reading-time statistics (FASE B "Stats"): seconds spent reading with
// auto-scroll active, per document (FileState.readingTimeSec, persisted with
// the normal file-history settings) and per calendar day (sidecar file
// "reading-stats.txt" next to the settings file, one "YYYYMMDD <sec>" line
// per day — deliberately plain text, no JSON dependency).

#include "base/Base.h"
#include "base/File.h"

#include "Settings.h" // FileState (FileHistory.h relies on it being declared)
#include "AppSettings.h"
#include "DocController.h" // MarkdownModel fwd (WindowTab.h relies on it)
#include "FileHistory.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "ReadingStats.h"

struct DailyEntry {
    int dateKey; // YYYYMMDD
    int sec;
};

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

static TempStr ReadingStatsPathTemp() {
    TempStr settingsPath = GetSettingsPathTemp();
    TempStr dir = path::GetDirTemp(settingsPath);
    return path::JoinTemp(dir, StrL("reading-stats.txt"));
}

static void ReadingStatsLoadIfNeeded() {
    if (gReadingStats.loaded) {
        return;
    }
    gReadingStats.loaded = true;
    TempStr p = ReadingStatsPathTemp();
    if (!file::Exists(p)) {
        return;
    }
    Str d = file::ReadFile(p);
    StrVec lines;
    Split(&lines, d, StrL("\n"), true);
    for (Str line : lines) {
        char buf[64]{};
        int lenCopy = std::min(len(line), (int)(dimof(buf) - 1));
        memcpy(buf, line.s, (size_t)lenCopy);
        int dateKey = 0;
        int sec = 0;
        int n = sscanf(buf, "%d %d", &dateKey, &sec);
        if (n == 2 && dateKey > 19000101 && sec >= 0) {
            DailyEntry e{dateKey, sec};
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

    // per-document time (FileState is the same store the open-count uses)
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->filePath) {
        FileState* fs = gFileHistory.FindByPath(tab->filePath);
        if (fs) {
            fs->readingTimeSec += whole;
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
    file::WriteFile(ReadingStatsPathTemp(), data);
    str::Free(data);
    gReadingStats.dirtySec = 0;
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
