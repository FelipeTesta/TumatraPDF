/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef Commands_Edit_h
#define Commands_Edit_h

struct MainWindow;

// Handle edit-mode commands (Edit toggle, +Text, +Img). Called from the main
// command dispatch. Returns true if the command was handled.
bool HandleCommandEdit(MainWindow* win, int cmd);

// Place the armed (pending) clipboard image at canvas position (x, y) as a
// Stamp annotation. Returns true if the click was consumed by edit mode.
bool EditToolsPlacePendingImage(MainWindow* win, int x, int y);

// Free the pending clipboard image (if any) and disarm placement.
void EditToolsClearPendingImage(MainWindow* win);

#endif
