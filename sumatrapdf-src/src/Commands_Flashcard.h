/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Commands_Flashcard_h
#define Commands_Flashcard_h

struct MainWindow;

// Handle flashcard-related commands. Called from the main command dispatch.
// Returns true if the command was handled.
bool HandleCommandFlashcard(MainWindow* win, int cmd);

#endif