/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "MainWindow.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "ArchScaleDialog.h"
#include "SumatraLog.h"
#include "Theme.h"

// Conversion table: meters per unit (index matches ArchUnit enum: 0=mm,1=cm,2=m,3=in,4=ft)
static const float kMeterPerUnit[5] = {0.001f, 0.01f, 1.0f, 0.0254f, 0.3048f};

static ArchScaleDialogWnd* gArchScaleDialog = nullptr;

static void SafeDeleteArchScaleDialog() {
    if (!gArchScaleDialog) {
        return;
    }
    auto* tmp = gArchScaleDialog;
    gArchScaleDialog = nullptr;
    delete tmp;
}

void ArchScaleDialogWnd::ScheduleDelete() {
    if (gArchScaleDialog != this) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteArchScaleDialog);
    uitask::Post(fn, "SafeDeleteArchScaleDialog");
}

static void PositionDialog(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = HwndWindowRect(hwndRelative);
    Rect r = HwndWindowRect(hwnd);
    Rect work = GetWorkAreaRect(rRelative, hwndRelative);

    int gap = DpiScale(hwnd, 8);
    int spaceLeft = rRelative.x - work.x;
    int spaceRight = work.Right() - rRelative.Right();
    bool fitsLeft = spaceLeft >= r.dx + gap;
    bool fitsRight = spaceRight >= r.dx + gap;

    int x;
    if (fitsRight && (!fitsLeft || spaceRight >= spaceLeft)) {
        x = rRelative.Right() + gap;
    } else if (fitsLeft) {
        x = rRelative.x - gap - r.dx;
    } else {
        x = work.Right() - r.dx;
    }
    int y = rRelative.y + ((rRelative.dy - r.dy) / 2);

    r = {x, y, r.dx, r.dy};
    Rect r2 = ShiftRectToWorkArea(r, hwndRelative, true);
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void ArchScaleDialogWnd::SyncUnitFromWin() {
    if (comboUnit) {
        SendMessageW(comboUnit->hwnd, CB_SETCURSEL, win->archTools.unit, 0);
    }
}

void ArchScaleDialogWnd::SetLength(float realLen) {
    if (editLen) {
        TempStr txt = str::FormatTemp("%.4g", realLen);
        editLen->SetText(txt);
    }
}

void ArchScaleDialogWnd::OnDrawLine() {
    if (!win) return;
    // Clear any existing scale line so user can draw a new one
    win->archTools.scaleLineDefined = false;
    win->archTools.dragLine = 0;
    win->archTools.point1 = Point{0, 0};
    win->archTools.point2 = Point{0, 0};
    win->archTools.toolMode = 1; // scale mode
    // Do NOT SetCapture or set mouseAction=Dragging here.
    // Capture is acquired on first canvas click in OnMouseLeftButtonDown.
    LogInfo("[arch] OnDrawLine: archToolMode=%d archDragLine=%d", win->archTools.toolMode, win->archTools.dragLine);
    ScheduleRepaint(win, 0);
}

void ArchScaleDialogWnd::OnOk() {
    if (!win) return;

    // Require a valid scale line: either just drawn (archScaleLineDefined)
    // or an existing scale whose endpoints are still stored (archScaleSet)
    if (!(win->archTools.scaleLineDefined || win->archTools.scaleSet)) {
        MessageBeep(MB_ICONWARNING);
        LogInfo("[arch] OnOk: no scale line defined");
        return;
    }

    // Get real length from edit
    TempStr txt = HwndGetTextTemp(editLen->hwnd);
    if (!txt) {
        MessageBeep(MB_ICONERROR);
        LogInfo("[arch] OnOk: empty length edit");
        return;
    }

    // Normalize decimal separator: replace comma with dot for parsing
    char txtBuf[64];
    int toCopy = std::min(63, txt.len);
    memcpy(txtBuf, txt.s, toCopy);
    txtBuf[toCopy] = '\0';
    for (char* p = txtBuf; *p; ++p) {
        if (*p == ',') *p = '.';
    }

    float realLen = 0.0f;
    Str end = str::Parse(txtBuf, "%f", &realLen);
    if (str::IsNull(end) || realLen <= 0.0f) {
        MessageBeep(MB_ICONERROR);
        LogInfo("[arch] OnOk: invalid length");
        return;
    }

    // Get selected unit from combo
    int selUnit = 0;
    if (comboUnit) {
        selUnit = (int)SendMessageW(comboUnit->hwnd, CB_GETCURSEL, 0, 0);
        if (selUnit < 0) selUnit = 0;
        if (selUnit > 4) selUnit = 4;
    }

    // Compute page length from the two endpoints (in page coords)
    float dx = win->archTools.scaleLineP2x - win->archTools.scaleLineP1x;
    float dy = win->archTools.scaleLineP2y - win->archTools.scaleLineP1y;
    float pageLenPt = sqrtf(dx * dx + dy * dy);

    if (pageLenPt <= 0.0f) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    // Convert realLen from selected unit to meters (canonical)
    float realLenM = realLen * kMeterPerUnit[selUnit];

    // Scale factor = page units per METER (canonical)
    win->archTools.scaleFactor = pageLenPt / realLenM;
    win->archTools.scaleSet = true;
    win->archTools.scaleAnchorX = win->archTools.scaleLineP1x;
    win->archTools.scaleAnchorY = win->archTools.scaleLineP1y;

    // Persist the selected unit as global preference
    gGlobalPrefs->archUnit = selUnit;

    // Exit scale mode
    win->archTools.toolMode = 0;
    win->archTools.dragLine = 0;
    // Restore normal cursor
    if (win->hwndCanvas) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }

    // Close dialog
    OnClose();
}

