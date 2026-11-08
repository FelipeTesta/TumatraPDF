#include "base/Base.h"

#include "base/Dpi.h"
#include "base/File.h"

#include "AppTools.h"
#include "MainWindow.h"
#include "ProfileBackupWnd.h"
#include "ProfileData.h"
#include "Theme.h"
#include "ProfileSyncWnd.h"

// DWM attribute for immersive dark title bar (DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
// not declared in older SDK headers, so the literal is used directly)
#include <dwmapi.h>

// gForTesting is set by -for-testing cmd-line flag (SumatraConfig.cpp)
extern bool gForTesting;

// Import prompt result: IDOK=Replace, 2=Merge, IDCANCEL=Cancel
static int gImportChoice = IDCANCEL;
static LRESULT CALLBACK ImportPromptProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wp;
            RECT rc;
            GetClientRect(h, &rc);
            HBRUSH br = CreateSolidBrush(ThemeWindowBackgroundColor());
            FillRect(hdc, &rc, br);
            DeleteObject(br);
            return TRUE;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, ThemeWindowTextColor());
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)CreateSolidBrush(ThemeWindowBackgroundColor());
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            HBRUSH br = CreateSolidBrush(ThemeWindowBackgroundColor());
            FillRect(hdc, &rc, br);
            DeleteObject(br);
            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT oldFont = (HFONT)SelectObject(hdc, font);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, ThemeWindowTextColor());
            wchar_t caption[64]{};
            GetWindowTextW(h, caption, dimof(caption));
            RECT trc{DpiScale(h, 16), DpiScale(h, 14), rc.right - DpiScale(h, 16), DpiScale(h, 14) + DpiScale(h, 22)};
            DrawTextW(hdc, caption, -1, &trc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, oldFont);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == 1) {
                gImportChoice = IDOK;
                DestroyWindow(h);
                return TRUE;
            } // Replace
            if (LOWORD(wp) == 2) {
                gImportChoice = 2;
                DestroyWindow(h);
                return TRUE;
            } // Merge
            if (LOWORD(wp) == 3) {
                gImportChoice = IDCANCEL;
                DestroyWindow(h);
                return TRUE;
            } // Cancel
            break;
        case WM_CLOSE:
            gImportChoice = IDCANCEL;
            DestroyWindow(h);
            return TRUE;
        case WM_DESTROY:
            return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

// Profile Sync modal dialog: read-only status of the profile file
// (tumatraPDF-data_1.json) plus Save Now / Open Folder / Backups / Close.
// Modal pump mirrors ShowProfileBackupsDialog (ProfileBackupWnd.cpp).

struct ProfileSyncDialog {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    // owned row strings (UTF-8), (re)built by SyncRefreshRows
    Str destDir;
    Str fileName;
    Str formatVersion;
    Str fileSize;
    Str lastSave;
    Str machine;
    Str booksCount;
    Str readingDays;
    Str studyDays;
    Str backupsCount;
};

static ProfileSyncDialog* gSyncDialog = nullptr;

static int gap8(HWND hwnd);

// button width sized to its label extent + padding (physical px, matches
// CreateWindowW coords). Measures using a compatible DC with the prompt's font
// so long labels ("Replace local data", "Merge (newest-wins)") are not truncated.
static int BtnTextW(HWND hwnd, WStr label) {
    // screen DC always available; measure with the prompt's font (or default)
    HDC hdc = GetDC(nullptr);
    if (!hdc) {
        return (int)len(label) * 10; // conservative fallback so nothing clips
    }
    HFONT font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    if (!font) {
        font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SIZE sz{};
    GetTextExtentPoint32W(hdc, label.s, len(label), &sz);
    SelectObject(hdc, oldFont);
    ReleaseDC(nullptr, hdc);
    return sz.cx;
}

// folder picker (IFileDialog in pick-folders mode) — same pattern as FlashcardToolbar::FcPickFolder
static bool SyncPickFolder(HWND hwndOwner, TempStr& outPath) {
    IFileDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) {
        logf("[profile] export - CoCreateInstance(FileOpenDialog) failed hr=0x%x\n", (unsigned)hr);
        return false;
    }
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    bool ok = false;
    hr = dlg->Show(hwndOwner);
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR wpath = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wpath)) && wpath) {
                outPath = ToUtf8Temp(WStr(wpath));
                CoTaskMemFree(wpath);
                ok = true;
            }
            item->Release();
        }
    } else {
        logf("[profile] export - folder picker cancelled\n");
    }
    dlg->Release();
    return ok;
}

