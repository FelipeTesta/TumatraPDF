/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "Commands.h"
#include "Settings.h"
#include "GlobalPrefs.h"
#include "Translations.h"
#include "resource.h"
#include "MainWindow.h"
#include "Toolbar.h"
#include "ToolbarLayout.h"
#include "Flashcard.h"

// Flashcard secondary toolbar (study controls).
// Created when flashcard mode is toggled on.
// Pattern follows Arch Tools secondary toolbar (Toolbar2).

void FlashcardToolbarCreate(MainWindow* win) {
    if (win->flashcard.hwndToolbarFlashcard) {
        return; // already created
    }
    logf("FC: FlashcardToolbarCreate - creating secondary toolbar\n");
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;

    // Create the rebar that hosts the flashcard toolbar below the main toolbar
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | RBS_VARHEIGHT | CCS_NODIVIDER | CCS_NOPARENTALIGN;
    DWORD exStyle = WS_EX_TOOLWINDOW;
    HWND hwndRebar = CreateWindowExW(exStyle, REBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndParent,
                                     (HMENU)IDC_REBAR, hinst, nullptr);

    REBARINFO rbi{};
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask = 0;
    rbi.himl = (HIMAGELIST)nullptr;
    SendMessageW(hwndRebar, RB_SETBARINFO, 0, (LPARAM)&rbi);

    // Position the rebar directly below the main toolbar rebar
    Rect rcMainRebar = HwndWindowRect(win->hwndReBar);
    Rect rcFrame = HwndWindowRect(hwndParent);
    SetWindowPos(hwndRebar, nullptr, 0, rcMainRebar.dy, rcFrame.dx, 0, SWP_NOZORDER);

    // Create the flashcard toolbar inside the rebar
    style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | TBSTYLE_LIST | CCS_NODIVIDER |
            CCS_NOPARENTALIGN | TBSTYLE_WRAPABLE;
    exStyle = 0;
    HWND hwndToolbar = CreateWindowExW(exStyle, TOOLBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndRebar,
                                       (HMENU)IDC_TOOLBAR, hinst, nullptr);
    TbSetButtonStructSize(hwndToolbar, sizeofi(TBBUTTON));

    // Add study-control buttons (text-only, no icons)
    struct ToolbarButtonInfo2 {
        int cmdId;
        Str toolTip;
    };
    static ToolbarButtonInfo2 gFlashcardToolbarButtons[] = {
        {CmdFlashcardStudy, _TRN("Study")},
        {CmdFlashcardBack, _TRN("Back")},
        {CmdFlashcardLista, _TRN("Lista")},
    };
    constexpr int kFlashcardToolbarButtonsCount = dimof(gFlashcardToolbarButtons);

    TBBUTTON tbButtons[kFlashcardToolbarButtonsCount];
    for (int i = 0; i < kFlashcardToolbarButtonsCount; i++) {
        const ToolbarButtonInfo2& bi = gFlashcardToolbarButtons[i];
        TBBUTTON b{};
        b.idCommand = bi.cmdId;
        b.iBitmap = 0;
        b.fsState = TBSTATE_ENABLED;
        b.fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_AUTOSIZE;
        Str s = trans::GetTranslation(bi.toolTip);
        b.iString = (INT_PTR)CWStrTemp(s);
        tbButtons[i] = b;
    }
    TbAddButtons(hwndToolbar, kFlashcardToolbarButtonsCount, tbButtons);

    // Use the same icon/button size as the main toolbar
    int iconSize = DpiScale(hwndParent, gGlobalPrefs->toolbarSize);
    iconSize = RoundUp(iconSize, 4);
    TbSetBitmapSize(hwndToolbar, Size(iconSize, iconSize));
    TbSetButtonSize(hwndToolbar, Size(iconSize, iconSize));

    Rect rc = TbGetItemRect(hwndToolbar, 0);

    ShowWindow(hwndToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE;
    rbBand.hbmBack = nullptr;
    rbBand.lpText = (WCHAR*)L"Flashcard Toolbar"; // NOLINT
    rbBand.hwndChild = hwndToolbar;
    rbBand.cxMinChild = rc.dx * kFlashcardToolbarButtonsCount;
    rbBand.cyMinChild = rc.dy + (2 * rc.y);
    rbBand.cx = 0;
    SendMessageW(hwndRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    ShowWindow(hwndRebar, SW_SHOW);

    win->flashcard.hwndReBarFlashcard = hwndRebar;
    win->flashcard.hwndToolbarFlashcard = hwndToolbar;

    logf("FC: FlashcardToolbarCreate - toolbar created with %d buttons\n", 3);

    // Reposition floating child windows (timer/speed/ETA) now that the secondary toolbar exists
    LayoutToolbarChildWindows(win);

    logf("FC: FlashcardToolbarCreate: created secondary toolbar\n");
}

void FlashcardToolbarDestroy(MainWindow* win) {
    logf("FC: FlashcardToolbarDestroy - destroying secondary toolbar\n");
    if (!win->flashcard.hwndToolbarFlashcard) {
        return;
    }
    DestroyWindow(win->flashcard.hwndToolbarFlashcard);
    win->flashcard.hwndToolbarFlashcard = nullptr;
    DestroyWindow(win->flashcard.hwndReBarFlashcard);
    win->flashcard.hwndReBarFlashcard = nullptr;

    LayoutToolbarChildWindows(win);

    logf("FC: FlashcardToolbarDestroy - done\n");
    logf("FC: FlashcardToolbarDestroy: destroyed secondary toolbar\n");
}