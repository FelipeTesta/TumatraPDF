/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef ReadingStats_h
#define ReadingStats_h

struct MainWindow;

// credit `sec` seconds of reading time to the current document + the daily
// bucket; flushes the daily file when enough unsaved time accumulated
void ReadingStatsAddSeconds(MainWindow* win, float sec);

// webview (markdown) documents scroll via rAF inside the page; the guardian
// tick only runs occasionally — credit the wall-clock delta since the last
// call (sanitized), not the tick interval
void ReadingStatsTickWebview(MainWindow* win);

// drop the webview wall-clock reference (call on every stall/stop so a later
// tick doesn't credit the gap)
void ReadingStatsResetTick();

// write the daily-stats sidecar file; pass force=false to skip when nothing
// changed since the last write
void ReadingStatsFlush(bool force);

// seconds of reading time recorded for the given day (YYYYMMDD); 0 when none
int ReadingStatsGetDaySec(int dateKey);

#endif
