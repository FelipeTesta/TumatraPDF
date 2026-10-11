/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

// "Edit" mode: a toolbar toggle like Arch Tools / Flashcards. When on, a
// second toolbar offers +Text / +Img. +Img (or Ctrl+V while in edit mode)
// grabs the clipboard image; the next left-click on a page places it as a
// Stamp annotation (click-to-place), clamped to fit the page.

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/Pixmap.h"
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
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "EditAnnotations.h"
#include "Toolbar.h"
#include "ToolbarLayout.h"
#include "Theme.h"
#include "AppSettings.h"
#include "Commands_Edit.h"

// Forward declarations — these are defined in other translation units
bool IsCurrentDocMarkdown(MainWindow* win);
bool RelayoutFrame(MainWindow* win, bool updateToolbars, int sidebarDx);

// notify the user with a transient toast anchored to the canvas
static void EditNotify(MainWindow* win, Str msg) {
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.timeoutMs = 3000;
    nargs.msg = Str(msg);
    ShowNotification(nargs);
}

void EditToolsClearPendingImage(MainWindow* win) {
    if (win->editTools.pendingImage) {
        FreePixmap(win->editTools.pendingImage);
        win->editTools.pendingImage = nullptr;
        logf("[edit] pending image cleared\n");
    }
}

// ---- second toolbar (+Text / +Img) ----------------------------------------
// Pattern follows the Flashcard secondary toolbar (FlashcardToolbar.cpp),
// which itself follows Arch Tools' toolbar2.

