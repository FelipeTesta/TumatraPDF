/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#include "base/Base.h"
#include "SumatraLog.h"
#include "Commands_ArchTools.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "Commands.h"
#include "MainWindow.h"
#include "Toolbar.h"

// Forward declarations — these are defined in other translation units
void ShowArchScaleDialog(MainWindow* win);
void UpdateToolbar2State(MainWindow*);
void SetToolbarButtonCheckedState(MainWindow* win, int cmdId, bool isChecked);
bool RelayoutFrame(MainWindow* win, bool updateToolbars, int sidebarDx);
void ScheduleRepaint(MainWindow* win, int delay);
bool IsCurrentDocMarkdown(MainWindow* win);

// HandleCmdArchScale — open floating scale dialog
void HandleCmdArchScale(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    LogInfo("[arch] CmdArchScale: opening scale dialog");
    if (!win->archTools.scaleLineDefined) {
        win->archTools.toolMode = 1; // scale drawing mode
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_CROSS));
        }
    }
    ShowArchScaleDialog(win);
}

// HandleCmdArchMeasure — toggle measure mode
void HandleCmdArchMeasure(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int oldMode = win->archTools.toolMode;
    if (win->archTools.toolMode == 2) {
        win->archTools.toolMode = 0;
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
        }
    } else {
        win->archTools.toolMode = 2; // Measure mode
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_CROSS));
        }
    }
    LogInfo("[arch] CmdArchMeasure: archToolMode=%d -> %d", oldMode, win->archTools.toolMode);
    UpdateToolbar2State(win);
}

// HandleCmdArchClear — clear measurements only, keep scale calibration intact
void HandleCmdArchClear(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int count = (int)len(win->archTools.measurements);
    win->archTools.measurements.Reset();
    LogInfo("[arch] CmdArchClear: cleared %d measurements (scale kept)", count);
    ScheduleRepaint(win, 0);
}

// HandleCmdArchResetScale — reset only the scale calibration (keep measurements)
void HandleCmdArchResetScale(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    win->archTools.scaleSet = false;
    win->archTools.scaleLineDefined = false;
    win->archTools.scaleFactor = 0.0f;
    win->archTools.scaleLineP1x = win->archTools.scaleLineP1y = 0.0f;
    win->archTools.scaleLineP2x = win->archTools.scaleLineP2y = 0.0f;
    LogInfo("[arch] CmdArchResetScale: reset scale only");
    ScheduleRepaint(win, 0);
}

// HandleCmdArchToolsToggle — toggle arch tools on/off
void HandleCmdArchToolsToggle(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    if (IsCurrentDocMarkdown(win)) return;
    win->archTools.on = !win->archTools.on;
    if (win->archTools.on) {
        if (win->archTools.hwndReBar2) {
            ShowWindow(win->archTools.hwndReBar2, SW_SHOW);
        }
        UpdateToolbar2State(win);
    } else {
        if (win->archTools.hwndReBar2) {
            ShowWindow(win->archTools.hwndReBar2, SW_HIDE);
        }
        win->archTools.toolMode = 0;
        win->archTools.eraseMode = false;
        win->archTools.dragLine = 0;
        win->archTools.point1 = Point{0, 0};
        win->archTools.point2 = Point{0, 0};
        if (GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
        win->mouseAction = MouseAction::None;
    }
    // Explicitly set checked state for the toggle button (fixes underline drawing)
    SetToolbarButtonCheckedState(win, CmdArchToolsToggle, win->archTools.on);
    LogInfo("[arch] CmdArchToolsToggle: archToolsOn=%d", win->archTools.on);
    RelayoutFrame(win, true, -1);
    ScheduleRepaint(win, 0);
}