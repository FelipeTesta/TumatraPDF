/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Flashcard Lista panel — docked RIGHT-side sidebar (AI-chat panel pattern)
// listing the current document's flashcards in 4 columns:
//   Flashcard (masked-text snippet) | Pág | Due (countdown) | Status
// Status colors: new=blue, due=orange, learn=purple, ok=green. Clicking a
// row navigates the view to that card. Width is drag-resizable via a
// vertical splitter; the LabelWithClose title hides the panel.

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/LabelWithCloseWnd.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Theme.h"
#include "Translations.h"
#include "resource.h"
#include "DarkModeSubclass.h"
#include "Flashcard.h"
#include "FlashcardSidebar.h"

// status colors (readable on light and dark backgrounds)
static COLORREF kFcStatusNew = RGB(33, 150, 243);   // blue
static COLORREF kFcStatusDue = RGB(239, 108, 0);    // orange
static COLORREF kFcStatusLearn = RGB(156, 39, 176); // purple
static COLORREF kFcStatusOk = RGB(67, 160, 71);     // green

constexpr int kFcListMinDx = 260;

// width of the visual splitter (must match SumatraPDF.cpp's kSplitterDx)
constexpr int kFcSplitterDx = 4;

static WNDPROC gWndProcListaBox = nullptr;
static WNDPROC gWndProcListaHeader = nullptr;
static HBRUSH gFcListBgBrush = nullptr;
static COLORREF gFcListBgColor = 0;

// ---- column anchors (shared by the header strip and item drawing) --------

struct FcListCols {
    int xLabel = 0; // left edge of the label column
    int labelMaxW = 0;
    int xPagR = 0;   // right edge of the Pág number
    int xDueR = 0;   // right edge of the Due countdown
    int xStatus = 0; // left edge of the Status word
};

static FcListCols FcListColumnAnchors(HWND hwnd, int w) {
    FcListCols c;
    int gap = DpiScale(hwnd, 8);
    int wStatus = DpiScale(hwnd, 78);
    int wDue = DpiScale(hwnd, 64);
    int wPag = DpiScale(hwnd, 52);
    c.xStatus = w - gap - wStatus;
    c.xDueR = c.xStatus - gap;
    c.xPagR = c.xDueR - wDue - gap;
    c.xLabel = gap;
    c.labelMaxW = c.xPagR - wPag - gap * 2 - c.xLabel;
    if (c.labelMaxW < 40) {
        c.labelMaxW = 40;
    }
    return c;
}

// ---- per-row status / countdown ------------------------------------------

// status word + color from the card's study state (computed at draw time so
// the Due countdown stays fresh without repopulating)
static COLORREF FcListStatusColor(const FcListRow& row, i64 now) {
    bool isNew = row.rating == 0;
    if (isNew) {
        return kFcStatusNew;
    }
    if (row.nextReviewAt <= now) {
        return kFcStatusDue;
    }
    if (row.interval <= 1) {
        return kFcStatusLearn;
    }
    return kFcStatusOk;
}

static TempStr FcListStatusTextTemp(const FcListRow& row, i64 now) {
    if (row.rating == 0) {
        return StrL("new");
    }
    if (row.nextReviewAt <= now) {
        return StrL("due");
    }
    if (row.interval <= 1) {
        return StrL("learn");
    }
    return StrL("ok");
}

// countdown until the next review: "0min" when due, then min/h/day buckets.
// New cards have no schedule — show an em dash.
static TempStr FcListDueTextTemp(const FcListRow& row, i64 now) {
    if (row.rating == 0) {
        return StrL("\xe2\x80\x94"); // em dash
    }
    i64 delta = row.nextReviewAt - now;
    if (delta <= 0) {
        return StrL("0min");
    }
    i64 mins = delta / 60000;
    if (mins < 60) {
        return fmt("%dmin", (int)mins);
    }
    i64 hours = delta / 3600000;
    if (hours < 48) {
        return fmt("%dh", (int)hours);
    }
    return fmt("%dd", (int)(delta / 86400000));
}

// ---- masked-text label extraction ----------------------------------------

