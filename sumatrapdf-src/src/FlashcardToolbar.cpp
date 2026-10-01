/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/File.h"
#include <shobjidl.h>
#include "Commands.h"
#include "Settings.h"
#include "GlobalPrefs.h"
#include "Translations.h"
#include "resource.h"
#include "DocController.h"
#include "TreeModel.h"
#include "Annotation.h"
#include "EngineBase.h"
extern "C" {
#include <mupdf/pdf.h>
}
#include "EngineMupdf.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Toolbar.h"
#include "ToolbarLayout.h"
#include "Theme.h"
#include "AppSettings.h"
#include "Flashcard.h"

// Flashcard secondary toolbar (study controls).
// Created when flashcard mode is toggled on.
// Pattern follows Arch Tools secondary toolbar (Toolbar2).

void FlashcardToolbarCreate(MainWindow* win) {
    if (win->flashcard.hwndToolbarFlashcard) {
        return; // already created
    }
    logf("[fc] FlashcardToolbarCreate - creating secondary toolbar\n");
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;

    // Create the rebar that hosts the flashcard toolbar below the main toolbar
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | RBS_VARHEIGHT | CCS_NODIVIDER | CCS_NOPARENTALIGN;
    if (IsCurrentThemeDefault()) {
        style |= WS_BORDER | RBS_BANDBORDERS;
    }
    DWORD exStyle = WS_EX_TOOLWINDOW;
    HWND hwndRebar = CreateWindowExW(exStyle, REBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndParent,
                                     (HMENU)IDC_FLASHCARD_REBAR, hinst, nullptr);

    REBARINFO rbi{};
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask = 0;
    rbi.himl = (HIMAGELIST) nullptr;
    SendMessageW(hwndRebar, RB_SETBARINFO, 0, (LPARAM)&rbi);

    // Create the flashcard toolbar inside the rebar
    style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | TBSTYLE_LIST | CCS_NODIVIDER |
            CCS_NOPARENTALIGN | TBSTYLE_WRAPABLE;
    exStyle = 0;
    bool isRtl = (GetWindowLong(hwndParent, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;
    HWND hwndToolbar = CreateWindowExW(exStyle, TOOLBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndRebar,
                                       (HMENU)IDC_FLASHCARD_TOOLBAR, hinst, nullptr);
    TbSetButtonStructSize(hwndToolbar, sizeofi(TBBUTTON));

    // Add study-control buttons (text-only, no icons)
    struct ToolbarButtonInfo2 {
        int cmdId;
        Str toolTip;
    };
    static ToolbarButtonInfo2 gFlashcardToolbarButtons[] = {
        {0, StrL("Cards: 0/0/0 | ")}, // card count display (button index 0, idCommand=0 = no action)
        {CmdFlashcardStudy, _TRN("Study")},
        {CmdFlashcardReveal, _TRN("Reveal")},
        {CmdFlashcardBack, _TRN("Back")},
        {CmdFlashcardNext, _TRN("Next")},
        {CmdFlashcardOrderOptions, _TRN("Order")},
        {CmdFlashcardLista, _TRN("List")},
        {CmdFlashcardFilter, _TRN("Filter")},
        {CmdFlashcardImport, _TRN("Import")},
        {CmdFlashcardSession, _TRN("Session")},
        {CmdFlashcardConfig, _TRN("Settings")},
    };
    constexpr int kFlashcardToolbarButtonsCount = dimof(gFlashcardToolbarButtons);

    TBBUTTON tbButtons[kFlashcardToolbarButtonsCount];
    for (int i = 0; i < kFlashcardToolbarButtonsCount; i++) {
        const ToolbarButtonInfo2& bi = gFlashcardToolbarButtons[i];
        TBBUTTON b{};
        b.idCommand = bi.cmdId;
        b.iBitmap = 0;
        b.fsState = TBSTATE_ENABLED;
        b.fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_AUTOSIZE;
        Str s = trans::GetTranslation(bi.toolTip);
        b.iString = (INT_PTR)CWStrTemp(s);
        tbButtons[i] = b;
    }
    TbAddButtons(hwndToolbar, kFlashcardToolbarButtonsCount, tbButtons);

    // Use the same icon/button size as the main toolbar
    int iconSize = DpiScale(hwndParent, gGlobalPrefs->toolbarSize);
    iconSize = RoundUp(iconSize, 4);
    TbSetBitmapSize(hwndToolbar, Size(iconSize, iconSize));
    TbSetButtonSize(hwndToolbar, Size(iconSize, iconSize));

    // Apply theme settings (dark mode, custom draw, etc.) to match arch tools toolbar
    ToolbarApplyThemeToRebar(hwndRebar, hwndToolbar);

    Rect rc = TbGetItemRect(hwndToolbar, 0);

    ShowWindow(hwndToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE;
    if (IsAppThemed() && IsCurrentThemeDefault()) {
        rbBand.fStyle |= RBBS_CHILDEDGE;
    }
    rbBand.hbmBack = nullptr;
    rbBand.lpText = (WCHAR*)L"Flashcard Toolbar"; // NOLINT
    rbBand.hwndChild = hwndToolbar;
    // Approximate width: button width * number of buttons
    rbBand.cxMinChild = rc.dx * kFlashcardToolbarButtonsCount;
    rbBand.cyMinChild = rc.dy + (2 * rc.y);
    rbBand.cx = 0;
    SendMessageW(hwndRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    ShowWindow(hwndRebar, SW_SHOW);

    win->flashcard.hwndReBarFlashcard = hwndRebar;
    win->flashcard.hwndToolbarFlashcard = hwndToolbar;

    logf("[fc] FlashcardToolbarCreate - toolbar created with %d buttons\n", kFlashcardToolbarButtonsCount);

    // Update the card count label
    FlashcardToolbarUpdateCount(win);

    logf("[fc] FlashcardToolbarCreate: created secondary toolbar\n");
}

void FlashcardToolbarDestroy(MainWindow* win) {
    logf("[fc] FlashcardToolbarDestroy - destroying secondary toolbar\n");
    if (!win->flashcard.hwndToolbarFlashcard) {
        return;
    }
    DestroyWindow(win->flashcard.hwndToolbarFlashcard);
    win->flashcard.hwndToolbarFlashcard = nullptr;
    DestroyWindow(win->flashcard.hwndReBarFlashcard);
    win->flashcard.hwndReBarFlashcard = nullptr;

    logf("[fc] FlashcardToolbarDestroy - done\n");
    logf("[fc] FlashcardToolbarDestroy: destroyed secondary toolbar\n");
}

// Update the flashcard toolbar card count label: "Cards: {total}/{new}/{due} | "
// Reflects the study scope: current document only, or (global session mode)
// the sum over every PDF tab of THIS window.
void FlashcardToolbarUpdateCount(MainWindow* win) {
    if (!win->flashcard.hwndToolbarFlashcard) {
        return;
    }
    int total = 0;
    int newCount = 0;
    int dueCount = 0;
    i64 now = (i64)time(nullptr) * 1000;
    auto countTab = [&](WindowTab* tab) {
        for (int i = 0; i < len(tab->flashcard.cards); i++) {
            const Flashcard& card = tab->flashcard.cards[i];
            total++;
            bool found = false;
            for (int j = 0; j < len(tab->flashcard.studyDoc.states); j++) {
                if (tab->flashcard.studyDoc.states.els[j].key == card.key) {
                    found = true;
                    const FlashcardStudyState& s = tab->flashcard.studyDoc.states.els[j].state;
                    if (s.rating == 0) {
                        newCount++;
                    } else if (s.nextReviewAt <= now) {
                        dueCount++;
                    }
                    break;
                }
            }
            if (!found) {
                newCount++;
            }
        }
    };
    if (win->flashcard.crossDocSession) {
        auto tabs = win->Tabs();
        for (WindowTab* tab : tabs) {
            if (FlashcardEnsureTabCards(tab)) {
                countTab(tab);
            }
        }
    } else {
        WindowTab* tab = win->CurrentTab();
        if (tab && FlashcardEnsureTabCards(tab)) {
            countTab(tab);
        }
    }
    TempStr text = fmt("Cards: %d/%d/%d | ", total, newCount, dueCount);
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(TBBUTTONINFOW);
    bi.dwMask = TBIF_TEXT;
    bi.pszText = (WCHAR*)CWStrTemp(ToWStrTemp(Str(text)));
    SendMessageW(win->flashcard.hwndToolbarFlashcard, TB_SETBUTTONINFOW, 0, (LPARAM)&bi);
}

// Reflect study/reveal state on the toolbar buttons (checked = active).
void FlashcardToolbarUpdateState(MainWindow* win) {
    if (!win->flashcard.hwndToolbarFlashcard) {
        return;
    }
    HWND hwnd = win->flashcard.hwndToolbarFlashcard;
    WPARAM studyState = win->flashcard.studyMode ? (TBSTATE_ENABLED | TBSTATE_CHECKED) : TBSTATE_ENABLED;
    WPARAM revealState = win->flashcard.revealMode ? (TBSTATE_ENABLED | TBSTATE_CHECKED) : TBSTATE_ENABLED;
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardStudy, MAKELONG(studyState, 0));
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardReveal, MAKELONG(revealState, 0));
    // Next button: only meaningful in study mode with a card after the current
    // one (state 0 = grayed out)
    bool hasNext = win->flashcard.studyMode && win->flashcard.currentCardIdx + 1 < len(win->flashcard.studyOrder);
    WPARAM nextState = hasNext ? TBSTATE_ENABLED : 0;
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardNext, MAKELONG(nextState, 0));
    // Filter button: checked when the current tab has an ACTIVE page filter
    // (enabled + non-empty expression)
    WindowTab* tab = win->CurrentTab();
    bool filterActive = tab && tab->flashcard.filterEnabled && tab->flashcard.filterExpr;
    WPARAM filterState = filterActive ? (TBSTATE_ENABLED | TBSTATE_CHECKED) : TBSTATE_ENABLED;
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardFilter, MAKELONG(filterState, 0));
    // Lista button: checked while the Lista panel is open
    bool listaVisible = win->flashcard.hwndListaBox && win->uiState.fcListVisible;
    WPARAM listaState = listaVisible ? (TBSTATE_ENABLED | TBSTATE_CHECKED) : TBSTATE_ENABLED;
    SendMessageW(hwnd, TB_SETSTATE, (WPARAM)CmdFlashcardLista, MAKELONG(listaState, 0));
}

// ---- shared dialog scaffolding (used by Order / Filter / Config) ---------
// The flashcard windows repeat one raw-Win32 pattern: register a window
// class, create the window centered on the parent, then run a modal pump
// with the parent disabled. These helpers carry the shared parts (extracted
// from the former per-dialog copies).

static void FlashcardRegisterDialogClass(const WCHAR* className, WNDPROC proc) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // painted in WM_PAINT
    wc.lpszClassName = className;
    // re-registering the same class+proc is harmless (ERROR_CLASS_HAS_...)
    RegisterClassExW(&wc);
}

static HWND FlashcardCreateDialogWindow(const WCHAR* className, const WCHAR* title, int cx, int cy, HWND hwndParent) {
    cx = DpiScale(hwndParent, cx);
    cy = DpiScale(hwndParent, cy);
    RECT rw;
    GetWindowRect(hwndParent, &rw);
    int x = rw.left + ((rw.right - rw.left) - cx) / 2;
    int y = rw.top + ((rw.bottom - rw.top) - cy) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, className, title, WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, cx, cy,
                                hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);
    if (hwnd) {
        // dark-mode title bar / system buttons when a dark theme is active
        ApplyDarkModeToPopupWindow(hwnd);
    }
    return hwnd;
}

