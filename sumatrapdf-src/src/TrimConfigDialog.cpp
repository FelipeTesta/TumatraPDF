/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
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
#include "TrimConfigDialog.h"
#include "Theme.h"
#include "Canvas.h"

static TrimConfigWnd* gTrimConfigWnd = nullptr;

static void SafeDeleteTrimConfigDialog() {
    if (!gTrimConfigWnd) {
        return;
    }
    auto* tmp = gTrimConfigWnd;
    gTrimConfigWnd = nullptr;
    delete tmp;
}

void TrimConfigWnd::ScheduleDelete() {
    if (gTrimConfigWnd != this) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteTrimConfigDialog);
    uitask::Post(fn, "SafeDeleteTrimConfigDialog");
}

// Put the dialog beside the main window instead of on top of it, so the page
// stays visible while the trim line is dragged. Of the two sides, whichever
// has room for it wins; if both do, the roomier one. When the main window fills
// the monitor (maximized or full screen) neither side has room, so the dialog
// goes against the right edge of the work area.
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
    // vertically centered on the main window
    int y = rRelative.y + ((rRelative.dy - r.dy) / 2);

    r = {x, y, r.dx, r.dy};
    // last word on staying fully on screen, e.g. a main window taller than the
    // work area, or dragged partly off it
    Rect r2 = ShiftRectToWorkArea(r, hwndRelative, true);
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void TrimConfigWnd::OnEditTopChanged() {
    if (suppressEditUpdate) {
        return;
    }
    TempStr txt = HwndGetTextTemp(editTop->hwnd);
    int val = atoi(txt.s);
    if (val < 0) {
        val = 0;
    }
    auto* dm = win->AsFixed();
    if (dm) {
        RectF mb = dm->GetEngine()->PageMediabox(dm->CurrentPageNo());
        int maxTop = (int)mb.dy - win->trimConfigBottom;
        if (val > maxTop) {
            val = maxTop;
        }
    }
    win->trimConfigTop = val;
    HwndRepaintNow(win->hwndCanvas); // line position derives from value → moves live
}

void TrimConfigWnd::OnEditBottomChanged() {
    if (suppressEditUpdate) {
        return;
    }
    TempStr txt = HwndGetTextTemp(editBottom->hwnd);
    int val = atoi(txt.s);
    if (val < 0) {
        val = 0;
    }
    auto* dm = win->AsFixed();
    if (dm) {
        RectF mb = dm->GetEngine()->PageMediabox(dm->CurrentPageNo());
        int maxBottom = (int)mb.dy - win->trimConfigTop;
        if (val > maxBottom) {
            val = maxBottom;
        }
    }
    win->trimConfigBottom = val;
    HwndRepaintNow(win->hwndCanvas);
}

void TrimConfigWnd::OnEditColGapChanged() {
    if (suppressEditUpdate) {
        return;
    }
    TempStr txt = HwndGetTextTemp(editColGap->hwnd);
    int val = atoi(txt.s);
    if (val < 0) {
        val = 0;
    }
    // column overlap (two-column v2, FASE 6): the column width is half the page
    // plus this gap, so each column shows a bit of the other half instead of
    // cutting text flush at the page center. 0 = exact half split.
    gGlobalPrefs->viewportCrop.colGap = val;
    auto* dm = win->AsFixed();
    if (dm && dm->IsViewportCropV2Active()) {
        // column width changed → the virtual page's media box changed, so re-layout
        // and drop the cached tiles (rendered with the old gap)
        dm->RelayoutKeepingView();
        gRenderCache->FreeForDisplayModel(dm);
    }
    HwndRepaintNow(win->hwndCanvas);
}

void TrimConfigWnd::OnReset() {
    win->trimConfigTop = 0;
    win->trimConfigBottom = 0;
    gGlobalPrefs->viewportCrop.colGap = 0;
    SyncEditsFromLine();
    auto* dm = win->AsFixed();
    if (dm && dm->IsViewportCropV2Active()) {
        dm->RelayoutKeepingView();
        gRenderCache->FreeForDisplayModel(dm);
    }
    HwndRepaintNow(win->hwndCanvas);
}