// true if the codepoint rect overlaps any mask rect of the card (inflated a
// bit so adjacent lines whose bbox clips the mask still match)
static bool FcRectHitsCard(const Rect& cpRect, const Flashcard& card) {
    float inf = 1.5f;
    for (const RectF& r : card.rects) {
        float rx = r.x - inf, ry = r.y - inf;
        float rdx = r.dx + 2 * inf, rdy = r.dy + 2 * inf;
        bool overlap = (float)cpRect.x < rx + rdx && (float)(cpRect.x + cpRect.dx) > rx && (float)cpRect.y < ry + rdy &&
                       (float)(cpRect.y + cpRect.dy) > ry;
        if (overlap) {
            return true;
        }
    }
    return false;
}

// build the card label by walking the page's extracted text and keeping the
// codepoints that sit under the card's mask rects. pageText is a cache owned
// by the caller (re-extracted when pageNo changes). Falls back to "Card N"
// when nothing matches.
static TempWStr FcListCardLabelTemp(WindowTab* tab, const Flashcard& card, int cardNo, PageText& pt, int& ptPage) {
    TempStr fallback = fmt("Card %d", cardNo);
    EngineBase* engine = tab->GetEngine();
    if (!engine) {
        return str::DupTemp(ToWStrTemp(fallback));
    }
    if (ptPage != card.pageNo || !pt.text || !pt.coords) {
        FreePageText(&pt);
        pt = engine->ExtractPageText(card.pageNo);
        ptPage = card.pageNo;
    }
    if (!pt.text || !pt.coords || pt.nCodepoints <= 0) {
        return str::DupTemp(ToWStrTemp(fallback));
    }

    WCHAR buf[256];
    int n = 0;
    int byteIdx = 0;
    int cpIdx = 0;
    bool lastWasSpace = false;
    while (byteIdx < pt.len && cpIdx < pt.nCodepoints && n < 250) {
        int nBytes = 0;
        u32 cp = (u32)Utf8CodepointAtByte(pt.text, byteIdx, &nBytes);
        if (nBytes <= 0) {
            break;
        }
        if (FcRectHitsCard(pt.coords[cpIdx], card)) {
            // collapse whitespace runs (multi-line masks read as one line)
            if (cp == '\n' || cp == '\r' || cp == ' ' || cp == '\t') {
                if (!lastWasSpace) {
                    buf[n++] = ' ';
                    lastWasSpace = true;
                }
            } else {
                lastWasSpace = false;
                if (cp > 0xffff && n < 248) {
                    cp -= 0x10000;
                    buf[n++] = (WCHAR)(0xd800 + (cp >> 10));
                    buf[n++] = (WCHAR)(0xdc00 + (cp & 0x3ff));
                } else if (cp <= 0xffff) {
                    buf[n++] = (WCHAR)cp;
                }
            }
        }
        byteIdx += nBytes;
        cpIdx++;
    }
    while (n > 0 && buf[n - 1] == ' ') {
        n--; // trim trailing space
    }
    if (n == 0) {
        return str::DupTemp(ToWStrTemp(fallback));
    }
    buf[n] = 0;
    if (n >= 250 && cpIdx < pt.nCodepoints) {
        // truncated because the card is huge: hint with an ellipsis
        buf[n - 1] = 0x2026; // …
    }
    return str::DupTemp(WStr(buf, n));
}

// ---- painting -------------------------------------------------------------

static void FcListEnsureBgBrush() {
    COLORREF bg = ThemeControlBackgroundColor();
    if (!gFcListBgBrush || gFcListBgColor != bg) {
        if (gFcListBgBrush) {
            DeleteObject(gFcListBgBrush);
        }
        gFcListBgBrush = CreateSolidBrush(bg);
        gFcListBgColor = bg;
    }
}

