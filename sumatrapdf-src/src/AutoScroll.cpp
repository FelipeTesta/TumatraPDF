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
#include "MarkdownModel.h"
#include "wingui/WebView.h"
#include "SumatraLog.h"

static constexpr float kBaseIntervalMs = 20.0f;

// Round-step speed lookup table in px/min
static constexpr int kSpeedSteps[] = {25, 50, 75, 100, 150, 200, 300, 400, 600, 800, 1200, 1600};
static constexpr int kSpeedStepCount = sizeof(kSpeedSteps) / sizeof(kSpeedSteps[0]);

// Find nearest speed step index from current speedMultiplier
static int FindCurrentSpeedStep(float speedMultiplier, float baseSpeed) {
    float pxPerMin = baseSpeed * speedMultiplier * 100.0f * 60.0f;
    int best = 0;
    int bestDist = abs((int)pxPerMin - kSpeedSteps[0]);
    for (int i = 1; i < kSpeedStepCount; i++) {
        int dist = abs((int)pxPerMin - kSpeedSteps[i]);
        if (dist < bestDist) {
            best = i;
            bestDist = dist;
        }
    }
    return best;
}

// Calculate effective scroll speed in pixels per second
float AutoScrollPxPerSec(MainWindow* win) {
    float speed = win->autoScroll.speed * win->autoScroll.speedMultiplier;
    return speed * 100.0f; // speed is pixels per tick, timer fires ~100x/sec
}

// Recompute ETA from remaining pages and current speed
void RecalcAutoScrollEta(MainWindow* win) {
    if (!win->autoScroll.active) {
        return;
    }
    auto dm = win->AsFixed();
    if (dm) {
        int remainingPages = dm->PageCount() - dm->CurrentPageNo();
        float speedPxPerSec = AutoScrollPxPerSec(win);
        if (remainingPages <= 0 || speedPxPerSec <= 0) {
            win->autoScroll.etaMinutes = 0;
        } else {
            auto pageInfo = dm->GetPageInfo(dm->CurrentPageNo());
            if (!pageInfo) {
                win->autoScroll.etaMinutes = 0;
            } else {
                int pageHeightPx = (int)pageInfo->pos.dy;
                float etaSec = (remainingPages * pageHeightPx) / speedPxPerSec;
                win->autoScroll.etaMinutes = (int)(etaSec / 60.0f);
                if (win->autoScroll.etaMinutes < 0) {
                    win->autoScroll.etaMinutes = 0;
                }
            }
        }
        win->autoScroll.etaStartTick = GetTickCount();
        win->autoScroll.etaLastShown = -1;
        win->autoScroll.etaPageNo = dm->CurrentPageNo();
        return;
    }
    // WebView path (epub/markdown): trigger a one-shot JS eval to report remaining
    // height. The result arrives async via __sumatra__.notify -> OnAutoScrollProgress
    // which updates the ETA label.
    auto mm = win->AsMarkdown();
    if (mm) {
        struct WebviewWnd* wv = mm->GetWebviewWnd();
        if (wv && wv->webview) {
            TempStr js =
                fmt("(function(){var y=window.scrollY||window.pageYOffset||0;"
                    "var h=window.innerHeight;"
                    "var sh=document.documentElement.scrollHeight;"
                    "var rem=sh-(y+h);"
                    "if(rem<0) rem=0;"
                    "window.__sumatra__.notify('autoscrollProgress',rem);}())");
            wv->Eval(js);
        }
    }
}

// Toggle continuous auto-scroll on/off
void AutoScrollToggle(MainWindow* win) {
    if (!win) {
        return;
    }
    // Accept both fixed-page (PDF/ebook) and webview (markdown) documents
    auto dm = win->AsFixed();
    auto mm = win->AsMarkdown();
    if (!dm && !mm) {
        return;
    }
    win->autoScroll.active = !win->autoScroll.active;
    if (win->autoScroll.active) {
        win->autoScroll.accum = 0;
        win->autoScroll.startTick = GetTickCount();
        // Feed the timer stop-logic: use configured minutes if timer enabled, else 0 (no limit)
        win->autoScroll.timerMinutes = win->autoScroll.timerEnabled ? win->autoScroll.timerMinutesSetting : 0;
        // Snap to nearest round step on start
        int step = FindCurrentSpeedStep(win->autoScroll.speedMultiplier, win->autoScroll.speed);
        int pxPerMin = kSpeedSteps[step];
        win->autoScroll.speedMultiplier = (float)pxPerMin / (win->autoScroll.speed * 100.0f * 60.0f);
        RecalcAutoScrollEta(win);
        UpdateToolbarEtaText(win, win->autoScroll.etaMinutes);
        SetTimer(win->hwndCanvas, kContinuousAutoScrollTimerID, USER_TIMER_MINIMUM, nullptr);
        const char* mode = dm ? "fixed" : "webview";
        LogInfo("[autoscroll] start mode=%s speed=%d px/min timerMinutes=%u", mode, pxPerMin,
                win->autoScroll.timerMinutes);
    } else {
        KillTimer(win->hwndCanvas, kContinuousAutoScrollTimerID);
        UpdateToolbarEtaText(win, -1);
        LogInfo("[autoscroll] stop");
    }
    SetToolbarButtonCheckedState(win, CmdAutoScrollToggle, win->autoScroll.active);
    UpdateToolbarSpeedLabel(win);
}

