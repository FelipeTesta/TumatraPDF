/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef Commands_Image_h
#define Commands_Image_h

struct MainWindow;

// timer id of the "Edit With Paint" temp-file watch (main window WM_TIMER)
constexpr int kPaintWatchTimerId = 41;

void HandleCmdReplaceImage(MainWindow* win);
void HandleCmdEditImageWithPaint(MainWindow* win);

// one poll step of the paint watch; call when the timer fires
void TickPaintImageWatch();

#endif
