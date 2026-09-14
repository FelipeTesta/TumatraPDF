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

    // Get main toolbar rebar height for card count label sizing
    Rect rcMainRebar = HwndWindowRect(win->hwndReBar);

    // Card counter label as child of the flashcard rebar (positioned at left edge)
    HWND hwndCount = CreateWindowExW(0, L"STATIC", L"Cards: 0/0/0 | ",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        DpiScale(hwndParent, 4), 0, DpiScale(hwndParent, 120), rcMainRebar.dy,
        hwndRebar, nullptr, hinst, nullptr);
    win->flashcard.hwndCardCount = hwndCount;

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
        {CmdFlashcardFilter, _TRN("Filter")},
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

    // Apply theme settings (dark mode, custom draw, etc.) to match arch tools toolbar
    ToolbarApplyThemeToRebar(hwndRebar, hwndToolbar);

    // Set font for card count label to match toolbar font
    SendMessageW(win->flashcard.hwndCardCount, WM_SETFONT, (WPARAM)SendMessageW(hwndToolbar, WM_GETFONT, 0, 0), TRUE);

    Rect rc = TbGetItemRect(hwndToolbar, 0);

    ShowWindow(hwndToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE;
    rbBand.hbmBack = nullptr;
    rbBand.lpText = (WCHAR*)L"Flashcard Toolbar"; // NOLINT
    rbBand.hwndChild = hwndToolbar;
    // Approximate width: button width * number of buttons
    rbBand.cxMinChild = rc.dx * kFlashcardToolbarButtonsCount;
    rbBand.cyMinChild = rc.dy + (2 * rc.y);
    rbBand.cx = 0;
    SendMessageW(hwndRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    ShowWindow(hwndRebar, SW_SHOW);

    win->flashcard.hwndReBarFlashcard = hwndRebar;
    win->flashcard.hwndToolbarFlashcard = hwndToolbar;

    logf("FC: FlashcardToolbarCreate - toolbar created with %d buttons\n", kFlashcardToolbarButtonsCount);

    // Update the card count label
    FlashcardToolbarUpdateCount(win);

    logf("FC: FlashcardToolbarCreate: created secondary toolbar\n");
}

void FlashcardToolbarDestroy(MainWindow* win) {
    logf("FC: FlashcardToolbarDestroy - destroying secondary toolbar\n");
    if (!win->flashcard.hwndToolbarFlashcard) {
        return;
    }
    DestroyWindow(win->flashcard.hwndToolbarFlashcard);
    win->flashcard.hwndToolbarFlashcard = nullptr;
    if (win->flashcard.hwndCardCount) {
        DestroyWindow(win->flashcard.hwndCardCount);
        win->flashcard.hwndCardCount = nullptr;
    }
    DestroyWindow(win->flashcard.hwndReBarFlashcard);
    win->flashcard.hwndReBarFlashcard = nullptr;

    logf("FC: FlashcardToolbarDestroy - done\n");
    logf("FC: FlashcardToolbarDestroy: destroyed secondary toolbar\n");
}

// Update the flashcard toolbar card count label: "Cards: {total}/{new}/{due} | "
void FlashcardToolbarUpdateCount(MainWindow* win) {
    if (!win->flashcard.hwndCardCount) {
        return;
    }
    int total = len(win->flashcard.cards);
    int newCount = 0;
    int dueCount = 0;
    i64 now = (i64)time(nullptr) * 1000; // milliseconds since epoch
    for (int i = 0; i < total; i++) {
        const Flashcard& card = win->flashcard.cards[i];
        // Find study state
        bool found = false;
        for (int j = 0; j < len(win->flashcard.studyDoc.states); j++) {
            if (win->flashcard.studyDoc.states.els[j].annotId == card.annotId) {
                found = true;
                const FlashcardStudyState& s = win->flashcard.studyDoc.states.els[j].state;
                if (s.rating == 0) newCount++;
                else if (s.nextReviewAt <= now) dueCount++;
                break;
            }
        }
        if (!found) newCount++;
    }
    TempStr text = fmt("Cards: %d/%d/%d | ", total, newCount, dueCount);
    SetWindowTextW(win->flashcard.hwndCardCount, CWStrTemp(ToWStrTemp(Str(text))));
}