// file picker (IFileDialog in file mode, *.json filter)
static bool SyncPickJsonFile(HWND hwndOwner, TempStr& outPath) {
    IFileDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) {
        logf("[profile] import - CoCreateInstance(FileOpenDialog) failed hr=0x%x\n", (unsigned)hr);
        return false;
    }
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    COMDLG_FILTERSPEC filters[] = {{L"Profile JSON", L"*.json"}};
    dlg->SetFileTypes(1, filters);
    bool ok = false;
    hr = dlg->Show(hwndOwner);
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR wpath = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wpath)) && wpath) {
                outPath = ToUtf8Temp(WStr(wpath));
                CoTaskMemFree(wpath);
                ok = true;
            }
            item->Release();
        }
    } else {
        logf("[profile] import - file picker cancelled\n");
    }
    dlg->Release();
    return ok;
}

// abbreviate a filesystem path for display: keep drive+first segment, "…",
// last 2 segments + leaf. Always fits the value column.
static TempStr SyncAbbrevPathTemp(Str path) {
    if (!path || len(path) == 0) {
        return TempStr(StrL(""));
    }
    int bs[32]{};
    int nb = 0;
    for (int i = 0; i < len(path); i++) {
        if (path.s[i] == '\\' && nb < dimof(bs)) {
            bs[nb++] = i;
        }
    }
    if (nb < 3) {
        return TempStr(path);
    }
    // prefix: drive + first segment up to and including 2nd backslash
    int prefixEnd = bs[1] + 1;
    // suffix: from 2nd-to-last backslash + 1 (last 2 segments + leaf)
    int suffixStart = bs[nb - 2] + 1;
    str::Builder sb;
    sb.Append(Str(path.s, prefixEnd));
    sb.Append(Str("\xE2\x80\xA6", 3)); // UTF-8 ellipsis U+2026
    sb.Append(Str(path.s + suffixStart, len(path) - suffixStart));
    return ToStrTemp(sb);
}

// ---- row computation (reuse ProfileData parser; no duplicate JSON scanners) --

