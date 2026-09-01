/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef Commands_ArchTools_h
#define Commands_ArchTools_h

struct MainWindow;

// Arch Tools command handlers (extracted from SumatraPDF.cpp)
void HandleCmdArchScale(MainWindow* win);
void HandleCmdArchMeasure(MainWindow* win);
void HandleCmdArchClear(MainWindow* win);
void HandleCmdArchResetScale(MainWindow* win);
void HandleCmdArchToolsToggle(MainWindow* win);

#endif