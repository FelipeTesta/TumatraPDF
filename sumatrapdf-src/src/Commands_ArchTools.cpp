/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#include "base/Base.h"
#include "SumatraLog.h"
#include "Commands_ArchTools.h"

#include "Flashcard.h"
#include "Settings.h"
#include "GlobalPrefs.h"
#include "Commands.h"
#include "MainWindow.h"
#include "Toolbar.h"
#include "DocController.h"

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
    int pageNo = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    auto& ps = win->archTools.GetScaleForPage(pageNo);
    LogInfo("[arch] CmdArchScale: page %d, opening scale dialog", pageNo);
    if (!ps.scaleLineDefined) {
        win->archTools.toolMode = 1;
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

// HandleCmdArchClear — clear only current page's measurements
void HandleCmdArchClear(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int pageNo = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    int count = 0;
    Vec<ArchMeasurement> remaining;
    for (int i = 0; i < len(win->archTools.measurements); i++) {
        if (win->archTools.measurements[i].pageNo == pageNo) {
            count++;
        } else {
            remaining.Append(win->archTools.measurements[i]);
        }
    }
    win->archTools.measurements = remaining;
    LogInfo("[arch] CmdArchClear: cleared %d measurements on page %d", count, pageNo);
    ScheduleRepaint(win, 0);
}

// HandleCmdArchResetScale — reset only current page's scale
void HandleCmdArchResetScale(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int pageNo = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    auto& ps = win->archTools.GetScaleForPage(pageNo);
    ps.scaleSet = false;
    ps.scaleLineDefined = false;
    ps.scaleFactor = 0.0f;
    ps.scaleLineP1x = ps.scaleLineP1y = 0.0f;
    ps.scaleLineP2x = ps.scaleLineP2y = 0.0f;
    LogInfo("[arch] CmdArchResetScale: reset scale for page %d only", pageNo);
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
        // Close flashcard if open (mutually exclusive toolbars)
        if (win->flashcard.on) {
            win->flashcard.on = false;
            FlashcardToolbarDestroy(win);
            win->flashcard.studyMode = false;
            win->flashcard.revealMode = false;
            win->flashcard.currentCardIdx = -1;
            win->flashcard.studyOrder.Reset();
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

// HandleCmdArchCleanAll — clear ALL pages
void HandleCmdArchCleanAll(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int count = (int)len(win->archTools.measurements);
    win->archTools.measurements.Reset();
    win->archTools.scaleStates.Reset();
    win->archTools.toolMode = 0;
    if (win->hwndCanvas) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    LogInfo("[arch] CmdArchCleanAll: cleared %d measurements + all page scales", count);
    UpdateToolbar2State(win);
    ScheduleRepaint(win, 0);
}