// Modal pump: disable the parent while the dialog is up.
static void FlashcardRunModalDialog(HWND hwnd, HWND hwndParent, HWND hwndFocus = nullptr) {
    EnableWindow(hwndParent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetActiveWindow(hwnd);
    if (hwndFocus) {
        SetFocus(hwndFocus);
    }
    MSG msg;
    while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(hwndParent, TRUE);
    SetActiveWindow(hwndParent);
}

// ---- theme-aware drawing (works in light AND dark themes) -----------------
// All flashcard windows paint with the global theme colors (Theme.h); the
// former hard-coded white background / near-black text broke dark themes.

static void FlashcardFillBg(HDC hdc, const RECT& rc, COLORREF col) {
    HBRUSH bg = CreateSolidBrush(col);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
}

// cached brush for WM_CTLCOLOREDIT / WM_CTLCOLORLISTBOX answers (the caller
// must NOT delete the returned brush). Rebuilt when the theme color changes.
static HBRUSH FlashcardCtlColorBrush() {
    static HBRUSH brush = nullptr;
    static COLORREF brushCol = (COLORREF)-1;
    COLORREF col = ThemeWindowBackgroundColor();
    if (!brush || brushCol != col) {
        if (brush) {
            DeleteObject(brush);
        }
        brush = CreateSolidBrush(col);
        brushCol = col;
    }
    return brush;
}

// Read an EDIT control's text (NUL-terminated by GetWindowTextW); used by
// the Filter expression input and the Config path input
static Str FcGetEditText(HWND hwndEdit) {
    WCHAR buf[4096];
    int n = GetWindowTextW(hwndEdit, buf, dimofi(buf));
    if (n <= 0) {
        return Str();
    }
    return ToUtf8Temp(WStr(buf));
}

// flat theme-colored button (hot state on hover/hold)
static void FlashcardDrawButton(HDC hdc, const RECT& rc, const WCHAR* label, bool hot) {
    FlashcardFillBg(hdc, rc, hot ? ThemeHotBackgroundColor() : ThemeWindowControlBackgroundColor());
    HPEN pen = CreatePen(PS_SOLID, 1, hot ? ThemeHotEdgeColor() : ThemeEdgeColor());
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ThemeWindowTextColor());
    RECT rcText = rc;
    DrawTextW(hdc, label, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// one hold-to-confirm button: idle track line under the label + draining
// colored line while holding (progress 1 -> 0). Used by the Config window's
// clear actions and the Filter window's clear button.
static void FlashcardDrawHoldButton(HWND hwnd, HDC hdc, const RECT& rc, const WCHAR* label, bool holding,
                                    double progress, COLORREF drainColor) {
    FlashcardDrawButton(hdc, rc, label, holding);
    int lineH = DpiScale(hwnd, 3);
    int inset = DpiScale(hwnd, 10);
    int yLine = rc.bottom - DpiScale(hwnd, 9);
    int fullW = rc.right - rc.left - 2 * inset;
    int wNow = (int)(fullW * progress);
    RECT lineIdle = {rc.left + inset, yLine, rc.right - inset, yLine + lineH};
    FlashcardFillBg(hdc, lineIdle, ThemeEdgeColor()); // subtle idle hint: full-width track
    if (holding && wNow > 0) {
        RECT line = {rc.left + inset, yLine, rc.left + inset + wNow, yLine + lineH};
        FlashcardFillBg(hdc, line, drainColor);
    }
}

// ---- Flashcard Config window ----------------------------------------------
// Opens from the toolbar "Config" button. Layout (top to bottom):
//   Pasta do histórico de estudos:  [EDIT path] [📂]   — change where the
//     study-history JSONs are stored (e.g. a Google Drive folder so they
//     stay backed up). Empty = default app-data dir. Existing files are NOT
//     migrated: the new dir starts empty (changing back restores them).
//   Documentos no histórico:
//   +---------------------------------------------+
//   | book.pdf       12    3    2026-09-30 14:05 |   — owner-drawn list,
//   | other.pdf       5    0    2026-09-28 09:11 |     ~10 rows + scroll
//   +---------------------------------------------+
//   [Clear Selected (2s)] [Clear All (5s)]
//   [Link to PDF...] [Restore Backup (5s)]
//
// The clear buttons hold-to-confirm (orange 2s for the SELECTED book, red 5s
// for ALL books). X / Esc closes. Actions execute inside the window (it owns
// the selection), and every step is [fc]-logged.

enum {
    IDC_FC_CONFIG_EDIT = 60011,
    IDC_FC_CONFIG_LIST = 60012,
    IDC_FC_SESSION_SEARCH = 60013,
    IDC_FC_SESSION_LIST = 60014,
};

enum {
    kFcConfigHoldNone = 0,
    kFcConfigHoldSelected = 1,
    kFcConfigHoldAll = 2,
    kFcConfigHoldRecover = 3, // "Restore Backup" (5s hold)
};

struct ConfigDialog {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    HWND hwndPathEdit = nullptr;
    HWND hwndList = nullptr;
    Vec<FlashcardStudyDocInfo> docs;
    int selIdx = -1; // selected row (-1 = none)
    RECT rcEditCaption{}, rcListCaption{}, rcListHeader{};
    RECT rcBrowse{};   // 📂 folder button
    RECT rcClearSel{}; // "Clear Selected" (2s hold)
    RECT rcClearAll{}; // "Clear All" (5s hold)
    RECT rcLink{};     // "Link to PDF..." (manual resync)
    RECT rcRecover{};  // "Restore Backup" (5s hold)
    RECT rcRecheck{};  // "Re-check" (re-validate sync markers)
    int holding = kFcConfigHoldNone;
    ULONGLONG holdStartTick = 0;
    int holdMs = 2000;
    double progress = 1.0; // 1 = full drain line, drains to 0 during the hold
};

static ConfigDialog* gConfigDialog = nullptr;
static WNDPROC gConfigEditOrigProc = nullptr;

// free the owned strings of a docs vec (rows filled by FlashcardStudyListDocs)
static void ConfigFreeDocs(Vec<FlashcardStudyDocInfo>& docs) {
    for (int i = 0; i < len(docs); i++) {
        ::free(docs[i].fileName);
        ::free(docs[i].docName);
        ::free(docs[i].docPath);
    }
    docs.Reset();
}

// "YYYY-MM-DD HH:MM" from a ms timestamp; never-reviewed shows as em dash
static TempStr ConfigFormatDateTemp(i64 ms) {
    if (ms <= 0) {
        return StrL("—");
    }
    time_t secs = (time_t)(ms / 1000);
    struct tm tmv;
    if (localtime_s(&tmv, &secs) != 0) {
        return StrL("—");
    }
    return fmt("%04d-%02d-%02d %02d:%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min);
}

// emoji-capable font for the 📂 browse button (DEFAULT_GUI_FONT has no
// U+1F4C2 glyph); created once
static HFONT ConfigEmojiFont(HWND hwnd) {
    static HFONT font = nullptr;
    if (!font) {
        font =
            CreateFontW(-DpiScale(hwnd, 16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Emoji");
    }
    return font;
}

// modern folder picker (IFileDialog in pick-folders mode)
static bool FcPickFolder(HWND hwndOwner, TempStr& outPath) {
    IFileDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) {
        logf("[fc] Config - CoCreateInstance(FileOpenDialog) failed hr=0x%x\n", (unsigned)hr);
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
        logf("[fc] Config - folder picker cancelled\n");
    }
    dlg->Release();
    return ok;
}

// PDF file picker (IFileDialog in file mode) — used by the manual RESYNC
// ("Link to PDF...") so the user can point an orphaned history at the
// renamed book on disk
static bool FcPickPdfFile(HWND hwndOwner, TempStr& outPath) {
    IFileDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) {
        logf("[fc] Config - CoCreateInstance(FileOpenDialog) failed hr=0x%x\n", (unsigned)hr);
        return false;
    }
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    COMDLG_FILTERSPEC filters[] = {{L"PDF files", L"*.pdf"}};
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
        logf("[fc] Config - PDF picker cancelled\n");
    }
    dlg->Release();
    return ok;
}

// reload the history list + path edit from the current study dir
static void ConfigReloadList(ConfigDialog* d) {
    ConfigFreeDocs(d->docs);
    d->selIdx = -1;
    FlashcardStudyListDocs(d->docs);
    SendMessageW(d->hwndList, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < len(d->docs); i++) {
        SendMessageW(d->hwndList, LB_ADDSTRING, 0, (LPARAM)L"");
    }
    TempStr dir = FlashcardStudyDir();
    SetWindowTextW(d->hwndPathEdit, CWStrTemp(ToWStrTemp(Str(dir))));
    InvalidateRect(d->hwnd, nullptr, TRUE);
}

// apply a new study dir (from the 📂 picker or a typed path): persist the
// setting, make sure the dir exists, MIGRATE the existing study JSONs from
// the old dir to the new one (files already in the target are kept — the
// next save of an open book writes the up-to-date state anyway), reload the
// list. Clearing the field goes back to the default app-data dir (files
// migrate back).
static void ConfigApplyPath(ConfigDialog* d, Str path) {
    str::TrimWSInPlace(path, str::TrimOpt::Both);
    TempStr oldDir = FlashcardStudyDir();
    str::ReplaceWithCopy(&gGlobalPrefs->flashcardStudyDir, path);
    if (path) {
        dir::CreateAll(path);
    }
    SaveSettings();
    int nMoved = FlashcardStudyMigrateFiles(oldDir, FlashcardStudyDir());
    logf("[fc] Config - study dir set to '%s' (moved %d file(s) from '%s')\n",
         path ? path : StrL("(default app-data)"), nMoved, Str(oldDir));
    ConfigReloadList(d);
}

// delete the JSON of the book selected in the list (2s hold). When that book
// is open in any tab of this window, its in-memory studyDoc resets too.
static void ConfigClearSelected(ConfigDialog* d) {
    if (d->selIdx < 0 || d->selIdx >= len(d->docs)) {
        logf("[fc] Config - clear selected ignored: no row selected\n");
        return;
    }
    FlashcardStudyDocInfo& info = d->docs[d->selIdx];
    TempStr dir = FlashcardStudyDir();
    TempStr file = path::JoinTemp(dir, Str(info.fileName ? info.fileName : ""));
    if (DeleteFileW(CWStrTemp(ToWStrTemp(Str(file))))) {
        logf("[fc] Config - cleared history of '%s' (deleted %s)\n", Str(info.docName ? info.docName : "?"),
             Str(info.fileName ? info.fileName : "?"));
    } else {
        logf("[fc] Config - ERROR: failed to delete '%s' (lastError=%u)\n", Str(file), (unsigned)GetLastError());
    }
    // reset the in-memory history of any tab that IS this document (md5 match)
    auto tabs = d->win->Tabs();
    for (WindowTab* tab : tabs) {
        if (!tab->filePath) {
            continue;
        }
        TempStr studyPath = FlashcardStudyPath(tab->filePath.s);
        TempStr base = path::GetBaseNameTemp(Str(studyPath));
        if (str::Eq(Str(base), Str(info.fileName ? info.fileName : ""))) {
            tab->flashcard.studyDoc.states.Reset();
            FlashcardStopSession(d->win);
            FlashcardToolbarUpdateCount(d->win);
            FlashcardToolbarUpdateState(d->win);
            FlashcardSidebarPopulate(d->win); // every card is "new" now
            MainWindowRerender(d->win);
        }
    }
    ConfigReloadList(d);
}

// delete EVERY study JSON + reset every tab's in-memory history (5s hold)
static void ConfigClearAll(ConfigDialog* d) {
    int nDeleted = FlashcardDeleteAllStudyFiles();
    auto tabs = d->win->Tabs();
    for (WindowTab* tab : tabs) {
        tab->flashcard.studyDoc.states.Reset();
    }
    FlashcardStopSession(d->win);
    FlashcardToolbarUpdateCount(d->win);
    FlashcardToolbarUpdateState(d->win);
    FlashcardSidebarPopulate(d->win); // refresh hook: every card is "new" now
    MainWindowRerender(d->win);
    logf("[fc] Config - cleared history for ALL books (%d files)\n", nDeleted);
    ConfigReloadList(d);
}

// manual RESYNC: re-point the SELECTED book's history at a PDF picked by the
// user. Covers the rename case the automatic adopt-by-docName cannot (the
// base name changed too). The picked file's history, if open in a tab, is
// reloaded so counts/sidebar reflect the adopted states immediately.
static void ConfigResyncSelected(ConfigDialog* d) {
    if (d->selIdx < 0 || d->selIdx >= len(d->docs)) {
        logf("[fc] Config - resync ignored: no row selected\n");
        return;
    }
    FlashcardStudyDocInfo& info = d->docs[d->selIdx];
    TempStr picked = nullptr;
    if (!FcPickPdfFile(d->hwnd, picked)) {
        return; // cancelled — the picker is the confirmation
    }
    TempStr dir = FlashcardStudyDir();
    TempStr jsonPath = path::JoinTemp(dir, Str(info.fileName ? info.fileName : ""));
    if (!FlashcardStudyResync(CStrTemp(Str(jsonPath)), CStrTemp(Str(picked)))) {
        logf("[fc] Config - resync FAILED for '%s'\n", Str(info.docName ? info.docName : "?"));
        return;
    }
    auto tabs = d->win->Tabs();
    for (WindowTab* tab : tabs) {
        if (tab->filePath && str::EqI(tab->filePath, Str(picked))) {
            tab->flashcard.studyDoc = FlashcardStudyLoad(tab->filePath.s);
            FlashcardToolbarUpdateCount(d->win);
            FlashcardToolbarUpdateState(d->win);
            FlashcardSidebarPopulate(d->win);
            MainWindowRerender(d->win);
        }
    }
    ConfigReloadList(d);
}

// restore the newest backup slot over the study dir (5s hold): every open
// tab reloads its history from disk, then the list/counts/sidebar refresh
static void ConfigRecoverBackup(ConfigDialog* d) {
    int nRestored = FlashcardStudyRecoverBackup();
    if (nRestored > 0) {
        auto tabs = d->win->Tabs();
        for (WindowTab* tab : tabs) {
            if (tab->filePath) {
                tab->flashcard.studyDoc = FlashcardStudyLoad(tab->filePath.s);
            }
        }
        FlashcardToolbarUpdateCount(d->win);
        FlashcardToolbarUpdateState(d->win);
        FlashcardSidebarPopulate(d->win);
        MainWindowRerender(d->win);
    }
    ConfigReloadList(d);
}

// import the NEW flashcards from another copy of the SAME book (study
// partner's PDF) into the CURRENT tab's document: creates the missing
// highlight annotations, study history untouched (imported cards are "new")
static void ConfigLayout(ConfigDialog* d) {
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int pad = DpiScale(d->hwnd, 14);
    int capDy = DpiScale(d->hwnd, 22);
    int editH = DpiScale(d->hwnd, 26);
    int btnH = DpiScale(d->hwnd, 44);
    int gap = DpiScale(d->hwnd, 10);
    int browseW = DpiScale(d->hwnd, 40);

    int y = pad;
    d->rcEditCaption = {pad, y, w - pad, y + capDy};
    y += capDy + DpiScale(d->hwnd, 4);
    if (d->hwndPathEdit && d->hwndList) {
        MoveWindow(d->hwndPathEdit, pad, y, w - 2 * pad - browseW - gap, editH, TRUE);
        d->rcBrowse = {w - pad - browseW, y, w - pad, y + editH};
    }
    y += editH + DpiScale(d->hwnd, 10);
    // "Re-check" (re-validate sync markers, e.g. after a Drive folder comes
    // back online) shares the list-caption row, right-aligned; the caption
    // text stops before it
    int recheckW = DpiScale(d->hwnd, 92);
    d->rcListCaption = {pad, y, w - pad - recheckW - gap, y + capDy};
    d->rcRecheck = {w - pad - recheckW, y, w - pad, y + capDy};
    y += capDy + DpiScale(d->hwnd, 4);
    // column-header strip over the list (labels line up with the row columns,
    // same anchors as ConfigDrawListItem) — s30-B
    int hdrDy = DpiScale(d->hwnd, 20);
    d->rcListHeader = {pad, y, w - pad, y + hdrDy};
    y += hdrDy;
    // two bottom rows of buttons: clears on top, resync/recover below
    int yBtn2 = h - pad - btnH;      // bottom row
    int yBtn1 = yBtn2 - gap - btnH;  // row above
    if (d->hwndList) {
        MoveWindow(d->hwndList, pad, y, w - 2 * pad, yBtn1 - gap - y, TRUE);
    }
    // row 1: "Clear Selected" (2s) | "Clear All" (5s)
    int selW = DpiScale(d->hwnd, 185);
    int allW = DpiScale(d->hwnd, 185);
    int totalW = selW + allW + gap;
    int x0 = (w - totalW) / 2;
    if (x0 < 0) {
        x0 = 0; // tiny window: left-align instead of center
    }
    d->rcClearSel = {x0, yBtn1, x0 + selW, yBtn1 + btnH};
    d->rcClearAll = {x0 + selW + gap, yBtn1, x0 + selW + gap + allW, yBtn1 + btnH};
    // row 2: "Link to PDF..." | "Restore Backup"
    // (Import moved to its own toolbar button/window — s30-C)
    int linkW = DpiScale(d->hwnd, 165);
    int recW = DpiScale(d->hwnd, 165);
    int totalW2 = linkW + recW + gap;
    int x1 = (w - totalW2) / 2;
    if (x1 < 0) {
        x1 = 0;
    }
    d->rcLink = {x1, yBtn2, x1 + linkW, yBtn2 + btnH};
    d->rcRecover = {x1 + linkW + gap, yBtn2, x1 + linkW + gap + recW, yBtn2 + btnH};
}

// owner-drawn one history row: |PDF |cartões|due|última revisão| — name
// flexible on the left, numbers/date right-aligned on the right
static void ConfigDrawListItem(ConfigDialog* d, const DRAWITEMSTRUCT* dis) {
    int idx = (int)dis->itemID;
    if (idx < 0 || idx >= len(d->docs)) {
        return;
    }
    const FlashcardStudyDocInfo& info = d->docs[idx];
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool selected = idx == d->selIdx;
    FlashcardFillBg(hdc, rc, selected ? ThemeHotBackgroundColor() : ThemeWindowBackgroundColor());
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ThemeWindowTextColor());
    int pad = DpiScale(d->hwnd, 6);
    int gap = DpiScale(d->hwnd, 10);
    int cLastW = DpiScale(d->hwnd, 110);
    int cNumW = DpiScale(d->hwnd, 56);
    int dotD = DpiScale(d->hwnd, 8);
    // sync marker: green = PDF found at the saved path, red = missing
    // (moved/renamed — resync needed), grey = legacy file (no path saved)
    {
        int dotX = rc.left + pad;
        int dotY = (rc.top + rc.bottom - dotD) / 2;
        COLORREF col = RGB(158, 158, 158);
        if (info.syncStatus == 1) {
            col = RGB(67, 160, 71);
        } else if (info.syncStatus == 2) {
            col = RGB(229, 57, 53);
        }
        HBRUSH br = CreateSolidBrush(col);
        HGDIOBJ oldBr = SelectObject(hdc, br);
        HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, dotX, dotY, dotX + dotD, dotY + dotD);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(br);
    }
    // name on the left (ellipsized), up to where the numbers start
    RECT rcName = rc;
    rcName.left += pad + dotD + pad;
    rcName.right = rc.right - pad - cLastW - 2 * (gap + cNumW);
    TempStr name = fmt("%s", Str(info.docName ? info.docName : "?"));
    DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(name))), -1, &rcName,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    auto drawRight = [&](int xRight, TempStr txt) {
        RECT rcT = {xRight - cNumW, rc.top, xRight, rc.bottom};
        DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(txt))), -1, &rcT, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    };
    drawRight(rc.right - pad - cLastW - gap - cNumW, fmt("%d", info.dueCount));
    drawRight(rc.right - pad - cLastW - 2 * (gap + cNumW), fmt("%d", info.totalCards));
    RECT rcLast = {rc.right - pad - cLastW, rc.top, rc.right - pad, rc.bottom};
    DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(ConfigFormatDateTemp(info.lastReviewedAt)))), -1, &rcLast,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