void TrimConfigWnd::OnSave() {
    gGlobalPrefs->trim.top = win->trimConfigTop;
    gGlobalPrefs->trim.bottom = win->trimConfigBottom;
    SaveSettings();
    win->trimConfigMode = 0;
    // page heights depend on the trim values, so re-layout (keeping the view)
    // before repainting
    auto* trimDm = win->AsFixed();
    if (trimDm && (trimDm->marginTrimEnabled || trimDm->IsViewportCropV2Active())) {
        trimDm->RelayoutKeepingView();
        // cached tiles were rendered with the old trim/colGap values
        gRenderCache->FreeForDisplayModel(trimDm);
    }
    HwndRepaintNow(win->hwndCanvas);
    // Ensure contrast overlay stays positioned correctly after trim changes
    if (win->hwndContrastOverlay) {
        UpdateContrastOverlay(win);
    }
    // Clear modeless dialog registration
    if (GetCurrentModelessDialog() == hwnd) {
        SetCurrentModelessDialog(nullptr);
    }
    ScheduleDelete();
}

void TrimConfigWnd::OnCancel() {
    gGlobalPrefs->trim.top = savedTop;
    gGlobalPrefs->trim.bottom = savedBottom;
    gGlobalPrefs->viewportCrop.colGap = savedColGap;
    win->trimConfigMode = 0;
    auto* trimDm = win->AsFixed();
    if (trimDm && (trimDm->marginTrimEnabled || trimDm->IsViewportCropV2Active())) {
        trimDm->RelayoutKeepingView();
    }
    HwndRepaintNow(win->hwndCanvas);
    // Ensure contrast overlay stays positioned correctly after cancel
    if (win->hwndContrastOverlay) {
        UpdateContrastOverlay(win);
    }
    // Clear modeless dialog registration
    if (GetCurrentModelessDialog() == hwnd) {
        SetCurrentModelessDialog(nullptr);
    }
    ScheduleDelete();
}

void TrimConfigWnd::SyncEditsFromLine() {
    suppressEditUpdate = true;
    if (editTop) {
        HwndSetText(editTop->hwnd, fmt("%d", win->trimConfigTop));
    }
    if (editBottom) {
        HwndSetText(editBottom->hwnd, fmt("%d", win->trimConfigBottom));
    }
    if (editColGap) {
        HwndSetText(editColGap->hwnd, fmt("%d", gGlobalPrefs->viewportCrop.colGap));
    }
    suppressEditUpdate = false;
}

void TrimConfigDialogSyncEdits() {
    if (gTrimConfigWnd) {
        gTrimConfigWnd->SyncEditsFromLine();
    }
}

static void OnClose(Wnd::CloseEvent* /*ev*/) {
    if (gTrimConfigWnd) {
        gTrimConfigWnd->OnCancel();
    }
}

static void OnDestroy(Wnd::DestroyEvent* /*ev*/) {
    if (gTrimConfigWnd) {
        gTrimConfigWnd->ScheduleDelete();
    }
}

