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
        {CmdFlashcardStudy, _TRN("Study")},
        {CmdFlashcardReveal, _TRN("Reveal")},
        {CmdFlashcardBack, _TRN("Back")},
        {CmdFlashcardNext, _TRN("Next")},
        {CmdFlashcardOrderToggle, _TRN("Order")},
        {CmdFlashcardLista, _TRN("Lista")},
        {CmdFlashcardFilter, _TRN("Filter")},
        {CmdFlashcardCleanHistory, _TRN("Clean")},
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
    // Order button: checked while the study queue is shuffled (random)
    WPARAM orderState =
        gGlobalPrefs->flashcardSettings.randomOrder ? (TBSTATE_ENABLED | TBSTATE_CHECKED) : TBSTATE_ENABLED;
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardOrderToggle, MAKELONG(orderState, 0));
    // Next button: only meaningful in study mode with a card after the current
    // one (state 0 = grayed out)
    bool hasNext = win->flashcard.studyMode && win->flashcard.currentCardIdx + 1 < len(win->flashcard.studyOrder);
    WPARAM nextState = hasNext ? TBSTATE_ENABLED : 0;
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardNext, MAKELONG(nextState, 0));
}

// ---- Clean History confirmation dialog -----------------------------------
// Destructive-action guard: "Yes" must be clicked AND HELD for 2 seconds to
// confirm. While holding, a red line under the "Yes" text drains (shrinks
// right-to-left); releasing early resets it. "No" / Esc / closing cancels.
// Returns true only when the 2s hold completed.

struct CleanHistoryDialog {
    HWND hwnd = nullptr;
    RECT yesRect{}; // client coords, recomputed in WM_SIZE
    RECT noRect{};
    bool holding = false;
    ULONGLONG holdStartTick = 0;
    double progress = 1.0; // 1 = full line, drains to 0 during the hold
    bool confirmed = false;
};

static CleanHistoryDialog* gCleanHistoryDialog = nullptr;

static void CleanHistoryLayout(CleanHistoryDialog* d) {
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int pad = DpiScale(d->hwnd, 14);
    int btnW = DpiScale(d->hwnd, 110);
    int btnH = DpiScale(d->hwnd, 44);
    int gap = DpiScale(d->hwnd, 12);
    int yBtn = h - pad - btnH;
    int totalW = 2 * btnW + gap;
    int x0 = (w - totalW) / 2;
    d->noRect = {x0, yBtn, x0 + btnW, yBtn + btnH};
    d->yesRect = {x0 + btnW + gap, yBtn, x0 + 2 * btnW + gap, yBtn + btnH};
}

static void CleanHistoryDrawButton(HDC hdc, const RECT& rc, const WCHAR* label, bool hot) {
    HBRUSH bg = CreateSolidBrush(hot ? RGB(228, 230, 234) : RGB(240, 240, 240));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(170, 170, 170));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(20, 20, 20));
    RECT rcText = rc;
    DrawTextW(hdc, label, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void CleanHistoryPaint(CleanHistoryDialog* d) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->hwnd, &ps);
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    // question text, wrapped, centered in the area above the buttons
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(20, 20, 20));
    HFONT font = (HFONT)SendMessageW(d->hwnd, WM_GETFONT, 0, 0);
    HFONT oldFont = font ? (HFONT)SelectObject(hdc, font) : nullptr;
    RECT rcText = rc;
    rcText.bottom = d->yesRect.top - DpiScale(d->hwnd, 8);
    InflateRect(&rcText, -DpiScale(d->hwnd, 12), 0);
    DrawTextW(hdc, L"tem certeza que deseja limpar o histórico de revisões?", -1, &rcText,
              DT_CENTER | DT_VCENTER | DT_WORDBREAK);

    CleanHistoryDrawButton(hdc, d->noRect, L"No", false);

    // "Yes": while holding, a line below the text drains right-to-left
    CleanHistoryDrawButton(hdc, d->yesRect, L"Yes", d->holding);
    int lineH = DpiScale(d->hwnd, 3);
    int inset = DpiScale(d->hwnd, 10);
    int yLine = d->yesRect.bottom - DpiScale(d->hwnd, 9);
    int fullW = d->yesRect.right - d->yesRect.left - 2 * inset;
    int wNow = (int)(fullW * d->progress);
    RECT lineIdle = {d->yesRect.left + inset, yLine, d->yesRect.right - inset, yLine + lineH};
    HBRUSH hint = CreateSolidBrush(RGB(214, 214, 214));
    FillRect(hdc, &lineIdle, hint); // subtle idle hint: full-width track
    DeleteObject(hint);
    if (d->holding && wNow > 0) {
        RECT line = {d->yesRect.left + inset, yLine, d->yesRect.left + inset + wNow, yLine + lineH};
        HBRUSH red = CreateSolidBrush(RGB(229, 57, 53));
        FillRect(hdc, &line, red);
        DeleteObject(red);
    }
    if (oldFont) {
        SelectObject(hdc, oldFont);
    }
    EndPaint(d->hwnd, &ps);
}

