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

// Global design tokens instance
ToolbarTokens gToolbarTokens;

// Slot specifications - declarative definition of all toolbar child windows
const ToolbarSlotSpec gToolbarSlots[] = {
    {TimerInfoId,
     "timer",
     {
         {CmdAutoScrollTimerToggle, "Button", BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE, "", 18, false, 0},
         {0, "Static", SS_CENTER | WS_CHILD | WS_VISIBLE, "Timer:", 40, true, 6},
         {CmdAutoScrollTimerEdit, "Edit", ES_NUMBER | ES_AUTOHSCROLL | WS_CHILD | WS_VISIBLE, "30", 20, false, 0},
     },
     3,
     4,
     18},
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
        if (slot.placeholderId == TimerInfoId)
            fallbackWidth = DpiScale(win->hwndFrame, gToolbarTokens.timerSlotW);
        else if (slot.placeholderId == PageInfoId)
            fallbackWidth = DpiScale(win->hwndFrame, 150); // page box wider

        TbSetButtonDx(hwndToolbar, slot.placeholderId, std::max(neededWidth, fallbackWidth));
    }
}

// Position all child windows within their slots
void PositionToolbarChildWindows(MainWindow* win) {
    HWND hwndToolbar = win->hwndToolbar;
    if (!hwndToolbar) return;

    // Timer trio (checkbox | "Timer:" | minutes edit). Anchored to the REAL
    // CmdAutoScrollToggle button, laid out right-to-left ending at its left
    // edge. The old anchor was the TimerInfoId separator; when comctl absorbs
    // that separator at a wrap point its TB_GETRECT goes empty and the controls
    // kept stale "flying" coordinates on every resize. A real button's rect is
    // always valid (or clearly hidden), and the trio follows its group to
    // whichever row the Autoscroll button wrapped into.
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
    int xEdit = anchor.x - gap - editW;
    int xLabel = xEdit - gap - labelSz.dx;
    int xCheck = xLabel - gap - ctrlH;

    // no anchor (button hidden / not created yet) or no room for the whole
    // trio before it -> hide; show otherwise (all-or-nothing, keeps the group
    // from visually tearing apart on narrow windows)
    bool show = !anchor.IsEmpty() && xCheck > gap;
    if (show) {
        int y = anchor.y + (anchor.dy - ctrlH) / 2;
        MoveWindow(hwndEdit, xEdit, y, editW, ctrlH, TRUE);
        MoveWindow(hwndLabel, xLabel, y, labelSz.dx, ctrlH, TRUE);
        MoveWindow(hwndCheck, xCheck, y, ctrlH, ctrlH, TRUE);
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
