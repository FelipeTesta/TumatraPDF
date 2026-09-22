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

// Speed multiplier range and step, exposed directly (no px/min conversion).
static constexpr float kMinSpeedMultiplier = 0.03f;
static constexpr float kMaxSpeedMultiplier = 0.4f;
static constexpr float kSpeedStepSize = 0.01f;

// Snap a multiplier to the allowed range and the fixed 0.01 step.
static float SnapSpeedMultiplier(float mlt) {
    mlt = std::max(mlt, kMinSpeedMultiplier);
    mlt = std::min(mlt, kMaxSpeedMultiplier);
    return (float)((int)(mlt * 100.0f + 0.5f)) / 100.0f;
}

// Calculate effective scroll speed in pixels per second
float AutoScrollPxPerSec(MainWindow* win) {
    float speed = win->autoScroll.speed * win->autoScroll.speedMultiplier;
    return speed * 100.0f; // speed is pixels per tick, timer fires ~100x/sec
}

// Estimate the remaining reading time as (time per page) x (remaining pages).
// Resynced on page changes (AutoScrollContinuousTick); between resyncs the
// display counts down from the last estimate like an inverted timer.
// TC2 is naturally accounted for: PageCount()/CurrentPageNo() are VIRTUAL
// (2N columns = each physical page counts twice). Margin trim doesn't matter:
// the page count is unchanged and the measured seconds-per-page covers it.
void RecalcAutoScrollEta(MainWindow* win) {
    if (!win->autoScroll.active) {
        return;
    }
    auto dm = win->AsFixed();
    if (dm) {
        int remainingPages = dm->PageCount() - dm->CurrentPageNo();
        // measured seconds per page once available; until then estimate from
        // the current page height and the nominal scroll speed
        float timePerPageSec = win->autoScroll.etaTimePerPageSec;
        if (timePerPageSec <= 0) {
            float pxPerSec = AutoScrollPxPerSec(win);
            PageInfo* pi = dm->GetPageInfo(dm->CurrentPageNo());
            float pageHeightPx = pi ? (float)pi->pos.dy : 0.0f;
            if (pxPerSec > 0 && pageHeightPx > 0) {
                timePerPageSec = pageHeightPx / pxPerSec;
            }
        }
        if (remainingPages <= 0 || timePerPageSec <= 0) {
            win->autoScroll.etaMinutes = 0;
        } else {
            float etaSec = (float)remainingPages * timePerPageSec;
            win->autoScroll.etaMinutes = (int)(etaSec / 60.0f) + 1; // round up, reader-friendly
        }
        win->autoScroll.etaResyncTick = GetTickCount();
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

// Install a requestAnimationFrame-driven autoscroll loop in the markdown page and
// start/update it at the given speed in px/sec (0 stops it). The JS loop scrolls
// smoothly in the renderer at ~60fps without a cross-process Eval per WM_TIMER tick,
// and notifies __sumatra__ 'autoscrollBottom' when it reaches the end.
static void SetWebviewAutoScroll(MainWindow* win, float pxPerSec) {
    auto mm = win->AsMarkdown();
    struct WebviewWnd* wv = mm ? mm->GetWebviewWnd() : nullptr;
    if (!wv) {
        return;
    }
    if (pxPerSec <= 0) {
        wv->Eval("if(window.__tumatraAS)window.__tumatraAS.stop();");
        return;
    }
    TempStr js =
        fmt("if(!window.__tumatraAS){window.__tumatraAS={raf:0,pxPerSec:0,last:0,accum:0,"
            "start:function(p){this.stop();this.pxPerSec=p;this.accum=0;this.last=performance.now();"
            "this.raf=requestAnimationFrame(this.tick.bind(this));},"
            "stop:function(){if(this.raf){cancelAnimationFrame(this.raf);this.raf=0;}this.pxPerSec=0;this.accum=0;},"
            "tick:function(t){var dt=(t-this.last)/1000;this.last=t;if(this.pxPerSec<=0)return;"
            "this.accum+=this.pxPerSec*dt;var dy=Math.floor(this.accum);"
            "if(dy>0){this.accum-=dy;window.scrollBy(0,dy);}"
            "var y=window.scrollY||window.pageYOffset,h=window.innerHeight,"
            "sh=document.documentElement.scrollHeight;"
            "if((y+h)>=sh-2){this.stop();window.__sumatra__.notify('autoscrollBottom',1);return;}"
            "this.raf=requestAnimationFrame(this.tick.bind(this));}};}"
            "window.__tumatraAS.start(%f);",
            (double)pxPerSec);
    wv->Eval(js);
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
        // fresh ETA state: measure seconds per page from scratch
        win->autoScroll.etaPageNo = 0;
        win->autoScroll.etaPageEnterTick = 0;
        win->autoScroll.etaPauseStartTick = 0;
        win->autoScroll.etaTimePerPageSec = 0;
        win->autoScroll.etaLastShown = -1;
        // Feed the timer stop-logic: use configured minutes if timer enabled, else 0 (no limit)
        win->autoScroll.timerMinutes = win->autoScroll.timerEnabled ? win->autoScroll.timerMinutesSetting : 0;
        // Snap to the multiplier range/step on start
        win->autoScroll.speedMultiplier = SnapSpeedMultiplier(win->autoScroll.speedMultiplier);
        // persist snapped speed to global prefs
        gGlobalPrefs->autoScrollSpeedMultiplier = win->autoScroll.speedMultiplier;
        RecalcAutoScrollEta(win);
        UpdateToolbarEtaText(win, win->autoScroll.etaMinutes);
        SetTimer(win->hwndCanvas, kContinuousAutoScrollTimerID, USER_TIMER_MINIMUM, nullptr);
        const char* mode = dm ? "fixed" : "webview";
        LogInfo("[autoscroll] start mode=%s speed=%.2f mlt timerMinutes=%u", mode, win->autoScroll.speedMultiplier,
                win->autoScroll.timerMinutes);
        if (mm) {
            win->autoScroll.webviewPxPerSec = AutoScrollPxPerSec(win);
            win->autoScroll.webviewCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            SetWebviewAutoScroll(win, win->autoScroll.webviewCtrlDown ? 0 : win->autoScroll.webviewPxPerSec);
        }
    } else {
        KillTimer(win->hwndCanvas, kContinuousAutoScrollTimerID);
        if (mm) {
            SetWebviewAutoScroll(win, 0);
            win->autoScroll.webviewCtrlDown = false;
        }
        UpdateToolbarEtaText(win, -1);
        LogInfo("[autoscroll] stop");
    }
    SetToolbarButtonCheckedState(win, CmdAutoScrollToggle, win->autoScroll.active);
    UpdateToolbarSpeedLabel(win);
}

// Adjust continuous auto-scroll speed multiplier (step of 0.01)
void AutoScrollSpeedAdjust(MainWindow* win, int direction) {
    if (!win) {
        return;
    }
    float mlt = win->autoScroll.speedMultiplier + (direction > 0 ? kSpeedStepSize : -kSpeedStepSize);
    win->autoScroll.speedMultiplier = SnapSpeedMultiplier(mlt);
    // persist to in-memory FileState; written to disk on next settings save (app exit)
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->filePath) {
        FileState* fs = gFileHistory.FindByPath(tab->filePath);
        if (fs) {
            fs->autoScrollSpeedMultiplier = win->autoScroll.speedMultiplier;
        }
    }
    // persist to global prefs as last-used speed for new documents
    gGlobalPrefs->autoScrollSpeedMultiplier = win->autoScroll.speedMultiplier;
    if (win->autoScroll.active) {
        // the measured seconds-per-page is stale at the new speed; the
        // fallback estimate (page height / nominal speed) covers the display
        // until the next page transition re-measures
        win->autoScroll.etaTimePerPageSec = 0;
        RecalcAutoScrollEta(win);
        if (win->AsMarkdown() && !(GetKeyState(VK_CONTROL) & 0x8000)) {
            win->autoScroll.webviewPxPerSec = AutoScrollPxPerSec(win);
            SetWebviewAutoScroll(win, win->autoScroll.webviewPxPerSec);
        }
    }
    UpdateToolbarSpeedLabel(win);
}

