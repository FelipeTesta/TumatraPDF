/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/Win.h"
#include "base/BitManip.h"

#include "Commands.h"
#include "ToolbarLayout.h"
#include "Toolbar.h"
#include "MainWindow.h"
#include "AutoScroll.h"
#include "Translations.h"
#include "Theme.h"

// Global design tokens instance
ToolbarTokens gToolbarTokens;

// --- shared design tokens (colors + fonts) ---
// True when the current theme is dark: decided from the window background
// luminance so it follows any theme (incl. custom ones), not a fixed list.
static bool DsIsDarkTheme() {
    COLORREF bg = ThemeWindowBackgroundColor();
    int lum = (GetRValue(bg) * 299 + GetGValue(bg) * 587 + GetBValue(bg) * 114) / 1000;
    return lum < 128;
}

DsAccents DsAccentsGet() {
    DsAccents a{};
    if (DsIsDarkTheme()) {
        a.text = RGB(130, 130, 130);
        a.textStrong = RGB(60, 60, 60);
        a.textHot = RGB(120, 130, 140);
        a.silver = RGB(120, 130, 140);
        a.silverHi = RGB(198, 209, 215);
        a.today = RGB(255, 140, 0);
        a.cellEmpty = RGB(20, 20, 20);
        a.cellOutline = RGB(90, 90, 90);
        a.separator = RGB(200, 200, 200);
        a.axis = RGB(120, 120, 120);
        a.cellGrade[0] = RGB(120, 120, 120);
        a.cellGrade[1] = RGB(150, 150, 150);
        a.cellGrade[2] = RGB(190, 190, 190);
        a.cellGrade[3] = RGB(225, 225, 225);
    } else {
        a.text = RGB(90, 90, 90);
        a.textStrong = RGB(30, 30, 30);
        a.textHot = RGB(90, 100, 120);
        a.silver = RGB(140, 150, 165);
        a.silverHi = RGB(90, 110, 140);
        a.today = RGB(215, 110, 0);
        a.cellEmpty = RGB(235, 235, 235);
        a.cellOutline = RGB(205, 205, 205);
        a.separator = RGB(210, 210, 210);
        a.axis = RGB(150, 150, 150);
        a.cellGrade[0] = RGB(200, 200, 200);
        a.cellGrade[1] = RGB(175, 175, 175);
        a.cellGrade[2] = RGB(150, 150, 150);
        a.cellGrade[3] = RGB(120, 120, 120);
    }
    return a;
}

const WCHAR* DsFontFamily() {
    return L"MS Shell Dlg";
}

float DsFontSizeLabel() {
    return 9.5f;
}

float DsFontSizeTitle() {
    return 11.0f;
}

// Slot specifications - declarative definition of all toolbar child windows
// NOTE: the timer trio no longer has a slot button — it lives INSIDE the
// widened Autoscroll button (Toolbar.cpp EnforceToolbarTextButtonWidths +
// PositionToolbarChildWindows below). The old TimerInfoId slot wrapped apart
// from its group on narrow windows: the trio lost its room and hid, and the
// slot's rect landed over row 1 as a dead zone (user 2026-10-16).
const ToolbarSlotSpec gToolbarSlots[] = {
    {PageInfoId,
     "page",
     {
         {0, "Static", WS_CHILD | WS_VISIBLE, "", 0, false, 0},                                        // pageBg
         {0, "Static", WS_CHILD | WS_VISIBLE, "Page:", 0, false, 6},                                   // pageLabel
         {0, "Edit", ES_NUMBER | ES_AUTOHSCROLL | ES_RIGHT | WS_CHILD | WS_VISIBLE, "0", 0, false, 0}, // pageEdit
         {0, "Static", WS_CHILD | WS_VISIBLE, " / 999", 0, true, 6},                                   // pageTotal
     },
     4,
     4,
     18},
};
const int gToolbarSlotCount = (int)dimof(gToolbarSlots);

// Main layout engine - single pass for ALL toolbar child windows
void LayoutToolbarChildWindows(MainWindow* win) {
    if (!win || !win->hwndToolbar) return;

    UpdateToolbarSlotWidths(win);
    PositionToolbarChildWindows(win);
    PositionFloatingLabels(win);
    HandleToolbarOverflow(win);
}

// Ensure all slots have correct width based on their children
void UpdateToolbarSlotWidths(MainWindow* win) {
    HWND hwndToolbar = win->hwndToolbar;
    if (!hwndToolbar) return;

    for (int i = 0; i < gToolbarSlotCount; i++) {
        const ToolbarSlotSpec& slot = gToolbarSlots[i];
        int neededWidth = 0;

        for (int j = 0; j < slot.childCount; j++) {
            const ToolbarChildSpec& child = slot.children[j];
            int childWidth = DpiScale(win->hwndFrame, child.minWidth);

            if (child.measureText && child.initialText) {
                // Find the child HWND to measure actual text
                HWND hwndChild = nullptr;
                if (child.ctrlId > 0) {
                    int buttons[4];
                    int n = GetToolbarButtonsByID(child.ctrlId, buttons);
                    if (n > 0) {
                        // Child windows are created with the same ctrlId as menu ID
                        // We need to find them differently - they're child windows of toolbar
                    }
                }
                // For now use fallback + padding
                childWidth = std::max(childWidth, DpiScale(win->hwndFrame, child.minWidth));
            }

            if (j == 0) {
                neededWidth += DpiScale(win->hwndFrame, slot.gapX); // left gap
            } else {
                neededWidth += DpiScale(win->hwndFrame, slot.gapX); // gap between children
            }
            neededWidth += childWidth;
        }
        if (slot.childCount > 0) {
            neededWidth += DpiScale(win->hwndFrame, slot.gapX); // right gap
        }

        // Apply minimum fallback
        int fallbackWidth = 0;
        if (slot.placeholderId == PageInfoId) fallbackWidth = DpiScale(win->hwndFrame, 150); // page box wider

        TbSetButtonDx(hwndToolbar, slot.placeholderId, std::max(neededWidth, fallbackWidth));
    }
}

