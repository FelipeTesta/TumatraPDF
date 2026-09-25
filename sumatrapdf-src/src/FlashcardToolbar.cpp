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
#include "DocController.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
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
    HWND hwndRebar = CreateWindowExW(exStyle, REBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndParent,
                                     (HMENU)IDC_FLASHCARD_REBAR, hinst, nullptr);

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
                                       (HMENU)IDC_FLASHCARD_TOOLBAR, hinst, nullptr);
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
        {CmdFlashcardOrderOptions, _TRN("Order")},
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
// Reflects the study scope: current document only, or (global session mode)
// the sum over every PDF tab of THIS window.
void FlashcardToolbarUpdateCount(MainWindow* win) {
    if (!win->flashcard.hwndToolbarFlashcard) {
        return;
    }
    int total = 0;
    int newCount = 0;
    int dueCount = 0;
    i64 now = (i64)time(nullptr) * 1000;
    auto countTab = [&](WindowTab* tab) {
        for (int i = 0; i < len(tab->flashcard.cards); i++) {
            const Flashcard& card = tab->flashcard.cards[i];
            total++;
            bool found = false;
            for (int j = 0; j < len(tab->flashcard.studyDoc.states); j++) {
                if (tab->flashcard.studyDoc.states.els[j].key == card.key) {
                    found = true;
                    const FlashcardStudyState& s = tab->flashcard.studyDoc.states.els[j].state;
                    if (s.rating == 0) {
                        newCount++;
                    } else if (s.nextReviewAt <= now) {
                        dueCount++;
                    }
                    break;
                }
            }
            if (!found) {
                newCount++;
            }
        }
    };
    if (win->flashcard.crossDocSession) {
        auto tabs = win->Tabs();
        for (WindowTab* tab : tabs) {
            if (FlashcardEnsureTabCards(tab)) {
                countTab(tab);
            }
        }
    } else {
        WindowTab* tab = win->CurrentTab();
        if (tab && FlashcardEnsureTabCards(tab)) {
            countTab(tab);
        }
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
    // Next button: only meaningful in study mode with a card after the current
    // one (state 0 = grayed out)
    bool hasNext = win->flashcard.studyMode && win->flashcard.currentCardIdx + 1 < len(win->flashcard.studyOrder);
    WPARAM nextState = hasNext ? TBSTATE_ENABLED : 0;
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardNext, MAKELONG(nextState, 0));
    // Filter button: checked when the current tab has an ACTIVE page filter
    // (enabled + non-empty expression)
    WindowTab* tab = win->CurrentTab();
    bool filterActive = tab && tab->flashcard.filterEnabled && tab->flashcard.filterExpr;
    WPARAM filterState = filterActive ? (TBSTATE_ENABLED | TBSTATE_CHECKED) : TBSTATE_ENABLED;
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardFilter, MAKELONG(filterState, 0));
    // Lista button: checked while the Lista panel is open
    bool listaVisible = win->flashcard.hwndListaBox && win->uiState.fcListVisible;
    WPARAM listaState = listaVisible ? (TBSTATE_ENABLED | TBSTATE_CHECKED) : TBSTATE_ENABLED;
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardLista, MAKELONG(listaState, 0));
}

// ---- Clean History confirmation dialog -----------------------------------
// Destructive-action guard: two hold-to-confirm buttons, no "No" — X / Esc
// cancels. "Clear current book" holds 2s (orange drain line); "Clear ALL
// books" holds 5s (red drain line — longer hold for the more destructive
// action). Releasing early resets the animation. Returns FlashcardCleanResult.

struct CleanHistoryDialog {
    HWND hwnd = nullptr;
    RECT currentRect{};    // "Clear current book" (2s hold)
    RECT allRect{};        // "Clear ALL books" (5s hold)
    int holdingButton = 0; // 0 none, 1 current book, 2 all books
    ULONGLONG holdStartTick = 0;
    int holdMs = 2000;     // duration of the active hold (2000 or 5000)
    double progress = 1.0; // 1 = full line, drains to 0 during the hold
    int result = kFlashcardCleanCancelled;
};

static CleanHistoryDialog* gCleanHistoryDialog = nullptr;

