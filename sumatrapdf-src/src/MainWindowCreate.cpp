/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/IconPixmap.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "Commands.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "Toolbar.h"
#include "Tabs.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "TableOfContents.h"
#include "Favorites.h"
#include "Notifications.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "Installer.h"
#include "SumatraProperties.h"
#include "Canvas.h"
#include "AIChatPanel.h"
#include "DarkModeSubclass.h"
#include "Theme.h"
#include "SumatraLog.h"

#include "MainWindowCreate.h"

void ShowMainWindow(MainWindow* win, int windowState) {
    if (WIN_STATE_FULLSCREEN == windowState || WIN_STATE_MAXIMIZED == windowState) {
        ShowWindow(win->hwndFrame, SW_MAXIMIZE);
    } else {
        ShowWindow(win->hwndFrame, SW_SHOW);
    }

    // Fire the deferred SWP_FRAMECHANGED for custom caption (tabsInTitlebar).
    // Must happen after ShowWindow so the shell sees a visible window and
    // creates the taskbar button before we remove the standard frame.
    if (win->tabsInTitlebar) {
        uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
        SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);
    }

    // go fullscreen before the first paint so the user doesn't see the
    // intermediate maximized window (EnterFullScreen requires a visible
    // window, so it can't happen before ShowWindow above)
    if (WIN_STATE_FULLSCREEN == windowState) {
        EnterFullScreen(win);
    }

    // Hidden startup windows can miss the final titlebar/menu-bar geometry
    // until they become visible. Force one relayout before the first paint.
    RelayoutFrame(win);
    UpdateWindow(win->hwndFrame);
    UpdateToolbarFindText(win);
    HwndEnsureOnScreen(win->hwndFrame);

    if (IsRunningOnWine()) {
        Rect wr = HwndWindowRect(win->hwndFrame);
        Rect cr = HwndClientRect(win->hwndFrame);
        logf("ShowMainWindow: windowRect=(%d,%d,%d,%d) clientRect=(%d,%d,%d,%d) captionRect=(%d,%d,%d,%d)\n", wr.x,
             wr.y, wr.dx, wr.dy, cr.x, cr.y, cr.dx, cr.dy, win->captionRect.x, win->captionRect.y, win->captionRect.dx,
             win->captionRect.dy);
    }

    // the `true ||` is deliberate (always foreground); silence /analyze C6286/C6240
#pragma warning(suppress : 6286 6240)
    if (len(gWindows) == 1 && (true || IsDebuggerPresent())) {
        HwndToForeground(win->hwndFrame);
    }

    if (win->tabsInTitlebar && !win->isFullScreen) {
        RECT r = ToRECT(win->captionRect);
        HwndInvalidateRect(win->hwndFrame, win->captionRect, true);
        RedrawWindow(win->hwndFrame, &r, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
        if (win->hwndMenuReBar && HwndIsVisible(win->hwndMenuReBar)) {
            RedrawWindow(win->hwndMenuReBar, nullptr, nullptr,
                         RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        }
        if (win->tabsCtrl && win->tabsCtrl->IsVisible()) {
            RedrawWindow(win->tabsCtrl->hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
        }
    }
}

// Kind used so only one default-app bar is shown at a time
static Kind kNotifDefaultApp = "defaultApp";
// Cap how many extension links we put in the bar
constexpr int kMaxDefaultAppLinks = 8;

// On the home page: if we registered as Open With for extensions that no longer
// open with us, show a bottom bar with per-extension fix links.
void MaybeShowDefaultAppNotification(MainWindow* win) {
    if (!win || !win->hwndCanvas || win->isBeingClosed) {
        return;
    }
    if (!win->IsCurrentTabAbout()) {
        return;
    }
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }
    if (!IsOurExeInstalled()) {
        return;
    }

    StrVec missing;
    CollectNonDefaultRegisteredExtensions(missing);
    if (len(missing) == 0) {
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifDefaultApp);
        return;
    }

    // "SumatraPDF is no longer the default app for opening [pdf](CmdFixDefaultApp .pdf), ..."
    str::Builder sb;
    sb.Append(StrL("SumatraPDF is no longer the default app for opening "));
    int nShow = std::min(len(missing), kMaxDefaultAppLinks);
    for (int i = 0; i < nShow; i++) {
        if (i > 0) {
            sb.Append(StrL(", "));
        }
        Str ext = missing[i]; // ".pdf"
        // link text without the leading dot: "pdf"
        Str label = (len(ext) > 0 && ext.s[0] == '.') ? Str(ext.s + 1, ext.len - 1) : ext;
        sb.Append(fmt("[%s](CmdFixDefaultApp %s)", label, ext));
    }
    if (len(missing) > nShow) {
        sb.Append(fmt(" and %d more", len(missing) - nShow));
    }
    sb.Append(StrL(". Click a link to fix."));

    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.msg = ToStrTemp(sb);
    args.timeoutMs = kNotifNoTimeout;
    args.groupId = kNotifDefaultApp;
    args.corner = NotifCorner::BottomBar;
    ShowNotification(args);
}

