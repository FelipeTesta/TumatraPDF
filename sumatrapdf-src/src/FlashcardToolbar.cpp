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
#include "Theme.h"
#include "Flashcard.h"

// Flashcard secondary toolbar (study controls).
// Created when flashcard mode is toggled on.
// Pattern follows Arch Tools secondary toolbar (Toolbar2).

void FlashcardToolbarCreate(MainWindow* win) {
    if (win->flashcard.hwndToolbarFlashcard) {
        return; // already created
    }
    logf("[fc] FlashcardToolbarCreate - creating secondary toolbar\n");
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;

    // Create the rebar that hosts the flashcard toolbar below the main toolbar
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | RBS_VARHEIGHT | CCS_NODIVIDER | CCS_NOPARENTALIGN;
    if (IsCurrentThemeDefault()) {
        style |= WS_BORDER | RBS_BANDBORDERS;
    }
    DWORD exStyle = WS_EX_TOOLWINDOW;
    HWND hwndRebar = CreateWindowExW(exStyle, REBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndParent, (HMENU)IDC_REBAR,
                                     hinst, nullptr);

    REBARINFO rbi{};
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask = 0;
    rbi.himl = (HIMAGELIST) nullptr;
    SendMessageW(hwndRebar, RB_SETBARINFO, 0, (LPARAM)&rbi);

    // Create the flashcard toolbar inside the rebar
    style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | TBSTYLE_LIST | CCS_NODIVIDER |
            CCS_NOPARENTALIGN | TBSTYLE_WRAPABLE;
    exStyle = 0;
    bool isRtl = (GetWindowLong(hwndParent, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;
    HWND hwndToolbar = CreateWindowExW(exStyle, TOOLBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndRebar,
                                       (HMENU)IDC_TOOLBAR, hinst, nullptr);
    TbSetButtonStructSize(hwndToolbar, sizeofi(TBBUTTON));

    // Add study-control buttons (text-only, no icons)
    struct ToolbarButtonInfo2 {
        int cmdId;
        Str toolTip;
    };
    static ToolbarButtonInfo2 gFlashcardToolbarButtons[] = {
        {0, StrL("Cards: 0/0/0 | ")}, // card count display (button index 0, idCommand=0 = no action)
        {CmdFlashcardStudy, _TRN("Study")}, {CmdFlashcardReveal, _TRN("Reveal")}, {CmdFlashcardBack, _TRN("Back")},
        {CmdFlashcardLista, _TRN("Lista")}, {CmdFlashcardFilter, _TRN("Filter")},
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

    Rect rc = TbGetItemRect(hwndToolbar, 0);

    ShowWindow(hwndToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE;
    if (IsAppThemed() && IsCurrentThemeDefault()) {
        rbBand.fStyle |= RBBS_CHILDEDGE;
    }
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

    logf("[fc] FlashcardToolbarCreate - toolbar created with %d buttons\n", kFlashcardToolbarButtonsCount);

    // Update the card count label
    FlashcardToolbarUpdateCount(win);

    logf("[fc] FlashcardToolbarCreate: created secondary toolbar\n");
}

void FlashcardToolbarDestroy(MainWindow* win) {
    logf("[fc] FlashcardToolbarDestroy - destroying secondary toolbar\n");
    if (!win->flashcard.hwndToolbarFlashcard) {
        return;
    }
    DestroyWindow(win->flashcard.hwndToolbarFlashcard);
    win->flashcard.hwndToolbarFlashcard = nullptr;
    DestroyWindow(win->flashcard.hwndReBarFlashcard);
    win->flashcard.hwndReBarFlashcard = nullptr;

    logf("[fc] FlashcardToolbarDestroy - done\n");
    logf("[fc] FlashcardToolbarDestroy: destroyed secondary toolbar\n");
}

// Update the flashcard toolbar card count label: "Cards: {total}/{new}/{due} | "
void FlashcardToolbarUpdateCount(MainWindow* win) {
    if (!win->flashcard.hwndToolbarFlashcard) {
        return;
    }
    int total = len(win->flashcard.cards);
    int newCount = 0;
    int dueCount = 0;
    i64 now = (i64)time(nullptr) * 1000;
    for (int i = 0; i < total; i++) {
        const Flashcard& card = win->flashcard.cards[i];
        bool found = false;
        for (int j = 0; j < len(win->flashcard.studyDoc.states); j++) {
            if (win->flashcard.studyDoc.states.els[j].key == card.key) {
                found = true;
                const FlashcardStudyState& s = win->flashcard.studyDoc.states.els[j].state;
                if (s.rating == 0)
                    newCount++;
                else if (s.nextReviewAt <= now)
                    dueCount++;
                break;
            }
        }
        if (!found) newCount++;
    }
    TempStr text = fmt("Cards: %d/%d/%d | ", total, newCount, dueCount);
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(TBBUTTONINFOW);
    bi.dwMask = TBIF_TEXT;
    bi.pszText = (WCHAR*)CWStrTemp(ToWStrTemp(Str(text)));
    SendMessageW(win->flashcard.hwndToolbarFlashcard, TB_SETBUTTONINFOW, 0, (LPARAM)&bi);
}

// Reflect study/reveal state on the toolbar buttons (checked = active).
void FlashcardToolbarUpdateState(MainWindow* win) {
    if (!win->flashcard.hwndToolbarFlashcard) {
        return;
    }
    HWND hwnd = win->flashcard.hwndToolbarFlashcard;
    WPARAM studyState = win->flashcard.studyMode ? (TBSTATE_ENABLED | TBSTATE_CHECKED) : TBSTATE_ENABLED;
    WPARAM revealState = win->flashcard.revealMode ? (TBSTATE_ENABLED | TBSTATE_CHECKED) : TBSTATE_ENABLED;
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardStudy, MAKELONG(studyState, 0));
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardReveal, MAKELONG(revealState, 0));
}