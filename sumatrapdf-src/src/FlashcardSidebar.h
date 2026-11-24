/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef FlashcardSidebar_h
#define FlashcardSidebar_h

struct MainWindow;

void FlashcardSidebarCreate(MainWindow* win);
void FlashcardSidebarDestroy(MainWindow* win);
void FlashcardSidebarToggle(MainWindow* win);
// ensure the Lista panel is open (never hides it)
void FlashcardSidebarShow(MainWindow* win);
void FlashcardSidebarPopulate(MainWindow* win);
// Incremental update: refresh a single row whose study state changed (rating).
// Returns true if incremental update was performed, false if full rebuild needed.
bool FlashcardSidebarUpdateIncremental(MainWindow* win, int cardIdxInStudyOrder, WindowTab* tab);

#endif