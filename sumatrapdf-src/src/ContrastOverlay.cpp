/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
License: GPLv3 */

#include "base/Base.h"
#include "Settings.h"
#include "MainWindow.h"
#include "Canvas.h"
#include "DocController.h"
#include "MarkdownModel.h"
#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"
#include "SumatraLog.h"
#include "Theme.h"

// Convert opacity percentage (0-100) to BYTE alpha (0-255)
static BYTE AlphaFromOpacity(int opacity) {
    return (BYTE)((opacity * 255) / 100);
}

static LRESULT CALLBACK WndProcContrastOverlay(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = (MainWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!win) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            // Draw solid black with alpha
            HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            // BUG-4 breadcrumb: catches EVERY destruction path (direct or
            // indirect), so we can see in sumlog who killed the overlay
            logfa("[contrast] overlay WM_DESTROY win=%p enabled=%d\n", win, (int)win->contrastEnabled);
            win->hwndContrastOverlay = nullptr;
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void CreateContrastOverlay(MainWindow* win) {
    if (win->hwndContrastOverlay) {
        return;
    }
    if (!win->hwndCanvas) {
        return;
    }
    // No-op for webview mode (markdown) - contrast is handled via in-page overlay
    if (win->AsMarkdown()) {
        return;
    }
    // Get canvas position and size
    RECT rcCanvas;
    GetWindowRect(win->hwndCanvas, &rcCanvas);
    MapWindowPoints(HWND_DESKTOP, GetParent(win->hwndCanvas), (LPPOINT)&rcCanvas, 2);

    const WCHAR* clsName = L"TumatraPDFContrastOverlay";
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = WndProcContrastOverlay;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = clsName;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.style = CS_DBLCLKS;
        RegisterClassW(&wc);
        classRegistered = true;
    }

    HWND hwndOverlay =
        CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT, clsName, L"", WS_CHILD | WS_VISIBLE, rcCanvas.left,
                        rcCanvas.top, rcCanvas.right - rcCanvas.left, rcCanvas.bottom - rcCanvas.top,
                        GetParent(win->hwndCanvas), nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!hwndOverlay) {
        return;
    }

    SetWindowLongPtrW(hwndOverlay, GWLP_USERDATA, (LONG_PTR)win);
    win->hwndContrastOverlay = hwndOverlay;
    logfa("[contrast] CreateContrastOverlay win=%p hwnd=%p\n", win, hwndOverlay);

    // Set initial transparency
    BYTE alpha = AlphaFromOpacity(win->contrastOpacity);
    SetLayeredWindowAttributes(hwndOverlay, 0, alpha, LWA_ALPHA);

    // Place above canvas in z-order
    SetWindowPos(hwndOverlay, win->hwndCanvas, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void DestroyContrastOverlay(MainWindow* win) {
    if (win->hwndContrastOverlay) {
        logfa("[contrast] DestroyContrastOverlay win=%p enabled=%d\n", win, (int)win->contrastEnabled);
        DestroyWindow(win->hwndContrastOverlay);
        win->hwndContrastOverlay = nullptr;
    }
}

void UpdateContrastOverlay(MainWindow* win) {
    if (!win->hwndContrastOverlay || !win->hwndCanvas) {
        return;
    }
    RECT rcCanvas;
    GetWindowRect(win->hwndCanvas, &rcCanvas);
    MapWindowPoints(HWND_DESKTOP, GetParent(win->hwndCanvas), (LPPOINT)&rcCanvas, 2);
    int w = rcCanvas.right - rcCanvas.left;
    int h = rcCanvas.bottom - rcCanvas.top;
    SetWindowPos(win->hwndContrastOverlay, nullptr, rcCanvas.left, rcCanvas.top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

// Keep the contrast overlay in sync with win->contrastEnabled: create it when
// enabled but missing, destroy when disabled but present, reposition when in
// sync. Session state (sessionData), per-document state (FileState) and the
// default state (fresh document) all write the flag at different times, and a
// write without a matching overlay update leaves the visual out of sync with
// the state - every such path (doc load, tab switch, trim config save,
// SaveSettings) calls this instead, so the overlay can never silently
// disappear while contrast is enabled (BUG-4).
void EnsureContrastOverlayState(MainWindow* win) {
    if (win->contrastEnabled) {
        if (!win->hwndContrastOverlay) {
            CreateContrastOverlay(win); // no-op for markdown (handled in-page)
        } else {
            UpdateContrastOverlay(win);
        }
    } else if (win->hwndContrastOverlay) {
        DestroyContrastOverlay(win);
    }
}

void UpdateContrastOverlayOpacity(MainWindow* win) {
    // Markdown contrast: native CSS filter replicating the PDF veil math
    if (win->AsMarkdown()) {
        auto mm = win->AsMarkdown();
        struct WebviewWnd* wv = mm->GetWebviewWnd();
        if (wv) {
            wv->Eval(MarkdownContrastJs(win->contrastEnabled, win->contrastOpacity, GetInvertPageColors()));
        }
        LogInfo("[md] contrast opacity -> %d", win->contrastOpacity);
        return;
    }
    // Fixed-page path: update layered window attributes (unchanged)
    if (!win->hwndContrastOverlay) {
        return;
    }
    BYTE alpha = AlphaFromOpacity(win->contrastOpacity);
    SetLayeredWindowAttributes(win->hwndContrastOverlay, 0, alpha, LWA_ALPHA);
    InvalidateRect(win->hwndContrastOverlay, nullptr, TRUE);
}
