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

// Flashcard study save worker (Phase 4): async per-book JSON write
void FlashcardStudySaveAsyncWorker(const Vec<FlashcardStudyDoc::StateEntry>& states, const char* resolvedPath, Str docName, Str docPath, int version);
bool FlashcardSaveWorkerJoinWorker();
// Wait for the async save worker to finish all pending writes (bounded timeout).
// Returns true if all writes completed, false on timeout.
bool FlashcardWaitForSaveWorker();

#endif