static void ConfigPaint(ConfigDialog* d) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->hwnd, &ps);
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    FlashcardFillBg(hdc, rc, ThemeWindowBackgroundColor());
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    SetTextColor(hdc, ThemeWindowDarkerTextColor());
    DrawTextW(hdc, L"Study history folder (empty = app default):", -1, &d->rcEditCaption,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, L"Documents in history:", -1, &d->rcListCaption, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // column header strip: labels at the SAME x anchors the rows use
    // (pad 6 / gap 10 / cNumW 56 / cLastW 110 — keep in sync with
    // ConfigDrawListItem) so each column is visually named and aligned
    {
        int pad = DpiScale(d->hwnd, 6);
        int gap = DpiScale(d->hwnd, 10);
        int cLastW = DpiScale(d->hwnd, 110);
        int cNumW = DpiScale(d->hwnd, 56);
        int dotD = DpiScale(d->hwnd, 8);
        RECT rcH = d->rcListHeader;
        SetTextColor(hdc, ThemeWindowDarkerTextColor());
        RECT rcPdf = rcH;
        rcPdf.left += pad + dotD + pad;
        DrawTextW(hdc, L"PDF", -1, &rcPdf, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT rcCards = {rcH.right - pad - cLastW - 2 * (gap + cNumW), rcH.top, rcH.right - pad - cLastW - 2 * gap - cNumW, rcH.bottom};
        DrawTextW(hdc, L"Cards", -1, &rcCards, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        RECT rcDue = {rcH.right - pad - cLastW - gap - cNumW, rcH.top, rcH.right - pad - cLastW - gap, rcH.bottom};
        DrawTextW(hdc, L"Due", -1, &rcDue, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        RECT rcLastH = {rcH.right - pad - cLastW, rcH.top, rcH.right - pad, rcH.bottom};
        DrawTextW(hdc, L"Last reviewed", -1, &rcLastH, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        // thin separator under the header
        RECT rcSep = {rcH.left, rcH.bottom, rcH.right, rcH.bottom + DpiScale(d->hwnd, 1)};
        FlashcardFillBg(hdc, rcSep, ThemeEdgeColor());
    }

    // 📂 browse button: frame + emoji glyph (needs the emoji font)
    FlashcardDrawButton(hdc, d->rcBrowse, L"", false);
    SelectObject(hdc, ConfigEmojiFont(d->hwnd));
    SetTextColor(hdc, ThemeWindowTextColor());
    RECT rcBrowse = d->rcBrowse;
    DrawTextW(hdc, L"📂", -1, &rcBrowse, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, font);

    // "Clear Selected": orange drain, 2s hold
    FlashcardDrawHoldButton(d->hwnd, hdc, d->rcClearSel, L"Clear Selected",
                            d->holding == kFcConfigHoldSelected,
                            d->holding == kFcConfigHoldSelected ? d->progress : 1.0, RGB(255, 152, 0));
    // "Clear All": red drain, 5s hold
    FlashcardDrawHoldButton(d->hwnd, hdc, d->rcClearAll, L"Clear All", d->holding == kFcConfigHoldAll,
                            d->holding == kFcConfigHoldAll ? d->progress : 1.0, RGB(229, 57, 53));
    // "Link to PDF...": instant (the file picker is the confirmation)
    FlashcardDrawButton(hdc, d->rcLink, L"Link to PDF...", false);
    // "Restore Backup": red drain, 5s hold (overwrites current history)
    FlashcardDrawHoldButton(d->hwnd, hdc, d->rcRecover, L"Restore Backup", d->holding == kFcConfigHoldRecover,
                            d->holding == kFcConfigHoldRecover ? d->progress : 1.0, RGB(229, 57, 53));
    // "Re-check": instant — re-validate every sync marker (e.g. a Drive
    // folder that came back online)
    FlashcardDrawButton(hdc, d->rcRecheck, L"Re-check", false);

    SelectObject(hdc, oldFont);
    EndPaint(d->hwnd, &ps);
}

// Enter in the path edit applies the typed dir; Esc closes the window
static LRESULT CALLBACK ConfigEditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        if (gConfigDialog) {
            ConfigApplyPath(gConfigDialog, FcGetEditText(gConfigDialog->hwndPathEdit));
        }
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        SendMessageW(GetParent(hwnd), WM_CLOSE, 0, 0);
        return 0;
    }
    return CallWindowProc(gConfigEditOrigProc, hwnd, msg, wp, lp);
}

static LRESULT CALLBACK ConfigWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ConfigDialog* d = gConfigDialog;
    if (msg == WM_CREATE) {
        gConfigDialog->hwnd = hwnd;
        return 0;
    }
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_SIZE:
            ConfigLayout(d);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_ERASEBKGND:
            return 1; // painted in WM_PAINT
        case WM_PAINT:
            ConfigPaint(d);
            return 0;
        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lp;
            if (mis->CtlID == IDC_FC_CONFIG_LIST) {
                mis->itemHeight = DpiScale(hwnd, 24);
                return TRUE;
            }
            break;
        }
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lp;
            if (dis->CtlID == IDC_FC_CONFIG_LIST) {
                ConfigDrawListItem(d, dis);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            int code = HIWORD(wp);
            if (id == IDC_FC_CONFIG_LIST && code == LBN_SELCHANGE) {
                // the selection PERSISTS (unlike the Filter's toggle list):
                // "Clear Selected" acts on the selected row
                d->selIdx = (int)SendMessageW(d->hwndList, LB_GETCURSEL, 0, 0);
                logf("[fc] Config - selected row %d\n", d->selIdx);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            break;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            // theme the EDIT / LISTBOX interiors (dark mode support)
            HDC hdc = (HDC)wp;
            SetBkColor(hdc, ThemeWindowBackgroundColor());
            SetTextColor(hdc, ThemeWindowTextColor());
            return (LRESULT)FlashcardCtlColorBrush();
        }
        case WM_LBUTTONDOWN: {
            POINT pt{(short)LOWORD(lp), (short)HIWORD(lp)};
            if (PtInRect(&d->rcRecheck, pt)) {
                logf("[fc] Config - re-check: re-validating sync markers\n");
                ConfigReloadList(d);
                d->selIdx = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (PtInRect(&d->rcBrowse, pt)) {
                TempStr path = nullptr;
                if (FcPickFolder(hwnd, path)) {
                    ConfigApplyPath(d, Str(path));
                }
            } else if (PtInRect(&d->rcLink, pt)) {
                ConfigResyncSelected(d);
            } else if (PtInRect(&d->rcClearSel, pt)) {
                if (d->selIdx < 0) {
                    logf("[fc] Config - clear selected ignored: no row selected\n");
                    return 0;
                }
                d->holding = kFcConfigHoldSelected;
                d->holdMs = 2000;
                d->holdStartTick = GetTickCount64();
                d->progress = 1.0;
                SetCapture(hwnd);
                SetTimer(hwnd, 1, 15, nullptr);
                logf("[fc] Config - hold start (selected book, %dms)\n", d->holdMs);
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (PtInRect(&d->rcClearAll, pt)) {
                d->holding = kFcConfigHoldAll;
                d->holdMs = 5000;
                d->holdStartTick = GetTickCount64();
                d->progress = 1.0;
                SetCapture(hwnd);
                SetTimer(hwnd, 1, 15, nullptr);
                logf("[fc] Config - hold start (ALL books, %dms)\n", d->holdMs);
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (PtInRect(&d->rcRecover, pt)) {
                d->holding = kFcConfigHoldRecover;
                d->holdMs = 5000;
                d->holdStartTick = GetTickCount64();
                d->progress = 1.0;
                SetCapture(hwnd);
                SetTimer(hwnd, 1, 15, nullptr);
                logf("[fc] Config - hold start (recover backup, %dms)\n", d->holdMs);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_TIMER: {
            if (d->holding == kFcConfigHoldNone || wp != 1) {
                return 0;
            }
            ULONGLONG elapsed = GetTickCount64() - d->holdStartTick;
            d->progress = 1.0 - (double)elapsed / (double)d->holdMs;
            if (elapsed >= (ULONGLONG)d->holdMs) {
                int action = d->holding;
                d->holding = kFcConfigHoldNone;
                logf("[fc] Config - hold CONFIRMED (action %d, %dms)\n", action, d->holdMs);
                if (action == kFcConfigHoldSelected) {
                    ConfigClearSelected(d);
                } else if (action == kFcConfigHoldRecover) {
                    ConfigRecoverBackup(d);
                } else {
                    ConfigClearAll(d);
                }
            } else {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            if (d->holding != kFcConfigHoldNone) {
                d->holding = kFcConfigHoldNone;
                d->progress = 1.0;
                logf("[fc] Config - hold released early\n");
            }
            KillTimer(hwnd, 1);
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                logf("[fc] Config - closed (Esc)\n");
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_CLOSE:
            logf("[fc] Config - closed (X)\n");
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void FlashcardConfigDialog(MainWindow* win) {
    FlashcardRegisterDialogClass(L"TUMATRA_FLASHCARD_CONFIG", ConfigWndProc);
    HWND hwndParent = win->hwndFrame;
    ConfigDialog dlg; // no {}: Vec's default ctor is explicit
    dlg.win = win;
    gConfigDialog = &dlg; // stack-local: safe, the modal pump below owns it
    HWND hwnd = FlashcardCreateDialogWindow(L"TUMATRA_FLASHCARD_CONFIG", L"Settings", 560, 506, hwndParent);
    if (!hwnd) {
        gConfigDialog = nullptr;
        return;
    }
    logf("[fc] Config - window opened\n");

    // child controls: study-dir EDIT + history LISTBOX
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    dlg.hwndPathEdit =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0,
                        0, 0, hwnd, (HMENU)IDC_FC_CONFIG_EDIT, hinst, nullptr);
    dlg.hwndList = CreateWindowExW(0, L"LISTBOX", nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_OWNERDRAWFIXED |
                                       LBS_NOINTEGRALHEIGHT | WS_TABSTOP,
                                   0, 0, 0, 0, hwnd, (HMENU)IDC_FC_CONFIG_LIST, hinst, nullptr);
    SendMessageW(dlg.hwndPathEdit, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(dlg.hwndList, WM_SETFONT, (WPARAM)font, TRUE);

    // subclass the path edit: Enter = apply, Esc = close
    if (nullptr == gConfigEditOrigProc) {
        gConfigEditOrigProc = (WNDPROC)GetWindowLongPtr(dlg.hwndPathEdit, GWLP_WNDPROC);
    }
    SetWindowLongPtr(dlg.hwndPathEdit, GWLP_WNDPROC, (LONG_PTR)ConfigEditProc);

    ConfigReloadList(&dlg);
    ConfigLayout(&dlg);
    FlashcardRunModalDialog(hwnd, hwndParent, dlg.hwndPathEdit);
    ConfigFreeDocs(dlg.docs); // owned strings must not outlive the pump
    gConfigDialog = nullptr;
}

// ---- Import window ---------------------------------------------------------
// Toolbar "Import" button (s30-C). Compares the CURRENT document's flashcards
// with a picked source PDF (another copy of the same book — e.g. a study
// partner's file): a | document | cards | table for both, the dry-run
// "New cards: N" diff count (same dedupe rule as the merge), and only then
// creates the new cards on confirm. Study history is never touched —
// imported cards are NEW cards. Every step is [fc]-logged.

struct ImportDialog {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    char* srcPath = nullptr; // owned (str::Dup): picked source PDF
    int srcCards = -1;       // source card count (-1 = nothing picked yet)
    int newCards = -1;       // dry-run diff count (-1 = not computed)
    int importedTotal = 0;   // cards created during this dialog's lifetime
    RECT rcPick{}, rcImportBtn{}, rcTable{}, rcNewLine{};
};

static ImportDialog* gImportDialog = nullptr;

static void ImportLayout(ImportDialog* d) {
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int pad = DpiScale(d->hwnd, 14);
    int btnH = DpiScale(d->hwnd, 40);
    int capDy = DpiScale(d->hwnd, 22);
    d->rcTable = {pad, pad, w - pad, pad + DpiScale(d->hwnd, 108)};
    d->rcNewLine = {pad, d->rcTable.bottom + DpiScale(d->hwnd, 10), w - pad,
                    d->rcTable.bottom + DpiScale(d->hwnd, 10) + capDy};
    int yBtn = h - pad - btnH;
    int pickW = DpiScale(d->hwnd, 130);
    int impW = DpiScale(d->hwnd, 240);
    d->rcPick = {pad, yBtn, pad + pickW, yBtn + btnH};
    d->rcImportBtn = {w - pad - impW, yBtn, w - pad, yBtn + btnH};
}

static void ImportPickSource(ImportDialog* d) {
    TempStr src = nullptr;
    if (!FcPickPdfFile(d->hwnd, src)) {
        return; // cancelled
    }
    WindowTab* tab = d->win->CurrentTab();
    if (!tab) {
        return;
    }
    ::free(d->srcPath);
    d->srcPath = str::Dup(src).s;
    FlashcardEnsureTabCards(tab);
    d->newCards = FlashcardImportDiffCount(tab->flashcard.cards, d->srcPath, &d->srcCards);
    logf("[fc] Import window - source picked: '%s' (%d cards, %d new)\n", Str(d->srcPath), d->srcCards, d->newCards);
    InvalidateRect(d->hwnd, nullptr, FALSE);
}

static void ImportDoImport(ImportDialog* d) {
    if (!d->srcPath || d->newCards <= 0) {
        logf("[fc] Import window - import ignored: no new cards (pick a source first)\n");
        return;
    }
    WindowTab* tab = d->win->CurrentTab();
    if (!tab) {
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine || !EngineSupportsAnnotations(engine)) {
        logf("[fc] Import window - ERROR: current tab is not an annotation-capable PDF\n");
        return;
    }
    int n = FlashcardImportFromPdf(AsEngineMupdf(engine), tab->flashcard.cards, d->srcPath);
    d->importedTotal += n;
    logf("[fc] Import window - created %d new card(s)\n", n);
    if (n > 0) {
        tab->flashcard.cards = FlashcardLoadFromDocument(AsEngineMupdf(engine));
        FlashcardToolbarUpdateCount(d->win);
        FlashcardToolbarUpdateState(d->win);
        FlashcardSidebarPopulate(d->win);
        MainWindowRerender(d->win);
    }
    // re-run the dry-run so the table stays truthful (usually 0 after success)
    d->newCards = FlashcardImportDiffCount(tab->flashcard.cards, d->srcPath, &d->srcCards);
    InvalidateRect(d->hwnd, nullptr, FALSE);
}

static void ImportPaint(ImportDialog* d) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->hwnd, &ps);
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    FlashcardFillBg(hdc, rc, ThemeWindowBackgroundColor());
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    WindowTab* tab = d->win->CurrentTab();
    TempStr curName = tab ? FlashcardTabLogName(tab) : StrL("?");
    int curCards = tab ? len(tab->flashcard.cards) : 0;

    SetTextColor(hdc, ThemeWindowTextColor());
    RECT rcCap = d->rcTable;
    rcCap.bottom = rcCap.top + DpiScale(d->hwnd, 22);
    DrawTextW(hdc, L"Compare flashcards with another copy of this document:", -1, &rcCap,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int colCardsW = DpiScale(d->hwnd, 64);
    int rowDy = DpiScale(d->hwnd, 26);
    int y = rcCap.bottom;
    // header row: | Document | Cards |
    SetTextColor(hdc, ThemeWindowDarkerTextColor());
    RECT rcHdr = {d->rcTable.left, y, d->rcTable.right - colCardsW - DpiScale(d->hwnd, 10), y + rowDy};
    DrawTextW(hdc, L"Document", -1, &rcHdr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT rcHdrC = {d->rcTable.right - colCardsW, y, d->rcTable.right, y + rowDy};
    DrawTextW(hdc, L"Cards", -1, &rcHdrC, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    y += rowDy;
    RECT rcSep = {d->rcTable.left, y, d->rcTable.right, y + DpiScale(d->hwnd, 1)};
    FlashcardFillBg(hdc, rcSep, ThemeEdgeColor());
    y += DpiScale(d->hwnd, 5);
    SetTextColor(hdc, ThemeWindowTextColor());
    // row 1: current document
    RECT rcN1 = {d->rcTable.left, y, d->rcTable.right - colCardsW - DpiScale(d->hwnd, 10), y + rowDy};
    TempStr row1 = fmt("%s   (current)", Str(curName));
    DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(row1))), -1, &rcN1,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT rcC1 = {d->rcTable.right - colCardsW, y, d->rcTable.right, y + rowDy};
    TempStr c1 = fmt("%d", curCards);
    DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(c1))), -1, &rcC1, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    y += rowDy;
    // row 2: picked source (or the empty-state hint)
    if (d->srcPath) {
        TempStr srcBase = path::GetBaseNameTemp(Str(d->srcPath));
        RECT rcN2 = {d->rcTable.left, y, d->rcTable.right - colCardsW - DpiScale(d->hwnd, 10), y + rowDy};
        DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(srcBase))), -1, &rcN2,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT rcC2 = {d->rcTable.right - colCardsW, y, d->rcTable.right, y + rowDy};
        TempStr c2 = fmt("%d", d->srcCards);
        DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(c2))), -1, &rcC2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    } else {
        RECT rcN2 = {d->rcTable.left, y, d->rcTable.right, y + rowDy};
        SetTextColor(hdc, ThemeWindowDarkerTextColor());
        DrawTextW(hdc, L"— pick a source PDF below —", -1, &rcN2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    // "New cards: N" summary (+ total imported during this dialog)
    SetTextColor(hdc, ThemeWindowTextColor());
    TempStr nl;
    if (d->srcPath && d->newCards >= 0) {
        nl = fmt("New cards: %d", d->newCards);
        if (d->importedTotal > 0) {
            nl = fmt("New cards: %d   (imported %d so far)", d->newCards, d->importedTotal);
        }
    } else {
        nl = fmt("New cards: —");
    }
    DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(nl))), -1, &d->rcNewLine, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    FlashcardDrawButton(hdc, d->rcPick, L"Select PDF...", false);
    FlashcardDrawButton(hdc, d->rcImportBtn, L"Import new cards to this document", false);
    SelectObject(hdc, oldFont);
    EndPaint(d->hwnd, &ps);
}

static LRESULT CALLBACK ImportWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ImportDialog* d = gImportDialog;
    if (msg == WM_CREATE) {
        gImportDialog->hwnd = hwnd;
        return 0;
    }
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_SIZE:
            ImportLayout(d);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            ImportPaint(d);
            return 0;
        case WM_LBUTTONDOWN: {
            POINT pt{(short)LOWORD(lp), (short)HIWORD(lp)};
            if (PtInRect(&d->rcPick, pt)) {
                ImportPickSource(d);
            } else if (PtInRect(&d->rcImportBtn, pt)) {
                ImportDoImport(d);
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void FlashcardImportDialog(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        logf("[fc] Import window - ERROR: no current tab\n");
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine || !EngineSupportsAnnotations(engine)) {
        logf("[fc] Import window - ERROR: current tab is not an annotation-capable PDF\n");
        return;
    }
    FlashcardEnsureTabCards(tab);
    FlashcardRegisterDialogClass(L"TUMATRA_FLASHCARD_IMPORT", ImportWndProc);
    HWND hwndParent = win->hwndFrame;
    ImportDialog dlg; // no {}: explicit-ctor members
    dlg.win = win;
    gImportDialog = &dlg;
    HWND hwnd = FlashcardCreateDialogWindow(L"TUMATRA_FLASHCARD_IMPORT", L"Import Flashcards", 560, 360, hwndParent);
    if (!hwnd) {
        gImportDialog = nullptr;
        return;
    }
    logf("[fc] Import window - opened\n");
    ImportLayout(&dlg);
    FlashcardRunModalDialog(hwnd, hwndParent, nullptr);
    ::free(dlg.srcPath);
    gImportDialog = nullptr;
}

// ---- Session window --------------------------------------------------------
// Toolbar "Session" button (s30-D): search field + multi-select list of every
// book in the study history — | PDF | cards | due | last reviewed | tags |
// (tags column is a placeholder until a tags data model exists). "Open
// Session" launches a NEW app window with all checked books (their saved
// docPaths); books missing on disk are skipped + logged. Combined with the
// global-session cross-doc study mode this becomes a study session over
// several books. Every step is [fc]-logged.

struct SessionDialog {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    HWND hwndSearch = nullptr;
    HWND hwndList = nullptr;
    Vec<FlashcardStudyDocInfo> docs; // every book in the study dir
    Vec<bool> checked;               // parallel to docs
    Vec<int> displayIdx;             // listbox row -> docs index (search filter)
    RECT rcOpen{};
};

static SessionDialog* gSessionDialog = nullptr;
static WNDPROC gSessionSearchOrigProc = nullptr;

// refill the listbox with the rows matching the search text (case-insensitive
// substring on the book name; empty text = all rows)
static void SessionApplyFilter(SessionDialog* d) {
    d->displayIdx.Reset();
    Str filter;
    if (d->hwndSearch) {
        filter = FcGetEditText(d->hwndSearch);
    }
    for (int i = 0; i < len(d->docs); i++) {
        const FlashcardStudyDocInfo& info = d->docs[i];
        Str name(info.docName && len(info.docName) > 0 ? info.docName : info.fileName);
        if (len(filter) == 0 || str::ContainsI(name, filter)) {
            d->displayIdx.Append(i);
        }
    }
    if (d->hwndList) {
        SendMessageW(d->hwndList, LB_RESETCONTENT, 0, 0);
        for (int k = 0; k < len(d->displayIdx); k++) {
            int idx = d->displayIdx[k];
            Str name(d->docs[idx].docName && len(d->docs[idx].docName) > 0 ? d->docs[idx].docName
                                                                           : d->docs[idx].fileName);
            SendMessageW(d->hwndList, LB_ADDSTRING, 0, (LPARAM)CWStrTemp(ToWStrTemp(name)));
            SendMessageW(d->hwndList, LB_SETITEMDATA, k, (LPARAM)idx);
        }
        SendMessageW(d->hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
    }
    logf("[fc] Session - list rebuilt: %d row(s) match\n", len(d->displayIdx));
}

static void SessionReloadList(SessionDialog* d) {
    ConfigFreeDocs(d->docs);
    d->checked.Reset();
    FlashcardStudyListDocs(d->docs);
    for (int i = 0; i < len(d->docs); i++) {
        d->checked.Append(false);
    }
    SessionApplyFilter(d);
}

static void SessionLayout(SessionDialog* d) {
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int pad = DpiScale(d->hwnd, 14);
    int capDy = DpiScale(d->hwnd, 22);
    int editH = DpiScale(d->hwnd, 26);
    int btnH = DpiScale(d->hwnd, 44);
    int y = pad + capDy + DpiScale(d->hwnd, 4);
    if (d->hwndSearch) {
        MoveWindow(d->hwndSearch, pad, y, w - 2 * pad, editH, TRUE);
    }
    y += editH + DpiScale(d->hwnd, 10);
    int yBtn = h - pad - btnH;
    if (d->hwndList) {
        MoveWindow(d->hwndList, pad, y, w - 2 * pad, yBtn - DpiScale(d->hwnd, 10) - y, TRUE);
    }
    int openW = DpiScale(d->hwnd, 200);
    int x0 = (w - openW) / 2;
    if (x0 < 0) {
        x0 = 0;
    }
    d->rcOpen = {x0, yBtn, x0 + openW, yBtn + btnH};
}

// one row: [checkbox] (dot) name | cards | due | last reviewed | tags
static void SessionDrawListItem(SessionDialog* d, const DRAWITEMSTRUCT* dis) {
    int idx = (int)dis->itemData;
    if (idx < 0 || idx >= len(d->docs)) {
        return;
    }
    const FlashcardStudyDocInfo& info = d->docs[idx];
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    FlashcardFillBg(hdc, rc, ThemeWindowBackgroundColor());
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ThemeWindowTextColor());
    int pad = DpiScale(d->hwnd, 6);
    int gap = DpiScale(d->hwnd, 10);
    // checkbox (blue = checked, Filter-window visual language)
    int boxD = DpiScale(d->hwnd, 13);
    RECT rcBox = {rc.left + pad, (rc.top + rc.bottom - boxD) / 2, rc.left + pad + boxD,
                  (rc.top + rc.bottom - boxD) / 2 + boxD};
    FlashcardFillBg(hdc, rcBox, d->checked[idx] ? RGB(66, 133, 244) : ThemeWindowControlBackgroundColor());
    // sync dot (green/red/grey, same as the Settings window)
    int dotD = DpiScale(d->hwnd, 8);
    int dotX = rcBox.right + pad;
    {
        int dotY = (rc.top + rc.bottom - dotD) / 2;
        COLORREF col = RGB(158, 158, 158);
        if (info.syncStatus == 1) {
            col = RGB(67, 160, 71);
        } else if (info.syncStatus == 2) {
            col = RGB(229, 57, 53);
        }
        HBRUSH br = CreateSolidBrush(col);
        HGDIOBJ oldBr = SelectObject(hdc, br);
        HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, dotX, dotY, dotX + dotD, dotY + dotD);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(br);
    }
    // columns: tags | last reviewed | due | cards (right-aligned), name left
    int cTagsW = DpiScale(d->hwnd, 56);
    int cLastW = DpiScale(d->hwnd, 110);
    int cNumW = DpiScale(d->hwnd, 56);
    auto drawRight = [&](int xRight, TempStr txt) {
        RECT rcT = {xRight - cNumW, rc.top, xRight, rc.bottom};
        DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(txt))), -1, &rcT, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    };
    int xRightBase = rc.right - pad - cTagsW;
    // tags column (placeholder — no data model yet)
    {
        RECT rcT = {xRightBase, rc.top, rc.right - pad, rc.bottom};
        SetTextColor(hdc, ThemeWindowDarkerTextColor());
        DrawTextW(hdc, L"—", -1, &rcT, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(hdc, ThemeWindowTextColor());
    }
    RECT rcDate = {xRightBase - gap - cLastW, rc.top, xRightBase - gap, rc.bottom};
    DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(ConfigFormatDateTemp(info.lastReviewedAt)))), -1, &rcDate,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    drawRight(xRightBase - gap - cLastW - gap - cNumW, fmt("%d", info.dueCount));
    drawRight(xRightBase - gap - cLastW - 2 * (gap + cNumW), fmt("%d", info.totalCards));
    RECT rcName = rc;
    rcName.left = dotX + dotD + pad;
    rcName.right = xRightBase - gap - cLastW - 2 * (gap + cNumW) - pad;
    Str name(info.docName && len(info.docName) > 0 ? info.docName : info.fileName);
    DrawTextW(hdc, CWStrTemp(ToWStrTemp(name)), -1, &rcName,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);
}

