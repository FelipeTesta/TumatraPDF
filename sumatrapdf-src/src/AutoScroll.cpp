/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#include "base/Base.h"
#include "base/BitManip.h"
#include "base/WinDynCalls.h"
#include "base/Dpi.h"
#include "base/File.h"
#include "base/Timer.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/ScopedWin.h"
#include "base/Http.h"
#include "base/GdiPlusUtil.h"
#include "base/GuessFileType.h"

#include <mmsystem.h>
#include <shlobj.h>
#pragma comment(lib, "winmm.lib")

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "wingui/FrameRateWnd.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "Annotation.h"
#include "FormFields.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "DisplayModel.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "RenderCache.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraConfig.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "EditAnnotations.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "Canvas.h"
#include "TrimConfigDialog.h"
#include "Menu.h"
#include "Commands.h"
#include "uia/Provider.h"
#include "SearchAndDDE.h"
#include "Selection.h"
#include "LinkFollow.h"
#include "SelectTextKeyboard.h"
#include "SelectionToolbar.h"
#include "ReadAloudHighlight.h"
#include "ReadAloudPlaybackBar.h"
#include "TextToSpeech.h"
#include "HomePage.h"
#include "Toolbar.h"
#include "FileHistory.h"
#include "AutoScroll.h"
#include "Translations.h"

#include "RefHover.h"

static constexpr float kBaseIntervalMs = 20.0f;
static constexpr float kMaxSpeedMultiplier = 10.0f;
static constexpr float kMinSpeedMultiplier = 0.1f;
static constexpr float kSpeedUpFactor = 1.2f;
static constexpr float kSpeedDownFactor = 0.8f;

// Calculate effective scroll speed in pixels per second
float AutoScrollPxPerSec(MainWindow* win) {
    float speed = win->autoScrollSpeed * win->autoScrollSpeedMultiplier;
    return speed * 100.0f; // speed is pixels per tick, timer fires ~100x/sec
}

// Recompute ETA from remaining pages and current speed
void RecalcAutoScrollEta(MainWindow* win) {
    if (!win->autoScrollActive) {
        return;
    }
    auto dm = win->AsFixed();
    if (!dm) {
        return;
    }
    int remainingPages = dm->PageCount() - dm->CurrentPageNo();
    float speedPxPerSec = AutoScrollPxPerSec(win);
    if (remainingPages <= 0 || speedPxPerSec <= 0) {
        win->autoScrollEtaMinutes = 0;
    } else {
        auto pageInfo = dm->GetPageInfo(dm->CurrentPageNo());
        if (!pageInfo) {
            win->autoScrollEtaMinutes = 0;
        } else {
            int pageHeightPx = (int)pageInfo->pos.dy;
            float etaSec = (remainingPages * pageHeightPx) / speedPxPerSec;
            win->autoScrollEtaMinutes = (int)(etaSec / 60.0f);
            if (win->autoScrollEtaMinutes < 0) {
                win->autoScrollEtaMinutes = 0;
            }
        }
    }
    win->autoScrollEtaStartTick = GetTickCount();
    win->autoScrollEtaLastShown = -1;
    win->autoScrollEtaPageNo = dm->CurrentPageNo();
}

// Toggle continuous auto-scroll on/off
void AutoScrollToggle(MainWindow* win) {
    if (!win || !win->AsFixed()) {
        return;
    }
    win->autoScrollActive = !win->autoScrollActive;
    if (win->autoScrollActive) {
        win->autoScrollAccum = 0;
        win->autoScrollStartTick = GetTickCount();
        RecalcAutoScrollEta(win);
        UpdateToolbarEtaText(win, win->autoScrollEtaMinutes);
        SetTimer(win->hwndCanvas, kContinuousAutoScrollTimerID, USER_TIMER_MINIMUM, nullptr);
    } else {
        KillTimer(win->hwndCanvas, kContinuousAutoScrollTimerID);
        UpdateToolbarEtaText(win, -1);
    }
    SetToolbarButtonCheckedState(win, CmdAutoScrollToggle, win->autoScrollActive);
}

