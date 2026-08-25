/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

struct MainWindow;

// AutoScroll module: logic extracted from Canvas.cpp, Toolbar.cpp, SumatraPDF.cpp
// State fields remain in MainWindow.h (documented there as "state consumed by AutoScroll module")

// Toggle continuous auto-scroll on/off
void AutoScrollToggle(MainWindow* win);

// Adjust continuous auto-scroll speed multiplier (+1 = faster, -1 = slower)
void AutoScrollSpeedAdjust(MainWindow* win, int direction);

// Start middle-click-style auto-scroll at current cursor position
void StartAutoScrollAtCursor(MainWindow* win);

// Timer tick handler for continuous auto-scroll (called from Canvas.cpp WM_TIMER)
void AutoScrollContinuousTick(MainWindow* win, HWND hwnd);

// Timer tick handler for middle-click auto-scroll (called from Canvas.cpp WM_TIMER)
void AutoScrollMiddleClickTick(MainWindow* win, HWND hwnd);

// Recompute ETA from remaining pages and current speed
void RecalcAutoScrollEta(MainWindow* win);

// Calculate effective scroll speed in pixels per second
float AutoScrollPxPerSec(MainWindow* win);