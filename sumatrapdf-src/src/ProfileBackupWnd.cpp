#include "base/Base.h"

#include "base/Dpi.h"
#include "MainWindow.h"
#include "ProfileData.h"
#include "Theme.h"
#include "ProfileBackupWnd.h"

// Simple profile backups window: 9 slots, stamp, restore with 5s hold

struct ProfileBackupDialog {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    RECT rcSlot[9];
    RECT rcRestore[9];
    int holdingSlot = -1;
    ULONGLONG holdStartTick = 0;
    double progress = 1.0;
};

static ProfileBackupDialog* gBackupDialog = nullptr;

static void BackupLayout(ProfileBackupDialog* d) {
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int pad = DpiScale(d->hwnd, 16);
    int rowH = DpiScale(d->hwnd, 36);
    int btnW = DpiScale(d->hwnd, 120);
    int y = pad;
    for (int i = 0; i < 9; i++) {
        int slotH = rowH;
        RECT r{};
        r.left = pad;
        r.top = y;
        r.right = w - pad - btnW - DpiScale(d->hwnd, 8);
        r.bottom = y + slotH;
        d->rcSlot[i] = r;
        RECT rb{};
        rb.left = w - pad - btnW;
        rb.top = y + (slotH - DpiScale(d->hwnd, 24)) / 2;
        rb.right = w - pad;
        rb.bottom = rb.top + DpiScale(d->hwnd, 24);
        d->rcRestore[i] = rb;
        y += slotH + DpiScale(d->hwnd, 8);
    }
}

static void BackupPaint(ProfileBackupDialog* d) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->hwnd, &ps);
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));

    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ThemeWindowTextColor());

    // title
    RECT titleRc = {DpiScale(d->hwnd, 16), DpiScale(d->hwnd, 12), rc.right - DpiScale(d->hwnd, 16),
                    DpiScale(d->hwnd, 12) + DpiScale(d->hwnd, 22)};
    DrawTextW(hdc, L"Profile Backups (9 slots × 8h)", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int y = DpiScale(d->hwnd, 12) + DpiScale(d->hwnd, 28);
    int pad = DpiScale(d->hwnd, 16);
    int rowH = DpiScale(d->hwnd, 36);
    for (int i = 0; i < 9; i++) {
        char slotNum[16];
        snprintf(slotNum, sizeof(slotNum), "Slot %d", i);
        wchar_t wNum[32];
        mbstowcs(wNum, slotNum, 32);
        RECT rcSlot = {pad, y, rc.right - pad - DpiScale(d->hwnd, 132), y + rowH};
        // draw stamp
        TempStr stamp = ProfileDataBackupStampTemp(i);
        if (stamp) {
            DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(stamp))), -1, &rcSlot,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            DrawTextW(hdc, L"(empty)", -1, &rcSlot, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        RECT rcBtn = {rc.right - pad - DpiScale(d->hwnd, 120), y + (rowH - DpiScale(d->hwnd, 24)) / 2, rc.right - pad,
                      y + (rowH - DpiScale(d->hwnd, 24)) / 2 + DpiScale(d->hwnd, 24)};
        // button appearance
        bool isHolding = d->holdingSlot == i;
        RECT rcBtnInner = rcBtn;
        InflateRect(&rcBtnInner, -2, -2);
        HBRUSH br = CreateSolidBrush(isHolding ? RGB(255, 200, 200) : GetSysColor(COLOR_BTNFACE));
        FillRect(hdc, &rcBtn, br);
        DeleteObject(br);
        FrameRect(hdc, &rcBtn, (HBRUSH)GetStockObject(BLACK_BRUSH));
        wchar_t btnText[64];
        if (isHolding) {
            swprintf(btnText, 64, L"Restore (%ds)", (int)((1.0 - d->progress) * 5.0 + 0.5));
        } else {
            wcscpy(btnText, L"Restore");
        }
        DrawTextW(hdc, btnText, -1, &rcBtnInner, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        y += rowH + DpiScale(d->hwnd, 8);
    }
    SelectObject(hdc, oldFont);
    EndPaint(d->hwnd, &ps);
}

static LRESULT CALLBACK BackupWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ProfileBackupDialog* d = gBackupDialog;
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_SIZE:
            BackupLayout(d);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_PAINT:
            BackupPaint(d);
            return 0;
        case WM_LBUTTONDOWN: {
            POINT pt{(short)LOWORD(lp), (short)HIWORD(lp)};
            for (int i = 0; i < 9; i++) {
                if (PtInRect(&d->rcRestore[i], pt)) {
                    TempStr stamp = ProfileDataBackupStampTemp(i);
                    if (!stamp) {
                        logf("[profile] Backup slot %d empty, ignore restore\n", i);
                        return 0;
                    }
                    d->holdingSlot = i;
                    d->holdStartTick = GetTickCount64();
                    d->progress = 1.0;
                    SetCapture(hwnd);
                    SetTimer(hwnd, 1, 30, nullptr);
                    logf("[profile] Backup restore hold start slot %d\n", i);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            return 0;
        }
        case WM_TIMER: {
            if (wp != 1 || d->holdingSlot < 0) return 0;
            ULONGLONG elapsed = GetTickCount64() - d->holdStartTick;
            d->progress = 1.0 - (double)elapsed / 5000.0;
            if (elapsed >= 5000) {
                int slot = d->holdingSlot;
                d->holdingSlot = -1;
                KillTimer(hwnd, 1);
                ReleaseCapture();
                logf("[profile] Backup restore confirmed slot %d\n", slot);
                if (ProfileDataRecoverBackupSlot(slot)) {
                    logf("[profile] Backup slot %d restored\n", slot);
                } else {
                    logf("[profile] Backup slot %d restore failed\n", slot);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            } else {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED: {
            if (d->holdingSlot >= 0) {
                d->holdingSlot = -1;
                d->progress = 1.0;
                KillTimer(hwnd, 1);
                if (GetCapture() == hwnd) ReleaseCapture();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            gBackupDialog = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowProfileBackupsDialog(MainWindow* win) {
    static const wchar_t* className = L"TUMATRA_PROFILE_BACKUP";
    WNDCLASSW wc{};
    wc.lpfnWndProc = BackupWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    int w = DpiScale(win->hwndFrame, 420);
    int h = DpiScale(win->hwndFrame, 520);
    RECT rcParent;
    GetWindowRect(win->hwndFrame, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

    ProfileBackupDialog dlg;
    dlg.win = win;
    gBackupDialog = &dlg;
    HWND hwnd = CreateWindowExW(0, className, L"Profile Backups", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                x, y, w, h, win->hwndFrame, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwnd) {
        gBackupDialog = nullptr;
        return;
    }
    dlg.hwnd = hwnd;
    // run modal message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (gBackupDialog == nullptr) break;
    }
}
