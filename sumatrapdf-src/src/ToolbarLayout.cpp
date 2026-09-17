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
    {SpeedInfoId,
     "speed",
     {
         {0, "Static", SS_CENTER | WS_CHILD | WS_VISIBLE, "0 px/min", 70, true, 8},
     },
     1,
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
        else if (slot.placeholderId == SpeedInfoId)
            fallbackWidth = DpiScale(win->hwndFrame, gToolbarTokens.speedSlotW);
        else if (slot.placeholderId == PageInfoId)
            fallbackWidth = DpiScale(win->hwndFrame, 150); // page box wider

        TbSetButtonDx(hwndToolbar, slot.placeholderId, std::max(neededWidth, fallbackWidth));
    }
}

// Position all child windows within their slots
void PositionToolbarChildWindows(MainWindow* win) {
    HWND hwndToolbar = win->hwndToolbar;
    if (!hwndToolbar) return;

    for (int i = 0; i < gToolbarSlotCount; i++) {
        const ToolbarSlotSpec& slot = gToolbarSlots[i];
        Rect slotRect = TbGetRect(hwndToolbar, slot.placeholderId);
        if (slotRect.IsEmpty()) continue;

        int ctrlGapX = DpiScale(win->hwndFrame, slot.gapX);
        int ctrlH = DpiScale(win->hwndFrame, slot.ctrlH);
        int x = slotRect.x + ctrlGapX;
        int y = slotRect.y + (slotRect.dy - ctrlH) / 2;

        for (int j = 0; j < slot.childCount; j++) {
            const ToolbarChildSpec& child = slot.children[j];
            HWND hwndChild = nullptr;

            // Find child window by class name and position heuristic
            // In practice, we store HWNDs in MainWindow.autoScroll or similar
            // For now, this is a framework - actual HWND mapping done in Toolbar.cpp

            if (child.ctrlId == CmdAutoScrollTimerToggle) {
                hwndChild = win->autoScroll.hwndTimerCheck;
            } else if (child.ctrlId == CmdAutoScrollTimerEdit) {
                hwndChild = win->autoScroll.hwndTimerEdit;
            } else if (slot.placeholderId == TimerInfoId && j == 1) { // Timer label
                hwndChild = win->autoScroll.hwndTimerLabel;
            } else if (slot.placeholderId == SpeedInfoId && j == 0) { // Speed label
                hwndChild = win->autoScroll.hwndSpeedLabel;
            }

            if (!hwndChild) continue;

            int childWidth = DpiScale(win->hwndFrame, child.minWidth);
            if (child.measureText && child.initialText) {
                Size textSize = HwndMeasureText(hwndChild, child.initialText);
                childWidth = std::max(childWidth, textSize.dx + DpiScale(win->hwndFrame, child.textPaddingRight));
            }

            MoveWindow(hwndChild, x, y, childWidth, ctrlH, TRUE);
            x += childWidth + ctrlGapX;
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
    // Priority 1: Hide speed label text or clamp — handled by slot sizing / PositionToolbarChildWindows
    // Priority 2: Hide timer edit (show only checkbox + label)
    // Priority 3: Hide timer label (show only checkbox)
    // Never hide checkbox (it's the toggle)
    HWND hwndToolbar = win->hwndToolbar;
    if (!hwndToolbar) return;

    Rect timerSlot = TbGetRect(hwndToolbar, TimerInfoId);
    if (timerSlot.IsEmpty()) return;

    HWND hwndCheck = win->autoScroll.hwndTimerCheck;
    HWND hwndLabel = win->autoScroll.hwndTimerLabel;
    HWND hwndEdit = win->autoScroll.hwndTimerEdit;

    if (!hwndCheck) return;

    // Determine available width for timer slot
    int slotW = timerSlot.dx;
    int ctrlH = gToolbarTokens.ctrlH;
    int minWidthForCheck = ctrlH + DpiScale(win->hwndFrame, 8);
    int minWidthForCheckAndLabel = minWidthForCheck + DpiScale(win->hwndFrame, 45);

    if (slotW < minWidthForCheckAndLabel && hwndEdit) {
        if (IsWindowVisible(hwndEdit)) {
            ShowWindow(hwndEdit, SW_HIDE);
        }
    } else if (hwndEdit && !IsWindowVisible(hwndEdit)) {
        ShowWindow(hwndEdit, SW_SHOW);
    }

    if (slotW < minWidthForCheck && hwndLabel) {
        if (IsWindowVisible(hwndLabel)) {
            ShowWindow(hwndLabel, SW_HIDE);
        }
    } else if (hwndLabel && !IsWindowVisible(hwndLabel)) {
        ShowWindow(hwndLabel, SW_SHOW);
    }
}
