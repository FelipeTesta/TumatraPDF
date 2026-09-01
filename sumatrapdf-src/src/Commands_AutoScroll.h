/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef Commands_AutoScroll_h
#define Commands_AutoScroll_h

struct MainWindow;

// AutoScroll command handlers (extracted from SumatraPDF.cpp)
void HandleCmdStartAutoScroll(MainWindow* win);
void HandleCmdAutoScrollToggle(MainWindow* win);
void HandleCmdAutoScrollSpeedUp(MainWindow* win);
void HandleCmdAutoScrollSpeedDown(MainWindow* win);
void HandleCmdAutoScrollTimerToggle(MainWindow* win);
void HandleCmdAutoScrollTimerEdit(MainWindow* win);

#endif