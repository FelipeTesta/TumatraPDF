/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Commands_Flashcard_h
#define Commands_Flashcard_h

struct MainWindow;

bool HandleCommandFlashcard(MainWindow* win, int cmd);
void FlashcardStudyTick(MainWindow* win, HWND hwnd);
void FlashcardStudyTimerStart(MainWindow* win);
void FlashcardStudyTimerStop(MainWindow* win);
void FlashcardStudyTimerUIStart(MainWindow* win);
void FlashcardStudyTimerUIStop(MainWindow* win);
void FlashcardShowTimerDoneOverlay(MainWindow* win);
void FlashcardCreateStudyTimerControls(MainWindow* win);

#endif