// Adjust continuous auto-scroll speed multiplier
void AutoScrollSpeedAdjust(MainWindow* win, int direction) {
    if (!win) {
        return;
    }
    int step = FindCurrentSpeedStep(win->autoScroll.speedMultiplier, win->autoScroll.speed);
    if (direction > 0) {
        step = std::min(step + 1, kSpeedStepCount - 1);
    } else {
        step = std::max(step - 1, 0);
    }
    int pxPerMin = kSpeedSteps[step];
    // Convert back to multiplier: multiplier = pxPerMin / (speed * 100 * 60)
    win->autoScroll.speedMultiplier = (float)pxPerMin / (win->autoScroll.speed * 100.0f * 60.0f);
    // persist to in-memory FileState; written to disk on next settings save (app exit)
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->filePath) {
        FileState* fs = gFileHistory.FindByPath(tab->filePath);
        if (fs) {
            fs->autoScrollSpeedMultiplier = win->autoScroll.speedMultiplier;
        }
    }
    if (win->autoScroll.active) {
        RecalcAutoScrollEta(win);
    }
    UpdateToolbarSpeedLabel(win);
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
    if (!win->autoScroll.active) {
        KillTimer(hwnd, kContinuousAutoScrollTimerID);
        return;
    }
    auto dm = win->AsFixed();
    if (dm) {
        // Fixed-page path (PDF, ebook, etc.)
        // Check end of document
        if (dm->IsAtDocumentEnd()) {
            win->autoScroll.active = false;
            KillTimer(hwnd, kContinuousAutoScrollTimerID);
            UpdateToolbarEtaText(win, -1);
            SetToolbarButtonCheckedState(win, CmdAutoScrollToggle, false);
            return;
        }
        // Timer check
        if (win->autoScroll.timerMinutes > 0 && win->autoScroll.startTick > 0) {
            DWORD elapsedMs = GetTickCount() - win->autoScroll.startTick;
            if (elapsedMs >= (DWORD)win->autoScroll.timerMinutes * 60 * 1000) {
                win->autoScroll.active = false;
                KillTimer(hwnd, kContinuousAutoScrollTimerID);
                UpdateToolbarEtaText(win, -1);
                SetToolbarButtonCheckedState(win, CmdAutoScrollToggle, false);
                return;
            }
        }
        // Scroll
        float speed = win->autoScroll.speed * win->autoScroll.speedMultiplier;
        win->autoScroll.accum += speed;
        int dy = (int)win->autoScroll.accum;
        if (dy != 0) {
            win->autoScroll.accum -= dy;
            win->MoveDocBy(0, dy);
        }
        // ETA: recalc on page change, countdown by wall clock between
        if (dm->CurrentPageNo() != win->autoScroll.etaPageNo) {
            RecalcAutoScrollEta(win);
        }
        int remaining = win->autoScroll.etaMinutes - (int)((GetTickCount() - win->autoScroll.etaStartTick) / 60000);
        if (remaining < 0) {
            remaining = 0;
        }
        if (remaining != win->autoScroll.etaLastShown) {
            win->autoScroll.etaLastShown = remaining;
            UpdateToolbarEtaText(win, remaining);
        }
    } else {
        // WebView2 path (markdown)
        auto mm = win->AsMarkdown();
        if (!mm) {
            return;
        }
        struct WebviewWnd* wv = mm->GetWebviewWnd();
        if (!wv) {
            return;
        }
        static bool sLoggedWebviewTickStart = false;
        if (!sLoggedWebviewTickStart) {
            sLoggedWebviewTickStart = true;
            LogInfo("[autoscroll] webview tick start");
        }
        // Scroll by the same pixel delta used for fixed pages
        float speed = win->autoScroll.speed * win->autoScroll.speedMultiplier;
        win->autoScroll.accum += speed;
        int dy = (int)win->autoScroll.accum;
        if (dy != 0) {
            win->autoScroll.accum -= dy;
            TempStr js = fmt("window.scrollBy(0, %d);", dy);
            wv->Eval(js);
        }
        // Stop-at-bottom detection: one-shot Eval to check scroll position
        // We piggyback on the existing scroll notification bridge (__sumatra__.notify)
        // by sending a one-shot check. The result comes async via jsNotify.
        // For simplicity, we also check synchronously via a one-shot eval that
        // posts a notify we can handle. But since jsNotify is async, we use a
        // simpler approach: check if scrollY + innerHeight >= scrollHeight - 2px.
        // We'll do this check periodically (every few ticks) to avoid overhead.
        static int sBottomCheckCounter = 0;
        if (++sBottomCheckCounter >= 5) { // check every ~5 ticks (~100ms)
            sBottomCheckCounter = 0;
            TempStr js =
                fmt("(function(){var y=window.scrollY||window.pageYOffset||0;"
                    "var h=window.innerHeight;"
                    "var sh=document.documentElement.scrollHeight;"
                    "var rem=sh-(y+h);"
                    "if(rem<0) rem=0;"
                    "window.__sumatra__.notify('autoscrollProgress',rem);"
                    "if((y+h)>=sh-2){window.__sumatra__.notify('autoscrollBottom',1);}}())");
            wv->Eval(js);
        }
        // Timer check (same as fixed-page)
        if (win->autoScroll.timerMinutes > 0 && win->autoScroll.startTick > 0) {
            DWORD elapsedMs = GetTickCount() - win->autoScroll.startTick;
            if (elapsedMs >= (DWORD)win->autoScroll.timerMinutes * 60 * 1000) {
                win->autoScroll.active = false;
                KillTimer(hwnd, kContinuousAutoScrollTimerID);
                UpdateToolbarEtaText(win, -1);
                SetToolbarButtonCheckedState(win, CmdAutoScrollToggle, false);
                return;
            }
        }
        // ETA: not implemented for webview mode (single-page scroll, no page count)
        // Could be added later by estimating from scroll position vs scrollHeight
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