void ArchScaleDialogWnd::OnClose() {
    if (!win) return;
    LogInfo("[arch] ArchScaleDialog close: archToolMode=%d", win->archTools.toolMode);
    win->archTools.hwndArchScaleDialog = nullptr;
    win->archTools.scaleDialog = nullptr;
    if (win->archTools.toolMode == 1) {
        win->archTools.toolMode = 0;
    }
    // Restore normal cursor
    if (win->hwndCanvas) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    // Clear modeless dialog registration
    if (GetCurrentModelessDialog() == hwnd) {
        SetCurrentModelessDialog(nullptr);
    }
    ScheduleDelete();
}

LRESULT ArchScaleDialogWnd::OnMessageReflect(UINT msg, WPARAM wparam, LPARAM /*lparam*/) {
    if (msg == WM_CTLCOLORDLG || msg == WM_CTLCOLORSTATIC) {
        HDC hdc = (HDC)wparam;
        COLORREF bgColor = ThemeWindowControlBackgroundColor();
        COLORREF textColor = ThemeWindowTextColor();
        SetBkColor(hdc, bgColor);
        SetTextColor(hdc, textColor);
        return (LRESULT)BackgroundBrush();
    }
    return 0;
}

LRESULT ArchScaleDialogWnd::WndProc(HWND hwndIn, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ACTIVATE) {
        if (LOWORD(wp) == WA_INACTIVE) {
            if (GetCurrentModelessDialog() == hwndIn) {
                SetCurrentModelessDialog(nullptr);
            }
        } else {
            SetCurrentModelessDialog(hwndIn);
        }
    } else if (msg == WM_DESTROY) {
        if (GetCurrentModelessDialog() == hwndIn) {
            SetCurrentModelessDialog(nullptr);
        }
    }
    return WndProcDefault(hwndIn, msg, wp, lp);
}

static void OnClose(Wnd::CloseEvent* /*ev*/) {
    if (gArchScaleDialog) {
        gArchScaleDialog->OnClose();
    }
}

static void OnDestroy(Wnd::DestroyEvent* /*ev*/) {
    if (gArchScaleDialog) {
        gArchScaleDialog->ScheduleDelete();
    }
}