static LRESULT CALLBACK CleanHistoryWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    CleanHistoryDialog* d = gCleanHistoryDialog;
    if (msg == WM_CREATE) {
        gCleanHistoryDialog->hwnd = hwnd;
        return 0;
    }
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_SIZE:
            CleanHistoryLayout(d);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_ERASEBKGND:
            return 1; // painted in WM_PAINT
        case WM_PAINT:
            CleanHistoryPaint(d);
            return 0;
        case WM_LBUTTONDOWN: {
            POINT pt{(short)LOWORD(lp), (short)HIWORD(lp)};
            if (PtInRect(&d->yesRect, pt)) {
                d->holding = true;
                d->holdStartTick = GetTickCount64();
                d->progress = 1.0;
                SetCapture(hwnd);
                SetTimer(hwnd, 1, 15, nullptr);
                logf("[fc] CleanHistory - hold start\n");
                InvalidateRect(hwnd, &d->yesRect, FALSE);
            } else if (PtInRect(&d->noRect, pt)) {
                logf("[fc] CleanHistory - cancelled (No)\n");
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_TIMER: {
            if (!d->holding || wp != 1) {
                return 0;
            }
            ULONGLONG elapsed = GetTickCount64() - d->holdStartTick;
            d->progress = 1.0 - (double)elapsed / 2000.0;
            if (elapsed >= 2000) {
                d->holding = false;
                d->confirmed = true;
                logf("[fc] CleanHistory - hold CONFIRMED (2s)\n");
                DestroyWindow(hwnd);
                return 0;
            }
            InvalidateRect(hwnd, &d->yesRect, FALSE);
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            if (d->holding) {
                d->holding = false;
                d->progress = 1.0;
                logf("[fc] CleanHistory - hold released early\n");
            }
            KillTimer(hwnd, 1);
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            InvalidateRect(hwnd, &d->yesRect, FALSE);
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                logf("[fc] CleanHistory - cancelled (Esc)\n");
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_CLOSE:
            logf("[fc] CleanHistory - cancelled (close)\n");
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool FlashcardCleanHistoryDialog(HWND hwndParent) {
    static const WCHAR* className = L"TUMATRA_FLASHCARD_CLEAN";
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = CleanHistoryWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr; // painted in WM_PAINT
        wc.lpszClassName = className;
        RegisterClassExW(&wc);
        registered = true;
    }

    CleanHistoryDialog dlg{};
    gCleanHistoryDialog = &dlg; // stack-local: safe, modal pump below owns it
    int cx = DpiScale(hwndParent, 420);
    int cy = DpiScale(hwndParent, 190);
    RECT rw;
    GetWindowRect(hwndParent, &rw);
    int x = rw.left + ((rw.right - rw.left) - cx) / 2;
    int y = rw.top + ((rw.bottom - rw.top) - cy) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, className, L"Clean History", WS_POPUP | WS_CAPTION | WS_SYSMENU, x,
                                y, cx, cy, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwnd) {
        gCleanHistoryDialog = nullptr;
        return false;
    }
    logf("[fc] CleanHistory - dialog opened\n");

    // modal pump: disable the parent while the dialog is up
    EnableWindow(hwndParent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetActiveWindow(hwnd);
    MSG msg;
    while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(hwndParent, TRUE);
    SetActiveWindow(hwndParent);
    gCleanHistoryDialog = nullptr;
    return dlg.confirmed;
}