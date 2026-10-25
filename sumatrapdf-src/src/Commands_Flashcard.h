/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Commands_Flashcard_h
#define Commands_Flashcard_h

struct MainWindow;

// Handle flashcard-related commands. Called from the main command dispatch.
// Returns true if the command was handled.
bool HandleCommandFlashcard(MainWindow* win, int cmd);

// Flashcard study timer tick: called from Canvas.cpp when kFlashcardStudyTimerID fires.
// Credits study time to the current card (capped at 60s/card), updates the main total
// and per-book study time, and advances/auto-pauses when the cap is reached.
void FlashcardStudyTick(MainWindow* win, HWND hwnd);

// Start/stop the flashcard study timer (kFlashcardStudyTimerID on hwndCanvas).
void FlashcardStudyTimerStart(MainWindow* win);
void FlashcardStudyTimerStop(MainWindow* win);

// Flashcard study timer UI: progress bar at bottom of screen that shrinks
// symmetrically to center over 60 seconds.
void FlashcardStudyTimerUIStart(MainWindow* win);
void FlashcardStudyTimerUIStop(MainWindow* win);
void FlashcardStudyTimerUITick(MainWindow* win);

// Show the "timer done" overlay (beep + timer glyph) when 60s cap is reached.
void FlashcardStudyTimerShowExpiry(MainWindow* win);

#endif