static void SessionToggleItem(SessionDialog* d, int row) {
    int idx = (int)SendMessageW(d->hwndList, LB_GETITEMDATA, row, 0);
    if (idx < 0 || idx >= len(d->docs)) {
        return;
    }
    d->checked[idx] = !d->checked[idx];
    int nChecked = 0;
    for (int i = 0; i < len(d->checked); i++) {
        if (d->checked[i]) {
            nChecked++;
        }
    }
    logf("[fc] Session - %s book %d ('%s'), %d checked\n", d->checked[idx] ? StrL("checked") : StrL("unchecked"), idx,
         Str(d->docs[idx].docName ? d->docs[idx].docName : "?"), nChecked);
    // clear the selection so clicking the SAME row again still fires
    SendMessageW(d->hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
    InvalidateRect(d->hwnd, nullptr, FALSE);
}

// launch a NEW app window with every CHECKED book that still exists on disk
static void SessionOpenSelected(SessionDialog* d) {
    int n = 0;
    int skipped = 0;
    str::Builder cmd;
    for (int i = 0; i < len(d->docs); i++) {
        if (!d->checked[i]) {
            continue;
        }
        FlashcardStudyDocInfo& info = d->docs[i];
        if (!info.docPath || !file::Exists(Str(info.docPath))) {
            skipped++;
            logf("[fc] Session - skipped '%s': PDF missing at '%s'\n",
                 Str(info.docName ? info.docName : "?"), Str(info.docPath ? info.docPath : "(none)"));
            continue;
        }
        cmd.AppendChar('"');
        cmd.Append(Str(info.docPath));
        cmd.AppendChar('"');
        cmd.AppendChar(' ');
        n++;
    }
    if (n == 0) {
        logf("[fc] Session - nothing to open (%d checked book(s) missing on disk)\n", skipped);
        return;
    }
    WCHAR exeW[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exeW, dimof(exeW));
    str::Builder full;
    full.AppendChar('"');
    full.Append(Str(ToUtf8Temp(WStr(exeW))));
    full.AppendChar('"');
    full.AppendChar(' ');
    Str cmdTail = cmd.TakeStr();
    full.Append(cmdTail);
    Str cmdline = full.TakeStr();
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    WCHAR* cmdW = CWStrTemp(ToWStrTemp(cmdline));
    BOOL ok = CreateProcessW(nullptr, cmdW, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    logf("[fc] Session - launched new window with %d book(s) (skipped %d missing) ok=%d\n", n, skipped,
         ok ? 1 : 0);
}

static void SessionPaint(SessionDialog* d) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->hwnd, &ps);
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    FlashcardFillBg(hdc, rc, ThemeWindowBackgroundColor());
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, ThemeWindowDarkerTextColor());
    int pad = DpiScale(d->hwnd, 14);
    int capDy = DpiScale(d->hwnd, 22);
    RECT rcCap = {pad, pad, rc.right - pad, pad + capDy};
    DrawTextW(hdc, L"Search your studied books:", -1, &rcCap, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    FlashcardDrawButton(hdc, d->rcOpen, L"Open Session", false);
    SelectObject(hdc, oldFont);
    EndPaint(d->hwnd, &ps);
}