static void CleanHistoryLayout(CleanHistoryDialog* d) {
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int pad = DpiScale(d->hwnd, 14);
    int btnW = DpiScale(d->hwnd, 170);
    int btnH = DpiScale(d->hwnd, 44);
    int gap = DpiScale(d->hwnd, 12);
    int yBtn = h - pad - btnH;
    int totalW = 2 * btnW + gap;
    int x0 = (w - totalW) / 2;
    d->currentRect = {x0, yBtn, x0 + btnW, yBtn + btnH};
    d->allRect = {x0 + btnW + gap, yBtn, x0 + 2 * btnW + gap, yBtn + btnH};
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

// one hold-to-confirm button: idle track line under the label + draining
// colored line while holding (progress 1 -> 0)
static void CleanHistoryDrawHoldButton(HWND hwnd, HDC hdc, const RECT& rc, const WCHAR* label, bool holding,
                                       double progress, COLORREF drainColor) {
    CleanHistoryDrawButton(hdc, rc, label, holding);
    int lineH = DpiScale(hwnd, 3);
    int inset = DpiScale(hwnd, 10);
    int yLine = rc.bottom - DpiScale(hwnd, 9);
    int fullW = rc.right - rc.left - 2 * inset;
    int wNow = (int)(fullW * progress);
    RECT lineIdle = {rc.left + inset, yLine, rc.right - inset, yLine + lineH};
    HBRUSH hint = CreateSolidBrush(RGB(214, 214, 214));
    FillRect(hdc, &lineIdle, hint); // subtle idle hint: full-width track
    DeleteObject(hint);
    if (holding && wNow > 0) {
        RECT line = {rc.left + inset, yLine, rc.left + inset + wNow, yLine + lineH};
        HBRUSH drain = CreateSolidBrush(drainColor);
        FillRect(hdc, &line, drain);
        DeleteObject(drain);
    }
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
    rcText.bottom = d->currentRect.top - DpiScale(d->hwnd, 8);
    InflateRect(&rcText, -DpiScale(d->hwnd, 12), 0);
    DrawTextW(hdc, L"tem certeza que deseja limpar o histórico de revisões?", -1, &rcText,
              DT_CENTER | DT_VCENTER | DT_WORDBREAK);

    // "Clear current book": orange drain, 2s hold
    CleanHistoryDrawHoldButton(d->hwnd, hdc, d->currentRect, L"Clear current book", d->holdingButton == 1,
                               d->holdingButton == 1 ? d->progress : 1.0, RGB(255, 152, 0));
    // "Clear ALL books": red drain, 5s hold
    CleanHistoryDrawHoldButton(d->hwnd, hdc, d->allRect, L"Clear ALL books", d->holdingButton == 2,
                               d->holdingButton == 2 ? d->progress : 1.0, RGB(229, 57, 53));
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
            int btn = 0;
            if (PtInRect(&d->currentRect, pt)) {
                btn = 1; // 2s hold
            } else if (PtInRect(&d->allRect, pt)) {
                btn = 2; // 5s hold — more destructive, longer hold
            }
            if (btn != 0) {
                d->holdingButton = btn;
                d->holdMs = (btn == 2) ? 5000 : 2000;
                d->holdStartTick = GetTickCount64();
                d->progress = 1.0;
                SetCapture(hwnd);
                SetTimer(hwnd, 1, 15, nullptr);
                logf("[fc] CleanHistory - hold start (button %d, %dms)\n", btn, d->holdMs);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_TIMER: {
            if (d->holdingButton == 0 || wp != 1) {
                return 0;
            }
            ULONGLONG elapsed = GetTickCount64() - d->holdStartTick;
            d->progress = 1.0 - (double)elapsed / (double)d->holdMs;
            if (elapsed >= (ULONGLONG)d->holdMs) {
                d->result = d->holdingButton;
                d->holdingButton = 0;
                logf("[fc] CleanHistory - hold CONFIRMED (button %d, %dms)\n", d->result, d->holdMs);
                DestroyWindow(hwnd);
                return 0;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            if (d->holdingButton != 0) {
                d->holdingButton = 0;
                d->progress = 1.0;
                logf("[fc] CleanHistory - hold released early\n");
            }
            KillTimer(hwnd, 1);
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
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

int FlashcardCleanHistoryDialog(HWND hwndParent) {
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
    int cx = DpiScale(hwndParent, 460);
    int cy = DpiScale(hwndParent, 200);
    RECT rw;
    GetWindowRect(hwndParent, &rw);
    int x = rw.left + ((rw.right - rw.left) - cx) / 2;
    int y = rw.top + ((rw.bottom - rw.top) - cy) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, className, L"Clean History", WS_POPUP | WS_CAPTION | WS_SYSMENU, x,
                                y, cx, cy, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwnd) {
        gCleanHistoryDialog = nullptr;
        return kFlashcardCleanCancelled;
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
    return dlg.result;
}

// ---- Study Order options dialog -------------------------------------------
// Opens from the toolbar "Order" button. Two groups:
//   Review order:  [Sequential] [Random]
//   New cards:     [Before due] [After due] [Mixed]
// Every option click applies INSTANTLY (persists the setting, rebuilds the
// study order and restarts from its first card when a session is active).
// Close with the X or Esc — there is no OK because everything already applied.

struct OrderOptionsDialog {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    RECT rcSeq{}, rcRandom{};
    RECT rcNewFirst{}, rcNewLast{}, rcNewMixed{};
    RECT rcBtns[5]{}; // hit-test helpers in the same order as the constants below
};

static OrderOptionsDialog* gOrderOptionsDialog = nullptr;

enum {
    kOrderOptSeq = 0,
    kOrderOptRandom = 1,
    kOrderOptNewFirst = 2,
    kOrderOptNewLast = 3,
    kOrderOptNewMixed = 4,
};

static void OrderOptionsLayout(OrderOptionsDialog* d) {
    int pad = DpiScale(d->hwnd, 14);
    int btnW = DpiScale(d->hwnd, 110);
    int btnH = DpiScale(d->hwnd, 40);
    int gap = DpiScale(d->hwnd, 10);
    int capDy = DpiScale(d->hwnd, 24);
    int y = pad;
    // group 1 caption + buttons
    y += capDy;
    int x = pad;
    d->rcSeq = {x, y, x + btnW, y + btnH};
    x += btnW + gap;
    d->rcRandom = {x, y, x + btnW, y + btnH};
    y += btnH + pad;
    // group 2 caption + buttons
    y += capDy;
    x = pad;
    d->rcNewFirst = {x, y, x + btnW, y + btnH};
    x += btnW + gap;
    d->rcNewLast = {x, y, x + btnW, y + btnH};
    x += btnW + gap;
    d->rcNewMixed = {x, y, x + btnW, y + btnH};
    y += btnH + pad;
    d->rcBtns[kOrderOptSeq] = d->rcSeq;
    d->rcBtns[kOrderOptRandom] = d->rcRandom;
    d->rcBtns[kOrderOptNewFirst] = d->rcNewFirst;
    d->rcBtns[kOrderOptNewLast] = d->rcNewLast;
    d->rcBtns[kOrderOptNewMixed] = d->rcNewMixed;
}

static void OrderOptionsDrawOption(HWND hwnd, HDC hdc, const RECT& rc, const WCHAR* label, bool checked) {
    // flat button: border + radio dot + label; the checked one gets the
    // accent border and a filled dot
    HBRUSH bg = CreateSolidBrush(checked ? RGB(232, 240, 254) : RGB(240, 240, 240));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
    HPEN pen = CreatePen(PS_SOLID, checked ? 2 : 1, checked ? RGB(66, 133, 244) : RGB(170, 170, 170));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    // radio dot at the left
    int cy = (rc.top + rc.bottom) / 2;
    int dotR = DpiScale(hwnd, 4); // dot radius, dpi-scaled
    RECT dot = {rc.left + dotR * 2, cy - dotR, rc.left + dotR * 2 + 2 * dotR, cy + dotR};
    HBRUSH dotBg = CreateSolidBrush(checked ? RGB(66, 133, 244) : RGB(255, 255, 255));
    HBRUSH oldDotBg = (HBRUSH)SelectObject(hdc, dotBg);
    Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
    SelectObject(hdc, oldDotBg);
    DeleteObject(dotBg);
    if (!checked) {
        HPEN dotPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
        HPEN oldDotPen = (HPEN)SelectObject(hdc, dotPen);
        Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
        SelectObject(hdc, oldDotPen);
        DeleteObject(dotPen);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(20, 20, 20));
    RECT rcText = rc;
    rcText.left = dot.right + DpiScale(hwnd, 4);
    DrawTextW(hdc, label, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void OrderOptionsPaint(OrderOptionsDialog* d) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->hwnd, &ps);
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(60, 60, 60));
    int pad = DpiScale(d->hwnd, 14);
    RECT cap1 = {pad, pad, rc.right - pad, pad + DpiScale(d->hwnd, 24)};
    DrawTextW(hdc, L"Review order", -1, &cap1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT cap2 = {pad, d->rcNewFirst.top - DpiScale(d->hwnd, 24), rc.right - pad, d->rcNewFirst.top};
    DrawTextW(hdc, L"New cards", -1, &cap2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    bool isRandom = gGlobalPrefs->flashcardSettings.randomOrder;
    int pos = gGlobalPrefs->flashcardSettings.newCardsPosition;
    OrderOptionsDrawOption(d->hwnd, hdc, d->rcSeq, L"Sequential", !isRandom);
    OrderOptionsDrawOption(d->hwnd, hdc, d->rcRandom, L"Random", isRandom);
    OrderOptionsDrawOption(d->hwnd, hdc, d->rcNewFirst, L"Before due", pos == 0);
    OrderOptionsDrawOption(d->hwnd, hdc, d->rcNewLast, L"After due", pos == 1);
    OrderOptionsDrawOption(d->hwnd, hdc, d->rcNewMixed, L"Mixed", pos == 2);
    EndPaint(d->hwnd, &ps);
}

static LRESULT CALLBACK OrderOptionsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    OrderOptionsDialog* d = gOrderOptionsDialog;
    if (msg == WM_CREATE) {
        gOrderOptionsDialog->hwnd = hwnd;
        return 0;
    }
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_SIZE:
            OrderOptionsLayout(d);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_ERASEBKGND:
            return 1; // painted in WM_PAINT
        case WM_PAINT:
            OrderOptionsPaint(d);
            return 0;
        case WM_LBUTTONDOWN: {
            POINT pt{(short)LOWORD(lp), (short)HIWORD(lp)};
            bool isRandom = gGlobalPrefs->flashcardSettings.randomOrder;
            int pos = gGlobalPrefs->flashcardSettings.newCardsPosition;
            int opt = -1;
            if (PtInRect(&d->rcSeq, pt)) {
                opt = kOrderOptSeq;
            } else if (PtInRect(&d->rcRandom, pt)) {
                opt = kOrderOptRandom;
            } else if (PtInRect(&d->rcNewFirst, pt)) {
                opt = kOrderOptNewFirst;
            } else if (PtInRect(&d->rcNewLast, pt)) {
                opt = kOrderOptNewLast;
            } else if (PtInRect(&d->rcNewMixed, pt)) {
                opt = kOrderOptNewMixed;
            }
            if (opt == kOrderOptSeq) {
                gGlobalPrefs->flashcardSettings.randomOrder = false;
            } else if (opt == kOrderOptRandom) {
                gGlobalPrefs->flashcardSettings.randomOrder = true;
            } else if (opt == kOrderOptNewFirst) {
                gGlobalPrefs->flashcardSettings.newCardsPosition = 0;
            } else if (opt == kOrderOptNewLast) {
                gGlobalPrefs->flashcardSettings.newCardsPosition = 1;
            } else if (opt == kOrderOptNewMixed) {
                gGlobalPrefs->flashcardSettings.newCardsPosition = 2;
            }
            if (opt >= 0) {
                logf("[fc] OrderOptions - order=%s newCardsPosition=%d (was order=%s pos=%d)\n",
                     gGlobalPrefs->flashcardSettings.randomOrder ? StrL("random") : StrL("sequential"),
                     gGlobalPrefs->flashcardSettings.newCardsPosition, isRandom ? StrL("random") : StrL("sequential"),
                     pos);
                // instant apply: persist + rebuild + restart from first card
                FlashcardApplyStudyOrder(d->win);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void FlashcardOrderOptionsDialog(MainWindow* win) {
    static const WCHAR* className = L"TUMATRA_FLASHCARD_ORDER";
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = OrderOptionsWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr; // painted in WM_PAINT
        wc.lpszClassName = className;
        RegisterClassExW(&wc);
        registered = true;
    }

    HWND hwndParent = win->hwndFrame;
    OrderOptionsDialog dlg{};
    dlg.win = win;
    gOrderOptionsDialog = &dlg; // stack-local: safe, modal pump below owns it
    int cx = DpiScale(hwndParent, 388);
    int cy = DpiScale(hwndParent, 218);
    RECT rw;
    GetWindowRect(hwndParent, &rw);
    int x = rw.left + ((rw.right - rw.left) - cx) / 2;
    int y = rw.top + ((rw.bottom - rw.top) - cy) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, className, L"Study Order", WS_POPUP | WS_CAPTION | WS_SYSMENU, x,
                                y, cx, cy, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwnd) {
        gOrderOptionsDialog = nullptr;
        return;
    }
    logf("[fc] OrderOptions - dialog opened\n");

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
    gOrderOptionsDialog = nullptr;
}

// ---- Study Filter dialog ---------------------------------------------------
// Opens from the toolbar "Filter" button. Layout (top to bottom):
//   [x] Estudar de todos os PDFs abertos (sessão)  — cross-doc session toggle
//   Filtro de páginas:  [EDIT expression]          — "1-15;20-25;-22-23;"
//   Capítulos (bookmarks):                          — checkbox mirror of the
//   +------------------------------------+           TOC; checking a bookmark
//   | [ ] Capítulo 1 (1-15)              |           injects its page range
//   | [ ] Capítulo 2 (16-30)             |           into the expression
//   +------------------------------------+
//   [Aplicar] [Filtros: ON/OFF] [Limpar filtros]    — clear is a 2s hold
//
// Apply happens on: Aplicar button, Enter in the edit, every checkbox click,
// the ON/OFF toggle, the cross-doc toggle and the Clear hold. X / Esc closes
// and keeps the last APPLIED state (pending typed text is not applied).
// Each book keeps its own filter (per-tab); the cross-doc checkbox is the
// window-level session scope: current document vs all PDF tabs of THIS
// window (tabs dragged to another window no longer count).

enum {
    IDC_FC_FILTER_EDIT = 60001,
    IDC_FC_FILTER_LIST = 60002,
};

struct FilterBookmarkItem {
    TocItem* tocItem = nullptr; // borrowed: owned by tab->currToc
    int depth = 0;
    int pageFrom = 1;
    int pageTo = 1;
};

struct FilterOptionsDialog {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr; // the tab whose filter is being edited
    HWND hwndEdit = nullptr;
    HWND hwndList = nullptr;
    Vec<FilterBookmarkItem> bmItems;
    Vec<bool> bmChecked;
    RECT rcCrossDoc{};
    RECT rcEditCaption{}, rcListCaption{};
    RECT rcApply{}, rcToggle{}, rcClear{};
    int holdingClear = 0; // 0 none, 1 holding the Clear button
    ULONGLONG holdStartTick = 0;
    int holdMs = 2000;
    double progress = 1.0; // 1 = full drain line, drains to 0 during the hold
};

static FilterOptionsDialog* gFilterOptionsDialog = nullptr;
static WNDPROC gFilterEditOrigProc = nullptr;

// Flatten the TOC depth-first in document order: an item, then its whole
// subtree, then its next sibling. Items with no valid page are skipped but
// still traversed (their children may point to pages).
static void FilterFlattenTocItem(TocItem* item, int depth, Vec<TocItem*>& flat, Vec<int>& depths) {
    if (!item) {
        return;
    }
    if (item->pageNo >= 1) {
        flat.Append(item);
        depths.Append(depth);
    }
    FilterFlattenTocItem(item->child, depth + 1, flat, depths);
    FilterFlattenTocItem(item->next, depth, flat, depths);
}

// Build the bookmark mirror. A bookmark's range runs from its own page to the
// page BEFORE the next bookmark in flattened order (children included), so a
// parent's range naturally covers all its sub-levels; the last one extends
// to the end of the document.
static void FilterBuildBookmarkItems(FilterOptionsDialog* d) {
    d->bmItems.Reset();
    d->bmChecked.Reset();
    DisplayModel* dm = d->tab->AsFixed();
    if (!dm) {
        return;
    }
    int pageCount = dm->PageCount();
    TocTree* toc = d->tab->currToc;
    if (!toc || !toc->root) {
        logf("[fc] Filter - no TOC, bookmark mirror empty\n");
        return;
    }
    Vec<TocItem*> flat;
    Vec<int> depths;
    for (TocItem* child = toc->root->child; child; child = child->next) {
        FilterFlattenTocItem(child, 0, flat, depths);
    }
    for (int i = 0; i < len(flat); i++) {
        FilterBookmarkItem item;
        item.tocItem = flat[i];
        item.depth = depths[i];
        item.pageFrom = flat[i]->pageNo;
        item.pageTo = (i + 1 < len(flat)) ? flat[i + 1]->pageNo - 1 : pageCount;
        if (item.pageTo < item.pageFrom) {
            item.pageTo = item.pageFrom; // next bookmark starts on the same page
        }
        d->bmItems.Append(item);
        d->bmChecked.Append(false);
    }
    logf("[fc] Filter - bookmark mirror: %d items\n", len(d->bmItems));
}

// Read the edit control's text (NUL-terminated by GetWindowTextW)
static Str FilterGetEditText(HWND hwndEdit) {
    WCHAR buf[4096];
    int n = GetWindowTextW(hwndEdit, buf, dimofi(buf));
    if (n <= 0) {
        return Str();
    }
    return ToUtf8Temp(WStr(buf));
}

static void FilterSetEditText(HWND hwndEdit, Str text) {
    SetWindowTextW(hwndEdit, CWStrTemp(ToWStrTemp(text)));
}

// Save the edit's expression as the tab's filter and rebuild the session
// queue (when a session is active). Called by Aplicar / Enter / checkbox.
static void FilterApplyText(FilterOptionsDialog* d) {
    Str text = FilterGetEditText(d->hwndEdit);
    str::ReplaceWithCopy(&d->tab->flashcard.filterExpr, text);
    logf("[fc] Filter - applied expr '%s' (enabled=%d) on '%s'\n", d->tab->flashcard.filterExpr,
         d->tab->flashcard.filterEnabled ? 1 : 0, FlashcardTabLogName(d->tab));
    FlashcardApplyStudyOrder(d->win);
    FlashcardToolbarUpdateState(d->win);
}

// Toggle the Filters ON/OFF switch: disables the filter without discarding
// the expression (compare study with/without the page restriction)
static void FilterToggleEnabled(FilterOptionsDialog* d) {
    d->tab->flashcard.filterEnabled = !d->tab->flashcard.filterEnabled;
    logf("[fc] Filter - toggle -> %s (expr kept: '%s')\n", d->tab->flashcard.filterEnabled ? StrL("ON") : StrL("OFF"),
         d->tab->flashcard.filterExpr);
    FlashcardApplyStudyOrder(d->win);
    FlashcardToolbarUpdateState(d->win);
    InvalidateRect(d->hwnd, nullptr, TRUE);
}

// Toggle the cross-document session scope (window-level, not per-book):
// current document vs all PDF tabs of THIS window
static void FilterToggleCrossDoc(FilterOptionsDialog* d) {
    MainWindow* win = d->win;
    win->flashcard.crossDocSession = !win->flashcard.crossDocSession;
    logf("[fc] Filter - study scope -> %s\n", win->flashcard.crossDocSession
                                                  ? StrL("GLOBAL SESSION (all PDF tabs of this window)")
                                                  : StrL("current document"));
    FlashcardApplyStudyOrder(win);    // rebuild queue per new scope when studying
    FlashcardToolbarUpdateCount(win); // count label is scope-aware
    InvalidateRect(d->hwnd, nullptr, TRUE);
}

// Clear (2s hold confirmed): empty expression, re-enable, uncheck all
static void FilterClearAll(FilterOptionsDialog* d) {
    str::ReplaceWithCopy(&d->tab->flashcard.filterExpr, Str());
    d->tab->flashcard.filterEnabled = true;
    FilterSetEditText(d->hwndEdit, Str());
    for (int i = 0; i < len(d->bmChecked); i++) {
        d->bmChecked[i] = false;
    }
    InvalidateRect(d->hwndList, nullptr, FALSE);
    logf("[fc] Filter - CLEARED (hold 2s confirmed), re-enabled\n");
    FlashcardApplyStudyOrder(d->win);
    FlashcardToolbarUpdateState(d->win);
    InvalidateRect(d->hwnd, nullptr, TRUE);
}

// A bookmark checkbox was clicked: inject/remove its page-range token into
// the edit's expression, then apply instantly (checkbox = one-click filter)
static void FilterToggleBookmark(FilterOptionsDialog* d, int idx) {
    if (idx < 0 || idx >= len(d->bmItems)) {
        return;
    }
    FilterBookmarkItem& item = d->bmItems[idx];
    d->bmChecked[idx] = !d->bmChecked[idx];
    Str text = FilterGetEditText(d->hwndEdit);
    Str token = fmt("%d-%d;", item.pageFrom, item.pageTo);
    Str newText;
    if (d->bmChecked[idx]) {
        if (str::IndexOf(text, token) < 0) {
            newText = fmt("%s%s", text, token);
        } else {
            newText = text;
        }
    } else {
        int pos = str::IndexOf(text, token);
        if (pos >= 0 && pos + len(token) <= len(text)) {
            newText = fmt("%s%s", Str(text.s, pos), Str(text.s + pos + len(token), len(text) - pos - len(token)));
        } else {
            newText = text;
        }
    }
    FilterSetEditText(d->hwndEdit, newText);
    logf("[fc] Filter - bookmark '%s' range %d-%d -> %s\n", item.tocItem ? item.tocItem->title : StrL("?"),
         item.pageFrom, item.pageTo, d->bmChecked[idx] ? StrL("checked (added)") : StrL("unchecked (removed)"));
    FilterApplyText(d);
    InvalidateRect(d->hwndList, nullptr, FALSE); // redraw the checkbox state
}

static void FilterOptionsLayout(FilterOptionsDialog* d) {
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int pad = DpiScale(d->hwnd, 14);
    int capDy = DpiScale(d->hwnd, 22);
    int editH = DpiScale(d->hwnd, 26);
    int btnH = DpiScale(d->hwnd, 44);
    int gap = DpiScale(d->hwnd, 10);

    int y = pad;
    d->rcCrossDoc = {pad, y, w - pad, y + DpiScale(d->hwnd, 28)};
    y = d->rcCrossDoc.bottom + DpiScale(d->hwnd, 8);
    d->rcEditCaption = {pad, y, w - pad, y + capDy};
    y += capDy + DpiScale(d->hwnd, 4);
    if (d->hwndEdit) {
        MoveWindow(d->hwndEdit, pad, y, w - 2 * pad, editH, TRUE);
    }
    y += editH + DpiScale(d->hwnd, 10);
    d->rcListCaption = {pad, y, w - pad, y + capDy};
    y += capDy + DpiScale(d->hwnd, 4);
    int yBtn = h - pad - btnH;
    if (d->hwndList) {
        MoveWindow(d->hwndList, pad, y, w - 2 * pad, yBtn - gap - y, TRUE);
    }
    // bottom button row: Aplicar | Filtros ON/OFF | Limpar filtros (hold 2s)
    int applyW = DpiScale(d->hwnd, 110);
    int toggleW = DpiScale(d->hwnd, 130);
    int clearW = DpiScale(d->hwnd, 170);
    int x = pad;
    d->rcApply = {x, yBtn, x + applyW, yBtn + btnH};
    x += applyW + gap;
    d->rcToggle = {x, yBtn, x + toggleW, yBtn + btnH};
    x += toggleW + gap;
    d->rcClear = {x, yBtn, x + clearW, yBtn + btnH};
}

static void FilterDrawCheckbox(HWND hwnd, HDC hdc, int x, int y, bool checked) {
    int side = DpiScale(hwnd, 14);
    RECT box = {x, y, x + side, y + side};
    HBRUSH bg = CreateSolidBrush(checked ? RGB(66, 133, 244) : RGB(255, 255, 255));
    FillRect(hdc, &box, bg);
    DeleteObject(bg);
    HPEN pen = CreatePen(PS_SOLID, checked ? 2 : 1, checked ? RGB(66, 133, 244) : RGB(150, 150, 150));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, box.left, box.top, box.right, box.bottom);
    if (checked) {
        // crude check mark: two lines inside the box
        SelectObject(hdc, GetStockObject(WHITE_PEN));
        MoveToEx(hdc, box.left + side / 4, box.top + side / 2, nullptr);
        LineTo(hdc, box.left + side * 2 / 5, box.bottom - side / 4);
        LineTo(hdc, box.right - side / 4, box.top + side / 4);
    }
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}

static void FilterOptionsPaint(FilterOptionsDialog* d) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->hwnd, &ps);
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    // cross-doc session checkbox row
    FilterDrawCheckbox(d->hwnd, hdc, d->rcCrossDoc.left + DpiScale(d->hwnd, 2), d->rcCrossDoc.top + 2,
                       d->win->flashcard.crossDocSession);
    SetTextColor(hdc, RGB(20, 20, 20));
    RECT rcCrossText = d->rcCrossDoc;
    rcCrossText.left += DpiScale(d->hwnd, 24);
    DrawTextW(hdc, L"Estudar de todos os PDFs abertos (sessão)", -1, &rcCrossText,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, RGB(60, 60, 60));
    DrawTextW(hdc, L"Filtro de páginas (ex.: 1-15;20-25;-22-23;)", -1, &d->rcEditCaption,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, L"Capítulos (bookmarks) — marcar injeta o intervalo no filtro", -1, &d->rcListCaption,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // bottom buttons
    bool filterOn = d->tab->flashcard.filterEnabled;
    CleanHistoryDrawButton(hdc, d->rcApply, L"Aplicar", false);
    TempStr toggleLabel = filterOn ? StrL("Filtros: ON") : StrL("Filtros: OFF");
    CleanHistoryDrawButton(hdc, d->rcToggle, (WCHAR*)CWStrTemp(ToWStrTemp(Str(toggleLabel))), false);
    CleanHistoryDrawHoldButton(d->hwnd, hdc, d->rcClear, L"Limpar filtros", d->holdingClear == 1,
                               d->holdingClear == 1 ? d->progress : 1.0, RGB(229, 57, 53));

    SelectObject(hdc, oldFont);
    EndPaint(d->hwnd, &ps);
}

// Owner-draw one bookmark row: checkbox (indented by tree depth) + title +
// its page range
static void FilterDrawListItem(FilterOptionsDialog* d, const DRAWITEMSTRUCT* dis) {
    int idx = (int)dis->itemID;
    if (idx < 0 || idx >= len(d->bmItems)) {
        return;
    }
    const FilterBookmarkItem& item = d->bmItems[idx];
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    int indent = DpiScale(d->hwnd, 16) * item.depth;
    int boxSide = DpiScale(d->hwnd, 14);
    FilterDrawCheckbox(d->hwnd, hdc, rc.left + DpiScale(d->hwnd, 6) + indent,
                       rc.top + (rc.bottom - rc.top - boxSide) / 2, d->bmChecked[idx]);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(20, 20, 20));
    TempStr label = fmt("%s  (%d-%d)", item.tocItem ? item.tocItem->title : StrL("?"), item.pageFrom, item.pageTo);
    RECT rcText = rc;
    rcText.left += DpiScale(d->hwnd, 6) + indent + boxSide + DpiScale(d->hwnd, 8);
    rcText.right -= DpiScale(d->hwnd, 8);
    DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(label))), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

// Enter in the edit applies; Esc closes the dialog
static LRESULT CALLBACK FilterEditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        if (gFilterOptionsDialog) {
            FilterApplyText(gFilterOptionsDialog);
        }
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        SendMessageW(GetParent(hwnd), WM_CLOSE, 0, 0);
        return 0;
    }
    return CallWindowProc(gFilterEditOrigProc, hwnd, msg, wp, lp);
}