// draw one listbox row: label (ellipsis), right-aligned page + countdown,
// color-coded status word
static void FcListDrawItem(MainWindow* win, const DRAWITEMSTRUCT* dis) {
    HWND hwnd = dis->hwndItem;
    int w = dis->rcItem.right - dis->rcItem.left;
    FcListCols cols = FcListColumnAnchors(hwnd, w);
    FcListEnsureBgBrush();

    bool selected = (dis->itemState & ODS_SELECTED) != 0;
    bool isDark = UseDarkModeLib() && !IsCurrentThemeDefault();
    COLORREF bg = selected ? (isDark ? RGB(55, 82, 122) : RGB(200, 221, 245)) : ThemeControlBackgroundColor();
    HBRUSH brush = CreateSolidBrush(bg);
    FillRect(dis->hDC, &dis->rcItem, brush);
    DeleteObject(brush);

    if ((int)dis->itemID >= len(win->flashcard.listaRows)) {
        return;
    }
    const FcListRow& row = win->flashcard.listaRows[dis->itemID];

    i64 now = (i64)time(nullptr) * 1000;
    COLORREF txtCol = selected ? (isDark ? RGB(240, 240, 240) : RGB(20, 20, 20)) : ThemeWindowTextColor();
    COLORREF statusCol = FcListStatusColor(row, now);
    if (selected) {
        // keep the status color readable over the selection background
        statusCol = isDark ? RGB(255, 183, 77) : RGB(156, 66, 0);
    }

    HFONT font = GetAppTreeFont(win->hwndFrame);
    HGDIOBJ prevFont = SelectObject(dis->hDC, font);
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, txtCol);

    RECT rcLabel{dis->rcItem.left + cols.xLabel, dis->rcItem.top, dis->rcItem.left + cols.xLabel + cols.labelMaxW,
                 dis->rcItem.bottom};
    WCHAR buf[256];
    LRESULT n = SendMessageW(hwnd, LB_GETTEXT, dis->itemID, (LPARAM)buf);
    if (n > 0) {
        RECT rc = rcLabel;
        DrawTextW(dis->hDC, buf, (int)n, &rc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    TempStr pagTxt = fmt("%d", row.pageNo);
    RECT rcPag{dis->rcItem.left + cols.xPagR - 60, dis->rcItem.top, dis->rcItem.left + cols.xPagR, dis->rcItem.bottom};
    WStr pagW = ToWStrTemp(pagTxt);
    DrawTextW(dis->hDC, pagW.s, len(pagW), &rcPag, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    TempStr dueTxt = FcListDueTextTemp(row, now);
    RECT rcDue{dis->rcItem.left + cols.xDueR - 60, dis->rcItem.top, dis->rcItem.left + cols.xDueR, dis->rcItem.bottom};
    WStr dueW = ToWStrTemp(dueTxt);
    DrawTextW(dis->hDC, dueW.s, len(dueW), &rcDue, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    TempStr statusTxt = FcListStatusTextTemp(row, now);
    SetTextColor(dis->hDC, statusCol);
    RECT rcStatus{dis->rcItem.left + cols.xStatus, dis->rcItem.top, dis->rcItem.left + cols.xStatus + 80,
                  dis->rcItem.bottom};
    WStr statusW = ToWStrTemp(statusTxt);
    DrawTextW(dis->hDC, statusW.s, len(statusW), &rcStatus, DT_SINGLELINE | DT_VCENTER);

    SelectObject(dis->hDC, prevFont);
}

// the column-title strip above the list
static LRESULT CALLBACK WndProcListaHeader(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            FcListEnsureBgBrush();
            FillRect(hdc, &rc, gFcListBgBrush);
            int w = rc.right - rc.left;
            FcListCols cols = FcListColumnAnchors(hwnd, w);

            MainWindow* win = FindMainWindowByHwnd(GetParent(hwnd));
            HFONT font = win ? GetAppTreeFont(win->hwndFrame) : nullptr;
            HGDIOBJ prevFont = font ? SelectObject(hdc, font) : nullptr;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, ThemeWindowTextColor());

            auto drawCol = [&](const WCHAR* s, int xRightOrLeft, bool rightAlign) {
                RECT rcCol{rc.left, rc.top, rc.right, rc.bottom};
                UINT fmt = DT_SINGLELINE | DT_VCENTER;
                if (rightAlign) {
                    rcCol.left = xRightOrLeft - 70;
                    rcCol.right = xRightOrLeft;
                    fmt |= DT_RIGHT;
                } else {
                    rcCol.left = xRightOrLeft;
                    rcCol.right = xRightOrLeft + 300;
                    fmt |= DT_LEFT;
                }
                DrawTextW(hdc, s, -1, &rcCol, fmt);
            };
            drawCol(L"Flashcard", cols.xLabel, false);
            drawCol(L"Page", cols.xPagR, true);
            drawCol(L"Due", cols.xDueR, true);
            drawCol(L"Status", cols.xStatus, false);

            if (prevFont) {
                SelectObject(hdc, prevFont);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return CallWindowProc(gWndProcListaHeader, hwnd, msg, wp, lp);
}

// ---- container (box) subclass ---------------------------------------------

// manual layout: title strip, column-header strip, list fills the rest (the
// AI-chat panel positions its webview the same way, via lastBounds)
static void LayoutListaContainer(MainWindow* win) {
    if (!win || !win->flashcard.hwndListaBox) {
        return;
    }
    Rect rc = HwndClientRect(win->flashcard.hwndListaBox);
    if (rc.IsEmpty()) {
        return;
    }
    int labelH = DpiScale(win->flashcard.hwndListaBox, 24);
    if (win->flashcard.listaLabel && win->flashcard.listaLabel->hwnd) {
        Size ideal = win->flashcard.listaLabel->GetIdealSize();
        if (ideal.dy > labelH) {
            labelH = ideal.dy;
        }
        MoveWindow(win->flashcard.listaLabel->hwnd, rc.x, rc.y, rc.dx, labelH, TRUE);
    }
    int headerH = DpiScale(win->flashcard.hwndListaBox, 20);
    MoveWindow(win->flashcard.hwndListaHeader, rc.x, rc.y + labelH, rc.dx, headerH, TRUE);
    int listY = rc.y + labelH + headerH;
    MoveWindow(win->flashcard.hwndListaView, rc.x, listY, rc.dx, rc.dy - labelH - headerH, TRUE);
}

static LRESULT CALLBACK WndProcListaBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        return CallWindowProc(gWndProcListaBox, hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_SIZE:
            LayoutListaContainer(win);
            break;

        case WM_MEASUREITEM: {
            // per-row height from the tree font (the listbox is owner-drawn)
            auto* mis = (MEASUREITEMSTRUCT*)lp;
            if (mis->CtlID != IDC_FLASHCARD_LIST) {
                break;
            }
            HWND hwndList = win->flashcard.hwndListaView;
            HDC hdc = GetDC(hwndList);
            HFONT font = GetAppTreeFont(win->hwndFrame);
            HGDIOBJ prev = SelectObject(hdc, font);
            TEXTMETRICW tm{};
            GetTextMetricsW(hdc, &tm);
            SelectObject(hdc, prev);
            ReleaseDC(hwndList, hdc);
            mis->itemHeight = tm.tmHeight + DpiScale(hwnd, 6);
            return TRUE;
        }

        case WM_DRAWITEM: {
            auto* dis = (DRAWITEMSTRUCT*)lp;
            if (dis->CtlID != IDC_FLASHCARD_LIST) {
                break;
            }
            FcListDrawItem(win, dis);
            return TRUE;
        }

        case WM_CTLCOLORLISTBOX:
            FcListEnsureBgBrush();
            SetBkColor((HDC)wp, ThemeControlBackgroundColor());
            SetTextColor((HDC)wp, ThemeWindowTextColor());
            return (LRESULT)gFcListBgBrush;

        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDC_FLASHCARD_LABEL_WITH_CLOSE) {
                // close button on the title strip hides the panel
                FlashcardSidebarToggle(win);
                return 0;
            }
            if (id == IDC_FLASHCARD_LIST && HIWORD(wp) == LBN_SELCHANGE) {
                LRESULT sel = SendMessageW(win->flashcard.hwndListaView, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR && sel < len(win->flashcard.listaRows)) {
                    FlashcardNavigateToCardInCurrentTab(win, win->flashcard.listaRows[(int)sel].cardIdx);
                }
                return 0;
            }
            break;
        }
    }
    return CallWindowProc(gWndProcListaBox, hwnd, msg, wp, lp);
}

