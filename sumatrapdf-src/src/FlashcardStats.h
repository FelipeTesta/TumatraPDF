/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef FlashcardStats_h
#define FlashcardStats_h

struct MainWindow;

// credit `sec` seconds of flashcard study time to the current document + the daily
// bucket; flushes the daily file when enough unsaved time accumulated
void FlashcardStatsAddSeconds(MainWindow* win, float sec);

// write the daily-stats sidecar file; pass force=false to skip when nothing
// changed since the last write
void FlashcardStatsFlush(bool force);

// seconds of flashcard study time recorded for the given day (YYYYMMDD); 0 when none
int FlashcardStatsGetDaySec(int dateKey);

// all-time flashcard study time (sum of every daily bucket). This is the MASTER total:
// it lives in the sidecar keyed by date, so it survives renaming, deleting or
// moving the documents that produced it
int FlashcardStatsGetTotalSec();

// Called when study session ends (flashcard mode off or session stopped)
// to flush any pending time
void FlashcardStatsOnSessionEnd();

#endif