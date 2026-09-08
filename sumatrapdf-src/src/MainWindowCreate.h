/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef MainWindowCreate_h
#define MainWindowCreate_h

struct MainWindow;
struct SessionData;

// UpdateAfterThemeChange was only declared as `extern` in Theme.cpp, not in a
// shared header; added here so MainWindowCreate.cpp can define it.
void UpdateAfterThemeChange();

// Helpers promoted from `static` to external linkage so MainWindowCreate.cpp can
// call them. Their bodies stay in SumatraPDF.cpp (CreateMainWindow is
// deliberately NOT extracted; 16C-F6).
MainWindow* CreateMainWindow();
void UpdateWindowFrameBorderColor(MainWindow* win);
void ApplyDarkModeToInfotip(MainWindow* win);

// RelayoutFrame is defined in SumatraPDF.cpp but was only forward-declared
// there; exported so MainWindowCreate.cpp (ShowMainWindow) can call it.
// Returns false when the relayout was skipped (nothing layout-affecting changed).
bool RelayoutFrame(MainWindow* win, bool updateToolbars = true, int sidebarDx = -1);

// CreateAndShowMainWindow, ShowMainWindow, MaybeShowDefaultAppNotification and
// DeleteMainWindow are already declared in SumatraPDF.h.

#endif