static LRESULT CALLBACK SessionSearchProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        SendMessageW(GetParent(hwnd), WM_CLOSE, 0, 0);
        return 0;
    }
    return CallWindowProc(gSessionSearchOrigProc, hwnd, msg, wp, lp);
}

static LRESULT CALLBACK SessionWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SessionDialog* d = gSessionDialog;
    if (msg == WM_CREATE) {
        gSessionDialog->hwnd = hwnd;
        return 0;
    }
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_SIZE:
            SessionLayout(d);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            SessionPaint(d);
            return 0;
        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lp;
            if (mis->CtlID == IDC_FC_SESSION_LIST) {
                mis->itemHeight = DpiScale(hwnd, 24);
                return TRUE;
            }
            break;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
            if (dis->CtlID == IDC_FC_SESSION_LIST) {
                SessionDrawListItem(d, dis);
                return TRUE;
            }
            break;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            // theme the search EDIT / book LISTBOX interiors (dark mode)
            HDC hdc = (HDC)wp;
            SetBkColor(hdc, ThemeWindowBackgroundColor());
            SetTextColor(hdc, ThemeWindowTextColor());
            return (LRESULT)FlashcardCtlColorBrush();
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            int code = HIWORD(wp);
            if (id == IDC_FC_SESSION_SEARCH && code == EN_CHANGE) {
                SessionApplyFilter(d);
                return 0;
            }
            if (id == IDC_FC_SESSION_LIST && code == LBN_SELCHANGE) {
                int sel = (int)SendMessageW(d->hwndList, LB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    SessionToggleItem(d, sel);
                }
                return 0;
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            POINT pt{(short)LOWORD(lp), (short)HIWORD(lp)};
            if (PtInRect(&d->rcOpen, pt)) {
                SessionOpenSelected(d);
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void FlashcardSessionDialog(MainWindow* win) {
    FlashcardRegisterDialogClass(L"TUMATRA_FLASHCARD_SESSION", SessionWndProc);
    HWND hwndParent = win->hwndFrame;
    SessionDialog dlg; // no {}: Vec's default ctor is explicit
    dlg.win = win;
    gSessionDialog = &dlg;
    HWND hwnd = FlashcardCreateDialogWindow(L"TUMATRA_FLASHCARD_SESSION", L"Study Session", 640, 520, hwndParent);
    if (!hwnd) {
        gSessionDialog = nullptr;
        return;
    }
    logf("[fc] Session window - opened\n");

    // child controls: search EDIT + book LISTBOX
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    dlg.hwndSearch =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0,
                        0, 0, hwnd, (HMENU)IDC_FC_SESSION_SEARCH, hinst, nullptr);
    dlg.hwndList = CreateWindowExW(0, L"LISTBOX", nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_OWNERDRAWFIXED |
                                       LBS_NOINTEGRALHEIGHT | WS_TABSTOP,
                                   0, 0, 0, 0, hwnd, (HMENU)IDC_FC_SESSION_LIST, hinst, nullptr);
    SendMessageW(dlg.hwndSearch, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(dlg.hwndList, WM_SETFONT, (WPARAM)font, TRUE);

    // subclass the search edit: Esc closes (typing filters via EN_CHANGE)
    if (nullptr == gSessionSearchOrigProc) {
        gSessionSearchOrigProc = (WNDPROC)GetWindowLongPtr(dlg.hwndSearch, GWLP_WNDPROC);
    }
    SetWindowLongPtr(dlg.hwndSearch, GWLP_WNDPROC, (LONG_PTR)SessionSearchProc);

    SessionReloadList(&dlg);
    SessionLayout(&dlg);
    FlashcardRunModalDialog(hwnd, hwndParent, dlg.hwndSearch);
    ConfigFreeDocs(dlg.docs);
    gSessionDialog = nullptr;
}

// ---- Study Order options dialog -------------------------------------------
// Opens from the toolbar "Order" button. Two groups:
//   Review order:  [Sequential] [Random]
//   New cards:     [Before due] [After due] [Mixed]
// Every option click applies INSTANTLY (persists the setting, rebuilds the
// study order and restarts from its first card when a session is active).
// Close with the X or Esc — there is no OK because everything already applied.

struct OrderOptionsDialog {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    RECT rcSeq{}, rcRandom{};
    RECT rcNewFirst{}, rcNewLast{}, rcNewMixed{};
    RECT rcBtns[5]{}; // hit-test helpers in the same order as the constants below
};

static OrderOptionsDialog* gOrderOptionsDialog = nullptr;

enum {
    kOrderOptSeq = 0,
    kOrderOptRandom = 1,
    kOrderOptNewFirst = 2,
    kOrderOptNewLast = 3,
    kOrderOptNewMixed = 4,
};

static void OrderOptionsLayout(OrderOptionsDialog* d) {
    int pad = DpiScale(d->hwnd, 14);
    int btnW = DpiScale(d->hwnd, 110);
    int btnH = DpiScale(d->hwnd, 40);
    int gap = DpiScale(d->hwnd, 10);
    int capDy = DpiScale(d->hwnd, 24);
    int y = pad;
    // group 1 caption + buttons
    y += capDy;
    int x = pad;
    d->rcSeq = {x, y, x + btnW, y + btnH};
    x += btnW + gap;
    d->rcRandom = {x, y, x + btnW, y + btnH};
    y += btnH + pad;
    // group 2 caption + buttons
    y += capDy;
    x = pad;
    d->rcNewFirst = {x, y, x + btnW, y + btnH};
    x += btnW + gap;
    d->rcNewLast = {x, y, x + btnW, y + btnH};
    x += btnW + gap;
    d->rcNewMixed = {x, y, x + btnW, y + btnH};
    y += btnH + pad;
    d->rcBtns[kOrderOptSeq] = d->rcSeq;
    d->rcBtns[kOrderOptRandom] = d->rcRandom;
    d->rcBtns[kOrderOptNewFirst] = d->rcNewFirst;
    d->rcBtns[kOrderOptNewLast] = d->rcNewLast;
    d->rcBtns[kOrderOptNewMixed] = d->rcNewMixed;
}

static void OrderOptionsDrawOption(HWND hwnd, HDC hdc, const RECT& rc, const WCHAR* label, bool checked) {
    // flat button: border + radio dot + label; the checked one gets the
    // accent border and a filled dot. Theme colors so it works in dark mode.
    FlashcardFillBg(hdc, rc, checked ? ThemeHotBackgroundColor() : ThemeWindowControlBackgroundColor());
    HPEN pen = CreatePen(PS_SOLID, checked ? 2 : 1, checked ? RGB(66, 133, 244) : ThemeEdgeColor());
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    // radio dot at the left
    int cy = (rc.top + rc.bottom) / 2;
    int dotR = DpiScale(hwnd, 4); // dot radius, dpi-scaled
    RECT dot = {rc.left + dotR * 2, cy - dotR, rc.left + dotR * 2 + 2 * dotR, cy + dotR};
    HBRUSH dotBg = CreateSolidBrush(checked ? RGB(66, 133, 244) : ThemeWindowBackgroundColor());
    HBRUSH oldDotBg = (HBRUSH)SelectObject(hdc, dotBg);
    Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
    SelectObject(hdc, oldDotBg);
    DeleteObject(dotBg);
    if (!checked) {
        HPEN dotPen = CreatePen(PS_SOLID, 1, ThemeEdgeColor());
        HPEN oldDotPen = (HPEN)SelectObject(hdc, dotPen);
        Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
        SelectObject(hdc, oldDotPen);
        DeleteObject(dotPen);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ThemeWindowTextColor());
    RECT rcText = rc;
    rcText.left = dot.right + DpiScale(hwnd, 4);
    DrawTextW(hdc, label, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void OrderOptionsPaint(OrderOptionsDialog* d) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->hwnd, &ps);
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    FlashcardFillBg(hdc, rc, ThemeWindowBackgroundColor());
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ThemeWindowDarkerTextColor());
    int pad = DpiScale(d->hwnd, 14);
    RECT cap1 = {pad, pad, rc.right - pad, pad + DpiScale(d->hwnd, 24)};
    DrawTextW(hdc, L"Review order", -1, &cap1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT cap2 = {pad, d->rcNewFirst.top - DpiScale(d->hwnd, 24), rc.right - pad, d->rcNewFirst.top};
    DrawTextW(hdc, L"New cards", -1, &cap2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    bool isRandom = gGlobalPrefs->flashcardSettings.randomOrder;
    int pos = gGlobalPrefs->flashcardSettings.newCardsPosition;
    OrderOptionsDrawOption(d->hwnd, hdc, d->rcSeq, L"Sequential", !isRandom);
    OrderOptionsDrawOption(d->hwnd, hdc, d->rcRandom, L"Random", isRandom);
    OrderOptionsDrawOption(d->hwnd, hdc, d->rcNewFirst, L"Before due", pos == 0);
    OrderOptionsDrawOption(d->hwnd, hdc, d->rcNewLast, L"After due", pos == 1);
    OrderOptionsDrawOption(d->hwnd, hdc, d->rcNewMixed, L"Mixed", pos == 2);
    EndPaint(d->hwnd, &ps);
}

static LRESULT CALLBACK OrderOptionsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    OrderOptionsDialog* d = gOrderOptionsDialog;
    if (msg == WM_CREATE) {
        gOrderOptionsDialog->hwnd = hwnd;
        return 0;
    }
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_SIZE:
            OrderOptionsLayout(d);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_ERASEBKGND:
            return 1; // painted in WM_PAINT
        case WM_PAINT:
            OrderOptionsPaint(d);
            return 0;
        case WM_LBUTTONDOWN: {
            POINT pt{(short)LOWORD(lp), (short)HIWORD(lp)};
            bool isRandom = gGlobalPrefs->flashcardSettings.randomOrder;
            int pos = gGlobalPrefs->flashcardSettings.newCardsPosition;
            int opt = -1;
            if (PtInRect(&d->rcSeq, pt)) {
                opt = kOrderOptSeq;
            } else if (PtInRect(&d->rcRandom, pt)) {
                opt = kOrderOptRandom;
            } else if (PtInRect(&d->rcNewFirst, pt)) {
                opt = kOrderOptNewFirst;
            } else if (PtInRect(&d->rcNewLast, pt)) {
                opt = kOrderOptNewLast;
            } else if (PtInRect(&d->rcNewMixed, pt)) {
                opt = kOrderOptNewMixed;
            }
            if (opt == kOrderOptSeq) {
                gGlobalPrefs->flashcardSettings.randomOrder = false;
            } else if (opt == kOrderOptRandom) {
                gGlobalPrefs->flashcardSettings.randomOrder = true;
            } else if (opt == kOrderOptNewFirst) {
                gGlobalPrefs->flashcardSettings.newCardsPosition = 0;
            } else if (opt == kOrderOptNewLast) {
                gGlobalPrefs->flashcardSettings.newCardsPosition = 1;
            } else if (opt == kOrderOptNewMixed) {
                gGlobalPrefs->flashcardSettings.newCardsPosition = 2;
            }
            if (opt >= 0) {
                logf("[fc] OrderOptions - order=%s newCardsPosition=%d (was order=%s pos=%d)\n",
                     gGlobalPrefs->flashcardSettings.randomOrder ? StrL("random") : StrL("sequential"),
                     gGlobalPrefs->flashcardSettings.newCardsPosition, isRandom ? StrL("random") : StrL("sequential"),
                     pos);
                // instant apply: persist + rebuild + restart from first card
                FlashcardApplyStudyOrder(d->win);
                InvalidateRect(hwnd, nullptr, TRUE);
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
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void FlashcardOrderOptionsDialog(MainWindow* win) {
    FlashcardRegisterDialogClass(L"TUMATRA_FLASHCARD_ORDER", OrderOptionsWndProc);
    HWND hwndParent = win->hwndFrame;
    OrderOptionsDialog dlg; // no {}: Vec's default ctor is explicit
    dlg.win = win;
    gOrderOptionsDialog = &dlg; // stack-local: safe, the modal pump below owns it
    HWND hwnd = FlashcardCreateDialogWindow(L"TUMATRA_FLASHCARD_ORDER", L"Study Order", 388, 218, hwndParent);
    if (!hwnd) {
        gOrderOptionsDialog = nullptr;
        return;
    }
    logf("[fc] OrderOptions - dialog opened\n");
    FlashcardRunModalDialog(hwnd, hwndParent);
    gOrderOptionsDialog = nullptr;
}

// ---- Study Filter dialog ---------------------------------------------------
// Opens from the toolbar "Filter" button. Layout (top to bottom):
//   [x] Study all open PDFs (session)   — cross-doc session toggle
//   Page filter:       [EDIT expression] — "1-15;20-25;-22-23;"
//   Chapters (bookmarks):                 — checkbox mirror of the
//   +------------------------------------+           TOC; checking a bookmark
//   | [ ] Chapter 1 (1-15)               |           injects its page range
//   | [ ] Chapter 2 (16-30)              |           into the expression
//   +------------------------------------+
//   [Apply] [Filters: ON/OFF] [Clear Filters]    — clear is a 2s hold
//
// Apply happens on: Apply button, Enter in the edit, every checkbox click,
// the ON/OFF toggle, the cross-doc toggle and the Clear hold. X / Esc closes
// and keeps the last APPLIED state (pending typed text is not applied).
// Each book keeps its own filter (per-tab); the cross-doc checkbox is the
// window-level session scope: current document vs all PDF tabs of THIS
// window (tabs dragged to another window no longer count).

enum {
    IDC_FC_FILTER_EDIT = 60001,
    IDC_FC_FILTER_LIST = 60002,
};

struct FilterBookmarkItem {
    TocItem* tocItem = nullptr; // borrowed: owned by tab->currToc
    int depth = 0;
    int pageFrom = 1;
    int pageTo = 1;
};

struct FilterOptionsDialog {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr; // the tab whose filter is being edited
    HWND hwndEdit = nullptr;
    HWND hwndList = nullptr;
    Vec<FilterBookmarkItem> bmItems;
    Vec<bool> bmChecked;
    RECT rcCrossDoc{};
    RECT rcEditCaption{}, rcListCaption{};
    RECT rcApply{}, rcToggle{}, rcClear{};
    int holdingClear = 0; // 0 none, 1 holding the Clear button
    ULONGLONG holdStartTick = 0;
    int holdMs = 2000;
    double progress = 1.0; // 1 = full drain line, drains to 0 during the hold
};

static FilterOptionsDialog* gFilterOptionsDialog = nullptr;
static WNDPROC gFilterEditOrigProc = nullptr;

// Flatten the TOC depth-first in document order: an item, then its whole
// subtree, then its next sibling. Items with no valid page are skipped but
// still traversed (their children may point to pages).
static void FilterFlattenTocItem(TocItem* item, int depth, Vec<TocItem*>& flat, Vec<int>& depths) {
    if (!item) {
        return;
    }
    if (item->pageNo >= 1) {
        flat.Append(item);
        depths.Append(depth);
    }
    FilterFlattenTocItem(item->child, depth + 1, flat, depths);
    FilterFlattenTocItem(item->next, depth, flat, depths);
}

// Build the bookmark mirror. A bookmark's range runs from its own page to the
// page BEFORE the next bookmark in flattened order (children included), so a
// parent's range naturally covers all its sub-levels; the last one extends
// to the end of the document.
static void FilterBuildBookmarkItems(FilterOptionsDialog* d) {
    d->bmItems.Reset();
    d->bmChecked.Reset();
    DisplayModel* dm = d->tab->AsFixed();
    if (!dm) {
        return;
    }
    int pageCount = dm->PageCount();
    TocTree* toc = d->tab->currToc;
    if (!toc || !toc->root) {
        logf("[fc] Filter - no TOC, bookmark mirror empty\n");
        return;
    }
    Vec<TocItem*> flat;
    Vec<int> depths;
    for (TocItem* child = toc->root->child; child; child = child->next) {
        FilterFlattenTocItem(child, 0, flat, depths);
    }
    for (int i = 0; i < len(flat); i++) {
        FilterBookmarkItem item;
        item.tocItem = flat[i];
        item.depth = depths[i];
        item.pageFrom = flat[i]->pageNo;
        item.pageTo = (i + 1 < len(flat)) ? flat[i + 1]->pageNo - 1 : pageCount;
        if (item.pageTo < item.pageFrom) {
            item.pageTo = item.pageFrom; // next bookmark starts on the same page
        }
        d->bmItems.Append(item);
        d->bmChecked.Append(false);
    }
    logf("[fc] Filter - bookmark mirror: %d items\n", len(d->bmItems));
}

static void FilterSetEditText(HWND hwndEdit, Str text) {
    SetWindowTextW(hwndEdit, CWStrTemp(ToWStrTemp(text)));
}

// Save the edit's expression as the tab's filter and rebuild the session
// queue (when a session is active). Called by Apply / Enter / checkbox.
static void FilterApplyText(FilterOptionsDialog* d) {
    Str text = FcGetEditText(d->hwndEdit);
    str::ReplaceWithCopy(&d->tab->flashcard.filterExpr, text);
    logf("[fc] Filter - applied expr '%s' (enabled=%d) on '%s'\n", d->tab->flashcard.filterExpr,
         d->tab->flashcard.filterEnabled ? 1 : 0, FlashcardTabLogName(d->tab));
    FlashcardApplyStudyOrder(d->win);
    FlashcardToolbarUpdateState(d->win);
}

// Toggle the Filters ON/OFF switch: disables the filter without discarding
// the expression (compare study with/without the page restriction)
static void FilterToggleEnabled(FilterOptionsDialog* d) {
    d->tab->flashcard.filterEnabled = !d->tab->flashcard.filterEnabled;
    logf("[fc] Filter - toggle -> %s (expr kept: '%s')\n", d->tab->flashcard.filterEnabled ? StrL("ON") : StrL("OFF"),
         d->tab->flashcard.filterExpr);
    FlashcardApplyStudyOrder(d->win);
    FlashcardToolbarUpdateState(d->win);
    InvalidateRect(d->hwnd, nullptr, TRUE);
}

// Toggle the cross-document session scope (window-level, not per-book):
// current document vs all PDF tabs of THIS window
static void FilterToggleCrossDoc(FilterOptionsDialog* d) {
    MainWindow* win = d->win;
    win->flashcard.crossDocSession = !win->flashcard.crossDocSession;
    logf("[fc] Filter - study scope -> %s\n", win->flashcard.crossDocSession
                                                  ? StrL("GLOBAL SESSION (all PDF tabs of this window)")
                                                  : StrL("current document"));
    FlashcardApplyStudyOrder(win);    // rebuild queue per new scope when studying
    FlashcardToolbarUpdateCount(win); // count label is scope-aware
    InvalidateRect(d->hwnd, nullptr, TRUE);
}

// Clear (2s hold confirmed): empty expression, re-enable, uncheck all
static void FilterClearAll(FilterOptionsDialog* d) {
    str::ReplaceWithCopy(&d->tab->flashcard.filterExpr, Str());
    d->tab->flashcard.filterEnabled = true;
    FilterSetEditText(d->hwndEdit, Str());
    for (int i = 0; i < len(d->bmChecked); i++) {
        d->bmChecked[i] = false;
    }
    InvalidateRect(d->hwndList, nullptr, FALSE);
    logf("[fc] Filter - CLEARED (hold 2s confirmed), re-enabled\n");
    FlashcardApplyStudyOrder(d->win);
    FlashcardToolbarUpdateState(d->win);
    InvalidateRect(d->hwnd, nullptr, TRUE);
}

// A bookmark checkbox was clicked: inject/remove its page-range token into
// the edit's expression, then apply instantly (checkbox = one-click filter)
static void FilterToggleBookmark(FilterOptionsDialog* d, int idx) {
    if (idx < 0 || idx >= len(d->bmItems)) {
        return;
    }
    FilterBookmarkItem& item = d->bmItems[idx];
    d->bmChecked[idx] = !d->bmChecked[idx];
    Str text = FcGetEditText(d->hwndEdit);
    Str token = fmt("%d-%d;", item.pageFrom, item.pageTo);
    Str newText;
    if (d->bmChecked[idx]) {
        if (str::IndexOf(text, token) < 0) {
            newText = fmt("%s%s", text, token);
        } else {
            newText = text;
        }
    } else {
        int pos = str::IndexOf(text, token);
        if (pos >= 0 && pos + len(token) <= len(text)) {
            newText = fmt("%s%s", Str(text.s, pos), Str(text.s + pos + len(token), len(text) - pos - len(token)));
        } else {
            newText = text;
        }
    }
    FilterSetEditText(d->hwndEdit, newText);
    logf("[fc] Filter - bookmark '%s' range %d-%d -> %s\n", item.tocItem ? item.tocItem->title : StrL("?"),
         item.pageFrom, item.pageTo, d->bmChecked[idx] ? StrL("checked (added)") : StrL("unchecked (removed)"));
    FilterApplyText(d);
    InvalidateRect(d->hwndList, nullptr, FALSE); // redraw the checkbox state
}

static void FilterOptionsLayout(FilterOptionsDialog* d) {
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int pad = DpiScale(d->hwnd, 14);
    int capDy = DpiScale(d->hwnd, 22);
    int editH = DpiScale(d->hwnd, 26);
    int btnH = DpiScale(d->hwnd, 44);
    int gap = DpiScale(d->hwnd, 10);

    int y = pad;
    d->rcCrossDoc = {pad, y, w - pad, y + DpiScale(d->hwnd, 28)};
    y = d->rcCrossDoc.bottom + DpiScale(d->hwnd, 8);
    d->rcEditCaption = {pad, y, w - pad, y + capDy};
    y += capDy + DpiScale(d->hwnd, 4);
    if (d->hwndEdit) {
        MoveWindow(d->hwndEdit, pad, y, w - 2 * pad, editH, TRUE);
    }
    y += editH + DpiScale(d->hwnd, 10);
    d->rcListCaption = {pad, y, w - pad, y + capDy};
    y += capDy + DpiScale(d->hwnd, 4);
    int yBtn = h - pad - btnH;
    if (d->hwndList) {
        MoveWindow(d->hwndList, pad, y, w - 2 * pad, yBtn - gap - y, TRUE);
    }
    // bottom button row: Apply | Filters ON/OFF | Clear Filters (hold 2s)
    int applyW = DpiScale(d->hwnd, 110);
    int toggleW = DpiScale(d->hwnd, 130);
    int clearW = DpiScale(d->hwnd, 170);
    int x = pad;
    d->rcApply = {x, yBtn, x + applyW, yBtn + btnH};
    x += applyW + gap;
    d->rcToggle = {x, yBtn, x + toggleW, yBtn + btnH};
    x += toggleW + gap;
    d->rcClear = {x, yBtn, x + clearW, yBtn + btnH};
}

static void FilterDrawCheckbox(HWND hwnd, HDC hdc, int x, int y, bool checked) {
    int side = DpiScale(hwnd, 14);
    RECT box = {x, y, x + side, y + side};
    // theme-aware: unchecked box in window bg, checked in the blue accent
    FlashcardFillBg(hdc, box, checked ? RGB(66, 133, 244) : ThemeWindowBackgroundColor());
    HPEN pen = CreatePen(PS_SOLID, checked ? 2 : 1, checked ? RGB(66, 133, 244) : ThemeEdgeColor());
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, box.left, box.top, box.right, box.bottom);
    if (checked) {
        // crude check mark: two lines inside the box (white = readable on
        // both the accent fill and in dark themes)
        SelectObject(hdc, GetStockObject(WHITE_PEN));
        MoveToEx(hdc, box.left + side / 4, box.top + side / 2, nullptr);
        LineTo(hdc, box.left + side * 2 / 5, box.bottom - side / 4);
        LineTo(hdc, box.right - side / 4, box.top + side / 4);
    }
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}

static void FilterOptionsPaint(FilterOptionsDialog* d) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->hwnd, &ps);
    RECT rc;
    GetClientRect(d->hwnd, &rc);
    FlashcardFillBg(hdc, rc, ThemeWindowBackgroundColor());
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    // cross-doc session checkbox row
    FilterDrawCheckbox(d->hwnd, hdc, d->rcCrossDoc.left + DpiScale(d->hwnd, 2), d->rcCrossDoc.top + 2,
                       d->win->flashcard.crossDocSession);
    SetTextColor(hdc, ThemeWindowTextColor());
    RECT rcCrossText = d->rcCrossDoc;
    rcCrossText.left += DpiScale(d->hwnd, 24);
    DrawTextW(hdc, L"Study all open PDFs (session)", -1, &rcCrossText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, ThemeWindowDarkerTextColor());
    DrawTextW(hdc, L"Page filter (e.g. 1-15;20-25;-22-23;)", -1, &d->rcEditCaption,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, L"Chapters (bookmarks) — checking injects the range into the filter", -1, &d->rcListCaption,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // bottom buttons
    bool filterOn = d->tab->flashcard.filterEnabled;
    FlashcardDrawButton(hdc, d->rcApply, L"Apply", false);
    TempStr toggleLabel = filterOn ? StrL("Filters: ON") : StrL("Filters: OFF");
    FlashcardDrawButton(hdc, d->rcToggle, (WCHAR*)CWStrTemp(ToWStrTemp(Str(toggleLabel))), false);
    FlashcardDrawHoldButton(d->hwnd, hdc, d->rcClear, L"Clear Filters", d->holdingClear == 1,
                            d->holdingClear == 1 ? d->progress : 1.0, RGB(229, 57, 53));

    SelectObject(hdc, oldFont);
    EndPaint(d->hwnd, &ps);
}

// Owner-draw one bookmark row: checkbox (indented by tree depth) + title +
// its page range
static void FilterDrawListItem(FilterOptionsDialog* d, const DRAWITEMSTRUCT* dis) {
    int idx = (int)dis->itemID;
    if (idx < 0 || idx >= len(d->bmItems)) {
        return;
    }
    const FilterBookmarkItem& item = d->bmItems[idx];
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    FlashcardFillBg(hdc, rc, ThemeWindowBackgroundColor());
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    int indent = DpiScale(d->hwnd, 16) * item.depth;
    int boxSide = DpiScale(d->hwnd, 14);
    FilterDrawCheckbox(d->hwnd, hdc, rc.left + DpiScale(d->hwnd, 6) + indent,
                       rc.top + (rc.bottom - rc.top - boxSide) / 2, d->bmChecked[idx]);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ThemeWindowTextColor());
    TempStr label = fmt("%s  (%d-%d)", item.tocItem ? item.tocItem->title : StrL("?"), item.pageFrom, item.pageTo);
    RECT rcText = rc;
    rcText.left += DpiScale(d->hwnd, 6) + indent + boxSide + DpiScale(d->hwnd, 8);
    rcText.right -= DpiScale(d->hwnd, 8);
    DrawTextW(hdc, CWStrTemp(ToWStrTemp(Str(label))), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

// Enter in the edit applies; Esc closes the dialog
static LRESULT CALLBACK FilterEditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        if (gFilterOptionsDialog) {
            FilterApplyText(gFilterOptionsDialog);
        }
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        SendMessageW(GetParent(hwnd), WM_CLOSE, 0, 0);
        return 0;
    }
    return CallWindowProc(gFilterEditOrigProc, hwnd, msg, wp, lp);
}

static LRESULT CALLBACK FilterOptionsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    FilterOptionsDialog* d = gFilterOptionsDialog;
    if (msg == WM_CREATE) {
        gFilterOptionsDialog->hwnd = hwnd;
        return 0;
    }
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_SIZE:
            FilterOptionsLayout(d);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_ERASEBKGND:
            return 1; // painted in WM_PAINT
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            // theme the expression EDIT / bookmark LISTBOX interiors (dark
            // mode support) — same answer the Settings window gives (s30-A)
            HDC hdc = (HDC)wp;
            SetBkColor(hdc, ThemeWindowBackgroundColor());
            SetTextColor(hdc, ThemeWindowTextColor());
            return (LRESULT)FlashcardCtlColorBrush();
        }
        case WM_PAINT:
            FilterOptionsPaint(d);
            return 0;
        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lp;
            if (mis->CtlID == IDC_FC_FILTER_LIST) {
                mis->itemHeight = DpiScale(hwnd, 24);
                return TRUE;
            }
            break;
        }
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lp;
            if (dis->CtlID == IDC_FC_FILTER_LIST) {
                FilterDrawListItem(d, dis);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            int code = HIWORD(wp);
            if (id == IDC_FC_FILTER_LIST && code == LBN_SELCHANGE) {
                int sel = (int)SendMessageW(d->hwndList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < len(d->bmItems)) {
                    FilterToggleBookmark(d, sel);
                }
                // clear the selection so clicking the SAME row again still
                // fires LBN_SELCHANGE (toggle semantics)
                SendMessageW(d->hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
                return 0;
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            POINT pt{(short)LOWORD(lp), (short)HIWORD(lp)};
            if (PtInRect(&d->rcCrossDoc, pt)) {
                FilterToggleCrossDoc(d);
            } else if (PtInRect(&d->rcApply, pt)) {
                FilterApplyText(d);
            } else if (PtInRect(&d->rcToggle, pt)) {
                FilterToggleEnabled(d);
            } else if (PtInRect(&d->rcClear, pt)) {
                // hold-to-confirm clear (2s, red draining line)
                d->holdingClear = 1;
                d->holdStartTick = GetTickCount64();
                d->progress = 1.0;
                SetCapture(hwnd);
                SetTimer(hwnd, 1, 15, nullptr);
                logf("[fc] Filter - clear hold start (2000ms)\n");
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_TIMER: {
            if (d->holdingClear == 0 || wp != 1) {
                return 0;
            }
            ULONGLONG elapsed = GetTickCount64() - d->holdStartTick;
            d->progress = 1.0 - (double)elapsed / (double)d->holdMs;
            if (elapsed >= (ULONGLONG)d->holdMs) {
                d->holdingClear = 0;
                logf("[fc] Filter - clear hold CONFIRMED\n");
                FilterClearAll(d);
            } else {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            if (d->holdingClear != 0) {
                d->holdingClear = 0;
                d->progress = 1.0;
                logf("[fc] Filter - clear hold released early\n");
            }
            KillTimer(hwnd, 1);
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                logf("[fc] Filter - closed (Esc)\n");
                DestroyWindow(hwnd);
            } else if (wp == VK_RETURN) {
                FilterApplyText(d);
            }
            return 0;
        case WM_CLOSE:
            logf("[fc] Filter - closed (X)\n");
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void FlashcardFilterOptionsDialog(MainWindow* win) {
    FlashcardRegisterDialogClass(L"TUMATRA_FLASHCARD_FILTER", FilterOptionsWndProc);

    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->AsFixed()) {
        logf("[fc] Filter - ERROR: no PDF document loaded\n");
        return;
    }
    HWND hwndParent = win->hwndFrame;
    FilterOptionsDialog dlg; // no {}: Vec's default ctor is explicit
    dlg.win = win;
    dlg.tab = tab;
    gFilterOptionsDialog = &dlg; // stack-local: safe, the modal pump below owns it

    HWND hwnd = FlashcardCreateDialogWindow(L"TUMATRA_FLASHCARD_FILTER", L"Study Filter", 560, 500, hwndParent);
    if (!hwnd) {
        gFilterOptionsDialog = nullptr;
        return;
    }
    logf("[fc] Filter - dialog opened\n");

    // child controls: expression EDIT + bookmark LISTBOX
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    dlg.hwndEdit =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0,
                        0, 0, hwnd, (HMENU)IDC_FC_FILTER_EDIT, hinst, nullptr);
    dlg.hwndList = CreateWindowExW(0, L"LISTBOX", nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_OWNERDRAWFIXED |
                                       LBS_NOINTEGRALHEIGHT | WS_TABSTOP,
                                   0, 0, 0, 0, hwnd, (HMENU)IDC_FC_FILTER_LIST, hinst, nullptr);
    SendMessageW(dlg.hwndEdit, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(dlg.hwndList, WM_SETFONT, (WPARAM)font, TRUE);

    FilterBuildBookmarkItems(&dlg);
    for (int i = 0; i < len(dlg.bmItems); i++) {
        SendMessageW(dlg.hwndList, LB_ADDSTRING, 0, (LPARAM)L"");
    }
    // preset the expression with the tab's current filter
    FilterSetEditText(dlg.hwndEdit, tab->flashcard.filterExpr);

    // subclass the edit: Enter = apply, Esc = close
    if (nullptr == gFilterEditOrigProc) {
        gFilterEditOrigProc = (WNDPROC)GetWindowLongPtr(dlg.hwndEdit, GWLP_WNDPROC);
    }
    SetWindowLongPtr(dlg.hwndEdit, GWLP_WNDPROC, (LONG_PTR)FilterEditProc);

    FilterOptionsLayout(&dlg);
    InvalidateRect(hwnd, nullptr, TRUE);
    FlashcardRunModalDialog(hwnd, hwndParent, dlg.hwndEdit);
    gFilterOptionsDialog = nullptr;
}