// ---- splitter -------------------------------------------------------------

static void OnFcListSplitterMove(Splitter::MoveEvent* ev) {
    Splitter* splitter = ev->w;
    MainWindow* win = FindMainWindowByHwnd(splitter->hwnd);
    if (!win) {
        return;
    }
    Point pcur = HwndGetCursorPos(win->hwndFrame);
    Rect rFrame = HwndClientRect(win->hwndFrame);
    int dx = rFrame.dx - pcur.x;
    // when the AI chat panel is also open it keeps its width; the Lista
    // panel must fit in what remains
    if (win->uiState.aiChatVisible && win->hwndAiChatBox) {
        dx -= win->aiChatDx + kFcSplitterDx;
    }
    int minDx = DpiScale(win->hwndFrame, kFcListMinDx);
    if (dx < minDx || dx > rFrame.dx / 2) {
        ev->resizeAllowed = false;
        return;
    }
    win->flashcard.listaDx = dx;
    if (ev->finishedDragging) {
        ScheduleUiUpdate(win, kUiRelayout | kUiNoToolbars);
    }
}

// ---- create / destroy / toggle / populate --------------------------------

void FlashcardSidebarCreate(MainWindow* win) {
    if (win->flashcard.hwndListaBox) {
        return;
    }
    logf("[fc] FlashcardSidebarCreate - creating docked Lista panel\n");

    HMODULE h = GetModuleHandleW(nullptr);
    DWORD style = WS_CHILD | WS_CLIPCHILDREN;
    win->flashcard.hwndListaBox =
        CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, 0, 0, win->hwndFrame, nullptr, h, nullptr);
    if (!win->flashcard.hwndListaBox) {
        return;
    }
    HWND box = win->flashcard.hwndListaBox;

    // splitter between the panel and the canvas (resize applied on release)
    {
        Splitter::CreateArgs args;
        args.parent = win->hwndFrame;
        args.type = SplitterType::Vert;
        args.isLive = false;
        win->flashcard.listaSplitter = new Splitter();
        win->flashcard.listaSplitter->onMove = MkFunc1Void(OnFcListSplitterMove);
        win->flashcard.listaSplitter->Create(args);
    }

    // title strip with close button
    auto* label = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = box;
        args.cmdId = IDC_FLASHCARD_LABEL_WITH_CLOSE;
        args.font = GetAppSidebarLabelFont(win->hwndFrame);
        args.isRtl = IsUIRtl();
        label->Create(args);
    }
    label->SetPaddingXY(2, 2);
    label->SetText(StrL("Flashcards"));
    win->flashcard.listaLabel = label;

    // column-title strip (custom painted to align with the row columns)
    win->flashcard.hwndListaHeader =
        CreateWindowExW(0, WC_STATIC, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, box, nullptr, h, nullptr);
    if (win->flashcard.hwndListaHeader && !gWndProcListaHeader) {
        gWndProcListaHeader = (WNDPROC)GetWindowLongPtr(win->flashcard.hwndListaHeader, GWLP_WNDPROC);
    }
    if (win->flashcard.hwndListaHeader) {
        SetWindowLongPtr(win->flashcard.hwndListaHeader, GWLP_WNDPROC, (LONG_PTR)WndProcListaHeader);
    }

    // the list itself: owner-drawn fixed-height rows; the label string is
    // stored in the item, numeric per-row state in win->flashcard.listaRows
    DWORD listStyle =
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT;
    win->flashcard.hwndListaView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTBOX, L"", listStyle, 0, 0, 0, 0, box,
                                                   (HMENU)IDC_FLASHCARD_LIST, h, nullptr);

    if (nullptr == gWndProcListaBox) {
        gWndProcListaBox = (WNDPROC)GetWindowLongPtr(box, GWLP_WNDPROC);
    }
    SetWindowLongPtr(box, GWLP_WNDPROC, (LONG_PTR)WndProcListaBox);

    // the box subclass must be in place before the list's item height is set:
    // WM_MEASUREITEM for an owner-draw-fixed listbox fires at window creation
    // (before the subclass) and never again, so apply the row height ourselves
    // with the tree font — LB_SETITEMHEIGHT re-measures internally
    HWND hwndList = win->flashcard.hwndListaView;
    if (hwndList) {
        HFONT treeFont = GetAppTreeFont(win->hwndFrame);
        SendMessageW(hwndList, WM_SETFONT, (WPARAM)treeFont, TRUE);
        HDC hdc = GetDC(hwndList);
        HGDIOBJ prev = SelectObject(hdc, treeFont);
        TEXTMETRICW tm{};
        GetTextMetricsW(hdc, &tm);
        SelectObject(hdc, prev);
        ReleaseDC(hwndList, hdc);
        int itemH = tm.tmHeight + DpiScale(box, 6);
        SendMessageW(hwndList, LB_SETITEMHEIGHT, 0, itemH);
    }

    if (UseDarkModeLib() && !IsCurrentThemeDefault()) {
        DarkMode::setChildCtrlsSubclassAndTheme(box);
    }

    LayoutListaContainer(win);
    UpdateControlsColors(win);
}

