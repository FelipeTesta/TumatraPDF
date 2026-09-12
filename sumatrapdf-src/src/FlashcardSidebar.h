/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef FlashcardSidebar_h
#define FlashcardSidebar_h

struct MainWindow;

void FlashcardSidebarCreate(MainWindow* win);
void FlashcardSidebarDestroy(MainWindow* win);
void FlashcardSidebarToggle(MainWindow* win);
void FlashcardSidebarPopulate(MainWindow* win);

#endif