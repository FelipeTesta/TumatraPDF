/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#include "base/Base.h"
#include "Commands_View.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "Commands.h"
#include "MainWindow.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "Notifications.h"
#include "SumatraDialogs.h"
#include "Selection.h"
#include "Menu.h"
#include "Translations.h"

// Set zoom to fit width and continuous view
void HandleCmdZoomFitWidthAndContinuous(MainWindow* win) {
    ChangeZoomLevel(win, kZoomFitWidth, true);
}

// Set zoom to fit page and single page view
void HandleCmdZoomFitPageAndSinglePage(MainWindow* win) {
    ChangeZoomLevel(win, kZoomFitPage, false);
}

// Zoom out
void HandleCmdZoomOut(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    auto zoom = win->ctrl->GetNextZoomStep(kZoomMin);
    Point mousePos = HwndGetCursorPos(win->hwndCanvas);
    SmartZoom(win, zoom, &mousePos, true);
}

// Zoom in
void HandleCmdZoomIn(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    auto zoom = win->ctrl->GetNextZoomStep(kZoomMax);
    Point mousePos = HwndGetCursorPos(win->hwndCanvas);
    SmartZoom(win, zoom, &mousePos, true);
}

// Zoom to 6400%
void HandleCmdZoom6400(MainWindow* win) {
    OnMenuZoom(win, CmdZoom6400);
}

// Zoom to 3200%
void HandleCmdZoom3200(MainWindow* win) {
    OnMenuZoom(win, CmdZoom3200);
}

// Zoom to 1600%
void HandleCmdZoom1600(MainWindow* win) {
    OnMenuZoom(win, CmdZoom1600);
}

// Zoom to 800%
void HandleCmdZoom800(MainWindow* win) {
    OnMenuZoom(win, CmdZoom800);
}

// Zoom to 400%
void HandleCmdZoom400(MainWindow* win) {
    OnMenuZoom(win, CmdZoom400);
}

// Zoom to 200%
void HandleCmdZoom200(MainWindow* win) {
    OnMenuZoom(win, CmdZoom200);
}

// Zoom to 150%
void HandleCmdZoom150(MainWindow* win) {
    OnMenuZoom(win, CmdZoom150);
}

// Zoom to 100%
void HandleCmdZoom100(MainWindow* win) {
    OnMenuZoom(win, CmdZoom100);
}

// Zoom to 75%
void HandleCmdZoom75(MainWindow* win) {
    OnMenuZoom(win, CmdZoom75);
}

// Zoom to 50%
void HandleCmdZoom50(MainWindow* win) {
    OnMenuZoom(win, CmdZoom50);
}

// Zoom to 25%
void HandleCmdZoom25(MainWindow* win) {
    OnMenuZoom(win, CmdZoom25);
}

// Zoom to 12.5%
void HandleCmdZoom12_5(MainWindow* win) {
    OnMenuZoom(win, CmdZoom12_5);
}

// Zoom to 8.33%
void HandleCmdZoom8_33(MainWindow* win) {
    OnMenuZoom(win, CmdZoom8_33);
}

// Zoom to fit page
void HandleCmdZoomFitPage(MainWindow* win) {
    OnMenuZoom(win, CmdZoomFitPage);
}

// Zoom to fit width
void HandleCmdZoomFitWidth(MainWindow* win) {
    OnMenuZoom(win, CmdZoomFitWidth);
}

// Zoom to fit height
void HandleCmdZoomFitHeight(MainWindow* win) {
    OnMenuZoom(win, CmdZoomFitHeight);
}

// Zoom to fit by orientation
void HandleCmdZoomFitByOrientation(MainWindow* win) {
    OnMenuZoom(win, CmdZoomFitByOrientation);
}

// Zoom to fit content
void HandleCmdZoomFitContent(MainWindow* win) {
    OnMenuZoom(win, CmdZoomFitContent);
}

// Zoom to shrink to fit
void HandleCmdZoomShrinkToFit(MainWindow* win) {
    OnMenuZoom(win, CmdZoomShrinkToFit);
}

// Zoom to actual size
void HandleCmdZoomActualSize(MainWindow* win) {
    OnMenuZoom(win, CmdZoomActualSize);
}