void FlashcardSidebarDestroy(MainWindow* win) {
    if (!win->flashcard.hwndListaBox) {
        return;
    }
    logf("[fc] FlashcardSidebarDestroy - destroying Lista panel\n");

    win->uiState.fcListVisible = false;
    win->flashcard.listaRows.Reset();
    if (win->flashcard.listaSplitter) {
        delete win->flashcard.listaSplitter;
        win->flashcard.listaSplitter = nullptr;
    }
    if (win->flashcard.listaLabel) {
        delete win->flashcard.listaLabel;
        win->flashcard.listaLabel = nullptr;
    }
    DestroyWindow(win->flashcard.hwndListaBox);
    win->flashcard.hwndListaBox = nullptr;
    win->flashcard.hwndListaView = nullptr;
    win->flashcard.hwndListaHeader = nullptr;
}

void FlashcardSidebarToggle(MainWindow* win) {
    logf("[fc] FlashcardSidebarToggle\n");
    // the panel only exists while flashcard mode is on (the toolbar that owns
    // the Lista button is created/destroyed with it)
    if (!win->flashcard.on) {
        logf("[fc] FlashcardSidebarToggle - flashcard mode off, ignoring\n");
        return;
    }
    if (!win->flashcard.hwndListaBox) {
        FlashcardSidebarCreate(win);
        if (!win->flashcard.hwndListaBox) {
            return;
        }
        win->uiState.fcListVisible = true;
        FlashcardSidebarPopulate(win);
    } else {
        win->uiState.fcListVisible = !win->uiState.fcListVisible;
        if (win->uiState.fcListVisible) {
            FlashcardSidebarPopulate(win);
        }
    }
    // apply the geometry change (canvas grows/shrinks)
    ScheduleUiUpdate(win, kUiRelayout);
    FlashcardToolbarUpdateState(win);
}

