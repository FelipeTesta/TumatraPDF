/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void CreateToolbar(MainWindow*);
void ReCreateToolbar(MainWindow* win);
// delete+re-add the toolbar buttons for the current width (re-wrap) and
// re-fit the rebar band; called after a resize settles (WM_TIMER debounce)
// and synchronously on the frame's WM_EXITSIZEMOVE
void ToolbarRewrapAfterSizing(MainWindow* win, HWND hwndToolbar, bool force = false);
// Set while RelayoutFrame resizes the rebar, to prevent WM_SIZE handler from
// calling ScheduleUiUpdate(kUiRelayout) → infinite relayout loop.
void SetInRelayoutFrame(bool inRelayout);
void ToolbarUpdateStateForWindow(MainWindow*, bool setButtonsVisibility);
void UpdateToolbarButtonsToolTipsForWindow(MainWindow*);
void UpdateToolbarFindText(MainWindow*);
void UpdateToolbarPageText(MainWindow*, int pageCount, bool updateOnly = false);
void UpdateToolbarEtaText(MainWindow* win, int minutes);
void UpdateEtaOverlayText(MainWindow* win, int minutes);
void PositionEtaOverlay(MainWindow* win);

void UpdateToolbarSpeedLabel(MainWindow* win);
void UpdateFindbox(MainWindow*);
void SetToolbarButtonEnableState(MainWindow*, int cmdId, bool isEnabled);
void SetToolbarButtonCheckedState(MainWindow*, int cmdId, bool isChecked);
bool ShouldShowToolbar(MainWindow*);
bool ShouldOverlayToolbar(MainWindow*);
void ShowOrHideToolbar(MainWindow*);
void PositionOverlayToolbar(MainWindow*);
void UpdateOverlayToolbarForMouse(MainWindow*);
void RevealOverlayToolbar(MainWindow*);
void OverlayToolbarHideTimerFired(MainWindow*);

// delay before the overlay toolbar hides after the mouse moves away
constexpr int kDelayToolbarHide = 500;
#define kHideOverlayToolbarTimerId 0x101
void UpdateToolbarState(MainWindow*);
void UpdateToolbarAfterThemeChange(MainWindow*);
HIMAGELIST BuildStdToolbarImageList(int dx);
Rect GetToolbarButtonScreenRect(MainWindow*, int cmdId);

TempStr ToolbarButtonsResultTemp(int* exitCodeOut);

int GetToolbarButtonsByID(int cmdId, int (&buttons)[4]);
void TbSetButtonDx(HWND hwndToolbar, int cmd, int dx);
void CreateToolbar2(MainWindow*);
void DestroyToolbar2(MainWindow*);
void UpdateToolbar2State(MainWindow*);

// Apply theme settings (dark mode, custom draw, etc.) to a rebar+toolbar pair
// Matches the pattern used in CreateToolbar/CreateToolbar2
void ToolbarApplyThemeToRebar(HWND hwndRebar, HWND hwndToolbar);
void ToolbarSetupSecondaryRebar(HWND hwndRebar);

// Arch/Edit rebar custom-draw subclass: wipes comctl's own button face +
// label before re-drawing text centered with theme colors. Installed AFTER
// dmlib on the rebar so it runs first (LIFO). Uses unique subclass id (42)
// to avoid colliding with ReBarWndProc (id 0) on the main rebar.
void ToolbarSubclassSecondaryRebar(MainWindow* win, HWND hwndRebar);

// Paint subclass for secondary toolbar windows (Arch/Edit): runs after comctl
// paints, wipes white button faces and re-draws text with theme colors.
// Installed on the toolbar HWND directly, bypassing dmlib interception.
void ToolbarSubclassPaint(MainWindow* win, HWND hwndToolbar);
void ToolbarEnforceSecondaryTextWidths(MainWindow* win, HWND hwndToolbar);
void PostAddTextButtonSizing(MainWindow* win, HWND hwndToolbar);

// Measure toolbar's wrapped button height and update rebar band accordingly.
// Returns true if band height changed.
bool SetToolbarBandToRowHeight(HWND hwndToolbar);

int GetMenuBarRebarHeight(MainWindow*);
void CreateMenuBarRebar(MainWindow*);
void DestroyMenuBarRebar(MainWindow*);
void ShowMenuBarRebar(MainWindow*);
void RebuildMenuBarButtons(MainWindow*);
bool IsShowingMenuBarRebar(MainWindow*);
bool HandleMenuBarCommand(MainWindow*, int cmdId);
bool ActivateMenuBarByAccel(MainWindow*, WCHAR accel);
void UpdateCustomMenuBarMenuSelect(MainWindow*, WPARAM, LPARAM);

#include "ToolbarIds.h"