void EditToolbarCreate(MainWindow* win) {
    if (win->editTools.hwndToolbarEdit) {
        return; // already created
    }
    logf("[edit] EditToolbarCreate - creating secondary toolbar\n");
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;

    DWORD style = WS_CHILD | WS_CLIPCHILDREN | RBS_VARHEIGHT | CCS_NODIVIDER | CCS_NOPARENTALIGN;
    if (IsCurrentThemeDefault()) {
        style |= WS_BORDER | RBS_BANDBORDERS;
    }
    DWORD exStyle = WS_EX_TOOLWINDOW;
    HWND hwndRebar = CreateWindowExW(exStyle, REBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndParent,
                                     (HMENU)IDC_EDIT_REBAR, hinst, nullptr);

    REBARINFO rbi{};
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask = 0;
    rbi.himl = (HIMAGELIST) nullptr;
    SendMessageW(hwndRebar, RB_SETBARINFO, 0, (LPARAM)&rbi);

    style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | TBSTYLE_LIST | CCS_NODIVIDER |
            CCS_NOPARENTALIGN | TBSTYLE_WRAPABLE;
    exStyle = 0;
    bool isRtl = (GetWindowLong(hwndParent, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
    if (isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }
    HWND hwndToolbar = CreateWindowExW(exStyle, TOOLBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndRebar,
                                       (HMENU)IDC_EDIT_TOOLBAR, hinst, nullptr);
    TbSetButtonStructSize(hwndToolbar, sizeofi(TBBUTTON));

    struct ToolbarButtonInfo2 {
        int cmdId;
        Str toolTip;
    };
    static ToolbarButtonInfo2 gEditToolbarButtons[] = {
        {CmdEditAddText, _TRN("+Text")},
        {CmdEditAddImage, _TRN("+Img")},
    };
    constexpr int kEditToolbarButtonsCount = dimof(gEditToolbarButtons);

    TBBUTTON tbButtons[kEditToolbarButtonsCount];
    for (int i = 0; i < kEditToolbarButtonsCount; i++) {
        const ToolbarButtonInfo2& bi = gEditToolbarButtons[i];
        TBBUTTON b{};
        b.idCommand = bi.cmdId;
        b.iBitmap = 0;
        b.fsState = TBSTATE_ENABLED;
        b.fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_AUTOSIZE;
        Str s = trans::GetTranslation(bi.toolTip);
        b.iString = (INT_PTR)CWStrTemp(s);
        tbButtons[i] = b;
    }
    TbAddButtons(hwndToolbar, kEditToolbarButtonsCount, tbButtons);

    int iconSize = DpiScale(hwndParent, gGlobalPrefs->toolbarSize);
    iconSize = RoundUp(iconSize, 4);
    TbSetBitmapSize(hwndToolbar, Size(iconSize, iconSize));
    TbSetButtonSize(hwndToolbar, Size(iconSize, iconSize));

    ToolbarApplyThemeToRebar(hwndRebar, hwndToolbar);

    Rect rc = TbGetItemRect(hwndToolbar, 0);

    ShowWindow(hwndToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE | RBBS_VARIABLEHEIGHT;
    if (IsAppThemed() && IsCurrentThemeDefault()) {
        rbBand.fStyle |= RBBS_CHILDEDGE;
    }
    rbBand.hbmBack = nullptr;
    rbBand.lpText = (WCHAR*)L"Edit Toolbar"; // NOLINT
    rbBand.hwndChild = hwndToolbar;
    rbBand.cxMinChild = rc.dx;
    rbBand.cyMinChild = rc.dy + (2 * rc.y);
    rbBand.cx = 0;
    SendMessageW(hwndRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    ShowWindow(hwndRebar, SW_SHOW);

    win->editTools.hwndReBarEdit = hwndRebar;
    win->editTools.hwndToolbarEdit = hwndToolbar;

    logf("[edit] EditToolbarCreate - toolbar created with %d buttons\n", kEditToolbarButtonsCount);
}

void EditToolbarDestroy(MainWindow* win) {
    if (!win->editTools.hwndToolbarEdit) {
        return;
    }
    DestroyWindow(win->editTools.hwndToolbarEdit);
    win->editTools.hwndToolbarEdit = nullptr;
    DestroyWindow(win->editTools.hwndReBarEdit);
    win->editTools.hwndReBarEdit = nullptr;
    logf("[edit] EditToolbarDestroy - destroyed secondary toolbar\n");
}

// ---- click-to-place ---------------------------------------------------------

// Grab the clipboard image and arm placement: the next left-click on a page
// places it (EditToolsPlacePendingImage, called from Canvas).
static void EditArmPlaceImage(MainWindow* win) {
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine || !EngineSupportsAnnotations(engine)) {
        EditNotify(win, _TRA("This document doesn't support adding images"));
        return;
    }
    Pixmap* image = GetClipboardImageAsPixmap();
    if (!image) {
        EditNotify(win, _TRA("No image in the clipboard"));
        return;
    }
    EditToolsClearPendingImage(win);
    win->editTools.pendingImage = image;
    logf("[edit] clipboard image %dx%d armed - click on a page to place it\n", image->width, image->height);
    EditNotify(win, _TRA("Click on the page to place the image"));
    if (win->hwndCanvas) {
        SetCursor(LoadCursor(nullptr, IDC_CROSS));
    }
}

// capture mupdf error text (fz_report_error output) into our log while a
// stamp create is in flight, so a failed placement says WHY
static void EditMupdfErrorCb(void* /*user*/, const char* message) {
    logf("[edit] mupdf error: %s\n", Str(message));
}

// Called from Canvas OnMouseLeftButtonDown when edit mode is on and an image
// is armed. Places the clipboard image as a Stamp annotation at the click.
bool EditToolsPlacePendingImage(MainWindow* win, int x, int y) {
    if (!win->editTools.on || !win->editTools.pendingImage) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        // document changed under us: drop the pending image, consume click
        EditToolsClearPendingImage(win);
        return true;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || !EngineSupportsAnnotations(engine)) {
        EditToolsClearPendingImage(win);
        EditNotify(win, _TRA("This document doesn't support adding images"));
        return true;
    }
    Point pt{x, y};
    int pageNo = dm->GetPageNoByPoint(pt);
    if (pageNo < 1) {
        // not on a page: keep armed, wait for a click that lands on one
        return true;
    }
    PointF ptOnPage = dm->CvtFromScreen(pt, pageNo);
    Pixmap* image = win->editTools.pendingImage;
    win->editTools.pendingImage = nullptr;
    logf("[edit] placing clipboard image %dx%d fmt=%d stride=%d xres=%d on page %d at (%.1f, %.1f)\n", image->width,
         image->height, (int)image->format, (int)image->stride, (int)image->xres, pageNo, ptOnPage.x, ptOnPage.y);
    AnnotCreateArgs args{AnnotationType::Stamp};
    args.stampImage = image;
    EngineMupdf* emDebug = AsEngineMupdf(engine);
    fz_set_error_callback(emDebug->Ctx(), EditMupdfErrorCb, nullptr);
    Annotation* annot = EngineMupdfCreateAnnotation(engine, pageNo, ptOnPage, &args);
    fz_set_error_callback(emDebug->Ctx(), nullptr, nullptr);
    // EngineMupdfCreateAnnotation wraps (not copies) the pixmap data, so the
    // Pixmap wrapper must be freed on our side like the upstream caller does
    FreePixmap(image);
    if (!annot) {
        logf("[edit] image placement FAILED (annot create returned null)\n");
        EditNotify(win, _TRA("Failed to add the image"));
        MainWindowRerender(win);
        return true;
    }
    // an image pasted at natural size (e.g. a 96dpi screenshot) can be far
    // larger than the page: clamp to 90% of the page, keeping proportions
    {
        RectF r = GetRect(annot);
        EngineMupdf* em = AsEngineMupdf(engine);
        auto* pageInfo = em->GetFzPageInfo(pageNo, true);
        if (pageInfo && pageInfo->page) {
            fz_rect pageRect = fz_bound_page(em->Ctx(), pageInfo->page);
            float pageW = pageRect.x1 - pageRect.x0;
            float pageH = pageRect.y1 - pageRect.y0;
            float maxW = pageW * 0.9f;
            float maxH = pageH * 0.9f;
            if (r.dx > maxW || r.dy > maxH) {
                float scale = std::min(maxW / r.dx, maxH / r.dy);
                if (scale > 0.0f && scale < 1.0f) {
                    r.dx *= scale;
                    r.dy *= scale;
                    SetRect(annot, r);
                    logf("[edit] stamp clamped to %.0f%% to fit the page\n", scale * 100.0f);
                }
            }
        }
    }
    // same tail as the FrameOnCommand annot-creation cases: refresh the
    // annotations window if open, select the new stamp so it can be moved,
    // and rerender so the stamp is visible immediately
    WindowTab* tab = win->CurrentTab();
    if (tab) {
        UpdateAnnotationsList(tab->editAnnotsWindow);
        SetSelectedAnnotation(tab, annot, true);
    }
    MainWindowRerender(win);
    EditNotify(win, _TRA("Image added (save the document to keep it)"));
    logf("[edit] image placed on page %d\n", pageNo);
    return true;
}

// ---- command dispatch ------------------------------------------------------

// HandleCmdEditToolsToggle — toggle edit mode on/off
static void HandleCmdEditToolsToggle(MainWindow* win) {
    win->editTools.on = !win->editTools.on;
    logf("[edit] CmdEditToolsToggle: editMode=%d\n", win->editTools.on ? 1 : 0);
    if (win->editTools.on) {
        EditToolbarCreate(win);
        // turn OFF mutually-exclusive modes that also own a toolbar row; they
        // go through FrameOnCommand, whose edit gate lets an OFF through
        if (win->archTools.on) {
            PostMessageW(win->hwndFrame, WM_COMMAND, CmdArchToolsToggle, 0);
        }
        if (win->flashcard.on) {
            PostMessageW(win->hwndFrame, WM_COMMAND, CmdFlashcardToggle, 0);
        }
        RelayoutFrame(win, true, -1);
    } else {
        EditToolsClearPendingImage(win);
        EditToolbarDestroy(win);
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
        }
        RelayoutFrame(win, true, -1);
    }
    SetToolbarButtonCheckedState(win, CmdEditToolsToggle, win->editTools.on);
    ToolbarUpdateStateForWindow(win, false);
    ScheduleRepaint(win, 0);
}

bool HandleCommandEdit(MainWindow* win, int cmd) {
    switch (cmd) {
        case CmdEditToolsToggle:
            if (IsCurrentDocMarkdown(win)) {
                return true;
            }
            HandleCmdEditToolsToggle(win);
            return true;
        case CmdEditAddText:
            // TODO(s34): dedicated click-to-place text flow; for now forward
            // to the existing FreeText annot creation (places at cursor pos
            // and opens the annotation editor)
            logf("[edit] CmdEditAddText -> CmdCreateAnnotFreeText\n");
            PostMessageW(win->hwndFrame, WM_COMMAND, CmdCreateAnnotFreeText, 0);
            return true;
        case CmdEditAddImage:
            if (!win->editTools.on) {
                return true;
            }
            EditArmPlaceImage(win);
            return true;
    }
    return false;
}