// --- Timer-done overlay: shows "∴" at bottom of screen for 3 seconds ---
static HWND gTimerDoneHwnd = nullptr;
static constexpr UINT_PTR kTimerDoneOverlayTimerId = 99;
static constexpr wchar_t kTimerDoneOverlayClass[] = L"TumatraTimerDoneOverlay";

static LRESULT CALLBACK TimerDoneOverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 165, 0));
            HFONT hFont = CreateFontW(52, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                      CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hOld = (HFONT)SelectObject(hdc, hFont);
            DrawTextW(hdc, L"\u2234", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOld);
            DeleteObject(hFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (wp == kTimerDoneOverlayTimerId) {
                KillTimer(hwnd, kTimerDoneOverlayTimerId);
                DestroyWindow(hwnd);
                gTimerDoneHwnd = nullptr;
            }
            return 0;
        case WM_LBUTTONDOWN:
            KillTimer(hwnd, kTimerDoneOverlayTimerId);
            DestroyWindow(hwnd);
            gTimerDoneHwnd = nullptr;
            return 0;
        case WM_DESTROY:
            gTimerDoneHwnd = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterTimerDoneOverlayClass() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TimerDoneOverlayProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kTimerDoneOverlayClass;
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    RegisterClassExW(&wc);
}

static void ShowTimerDoneOverlay(MainWindow* win) {
    if (gTimerDoneHwnd) {
        KillTimer(gTimerDoneHwnd, kTimerDoneOverlayTimerId);
        DestroyWindow(gTimerDoneHwnd);
        gTimerDoneHwnd = nullptr;
    }
    RegisterTimerDoneOverlayClass();
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int overlayW = 200;
    int overlayH = 80;
    int x = (screenW - overlayW) / 2;
    int y = screenH - overlayH - 40;
    HWND hwndPopup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kTimerDoneOverlayClass, L"",
                                     WS_POPUP | WS_VISIBLE, x, y, overlayW, overlayH, nullptr, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
    if (!hwndPopup) return;
    gTimerDoneHwnd = hwndPopup;
    ShowWindow(hwndPopup, SW_SHOW);
    UpdateWindow(hwndPopup);
    SetTimer(hwndPopup, kTimerDoneOverlayTimerId, 3000, nullptr);
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
        if (0 == win->autoScroll.etaPauseStartTick) {
            win->autoScroll.etaPauseStartTick = GetTickCount();
        }
        return;
    }
    if (!win->autoScroll.active) {
        KillTimer(hwnd, kContinuousAutoScrollTimerID);
        return;
    }
    auto dm = win->AsFixed();
    if (dm) {
        // Fixed-page path (PDF, ebook, etc.)
        // v2 quick view (shift-hold): temporarily pause the auto-scroll so the
        // user can freely drag/pan the full page; releasing Shift resumes it.
        if (dm->viewportCropV2QuickToggled) {
            win->autoScroll.accum = 0;
            if (0 == win->autoScroll.etaPauseStartTick) {
                win->autoScroll.etaPauseStartTick = GetTickCount();
            }
            return;
        }
        // resuming after a pause (shift-hold or Ctrl): exclude the paused
        // time from the seconds-per-page measurement
        if (win->autoScroll.etaPauseStartTick != 0) {
            if (win->autoScroll.etaPageEnterTick > 0) {
                win->autoScroll.etaPageEnterTick += GetTickCount() - win->autoScroll.etaPauseStartTick;
            }
            win->autoScroll.etaPauseStartTick = 0;
        }
        // Check end of document (quick view returns above, so this only sees
        // the column layout)
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
                MessageBeep(MB_OK);
                ShowTimerDoneOverlay(win);
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
        // ETA resync: once per page change, measure the seconds spent on the
        // page we just left and re-estimate as (time per page) x (remaining
        // pages). TC2 counts virtual pages so each physical page naturally
        // counts twice; trim doesn't change the page count.
        int currPage = dm->CurrentPageNo();
        if (currPage != win->autoScroll.etaPageNo) {
            DWORD now = GetTickCount();
            if (win->autoScroll.etaPageEnterTick > 0) {
                float pageSec = (float)(now - win->autoScroll.etaPageEnterTick) / 1000.0f;
                if (pageSec > 0.5f) { // ignore instant transitions / jitter
                    // light smoothing so a single outlier page doesn't skew the estimate
                    float prev = win->autoScroll.etaTimePerPageSec;
                    win->autoScroll.etaTimePerPageSec = prev > 0 ? (prev + pageSec) / 2.0f : pageSec;
                }
            }
            win->autoScroll.etaPageEnterTick = now;
            win->autoScroll.etaPageNo = currPage;
            RecalcAutoScrollEta(win);
        }
        // between resyncs the display counts down like an inverted timer
        int remaining = win->autoScroll.etaMinutes - (int)((GetTickCount() - win->autoScroll.etaResyncTick) / 60000);
        if (remaining < 0) {
            remaining = 0;
        }
        if (remaining != win->autoScroll.etaLastShown) {
            win->autoScroll.etaLastShown = remaining;
            UpdateToolbarEtaText(win, remaining);
        }
    } else {
        // WebView2 path (markdown). The actual scrolling is driven by a
        // requestAnimationFrame loop in the page (SetWebviewAutoScroll); this
        // WM_TIMER tick only guards it: Ctrl-pause and the timer expiry check.
        auto mm = win->AsMarkdown();
        if (!mm) {
            return;
        }
        // Ctrl held = pause the rAF loop (toggle only on state change)
        bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (ctrlDown != win->autoScroll.webviewCtrlDown) {
            win->autoScroll.webviewCtrlDown = ctrlDown;
            SetWebviewAutoScroll(win, ctrlDown ? 0 : win->autoScroll.webviewPxPerSec);
        }
        // Timer check (same as fixed-page)
        if (win->autoScroll.timerMinutes > 0 && win->autoScroll.startTick > 0) {
            DWORD elapsedMs = GetTickCount() - win->autoScroll.startTick;
            if (elapsedMs >= (DWORD)win->autoScroll.timerMinutes * 60 * 1000) {
                win->autoScroll.active = false;
                KillTimer(hwnd, kContinuousAutoScrollTimerID);
                SetWebviewAutoScroll(win, 0);
                win->autoScroll.webviewCtrlDown = false;
                UpdateToolbarEtaText(win, -1);
                SetToolbarButtonCheckedState(win, CmdAutoScrollToggle, false);
                MessageBeep(MB_OK);
                ShowTimerDoneOverlay(win);
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