static LRESULT CALLBACK FilterOptionsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    FilterOptionsDialog* d = gFilterOptionsDialog;
    if (msg == WM_CREATE) {
        gFilterOptionsDialog->hwnd = hwnd;
        return 0;
    }
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_SIZE:
            FilterOptionsLayout(d);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_ERASEBKGND:
            return 1; // painted in WM_PAINT
        case WM_PAINT:
            FilterOptionsPaint(d);
            return 0;
        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lp;
            if (mis->CtlID == IDC_FC_FILTER_LIST) {
                mis->itemHeight = DpiScale(hwnd, 24);
                return TRUE;
            }
            break;
        }
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lp;
            if (dis->CtlID == IDC_FC_FILTER_LIST) {
                FilterDrawListItem(d, dis);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            int code = HIWORD(wp);
            if (id == IDC_FC_FILTER_LIST && code == LBN_SELCHANGE) {
                int sel = (int)SendMessageW(d->hwndList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < len(d->bmItems)) {
                    FilterToggleBookmark(d, sel);
                }
                // clear the selection so clicking the SAME row again still
                // fires LBN_SELCHANGE (toggle semantics)
                SendMessageW(d->hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
                return 0;
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            POINT pt{(short)LOWORD(lp), (short)HIWORD(lp)};
            if (PtInRect(&d->rcCrossDoc, pt)) {
                FilterToggleCrossDoc(d);
            } else if (PtInRect(&d->rcApply, pt)) {
                FilterApplyText(d);
            } else if (PtInRect(&d->rcToggle, pt)) {
                FilterToggleEnabled(d);
            } else if (PtInRect(&d->rcClear, pt)) {
                // hold-to-confirm clear (2s, red draining line)
                d->holdingClear = 1;
                d->holdStartTick = GetTickCount64();
                d->progress = 1.0;
                SetCapture(hwnd);
                SetTimer(hwnd, 1, 15, nullptr);
                logf("[fc] Filter - clear hold start (2000ms)\n");
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_TIMER: {
            if (d->holdingClear == 0 || wp != 1) {
                return 0;
            }
            ULONGLONG elapsed = GetTickCount64() - d->holdStartTick;
            d->progress = 1.0 - (double)elapsed / (double)d->holdMs;
            if (elapsed >= (ULONGLONG)d->holdMs) {
                d->holdingClear = 0;
                logf("[fc] Filter - clear hold CONFIRMED\n");
                FilterClearAll(d);
            } else {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            if (d->holdingClear != 0) {
                d->holdingClear = 0;
                d->progress = 1.0;
                logf("[fc] Filter - clear hold released early\n");
            }
            KillTimer(hwnd, 1);
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                logf("[fc] Filter - closed (Esc)\n");
                DestroyWindow(hwnd);
            } else if (wp == VK_RETURN) {
                FilterApplyText(d);
            }
            return 0;
        case WM_CLOSE:
            logf("[fc] Filter - closed (X)\n");
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

void FlashcardFilterOptionsDialog(MainWindow* win) {
    static const WCHAR* className = L"TUMATRA_FLASHCARD_FILTER";
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = FilterOptionsWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr; // painted in WM_PAINT
        wc.lpszClassName = className;
        RegisterClassExW(&wc);
        registered = true;
    }

    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->AsFixed()) {
        logf("[fc] Filter - ERROR: no PDF document loaded\n");
        return;
    }
    HWND hwndParent = win->hwndFrame;
    FilterOptionsDialog dlg; // no {}: Vec's default ctor is explicit
    dlg.win = win;
    dlg.tab = tab;
    gFilterOptionsDialog = &dlg; // stack-local: safe, modal pump below owns it

    int cx = DpiScale(hwndParent, 560);
    int cy = DpiScale(hwndParent, 500);
    RECT rw;
    GetWindowRect(hwndParent, &rw);
    int x = rw.left + ((rw.right - rw.left) - cx) / 2;
    int y = rw.top + ((rw.bottom - rw.top) - cy) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, className, L"Filtro de Estudo", WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                x, y, cx, cy, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwnd) {
        gFilterOptionsDialog = nullptr;
        return;
    }
    logf("[fc] Filter - dialog opened\n");

    // child controls: expression EDIT + bookmark LISTBOX
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    dlg.hwndEdit =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0,
                        0, 0, hwnd, (HMENU)IDC_FC_FILTER_EDIT, hinst, nullptr);
    dlg.hwndList = CreateWindowExW(0, L"LISTBOX", nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_OWNERDRAWFIXED |
                                       LBS_NOINTEGRALHEIGHT | WS_TABSTOP,
                                   0, 0, 0, 0, hwnd, (HMENU)IDC_FC_FILTER_LIST, hinst, nullptr);
    SendMessageW(dlg.hwndEdit, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(dlg.hwndList, WM_SETFONT, (WPARAM)font, TRUE);

    FilterBuildBookmarkItems(&dlg);
    for (int i = 0; i < len(dlg.bmItems); i++) {
        SendMessageW(dlg.hwndList, LB_ADDSTRING, 0, (LPARAM)L"");
    }
    // preset the expression with the tab's current filter
    FilterSetEditText(dlg.hwndEdit, tab->flashcard.filterExpr);

    // subclass the edit: Enter = apply, Esc = close
    if (nullptr == gFilterEditOrigProc) {
        gFilterEditOrigProc = (WNDPROC)GetWindowLongPtr(dlg.hwndEdit, GWLP_WNDPROC);
    }
    SetWindowLongPtr(dlg.hwndEdit, GWLP_WNDPROC, (LONG_PTR)FilterEditProc);

    FilterOptionsLayout(&dlg);
    InvalidateRect(hwnd, nullptr, TRUE);

    // modal pump: disable the parent while the dialog is up
    EnableWindow(hwndParent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(dlg.hwndEdit);
    MSG msg;
    while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(hwndParent, TRUE);
    SetActiveWindow(hwndParent);
    gFilterOptionsDialog = nullptr;
}