// Custom zoom
void HandleCmdZoomCustom(MainWindow* win) {
    OnMenuCustomZoom(win);
}

// Zoom to selection
void HandleCmdZoomToSelection(MainWindow* win) {
    ZoomToSelection(win);
}

// Single page view
void HandleCmdSinglePageView(MainWindow* win) {
    SwitchToDisplayMode(win, DisplayMode::SinglePage, true);
    ShowViewModeNotification(win, CmdSinglePageView);
}

// Facing view
void HandleCmdFacingView(MainWindow* win) {
    SwitchToDisplayMode(win, DisplayMode::Facing, true);
    ShowViewModeNotification(win, CmdFacingView);
}

// Book view
void HandleCmdBookView(MainWindow* win) {
    SwitchToDisplayMode(win, DisplayMode::BookView, true);
    ShowViewModeNotification(win, CmdBookView);
}

// Toggle continuous view
void HandleCmdToggleContinuousView(MainWindow* win) {
    ToggleContinuousView(win);
}

// Toggle manga mode
void HandleCmdToggleMangaMode(MainWindow* win) {
    ToggleMangaModeInternal(win);
}

// Toggle presentation mode
void HandleCmdTogglePresentationMode(MainWindow* win) {
    TogglePresentationMode(win);
}

// Toggle fullscreen
void HandleCmdToggleFullscreen(MainWindow* win) {
    ToggleFullScreen(win, false);
}

// Rotate left
void HandleCmdRotateLeft(MainWindow* win) {
    RotateDocument(win, -90);
}

// Rotate right
void HandleCmdRotateRight(MainWindow* win) {
    RotateDocument(win, 90);
}

// toggles 'show pages continuously' state
void ToggleContinuousView(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }

    DisplayMode newMode = win->ctrl->GetDisplayMode();
    switch (newMode) {
        case DisplayMode::SinglePage:
        case DisplayMode::Continuous:
            newMode = IsContinuous(newMode) ? DisplayMode::SinglePage : DisplayMode::Continuous;
            break;
        case DisplayMode::Facing:
        case DisplayMode::ContinuousFacing:
            newMode = IsContinuous(newMode) ? DisplayMode::Facing : DisplayMode::ContinuousFacing;
            break;
        case DisplayMode::BookView:
        case DisplayMode::ContinuousBookView:
            newMode = IsContinuous(newMode) ? DisplayMode::BookView : DisplayMode::ContinuousBookView;
            break;
    }
    SwitchToDisplayMode(win, newMode, false);
}

void ShowViewModeNotification(MainWindow* win, int cmdId) {
    NotificationWnd* wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (wnd) {
        return;
    }
    Str viewName;
    if (cmdId == CmdSinglePageView) {
        viewName = _TRA("Single Page");
    } else if (cmdId == CmdFacingView) {
        viewName = _TRA("Facing");
    } else if (cmdId == CmdBookView) {
        viewName = _TRA("Book View");
    } else {
        return;
    }
    TempStr msg = fmt("%s: %s", _TRA("View"), viewName);
    NotificationCreateArgs args;
    args.groupId = kNotifZoomOrView;
    args.timeoutMs = 2000;
    args.hwndParent = win->hwndCanvas;
    args.msg = msg;
    ShowNotification(args);
}

// Zoom so that the current selection (Ctrl + drag rectangle or selected text)
// fills the window, and centre it. The selection itself is left alone so it can
// still be copied afterwards, and a navigation point is added first so Back
// returns to the view you zoomed from (issue #1699).
// Note: ZoomToSelection and ChangeZoomLevel are now non-static in SumatraPDF.cpp
// and are called directly by the command handlers in this file.

void TogglePresentationMode(MainWindow* win) {
    // only DisplayModel currently supports an actual presentation mode
    ToggleFullScreen(win, win->AsFixed() != nullptr);
}

void OnMenuCustomZoom(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }

    float virtZoom = win->ctrl->GetZoomVirtual();
    if (!Dialog_CustomZoom(win->hwndFrame, IsBrowserDocController(win->ctrl), &virtZoom)) {
        return;
    }
    SmartZoom(win, virtZoom, nullptr, true);
}