/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef FlashcardStats_h
#define FlashcardStats_h

struct MainWindow;

void FlashcardStatsAddSeconds(MainWindow* win, float sec);
void FlashcardStatsAddCard(MainWindow* win);
void FlashcardStatsFlush(bool force);
int FlashcardStatsGetDaySec(int dateKey);
int FlashcardStatsGetDayCards(int dateKey);
int FlashcardStatsGetTotalSec();
int FlashcardStatsGetTotalCards();
void FlashcardStatsOnSessionEnd();

#endif