bool TrimConfigWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    savedTop = gGlobalPrefs->trim.top;
    savedBottom = gGlobalPrefs->trim.bottom;
    savedColGap = gGlobalPrefs->viewportCrop.colGap;
    win->trimConfigTop = savedTop;
    win->trimConfigBottom = savedBottom;
    win->trimConfigMode = 1; // dialog open → both lines visible
    win->trimConfigDragLine = 0;
    win->trimDragging = false;

    {
        CreateCustomArgs args;
        args.title = _TRA("Trim Config");
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

    // Row 1: "margin top:" label + numeric edit
    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        Static::CreateArgs sargs;
        sargs.parent = hwnd;
        sargs.font = font;
        sargs.text = _TRA("margin top:");
        sargs.isRtl = isRtl;
        auto* lbl = new Static();
        lbl->Create(sargs);
        hbox->AddChild(new Padding(lbl, Insets{0, 0, 0, 8}));

        Edit::CreateArgs eargs;
        eargs.parent = hwnd;
        eargs.font = font;
        eargs.isMultiLine = false;
        eargs.withBorder = true;
        eargs.idealWidthChars = 6;
        eargs.isRtl = isRtl;
        editTop = new Edit();
        editTop->Create(eargs);
        editTop->onTextChanged = MkMethod0<TrimConfigWnd, &TrimConfigWnd::OnEditTopChanged>(this);
        hbox->AddChild(new Padding(editTop, pad));
        vbox->AddChild(hbox);
    }

    // Row 2: "margin bottom:" label + numeric edit
    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        Static::CreateArgs sargs;
        sargs.parent = hwnd;
        sargs.font = font;
        sargs.text = _TRA("margin bottom:");
        sargs.isRtl = isRtl;
        auto* lbl = new Static();
        lbl->Create(sargs);
        hbox->AddChild(new Padding(lbl, Insets{0, 0, 0, 8}));

        Edit::CreateArgs eargs;
        eargs.parent = hwnd;
        eargs.font = font;
        eargs.isMultiLine = false;
        eargs.withBorder = true;
        eargs.idealWidthChars = 6;
        eargs.isRtl = isRtl;
        editBottom = new Edit();
        editBottom->Create(eargs);
        editBottom->onTextChanged = MkMethod0<TrimConfigWnd, &TrimConfigWnd::OnEditBottomChanged>(this);
        hbox->AddChild(new Padding(editBottom, pad));
        vbox->AddChild(hbox);
    }

    // Row 2b: "column gap:" label + numeric edit (two-column v2 overlap)
    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        Static::CreateArgs sargs;
        sargs.parent = hwnd;
        sargs.font = font;
        sargs.text = _TRA("column gap:");
        sargs.isRtl = isRtl;
        auto* lbl = new Static();
        lbl->Create(sargs);
        hbox->AddChild(new Padding(lbl, Insets{0, 0, 0, 8}));

        Edit::CreateArgs eargs;
        eargs.parent = hwnd;
        eargs.font = font;
        eargs.isMultiLine = false;
        eargs.withBorder = true;
        eargs.idealWidthChars = 6;
        eargs.isRtl = isRtl;
        editColGap = new Edit();
        editColGap->Create(eargs);
        editColGap->onTextChanged = MkMethod0<TrimConfigWnd, &TrimConfigWnd::OnEditColGapChanged>(this);
        hbox->AddChild(new Padding(editColGap, pad));
        vbox->AddChild(hbox);
    }

    // Row 3: ✅ Save / Reset / Cancel (right-aligned)
    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        btnSave = CreateButton(hwnd, _TRA("✅"), MkMethod0<TrimConfigWnd, &TrimConfigWnd::OnSave>(this), isRtl);
        // Enter runs this one (see PreTranslateMessage), so draw it as the
        // default button to say so
        btnSave->isDefault = true;
        hbox->AddChild(new Padding(btnSave, pad));
        btnReset = CreateButton(hwnd, _TRA("Reset"), MkMethod0<TrimConfigWnd, &TrimConfigWnd::OnReset>(this), isRtl);
        hbox->AddChild(new Padding(btnReset, pad));
        btnCancel = CreateButton(hwnd, _TRA("Cancel"), MkMethod0<TrimConfigWnd, &TrimConfigWnd::OnCancel>(this), isRtl);
        hbox->AddChild(new Padding(btnCancel, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    layout = padding;

    int dx = DpiScale(hwnd, 320);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    PositionDialog(hwnd, win->hwndFrame);

    SyncEditsFromLine();
    SetIsVisible(true);
    // force full repaint of dialog + children so buttons/edits paint immediately
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    HwndSetFocus(hwnd);
    HwndRepaintNow(win->hwndCanvas); // show both red lines immediately

    // Register as modeless dialog for IsDialogMessage (enables Enter -> default button)
    isDialog = true;
    SetCurrentModelessDialog(hwnd);

    // Apply dark mode to popup window (title bar, controls)
    ApplyDarkModeToPopupWindow(hwnd);

    return true;
}

LRESULT TrimConfigWnd::OnMessageReflect(UINT msg, WPARAM wparam, LPARAM /*lparam*/) {
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

LRESULT TrimConfigWnd::WndProc(HWND hwndIn, UINT msg, WPARAM wp, LPARAM lp) {
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

void ShowTrimConfigDialog(MainWindow* win) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (gTrimConfigWnd) {
        HwndSetFocus(gTrimConfigWnd->hwnd);
        return;
    }
    auto* wnd = new TrimConfigWnd();
    wnd->onClose = MkFunc1Void<Wnd::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroy);
    wnd->font = GetAppFont(win->hwndFrame);
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gTrimConfigWnd = wnd;
}