/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
License: GPLv3 */

#include "base/Base.h"
#include "Settings.h"
#include "MainWindow.h"
#include "Canvas.h"

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
    BYTE alpha = (BYTE)((win->contrastOpacity * 255) / 100);
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
    if (!win->hwndContrastOverlay) {
        return;
    }
    BYTE alpha = (BYTE)((win->contrastOpacity * 255) / 100);
    SetLayeredWindowAttributes(win->hwndContrastOverlay, 0, alpha, LWA_ALPHA);
    InvalidateRect(win->hwndContrastOverlay, nullptr, TRUE);
}
