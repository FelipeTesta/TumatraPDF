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

    HWND hwndOverlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT, clsName, L"", WS_CHILD | WS_VISIBLE,
                                       rcCanvas.left, rcCanvas.top, rcCanvas.right - rcCanvas.left,
                                       rcCanvas.bottom - rcCanvas.top, GetParent(win->hwndCanvas), nullptr,
                                       GetModuleHandleW(nullptr), nullptr);

    if (!hwndOverlay) {
        return;
    }

    SetWindowLongPtrW(hwndOverlay, GWLP_USERDATA, (LONG_PTR)win);
    win->hwndContrastOverlay = hwndOverlay;

    // Set initial transparency
    BYTE alpha = AlphaFromOpacity(win->contrastOpacity);
    SetLayeredWindowAttributes(hwndOverlay, 0, alpha, LWA_ALPHA);

    // Place above canvas in z-order
    SetWindowPos(hwndOverlay, win->hwndCanvas, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void DestroyContrastOverlay(MainWindow* win) {
    if (win->hwndContrastOverlay) {
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

void UpdateContrastOverlayOpacity(MainWindow* win) {
    // Native CSS style approach for markdown contrast (replaces WebView overlay div)
    if (win->AsMarkdown()) {
        auto mm = win->AsMarkdown();
        struct WebviewWnd* wv = mm->GetWebviewWnd();
        if (wv) {
            int opacity = win->contrastOpacity;
            float cssOpacity = opacity / 100.0f;

            if (win->contrastEnabled) {
                bool invert = GetInvertPageColors();
                int textGray = invert
                    ? (int)(255 * cssOpacity)
                    : (int)(255 * (1.0f - cssOpacity));
                // Set background and text colors directly on body via inline styles
                TempStr js = fmt(
                    "document.body.style.backgroundColor = '%s';"
                    "document.body.style.color = 'rgb(%d,%d,%d)';",
                    invert ? StrL("#050505") : StrL("#FAFAFA"),
                    textGray, textGray, textGray);
                wv->Eval(js);
            } else {
                // Remove inline styles when contrast is OFF
                TempStr js = fmt(
                    "document.body.style.backgroundColor = '';"
                    "document.body.style.color = '';");
                wv->Eval(js);
            }
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
