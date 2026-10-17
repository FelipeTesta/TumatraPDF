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
    int ctrlGapX = 4;   // horizontal gap between adjacent custom controls
    int ctrlH = 18;     // standard height for checkbox / edit controls
    int labelW = 40;    // "Timer:" label fallback width
    int etaW = 50;      // ETA label fallback width
    int pagePadX = 12;  // page box padding
    int buttonGapX = 8; // gap between button and floating label
    int edgeMargin = 4; // toolbar edge margin
};

// --- Shared design tokens: colors + fonts (toolbar + home) ---
// Theme-aware: resolve DsAccentsGet() at paint time. A theme switch only
// repaints (it never re-runs cached setup), so these must not be baked into a
// class brush or a static.
struct DsAccents {
    COLORREF text;         // body text (labels, ticks, totals)
    COLORREF textStrong;   // emphasized text (values, titles)
    COLORREF textHot;      // interactive text / accent
    COLORREF silver;       // silver accent (bars, curve)
    COLORREF silverHi;     // hovered cell (bright silver)
    COLORREF today;        // TODAY marker (orange)
    COLORREF cellEmpty;    // heatmap: no-data cell fill
    COLORREF cellOutline;  // heatmap: outline around an empty cell
    COLORREF separator;    // section separator line
    COLORREF axis;         // chart axis + nav triangle glyphs
    COLORREF cellGrade[4]; // heatmap fill by minutes (<30, <60, <120, else)
};
DsAccents DsAccentsGet();

// Font tokens: family + GDI+ point sizes (shared with GDI text helpers)
const WCHAR* DsFontFamily();
float DsFontSizeLabel();
float DsFontSizeTitle();

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
    int placeholderId;            // e.g., TimerInfoId, PageInfoId
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