// Position all child windows within their slots
void PositionToolbarChildWindows(MainWindow* win) {
    HWND hwndToolbar = win->hwndToolbar;
    if (!hwndToolbar) return;

    // D5 (user 2026-10-16: timer controls kept showing with home=on): the
    // anchor button below is HIDDEN in focused mode and TB_GETRECT fails for
    // hidden buttons (debug report res==0), so check BEFORE asking for the
    // rect. Hide the trio for as long as the overlay is up; the regular
    // layout passes re-show it once the anchor is back.
    if (win->showHomeOverlay) {
        for (HWND h : {win->autoScroll.hwndTimerCheck, win->autoScroll.hwndTimerLabel, win->autoScroll.hwndTimerEdit}) {
            if (h && IsWindowVisible(h)) {
                ShowWindow(h, SW_HIDE);
            }
        }
        return;
    }

    // Timer trio (checkbox | "Timer:" | minutes edit). It lives INSIDE the
    // Autoscroll button's rect: EnforceToolbarTextButtonWidths widens that
    // button by exactly the trio's width, so the group is a single wrap unit
    // and can never be split by a row wrap. (The old scheme reserved a
    // dedicated slot button left of Autoscroll — on narrow windows comctl's
    // balanced wrap moved that slot to another row, the trio lost its room
    // and hid, and the slot's rect landed over row 1 as a dead zone.)
    HWND hwndCheck = win->autoScroll.hwndTimerCheck;
    HWND hwndLabel = win->autoScroll.hwndTimerLabel;
    HWND hwndEdit = win->autoScroll.hwndTimerEdit;
    if (!hwndCheck || !hwndLabel || !hwndEdit) return;

    Rect anchor = TbGetRect(hwndToolbar, CmdAutoScrollToggle);
    int gap = DpiScale(win->hwndFrame, gToolbarTokens.ctrlGapX);
    int ctrlH = DpiScale(win->hwndFrame, gToolbarTokens.ctrlH);

    // measure the three pieces from their actual text
    Size editSz = HwndMeasureText(hwndEdit, "8888");
    int editW = std::max(editSz.dx, DpiScale(win->hwndFrame, 20));
    Size labelSz = HwndMeasureText(hwndLabel, "Timer:");
    int trioDx = gap + ctrlH + gap + labelSz.dx + gap + editW;

    // no anchor (button hidden / not created yet) or a button narrower than
    // the trio (should not happen — Enforce widens it) -> hide; lay the trio
    // left-to-right from the button's left edge otherwise
    bool show = !anchor.IsEmpty() && anchor.dx >= trioDx + DpiScale(win->hwndFrame, 4);
    if (show) {
        int y = anchor.y + (anchor.dy - ctrlH) / 2;
        int x = anchor.x + gap;
        MoveWindow(hwndCheck, x, y, ctrlH, ctrlH, TRUE);
        x += ctrlH + gap;
        MoveWindow(hwndLabel, x, y, labelSz.dx, ctrlH, TRUE);
        x += labelSz.dx + gap;
        MoveWindow(hwndEdit, x, y, editW, ctrlH, TRUE);
    }
    for (HWND h : {hwndCheck, hwndLabel, hwndEdit}) {
        bool visible = IsWindowVisible(h) != FALSE;
        if (visible != show) {
            ShowWindow(h, show ? SW_SHOW : SW_HIDE);
        }
    }
}

// Position floating toolbar child labels (speed, timer) that float over the
// toolbar. The ETA label is no longer positioned here — it lives as a
// bottom-right overlay on the canvas (see PositionEtaOverlay in Toolbar.cpp).
void PositionFloatingLabels(MainWindow* win) {
    // Speed/timer labels are positioned by their toolbar slots in
    // PositionToolbarChildWindows; the ETA label is a canvas overlay now.
    (void)win;
}

// Create child windows for a slot if not already created
void CreateToolbarSlotChildren(MainWindow* win, const ToolbarSlotSpec& slot) {
    // This is called from Toolbar.cpp during toolbar creation
    // Actual creation logic remains in Toolbar.cpp for now
    // This function provides the spec for future unification
    (void)win;
    (void)slot;
}

// Measure text width for a child window
int MeasureChildTextWidth(MainWindow* win, HWND hwndChild, const char* fallbackText) {
    if (!hwndChild || !fallbackText) return 0;
    TempStr txt = HwndGetTextTemp(hwndChild);
    if (!txt) txt = fallbackText;
    Size size = HwndMeasureText(hwndChild, txt);
    return size.dx;
}

// Clamp/hide overflow for narrow windows
void HandleToolbarOverflow(MainWindow* win) {
    // All-or-nothing timer-trio visibility is now decided in
    // PositionToolbarChildWindows (anchored to the CmdAutoScrollToggle button);
    // the old slot-width-based progressive hiding read the TimerInfoId
    // separator rect, which goes empty at wrap points. Keep the call site in
    // LayoutToolbarChildWindows for future overflow rules.
    (void)win;
}
