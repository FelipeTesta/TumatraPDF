/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#include "base/Base.h"
#include "Commands_AutoScroll.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AutoScroll.h"
#include "Commands.h"
#include "MainWindow.h"

// Start middle-click-style auto-scroll at current cursor position
void HandleCmdStartAutoScroll(MainWindow* win) {
    StartAutoScrollAtCursor(win);
}

// Toggle continuous auto-scroll on/off
void HandleCmdAutoScrollToggle(MainWindow* win) {
    AutoScrollToggle(win);
}

// Increase continuous auto-scroll speed
void HandleCmdAutoScrollSpeedUp(MainWindow* win) {
    AutoScrollSpeedAdjust(win, +1);
}

// Decrease continuous auto-scroll speed
void HandleCmdAutoScrollSpeedDown(MainWindow* win) {
    AutoScrollSpeedAdjust(win, -1);
}

// Toggle auto-scroll timer on/off
void HandleCmdAutoScrollTimerToggle(MainWindow* win) {
    bool on = (IsDlgButtonChecked(win->hwndToolbar, CmdAutoScrollTimerToggle) == BST_CHECKED);
    win->autoScroll.timerEnabled = on;
    if (win->autoScroll.active) {
        win->autoScroll.timerMinutes = on ? win->autoScroll.timerMinutesSetting : 0;
        win->autoScroll.startTick = GetTickCount();
    }
    gGlobalPrefs->autoScrollTimerEnabled = on;
}

// Edit auto-scroll timer minutes
void HandleCmdAutoScrollTimerEdit(MainWindow* win) {
    WCHAR buf[32] = {0};
    GetWindowTextW(win->autoScroll.hwndTimerEdit, buf, 32);
    int m = _wtoi(buf);
    if (m < 1) m = 1;
    if (m > 600) m = 600;
    win->autoScroll.timerMinutesSetting = (DWORD)m;
    if (win->autoScroll.active && win->autoScroll.timerEnabled) {
        win->autoScroll.timerMinutes = (DWORD)m;
        win->autoScroll.startTick = GetTickCount();
    }
    gGlobalPrefs->autoScrollTimerMinutes = m;
}