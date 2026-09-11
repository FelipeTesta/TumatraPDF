/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef ToolbarLayout_h
#define ToolbarLayout_h

struct MainWindow;
struct Rect;
struct Size;
struct Point;

// Forward declarations for child window specs
struct ToolbarChildSpec;
struct ToolbarSlotSpec;

// Centralized design tokens for toolbar layout (DPI base units; wrap with DpiScale at use site)
struct ToolbarTokens {
    int ctrlGapX = 4;     // horizontal gap between adjacent custom controls
    int ctrlH = 18;       // standard height for checkbox / edit controls
    int labelW = 40;      // "Timer:" label fallback width
    int timerSlotW = 130; // timer controls slot fallback width
    int speedSlotW = 70;  // speed label slot fallback width
    int etaW = 50;        // ETA label fallback width
    int pagePadX = 12;    // page box padding
    int buttonGapX = 8;   // gap between button and floating label
    int edgeMargin = 4;   // toolbar edge margin
};

// Specification for a single child window within a toolbar slot
struct ToolbarChildSpec {
    int ctrlId;              // Command ID (e.g., CmdAutoScrollTimerToggle) or 0 for static
    const char* className;   // Window class: "Button", "Static", "Edit"
    DWORD style;             // Window style (WS_CHILD | WS_VISIBLE | ...)
    const char* initialText; // Initial text (UTF-8), nullptr for none
    int minWidth;            // Minimum width in DPI base units
    bool measureText;        // Whether to measure text for dynamic width
    int textPaddingRight;    // Extra padding for measured text
};

// Specification for a toolbar slot (placeholder button that reserves space for child windows)
struct ToolbarSlotSpec {
    int placeholderId;            // e.g., TimerInfoId, SpeedInfoId, PageInfoId
    const char* name;             // Debug name: "timer", "speed", "page"
    ToolbarChildSpec children[4]; // Max 4 children per slot
    int childCount = 0;
    int gapX = 4;   // Horizontal gap between children (DPI base)
    int ctrlH = 18; // Control height (DPI base)
};

// Global design tokens instance
extern ToolbarTokens gToolbarTokens;

// Slot specifications (defined in ToolbarLayout.cpp)
extern const ToolbarSlotSpec gToolbarSlots[];
extern const int gToolbarSlotCount;

// Main layout engine - single pass for ALL toolbar child windows
void LayoutToolbarChildWindows(MainWindow* win);

// Helper: ensure all slots have correct width based on their children
void UpdateToolbarSlotWidths(MainWindow* win);

// Helper: position all child windows within their slots
void PositionToolbarChildWindows(MainWindow* win);

// Helper: position floating labels (ETA) after rightmost visible element
void PositionFloatingLabels(MainWindow* win);

// Helper: create child windows for a slot if not already created
void CreateToolbarSlotChildren(MainWindow* win, const ToolbarSlotSpec& slot);

// Helper: measure text width for a child window
int MeasureChildTextWidth(MainWindow* win, HWND hwndChild, const char* fallbackText);

// Helper: clamp/hide overflow for narrow windows
void HandleToolbarOverflow(MainWindow* win);

#endif // ToolbarLayout_h