// Adjust continuous auto-scroll speed multiplier
void AutoScrollSpeedAdjust(MainWindow* win, int direction) {
    if (!win) {
        return;
    }
    if (direction > 0) {
        win->autoScrollSpeedMultiplier = std::min(win->autoScrollSpeedMultiplier * kSpeedUpFactor, kMaxSpeedMultiplier);
    } else {
        win->autoScrollSpeedMultiplier = std::max(win->autoScrollSpeedMultiplier * kSpeedDownFactor, kMinSpeedMultiplier);
    }
    // Persist the new multiplier
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->filePath) {
        FileState* fs = gFileHistory.FindByPath(tab->filePath);
        if (fs) {
            fs->autoScrollSpeedMultiplier = win->autoScrollSpeedMultiplier;
        }
    }
    if (win->autoScrollActive) {
        RecalcAutoScrollEta(win);
    }
}

// Start middle-click-style auto-scroll at current cursor position
void StartAutoScrollAtCursor(MainWindow* win) {
    if (!win || !win->AsFixed()) {
        return;
    }
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    win->xScrollAccum = 0;
    win->yScrollAccum = 0;
    SetTimer(win->hwndCanvas, kAutoScrollTimerID, USER_TIMER_MINIMUM, nullptr);
    // Reuse the middle-click down logic
    win->mouseAction = MouseAction::Scrolling;
    win->dragStartPending = true;
    win->dragStart = pt;
    SetCanvasCursor(win, IDC_SIZEALL);
}

// Timer tick handler for continuous auto-scroll
void AutoScrollContinuousTick(MainWindow* win, HWND hwnd) {
    // Ctrl held = pause auto-scroll temporarily
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        return;
    }
    if (!win->autoScrollActive) {
        KillTimer(hwnd, kContinuousAutoScrollTimerID);
        return;
    }
    auto dm = win->AsFixed();
    if (!dm) {
        return;
    }
    // Check end of document
    if (dm->IsAtDocumentEnd()) {
        win->autoScrollActive = false;
        KillTimer(hwnd, kContinuousAutoScrollTimerID);
        UpdateToolbarEtaText(win, -1);
        SetToolbarButtonCheckedState(win, CmdAutoScrollToggle, false);
        return;
    }
    // Timer check
    if (win->autoScrollTimerMinutes > 0 && win->autoScrollStartTick > 0) {
        DWORD elapsedMs = GetTickCount() - win->autoScrollStartTick;
        if (elapsedMs >= (DWORD)win->autoScrollTimerMinutes * 60 * 1000) {
            win->autoScrollActive = false;
            KillTimer(hwnd, kContinuousAutoScrollTimerID);
            UpdateToolbarEtaText(win, -1);
            SetToolbarButtonCheckedState(win, CmdAutoScrollToggle, false);
            return;
        }
    }
    // Scroll
    float speed = win->autoScrollSpeed * win->autoScrollSpeedMultiplier;
    win->autoScrollAccum += speed;
    int dy = (int)win->autoScrollAccum;
    if (dy != 0) {
        win->autoScrollAccum -= dy;
        win->MoveDocBy(0, dy);
    }
    // ETA: recalc on page change, countdown by wall clock between
    if (dm->CurrentPageNo() != win->autoScrollEtaPageNo) {
        RecalcAutoScrollEta(win);
    }
    int remaining = win->autoScrollEtaMinutes - (int)((GetTickCount() - win->autoScrollEtaStartTick) / 60000);
    if (remaining < 0) {
        remaining = 0;
    }
    if (remaining != win->autoScrollEtaLastShown) {
        win->autoScrollEtaLastShown = remaining;
        UpdateToolbarEtaText(win, remaining);
    }
}

// Timer tick handler for middle-click auto-scroll
void AutoScrollMiddleClickTick(MainWindow* win, HWND hwnd) {
    if (MouseAction::Scrolling == win->mouseAction) {
        float scale = (float)USER_TIMER_MINIMUM / kBaseIntervalMs;
        win->xScrollAccum += win->xScrollSpeed * scale;
        win->yScrollAccum += win->yScrollSpeed * scale;
        int dx = (int)win->xScrollAccum;
        int dy = (int)win->yScrollAccum;
        win->xScrollAccum -= (float)dx;
        win->yScrollAccum -= (float)dy;
        if (dx != 0 || dy != 0) {
            win->MoveDocBy(dx, dy);
        }
    } else {
        KillTimer(hwnd, kAutoScrollTimerID);
        win->xScrollSpeed = 0;
        win->yScrollSpeed = 0;
        win->xScrollAccum = 0;
        win->yScrollAccum = 0;
    }
}