static void SyncRefreshRows(ProfileSyncDialog* d) {
    TempStr path = ProfileDataPathTemp();
    TempStr dir = path::GetDirTemp(Str(path));
    str::ReplaceWithCopy(&d->destDir, dir ? Str(dir) : StrL(""));
    str::ReplaceWithCopy(&d->fileName, StrL("tumatraPDF-data_1.json"));

    Str json = file::ReadFile(Str(path));
    i64 sizeBytes = len(json);
    d->fileSize = fmt("%d KB", (int)((sizeBytes + 1023) / 1024));

    int version = 0, books = 0, readingDays = 0, studyDays = 0;
    i64 lastWriteTs = 0;
    bool haveStats = ProfileDataStats(&version, &books, &readingDays, &studyDays, &lastWriteTs);

    if (haveStats) {
        d->formatVersion = fmt("%d", version);
    } else {
        d->formatVersion = StrL("—");
    }

    // last save: file-system LastWriteTime (fall back to JSON lastWriteTs)
    if (file::Exists(Str(path))) {
        FILETIME ft = file::GetModificationTime(Str(path));
        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);
        wchar_t buf[64]{};
        swprintf(buf, dimof(buf), L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
        d->lastSave = str::Dup(ToUtf8Temp(WStr(buf, (int)wcslen(buf))));
    } else if (lastWriteTs > 0) {
        time_t tt = (time_t)lastWriteTs;
        struct tm tm{};
        localtime_s(&tm, &tt);
        wchar_t buf[64]{};
        swprintf(buf, dimof(buf), L"%04d-%02d-%02d %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                 tm.tm_min);
        d->lastSave = str::Dup(ToUtf8Temp(WStr(buf, (int)wcslen(buf))));
    } else {
        d->lastSave = StrL("—");
    }

    // machine: not exported by ProfileData parser; computer-name fallback
    WCHAR buf[64]{};
    DWORD n = dimof(buf);
    if (GetComputerNameW(buf, &n) && n > 0) {
        d->machine = str::Dup(ToUtf8Temp(WStr(buf, (int)n)));
    } else {
        d->machine = StrL("?");
    }

    d->booksCount = fmt("%d", books);
    d->readingDays = fmt("%d", readingDays);
    d->studyDays = fmt("%d", studyDays);

    int backups = ProfileDataBackupCount();
    d->backupsCount = fmt("%d", backups);
}

// ---- paint -----------------------------------------------------------------

static void SyncPaint(ProfileSyncDialog* d) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->hwnd, &ps);
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    HBRUSH bgBr = CreateSolidBrush(ThemeWindowBackgroundColor());
    FillRect(hdc, &rc, bgBr);
    DeleteObject(bgBr);

    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    int pad = DpiScale(d->hwnd, 16);
    int titleDy = DpiScale(d->hwnd, 22);
    int rowH = DpiScale(d->hwnd, 20);
    int gap = DpiScale(d->hwnd, 8);
    int labelW = DpiScale(d->hwnd, 150);
    int y = pad;

    // title
    SetTextColor(hdc, ThemeWindowTextColor());
    RECT trc{pad, y, rc.right - pad, y + titleDy};
    DrawTextW(hdc, L"Profile Sync", -1, &trc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += titleDy + pad;

    auto drawRow = [&](Str label, Str value, COLORREF col, UINT fmtFlags = DT_END_ELLIPSIS) {
        SetTextColor(hdc, col);
        RECT rl{pad, y, pad + labelW, y + rowH};
        DrawTextW(hdc, CWStrTemp(ToWStrTemp(label)), -1, &rl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT rv{pad + labelW + gap, y, rc.right - pad, y + rowH};
        DrawTextW(hdc, CWStrTemp(ToWStrTemp(value)), -1, &rv,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | fmtFlags);
        y += rowH + gap;
    };

    COLORREF textCol = ThemeWindowTextColor();
    COLORREF hintCol = ThemeWindowTextDisabledColor();

    drawRow(StrL("Destination dir"), Str(SyncAbbrevPathTemp(d->destDir)), textCol, DT_PATH_ELLIPSIS);
    drawRow(StrL("File"), Str(d->fileName), textCol);
    drawRow(StrL("Format version"), Str(d->formatVersion), textCol);
    drawRow(StrL("Size"), Str(d->fileSize), textCol);
    drawRow(StrL("Last save"), Str(d->lastSave), textCol);
    drawRow(StrL("Machine"), Str(d->machine), textCol);
    drawRow(StrL("Books"), Str(d->booksCount), textCol);
    drawRow(StrL("Reading days"), Str(d->readingDays), textCol);
    drawRow(StrL("Study days"), Str(d->studyDays), textCol);
    drawRow(StrL("Backups"), Str(d->backupsCount), textCol);

    // drive hint (full-width, disabled color)
    y += gap / 2;
    SetTextColor(hdc, hintCol);
    RECT hr{pad, y, rc.right - pad, y + rowH};
    DrawTextW(hdc, L"Place this folder in Google Drive to sync across devices", -1, &hr,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, oldFont);
    EndPaint(d->hwnd, &ps);
}

// ---- wndproc ---------------------------------------------------------------

static LRESULT CALLBACK SyncWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ProfileSyncDialog* d = gSyncDialog;
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wp;
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH br = CreateSolidBrush(ThemeWindowBackgroundColor());
            FillRect(hdc, &rc, br);
            DeleteObject(br);
            return TRUE;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, ThemeWindowTextColor());
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)CreateSolidBrush(ThemeWindowBackgroundColor());
        }
        case WM_PAINT:
            SyncPaint(d);
            return 0;
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == 1) { // Save Now
                ProfileDataSaveNow();
                SyncRefreshRows(d);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            if (id == 2) { // Open Folder
                TempStr dir = path::GetDirTemp(ProfileDataPathTemp());
                ShellExecuteW(hwnd, L"open", CWStrTemp(ToWStrTemp(Str(dir))), nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }
            if (id == 3) { // Backups
                DestroyWindow(hwnd);
                ShowProfileBackupsDialog(d->win);
                return 0;
            }
            if (id == 4) { // Close
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == 5) { // Export
                if (gForTesting) {
                    logf("[profile] export skipped (for-testing)\n");
                    return 0;
                }
                TempStr destDir;
                if (!SyncPickFolder(hwnd, destDir)) {
                    return 0; // cancelled or picker failed
                }
                if (ProfileDataExportToDir(Str(destDir).s)) {
                    logf("[profile] export: OK to '%s'\n", Str(destDir));
                    SyncRefreshRows(d);
                    InvalidateRect(hwnd, nullptr, TRUE);
                } else {
                    MessageBoxW(hwnd, L"Export failed — see log.", L"Profile Sync", MB_OK | MB_ICONWARNING);
                }
                return 0;
            }
            if (id == 6) { // Import
                if (gForTesting) {
                    logf("[profile] import skipped (for-testing)\n");
                    return 0;
                }
                TempStr srcPath;
                if (!SyncPickJsonFile(hwnd, srcPath)) {
                    return 0; // cancelled or picker failed
                }
                // two-choice prompt: Replace / Merge / Cancel — reuse ProfileBackupWnd prompt style
                gImportChoice = IDCANCEL;
                static const wchar_t* promptClass = L"TUMATRA_PROFILE_IMPORT";
                WNDCLASSW wc{};
                wc.lpfnWndProc = ImportPromptProc;
                wc.hInstance = GetModuleHandle(nullptr);
                wc.lpszClassName = promptClass;
                wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
                static HBRUSH sImportPromptBg = CreateSolidBrush(ThemeWindowControlBackgroundColor());
                wc.hbrBackground = sImportPromptBg;
                RegisterClassW(&wc);
                int w = DpiScale(hwnd, 360);
                int h = DpiScale(hwnd, 180);
                RECT rcParent;
                GetWindowRect(hwnd, &rcParent);
                int px = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
                int py = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;
                HWND phwnd = CreateWindowExW(0, promptClass, L"Import profile",
                                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, px, py, w, h, hwnd,
                                             nullptr, GetModuleHandle(nullptr), nullptr);
                if (!phwnd) {
                    return 0;
                }
                int pad = DpiScale(hwnd, 4);
                int btnH = DpiScale(hwnd, 28);
                int g = DpiScale(hwnd, 4);
                // size each button to its label extent so long labels are not truncated
                WStr lblReplace = WStrL(L"Replace local data");
                WStr lblMerge = WStrL(L"Merge (newest-wins)");
                WStr lblCancel = WStrL(L"Cancel");
                int tr = BtnTextW(phwnd, lblReplace);
                int tm = BtnTextW(phwnd, lblMerge);
                int tc = BtnTextW(phwnd, lblCancel);
                int wReplace, wMerge, wCancel;
                // per-button width = text extent + horizontal padding; never truncate labels.
                // Shrink gaps first, then reduce padding (floor = text width) to fit client width.
                int btnPadX = DpiScale(phwnd, 40);
                wReplace = tr + btnPadX;
                wMerge = tm + btnPadX;
                wCancel = tc + btnPadX;
                // keep all three buttons inside client width
                RECT rcPrompt{};
                GetClientRect(phwnd, &rcPrompt);
                int clientW = rcPrompt.right;
                int totalW = 2 * pad + wReplace + 2 * g + wMerge + wCancel;
                int overflow = totalW - clientW;
                if (overflow > 0) {
                    // shrink gaps first
                    int gapShrink = overflow < 2 * g ? overflow : 2 * g;
                    g -= gapShrink / 2;
                    overflow -= gapShrink;
                    totalW = 2 * pad + wReplace + 2 * g + wMerge + wCancel;
                }
                if (overflow > 0) {
                    // reduce per-button padding, floor at text width (no truncation)
                    int minPadX = DpiScale(phwnd, 32);
                    int curPad = wReplace - tr; // same padding for all buttons
                    int newPad = curPad - (overflow < curPad - minPadX ? overflow : curPad - minPadX);
                    if (newPad < minPadX) {
                        newPad = minPadX;
                    }
                    wReplace = tr + newPad;
                    wMerge = tm + newPad;
                    wCancel = tc + newPad;
                }
                // position buttons from CLIENT height (not outer h), else they fall below the
                // client area and get clipped (ground truth s40-p: bottom edge 164 vs clientH 142)
                int by = rcPrompt.bottom - pad - btnH;
                int x = pad;
                HWND btnReplace =
                    CreateWindowW(L"BUTTON", lblReplace.s, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x, by,
                                  wReplace, btnH, phwnd, (HMENU)1, GetModuleHandle(nullptr), nullptr);
                x += wReplace + g;
                HWND btnMerge = CreateWindowW(L"BUTTON", lblMerge.s, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                              x, by, wMerge, btnH, phwnd, (HMENU)2, GetModuleHandle(nullptr), nullptr);
                x += wMerge + g;
                HWND btnCancel =
                    CreateWindowW(L"BUTTON", lblCancel.s, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, x, by,
                                  wCancel, btnH, phwnd, (HMENU)3, GetModuleHandle(nullptr), nullptr);
                // disable Windows visual styles on buttons so WM_CTLCOLORBTN theming applies (same pattern as
                // SyncWndProc :535-542)
                SetWindowTheme(btnReplace, L"", L"");
                SetWindowTheme(btnMerge, L"", L"");
                SetWindowTheme(btnCancel, L"", L"");
                // modal loop for the prompt
                MSG pmsg;
                while (GetMessage(&pmsg, nullptr, 0, 0)) {
                    if (!IsDialogMessage(phwnd, &pmsg)) {
                        TranslateMessage(&pmsg);
                        DispatchMessage(&pmsg);
                    }
                    if (!IsWindow(phwnd)) {
                        break;
                    }
                }
                if (gImportChoice == IDCANCEL) {
                    return 0; // aborted
                }
                bool replace = (gImportChoice == IDOK);
                if (ProfileDataImportFromJson(Str(srcPath).s, replace)) {
                    logf("[profile] import: %s OK from '%s'\n", Str(replace ? "replace" : "merge"), Str(srcPath));
                    SyncRefreshRows(d);
                    InvalidateRect(hwnd, nullptr, TRUE);
                } else {
                    MessageBoxW(hwnd, L"Import failed — see log.", L"Profile Sync", MB_OK | MB_ICONWARNING);
                }
                return 0;
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
            gSyncDialog = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowProfileSyncDialog(MainWindow* win) {
    static const wchar_t* className = L"TUMATRA_PROFILE_SYNC";
    WNDCLASSW wc{};
    wc.lpfnWndProc = SyncWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // WM_ERASEBKGND paints the theme surface
    RegisterClassW(&wc);

    // ---- compute content height before sizing the window ----
    int pad = DpiScale(win->hwndFrame, 16);
    int titleDy = DpiScale(win->hwndFrame, 22);
    int rowH = DpiScale(win->hwndFrame, 20);
    int gap = DpiScale(win->hwndFrame, 8);
    int btnH = DpiScale(win->hwndFrame, 28);

    int contentH = pad;            // top pad
    contentH += titleDy + pad;     // title row
    contentH += 10 * (rowH + gap); // 10 data rows
    contentH += gap / 2 + rowH;    // hint row
    contentH += gap;               // gap before buttons
    contentH += btnH;              // button row 1
    contentH += gap;               // gap between button rows
    contentH += btnH;              // button row 2
    contentH += pad;               // bottom pad

    int w = DpiScale(win->hwndFrame, 580);
    int clientH = contentH;

    // convert client size to outer window size (caption + borders)
    RECT wr{0, 0, w, clientH};
    AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
    int outerW = wr.right - wr.left;
    int outerH = wr.bottom - wr.top;
    RECT rcParent;
    GetWindowRect(win->hwndFrame, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - outerW) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - outerH) / 2;

    ProfileSyncDialog dlg;
    dlg.win = win;
    gSyncDialog = &dlg;
    SyncRefreshRows(&dlg);
    HWND hwnd = CreateWindowExW(0, className, L"Profile Sync", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x,
                                y, outerW, outerH, win->hwndFrame, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwnd) {
        gSyncDialog = nullptr;
        return;
    }
    dlg.hwnd = hwnd;

    // dark title bar via DWM (DWMWA_USE_IMMERSIVE_DARK_MODE = 20; ignored on older Windows)
    BOOL darkTitle = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &darkTitle, sizeof(darkTitle));

    // place bottom-anchored two rows of 3 buttons inside actual client rect
    RECT rc;
    GetClientRect(hwnd, &rc);
    int clientW = rc.right;
    int realClientH = rc.bottom;
    int btnW = DpiScale(hwnd, 100);
    int btnH2 = DpiScale(hwnd, 28);
    int g = gap8(hwnd);
    // row 2 (bottom): Export(5), Import(6), Close(4)
    int by2 = realClientH - pad - btnH2;
    int x6 = clientW - pad - btnW; // Close
    int x5 = x6 - btnW - g;        // Import
    int x4 = x5 - btnW - g;        // Export
    // row 1 (above): Save Now(1), Open Folder(2), Backups(3)
    int by1 = by2 - btnH2 - g;
    int x3 = clientW - pad - btnW; // Backups
    int x2 = x3 - btnW - g;        // Open Folder
    int x1 = x2 - btnW - g;        // Save Now (default)
    HWND btnClose = CreateWindowW(L"BUTTON", L"Close", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x6, by2,
                                  btnW, btnH2, hwnd, (HMENU)4, GetModuleHandle(nullptr), nullptr);
    HWND btnImport = CreateWindowW(L"BUTTON", L"Import", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x5, by2,
                                   btnW, btnH2, hwnd, (HMENU)6, GetModuleHandle(nullptr), nullptr);
    HWND btnExport = CreateWindowW(L"BUTTON", L"Export", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x4, by2,
                                   btnW, btnH2, hwnd, (HMENU)5, GetModuleHandle(nullptr), nullptr);
    HWND btnBackups = CreateWindowW(L"BUTTON", L"Backups", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x3, by1,
                                    btnW, btnH2, hwnd, (HMENU)3, GetModuleHandle(nullptr), nullptr);
    HWND btnFolder = CreateWindowW(L"BUTTON", L"Open Folder", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x2,
                                   by1, btnW, btnH2, hwnd, (HMENU)2, GetModuleHandle(nullptr), nullptr);
    HWND btnSave = CreateWindowW(L"BUTTON", L"Save Now", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, x1, by1,
                                 btnW, btnH2, hwnd, (HMENU)1, GetModuleHandle(nullptr), nullptr);

    // disable Windows visual styles on buttons so WM_CTLCOLORBTN theming applies;
    // real BUTTON controls otherwise render with system visual style (light) and ignore our brush
    SetWindowTheme(btnClose, L"", L"");
    SetWindowTheme(btnImport, L"", L"");
    SetWindowTheme(btnExport, L"", L"");
    SetWindowTheme(btnBackups, L"", L"");
    SetWindowTheme(btnFolder, L"", L"");
    SetWindowTheme(btnSave, L"", L"");

    // run modal message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (gSyncDialog == nullptr) {
            break;
        }
    }
}