void FlashcardSidebarPopulate(MainWindow* win) {
    if (!win->flashcard.hwndListaBox || !win->flashcard.hwndListaView) {
        return;
    }
    HWND hwndList = win->flashcard.hwndListaView;
    SendMessageW(hwndList, LB_RESETCONTENT, 0, 0);
    win->flashcard.listaRows.Reset();

    // the list shows the CURRENT tab's cards (lazy-load if needed)
    WindowTab* tab = win->CurrentTab();
    if (!tab || !FlashcardEnsureTabCards(tab)) {
        logf("[fc] FlashcardSidebarPopulate - no PDF loaded, list empty\n");
        return;
    }

    int nCards = len(tab->flashcard.cards);
    logf("[fc] FlashcardSidebarPopulate - adding %d cards to list\n", nCards);

    // page-text cache shared while extracting labels (cards are in page order)
    PageText pt{};
    int ptPage = -1;

    int nNew = 0, nDue = 0, nLearn = 0, nOk = 0;
    for (int i = 0; i < nCards; i++) {
        Flashcard& card = tab->flashcard.cards[i];

        FcListRow row;
        row.cardIdx = i;
        row.pageNo = card.pageNo;
        for (auto& st : tab->flashcard.studyDoc.states) {
            if (st.key == card.key) {
                row.rating = st.state.rating;
                row.interval = st.state.interval;
                row.nextReviewAt = st.state.nextReviewAt;
                break;
            }
        }
        if (row.rating == 0) {
            nNew++;
        } else if (row.nextReviewAt <= (i64)time(nullptr) * 1000) {
            nDue++;
        } else if (row.interval <= 1) {
            nLearn++;
        } else {
            nOk++;
        }

        WStr label = FcListCardLabelTemp(tab, card, i + 1, pt, ptPage);
        int itemIdx = (int)SendMessageW(hwndList, LB_ADDSTRING, 0, (LPARAM)label.s);
        if (itemIdx != LB_ERR) {
            SendMessageW(hwndList, LB_SETITEMDATA, (WPARAM)itemIdx, (LPARAM)i);
        }
        win->flashcard.listaRows.Append(row);
    }
    FreePageText(&pt);

    // don't keep a selection from the previous population
    SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)-1, 0);

    logf("[fc] FlashcardSidebarPopulate - %d cards (new=%d due=%d learn=%d ok=%d)\n", nCards, nNew, nDue, nLearn, nOk);
}

// re-lay out + repaint after RelayoutFrame moved the panel
void RelayoutFlashcardListPanel(MainWindow* win) {
    if (!win || !win->flashcard.hwndListaBox || !win->uiState.fcListVisible) {
        return;
    }
    LayoutListaContainer(win);
    RedrawWindow(win->flashcard.hwndListaBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (win->flashcard.listaSplitter && win->flashcard.listaSplitter->hwnd) {
        HwndInvalidate(win->flashcard.listaSplitter->hwnd, true);
    }
}
