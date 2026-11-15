/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef FlashcardStats_h
#define FlashcardStats_h

struct WindowTab;

// Attribution rule (s64-R9n): the per-document credit (FileState + profile
// perDoc) must go to the tab that OWNS the card being studied —
// studyOrder[currentCardIdx].tab — never to the user's focused tab
// (win->CurrentTab()), which can differ after a manual Ctrl+Tab mid-session.
// Pass the owning tab (may be null: per-doc credit is skipped, the
// window-level daily bucket is still credited).
void FlashcardStatsAddSeconds(WindowTab* tab, float sec);
void FlashcardStatsAddCard(WindowTab* tab);
void FlashcardStatsFlush(bool force);
int FlashcardStatsGetDaySec(int dateKey);
int FlashcardStatsGetDayCards(int dateKey);
int FlashcardStatsGetTotalSec();
int FlashcardStatsGetTotalCards();
void FlashcardStatsOnSessionEnd();

#endif