// Dev-only: force-launch the profile import prompt.
// Mirrors the Import case in SyncWndProc (:371-444) but bypasses the -for-testing
// skip and uses a fabricated temp JSON path instead of SyncPickJsonFile.
// Runs a modal GetMessage loop; returns when the prompt window is destroyed.
void TestLaunchImportPrompt(MainWindow* owner) {
    if (!owner) {
        return;
    }

    // Fabricate a temp JSON file so the dialog renders a real path.
    TempStr srcPath = GetTempFilePathTemp(StrL("profile-import"));
    file::WriteFile(Str(srcPath), StrL("{}"));

    gImportChoice = IDCANCEL;
    static const wchar_t* promptClass = L"TUMATRA_PROFILE_IMPORT";
    WNDCLASSW wc{};
    wc.lpfnWndProc = ImportPromptProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = promptClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    static HBRUSH sImportPromptBg = CreateSolidBrush(ThemeWindowControlBackgroundColor());
    wc.hbrBackground = sImportPromptBg;
    RegisterClassW(&wc);

    int w = DpiScale(owner->hwndFrame, 360);
    int h = DpiScale(owner->hwndFrame, 180);
    RECT rcParent;
    GetWindowRect(owner->hwndFrame, &rcParent);
    int px = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int py = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;
    HWND phwnd =
        CreateWindowExW(0, promptClass, L"Import profile", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, px, py,
                        w, h, owner->hwndFrame, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!phwnd) {
        return;
    }

    int pad = DpiScale(phwnd, 4);
    int btnH = DpiScale(phwnd, 28);
    int g = DpiScale(phwnd, 4);
    // size each button to its label extent so long labels are not truncated
    WStr lblReplace = WStrL(L"Replace local data");
    WStr lblMerge = WStrL(L"Merge (newest-wins)");
    WStr lblCancel = WStrL(L"Cancel");
    int tr = BtnTextW(phwnd, lblReplace);
    int tm = BtnTextW(phwnd, lblMerge);
    int tc = BtnTextW(phwnd, lblCancel);
    int wReplace, wMerge, wCancel;
    // per-button width = text extent + horizontal padding; never truncate labels.
    // Shrink gaps first, then reduce padding (floor = text width) to fit client width.
    int btnPadX = DpiScale(phwnd, 40);
    wReplace = tr + btnPadX;
    wMerge = tm + btnPadX;
    wCancel = tc + btnPadX;
    // keep all three buttons inside client width
    RECT rcPrompt{};
    GetClientRect(phwnd, &rcPrompt);
    int clientW = rcPrompt.right;
    int totalW = 2 * pad + wReplace + 2 * g + wMerge + wCancel;
    int overflow = totalW - clientW;
    if (overflow > 0) {
        // shrink gaps first
        int gapShrink = overflow < 2 * g ? overflow : 2 * g;
        g -= gapShrink / 2;
        overflow -= gapShrink;
        totalW = 2 * pad + wReplace + 2 * g + wMerge + wCancel;
    }
    if (overflow > 0) {
        // reduce per-button padding, floor at text width (no truncation)
        int minPadX = DpiScale(phwnd, 32);
        int curPad = wReplace - tr; // same padding for all buttons
        int newPad = curPad - (overflow < curPad - minPadX ? overflow : curPad - minPadX);
        if (newPad < minPadX) {
            newPad = minPadX;
        }
        wReplace = tr + newPad;
        wMerge = tm + newPad;
        wCancel = tc + newPad;
    }
    // position buttons from CLIENT height (not outer h), else they fall below the client area
    int by = rcPrompt.bottom - pad - btnH;
    int x = pad;
    HWND btnReplace = CreateWindowW(L"BUTTON", lblReplace.s, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x, by,
                                    wReplace, btnH, phwnd, (HMENU)1, GetModuleHandle(nullptr), nullptr);
    x += wReplace + g;
    HWND btnMerge = CreateWindowW(L"BUTTON", lblMerge.s, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x, by,
                                  wMerge, btnH, phwnd, (HMENU)2, GetModuleHandle(nullptr), nullptr);
    x += wMerge + g;
    HWND btnCancel = CreateWindowW(L"BUTTON", lblCancel.s, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, x, by,
                                   wCancel, btnH, phwnd, (HMENU)3, GetModuleHandle(nullptr), nullptr);
    SetWindowTheme(btnReplace, L"", L"");
    SetWindowTheme(btnMerge, L"", L"");
    SetWindowTheme(btnCancel, L"", L"");

    // log the fabricated path so the test can see it
    logf("[profile] test import prompt: path='%s'\n", Str(srcPath));

    // modal loop for the prompt
    MSG pmsg;
    while (GetMessage(&pmsg, nullptr, 0, 0)) {
        if (!IsDialogMessage(phwnd, &pmsg)) {
            TranslateMessage(&pmsg);
            DispatchMessage(&pmsg);
        }
        if (!IsWindow(phwnd)) {
            break;
        }
    }
}

// small horizontal gap between buttons
static int gap8(HWND hwnd) {
    return DpiScale(hwnd, 8);
}