bool ArchScaleDialogWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    win->archTools.hwndArchScaleDialog = hwnd;

    {
        CreateCustomArgs args;
        args.title = _TRA("Definir Escala");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = font;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        // own the dialog to the main window so it stays on top while the canvas
        // captures the mouse for the trim-line drag (unowned popup would drop
        // behind the frame)
        args.parent = win->hwndFrame;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    bool isRtl = IsUIRtl();

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    // Row 1: "Comprimento real:" label + numeric edit
    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        Static::CreateArgs sargs;
        sargs.parent = hwnd;
        sargs.font = font;
        sargs.text = _TRA("Comprimento real:");
        sargs.isRtl = isRtl;
        auto* lbl = new Static();
        lbl->Create(sargs);
        hbox->AddChild(new Padding(lbl, Insets{0, 0, 0, 8}));

        Edit::CreateArgs eargs;
        eargs.parent = hwnd;
        eargs.font = font;
        eargs.isMultiLine = false;
        eargs.withBorder = true;
        eargs.idealWidthChars = 8;
        eargs.isRtl = isRtl;
        editLen = new Edit();
        editLen->Create(eargs);
        hbox->AddChild(new Padding(editLen, pad));
        vbox->AddChild(hbox);
    }

    // Row 2: "Unidade:" label + combobox
    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        Static::CreateArgs sargs;
        sargs.parent = hwnd;
        sargs.font = font;
        sargs.text = _TRA("Unidade:");
        sargs.isRtl = isRtl;
        auto* lbl = new Static();
        lbl->Create(sargs);
        hbox->AddChild(new Padding(lbl, Insets{0, 0, 0, 8}));

        DropDown::CreateArgs darg;
        darg.parent = hwnd;
        darg.font = font;
        darg.isRtl = isRtl;
        comboUnit = new DropDown();
        comboUnit->Create(darg);

        const WCHAR* unitNames[] = {L"mm", L"cm", L"m", L"in", L"ft"};
        for (int i = 0; i < 5; i++) {
            SendMessageW(comboUnit->hwnd, CB_ADDSTRING, 0, (LPARAM)unitNames[i]);
        }
        SendMessageW(comboUnit->hwnd, CB_SETCURSEL, win->archTools.unit, 0);

        hbox->AddChild(new Padding(comboUnit, pad));
        vbox->AddChild(hbox);
    }

    // Row 3: hint static
    {
        Static::CreateArgs sargs;
        sargs.parent = hwnd;
        sargs.font = font;
        sargs.text = _TRA("Desenhe uma linha no desenho");
        sargs.isRtl = isRtl;
        hint = new Static();
        hint->Create(sargs);
        vbox->AddChild(new Padding(hint, Insets{4, 8, 4, 8}));
    }

    // Row 4: [Desenhar linha] [✅] buttons (right-aligned)
    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        btnDraw = CreateButton(hwnd, _TRA("Desenhar linha"),
            MkMethod0<ArchScaleDialogWnd, &ArchScaleDialogWnd::OnDrawLine>(this), isRtl);
        hbox->AddChild(new Padding(btnDraw, pad));

        btnOk = CreateButton(hwnd, _TRA("✅"),
            MkMethod0<ArchScaleDialogWnd, &ArchScaleDialogWnd::OnOk>(this), isRtl);
        btnOk->isDefault = true;
        hbox->AddChild(new Padding(btnOk, pad));

        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    layout = padding;

    int dx = DpiScale(hwnd, 280);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    PositionDialog(hwnd, win->hwndFrame);

    // Set combo selection from global pref (persisted unit)
    if (comboUnit) {
        SendMessageW(comboUnit->hwnd, CB_SETCURSEL, gGlobalPrefs->archUnit, 0);
    }

    // Pre-fill edit with current length if a scale line already exists
    if (win->archTools.scaleSet && win->archTools.scaleFactor > 0.0f) {
        float sdx = win->archTools.scaleLineP2x - win->archTools.scaleLineP1x;
        float sdy = win->archTools.scaleLineP2y - win->archTools.scaleLineP1y;
        float pageLenPt = sqrtf(sdx * sdx + sdy * sdy);
        // archScaleFactor is now page units per METER (canonical)
        // realLen in meters = pageLenPt / archScaleFactor
        // Convert to current display unit for the edit box
        float realLenM = pageLenPt / win->archTools.scaleFactor;
        float realLenDisp = realLenM / kMeterPerUnit[gGlobalPrefs->archUnit];
        SetLength(realLenDisp);
    }

    SetIsVisible(true);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    HwndSetFocus(hwnd);
    HwndRepaintNow(win->hwndCanvas);

    // Register as modeless dialog for IsDialogMessage (enables Enter -> default button)
    isDialog = true;
    SetCurrentModelessDialog(hwnd);

    // Apply dark mode to popup window (title bar, controls)
    ApplyDarkModeToPopupWindow(hwnd);

    return true;
}

void ShowArchScaleDialog(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) {
        return;
    }
    if (gArchScaleDialog) {
        HwndSetFocus(gArchScaleDialog->hwnd);
        return;
    }
    LogInfo("[arch] ShowArchScaleDialog: archToolMode=%d", win->archTools.toolMode);
    auto* wnd = new ArchScaleDialogWnd();
    wnd->onClose = MkFunc1Void<Wnd::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroy);
    wnd->font = GetAppFont(win->hwndFrame);
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gArchScaleDialog = wnd;
    win->archTools.scaleDialog = wnd;
    // Set crosshair cursor when Scale dialog opens (drawing mode armed)
    if (win->hwndCanvas) {
        SetCursor(LoadCursor(nullptr, IDC_CROSS));
    }
}