MainWindow* CreateAndShowMainWindow(SessionData* data, bool showWin) {
    int windowState = gGlobalPrefs->windowState;
    MainWindow* win = CreateMainWindow();
    if (!win) {
        return nullptr;
    }
    // CreateMainWindow can inadvertently change windowState (e.g. via layout); restore it
    gGlobalPrefs->windowState = windowState;

    if (data) {
        windowState = data->windowState;
        Rect rect = ShiftRectToWorkArea(data->windowPos);
        HwndMoveWindow(win->hwndFrame, &rect);
        // TODO: also restore data->sidebarDx
    }

    // always set up toolbar and sidebar, even if we defer showing
    ShowOrHideToolbar(win);
    SetSidebarVisibility(win, false, gGlobalPrefs->showFavorites);
    ToolbarUpdateStateForWindow(win, true);

    if (showWin) {
        ShowMainWindow(win, windowState);
    }
    return win;
}

void DeleteMainWindow(MainWindow* win) {
    int winIdx = gWindows.Remove(win);

    int nWindowsLeft = len(gWindows);
    logf("DeleteMainWindow: win: 0x%p, hwndFrame: 0x%p, hwndCanvas: 0x%p, winIdx : %d, nWindowsLeft: %d\n", win,
         win->hwndFrame, win->hwndCanvas, winIdx, nWindowsLeft);
    if (winIdx < 0) {
        logf("  not deleting because not in gWindows, probably already deleted\n");
        return;
    }

    DeletePropertiesWindow(win->hwndFrame);
    ImageList_Destroy(TbGetImageList(win->hwndToolbar));
    RevokeCanvasDropTarget(win->hwndCanvas);

    ReportIf(win->findThread && WaitForSingleObject(win->findThread, 0) == WAIT_TIMEOUT);
    ReportIf(win->printThread && WaitForSingleObject(win->printThread, 0) == WAIT_TIMEOUT);

    // UIA disconnect/release is in ~MainWindow

    delete win;
}

void UpdateAfterThemeChange() {
    // the toolbar image list is rebuilt below, so the icons cached from it are
    // the wrong color now
    ClearIconPixmapCache();
    for (auto* win : gWindows) {
        DeleteObject(win->brControlBgColor);
        win->brControlBgColor = CreateSolidBrush(ThemeControlBackgroundColor());

        UpdateControlsColors(win);
        RebuildMenuBarForWindow(win);
        UpdateToolbarAfterThemeChange(win);
        RecreateFindBar(win);
        UpdateFindWindowTheme(win);
        UpdateAIChatTheme(win);
        if (UseDarkModeLib()) {
            DarkMode::setDarkTitleBarEx(win->hwndFrame, true);
            DarkMode::setChildCtrlsTheme(win->hwndFrame);
            if (win->tabsCtrl) {
                DarkMode::removeTabCtrlSubclass(win->tabsCtrl->hwnd);
            }
            DarkMode::setDarkScrollBar(win->hwndCanvas);
            DarkMode::setWindowMenuBarSubclass(win->hwndFrame);
            ApplyDarkModeToInfotip(win);
        }
        UpdateWindowFrameBorderColor(win);
        // TODO: this only rerenders canvas, not frame, even with
        // includingNonClientArea == true.
        MainWindowRerender(win, true);
        uint flags = RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN;
        RedrawWindow(win->hwndFrame, nullptr, nullptr, flags);
    }
    UpdateDocumentColors();
}