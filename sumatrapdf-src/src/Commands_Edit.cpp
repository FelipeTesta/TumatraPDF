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
#include "base/File.h"
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
#include "ImageReader.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "Tabs.h"
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
        {CmdEditImageCrop, _TRN("Crop")},
        {CmdEditImageApply, _TRN("Apply")},
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
        // Crop is a toggle: needs BTNS_CHECK for the visual checked state
        if (bi.cmdId == CmdEditImageCrop) {
            b.fsStyle |= BTNS_CHECK;
        }
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

    // size the rebar to the frame width right away (see the same comment in
    // FlashcardToolbarCreate): at width 0 the toolbar stacks every button on
    // its own row and the first RelayoutFrame would reserve that height
    {
        Rect rcFrame = HwndClientRect(hwndParent);
        SetWindowPos(hwndRebar, nullptr, 0, 0, rcFrame.dx, rc.dy + (2 * rc.y), SWP_NOZORDER | SWP_NOMOVE);
    }

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
    ToolbarUpdateStateForWindow(win, false); // stamp creation marks the doc modified — light the tab dot marker
    EditNotify(win, _TRA("Image added (save the document to keep it)"));
    logf("[edit] image placed on page %d\n", pageNo);
    return true;
}

// ---- in-page image bbox editing ---------------------------------------------
//
// While edit mode is on, a left-click on an embedded image selects it and
// shows a bounding box with drag handles directly over the page (no separate
// editor window). Dragging a handle resizes the region (clamped to the
// image's draw rect), dragging inside moves it. "Crop" toggles marquee-style
// dimming of the area outside the bbox. "Apply" writes every pending edit
// back into the PDF as an IMAGE-DATA swap (ReplaceImageByIdx): the visible
// content pixels are re-drawn into the bbox on a transparent canvas with the
// ORIGINAL image size, so the page footprint never changes — the image stays
// an image (no annotation is created). After Apply the selection keeps
// pointing at the applied region (bbox/content), so further edits compose on
// top of the visible result. Save the PDF (File > Save) to persist.

static bool EditRectEq(const RectF& a, const RectF& b) {
    return a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy == b.dy;
}

static RectF EditBBoxScreenRect(DisplayModel* dm, const MainWindow::EditToolsState::PageImageEdit& ed) {
    Point tl = dm->CvtToScreen(ed.pageNo, PointF{ed.bbox.x, ed.bbox.y});
    Point br = dm->CvtToScreen(ed.pageNo, PointF{ed.bbox.x + ed.bbox.dx, ed.bbox.y + ed.bbox.dy});
    float x = (float)std::min(tl.x, br.x);
    float y = (float)std::min(tl.y, br.y);
    return RectF{x, y, (float)std::abs(br.x - tl.x), (float)std::abs(br.y - tl.y)};
}

static RectF EditImageScreenRect(DisplayModel* dm, const MainWindow::EditToolsState::PageImageEdit& ed) {
    Point tl = dm->CvtToScreen(ed.pageNo, PointF{ed.imageRect.x, ed.imageRect.y});
    Point br = dm->CvtToScreen(ed.pageNo, PointF{ed.imageRect.x + ed.imageRect.dx, ed.imageRect.y + ed.imageRect.dy});
    float x = (float)std::min(tl.x, br.x);
    float y = (float)std::min(tl.y, br.y);
    return RectF{x, y, (float)std::abs(br.x - tl.x), (float)std::abs(br.y - tl.y)};
}

void EditToolsClearImageEdits(MainWindow* win) {
    for (auto& ed : win->editTools.imageEdits) {
        delete ed.preview;
        ed.preview = nullptr;
    }
    win->editTools.imageEdits.Reset();
    win->editTools.selEditIdx = -1;
    win->editTools.dragEditIdx = -1;
    win->editTools.dragEdge = 0;
    str::FreePtr(&win->editTools.editsTabPath);
}

// pending edits belong to one document: drop them when the tab changes
static void EditSyncTab(MainWindow* win) {
    auto& st = win->editTools;
    if (st.imageEdits.len == 0) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    bool sameDoc = tab && tab->filePath && st.editsTabPath && str::Eq(tab->filePath, st.editsTabPath);
    if (!sameDoc) {
        EditToolsClearImageEdits(win);
    }
}

// remember which document the pending edits belong to
static void EditRememberTabPath(MainWindow* win) {
    auto& st = win->editTools;
    WindowTab* tab = win->CurrentTab();
    str::FreePtr(&st.editsTabPath);
    st.editsTabPath = (tab && tab->filePath) ? str::Dup(tab->filePath) : Str{};
}

// -1 = nothing, 0 = inside the selected bbox (move), 1..8 = handle id
// (1 left, 2 right, 3 top, 4 bottom, 5 TL, 6 TR, 7 BL, 8 BR)
int EditToolsImageHoverTest(MainWindow* win, int x, int y) {
    if (!win->editTools.on) {
        return -1;
    }
    EditSyncTab(win);
    DisplayModel* dm = win->AsFixed();
    auto& st = win->editTools;
    if (!dm || st.selEditIdx < 0 || st.selEditIdx >= st.imageEdits.len) {
        return -1;
    }
    RectF sr = EditBBoxScreenRect(dm, st.imageEdits[st.selEditIdx]);
    const float hs = 5.0f; // handle hit half-size, px
    struct Hand {
        float x, y;
        int id;
    };
    Hand hands[8] = {
        {sr.x, sr.y, 5},
        {sr.x + sr.dx / 2, sr.y, 3},
        {sr.x + sr.dx, sr.y, 6},
        {sr.x, sr.y + sr.dy / 2, 1},
        {sr.x + sr.dx, sr.y + sr.dy / 2, 2},
        {sr.x, sr.y + sr.dy, 7},
        {sr.x + sr.dx / 2, sr.y + sr.dy, 4},
        {sr.x + sr.dx, sr.y + sr.dy, 8},
    };
    for (const Hand& h : hands) {
        if (std::abs((float)x - h.x) <= hs && std::abs((float)y - h.y) <= hs) {
            return h.id;
        }
    }
    // the whole border is draggable (not just the midpoint handles) — standard
    // image-editor behavior; corners win over plain sides
    const float kEdge = 6.0f;
    bool nearL = std::abs((float)x - sr.x) <= kEdge;
    bool nearR = std::abs((float)x - (sr.x + sr.dx)) <= kEdge;
    bool nearT = std::abs((float)y - sr.y) <= kEdge;
    bool nearB = std::abs((float)y - (sr.y + sr.dy)) <= kEdge;
    if (nearL && nearT) {
        return 5;
    }
    if (nearR && nearT) {
        return 6;
    }
    if (nearL && nearB) {
        return 7;
    }
    if (nearR && nearB) {
        return 8;
    }
    if (nearL) {
        return 1;
    }
    if (nearR) {
        return 2;
    }
    if (nearT) {
        return 3;
    }
    if (nearB) {
        return 4;
    }
    if ((float)x >= sr.x && (float)x <= sr.x + sr.dx && (float)y >= sr.y && (float)y <= sr.y + sr.dy) {
        return 0; // inside: move
    }
    return -1;
}

bool EditToolsImageMouseDown(MainWindow* win, int x, int y) {
    if (!win->editTools.on || win->editTools.pendingImage) {
        return false;
    }
    EditSyncTab(win);
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    auto& st = win->editTools;
    int hov = EditToolsImageHoverTest(win, x, y);
    if (hov >= 0) {
        // start a bbox drag (move or handle); the current Crop state decides
        // this drag's semantics (region writeback vs free resize)
        auto& ed = st.imageEdits[st.selEditIdx];
        ed.crop = st.cropMode;
        st.dragEditIdx = st.selEditIdx;
        st.dragEdge = hov;
        st.dragStartPage = dm->CvtFromScreen(Point{x, y}, ed.pageNo);
        st.dragStartBbox = ed.bbox;
        return true;
    }
    Point pt{x, y};
    int pageNo = dm->GetPageNoByPoint(pt);
    if (pageNo < 1) {
        st.selEditIdx = -1;
        return false;
    }
    IPageElement* pageEl = dm->GetElementAtPos(pt, nullptr);
    if (!pageEl || !pageEl->Is(kindPageElementImage)) {
        st.selEditIdx = -1; // click on a non-image: deselect, let the click through
        return false;
    }
    EngineMupdf* em = AsEngineMupdf(dm->GetEngine());
    int pg = 0, imageIdx = 0;
    if (!em || !em->GetImageElementCoords(pageEl, &pg, &imageIdx)) {
        return false;
    }
    RectF imageRect = pageEl->GetRect();
    int found = -1;
    for (int i = 0; i < st.imageEdits.len; i++) {
        if (st.imageEdits[i].pageNo == pg && st.imageEdits[i].imageIdx == imageIdx) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        MainWindow::EditToolsState::PageImageEdit ed{};
        ed.pageNo = pg;
        ed.imageIdx = imageIdx;
        ed.imageRect = imageRect;
        ed.content = imageRect;
        ed.bbox = imageRect;
        ed.crop = st.cropMode;
        st.imageEdits.Append(ed);
        found = st.imageEdits.len - 1;
    }
    EditRememberTabPath(win);
    st.selEditIdx = found;
    logf("[edit] image selected: page %d idx %d (%.1f x %.1f pt)\n", pg, imageIdx, imageRect.dx, imageRect.dy);
    HwndInvalidate(win->hwndCanvas);
    return true;
}

void EditToolsImageDragMove(MainWindow* win, int x, int y) {
    auto& st = win->editTools;
    if (st.dragEditIdx < 0 || st.dragEditIdx >= st.imageEdits.len) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    auto& ed = st.imageEdits[st.dragEditIdx];
    PointF cur = dm->CvtFromScreen(Point{x, y}, ed.pageNo);
    float dx = cur.x - st.dragStartPage.x;
    float dy = cur.y - st.dragStartPage.y;
    const RectF& b0 = st.dragStartBbox;
    const float kMin = 2.0f; // smallest bbox side, page units
    // crop is a source-region selection: bbox stays inside the image rect.
    // resize/move has the purpose of changing the footprint, so the bbox is
    // limited to the PAGE only — Apply extends the draw rect in the content
    // stream (SetImageDrawRect) to genuinely enlarge beyond the original size
    RectF limits = ed.crop ? ed.imageRect : dm->GetEngine()->PageMediabox(ed.pageNo);
    auto clampf = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
    RectF nb = b0;
    switch (st.dragEdge) {
        case 0: // move inside the limits
            nb.x = clampf(b0.x + dx, limits.x, limits.x + limits.dx - b0.dx);
            nb.y = clampf(b0.y + dy, limits.y, limits.y + limits.dy - b0.dy);
            break;
        case 1: // left edge
            nb.x = clampf(b0.x + dx, limits.x, b0.x + b0.dx - kMin);
            nb.dx = b0.x + b0.dx - nb.x;
            break;
        case 2: // right edge
            nb.dx = clampf(b0.dx + dx, kMin, limits.x + limits.dx - b0.x);
            break;
        case 3: // top edge
            nb.y = clampf(b0.y + dy, limits.y, b0.y + b0.dy - kMin);
            nb.dy = b0.y + b0.dy - nb.y;
            break;
        case 4: // bottom edge
            nb.dy = clampf(b0.dy + dy, kMin, limits.y + limits.dy - b0.y);
            break;
        case 5: // top-left
            nb.x = clampf(b0.x + dx, limits.x, b0.x + b0.dx - kMin);
            nb.dx = b0.x + b0.dx - nb.x;
            nb.y = clampf(b0.y + dy, limits.y, b0.y + b0.dy - kMin);
            nb.dy = b0.y + b0.dy - nb.y;
            break;
        case 6: // top-right
            nb.dx = clampf(b0.dx + dx, kMin, limits.x + limits.dx - b0.x);
            nb.y = clampf(b0.y + dy, limits.y, b0.y + b0.dy - kMin);
            nb.dy = b0.y + b0.dy - nb.y;
            break;
        case 7: // bottom-left
            nb.x = clampf(b0.x + dx, limits.x, b0.x + b0.dx - kMin);
            nb.dx = b0.x + b0.dx - nb.x;
            nb.dy = clampf(b0.dy + dy, kMin, limits.y + limits.dy - b0.y);
            break;
        case 8: // bottom-right
            nb.dx = clampf(b0.dx + dx, kMin, limits.x + limits.dx - b0.x);
            nb.dy = clampf(b0.dy + dy, kMin, limits.y + limits.dy - b0.y);
            break;
        default:
            break;
    }
    if (std::abs(nb.x - b0.x) < 0.001f && std::abs(nb.y - b0.y) < 0.001f && std::abs(nb.dx - b0.dx) < 0.001f &&
        std::abs(nb.dy - b0.dy) < 0.001f) {
        return; // nothing moved — don't invalidate at all
    }
    // flicker-free drag: repaint only the strip around the overlay (old bbox ∪
    // new bbox ∪ image rect — the dim bands/live preview cover the image rect),
    // not the whole canvas (standard editor marquee behavior)
    RectF oldSr = EditBBoxScreenRect(dm, ed);
    ed.bbox = nb;
    RectF newSr = EditBBoxScreenRect(dm, ed);
    RectF ir = EditImageScreenRect(dm, ed);
    float ux0 = std::min(oldSr.x, std::min(newSr.x, ir.x));
    float uy0 = std::min(oldSr.y, std::min(newSr.y, ir.y));
    float ux1 = std::max(oldSr.x + oldSr.dx, std::max(newSr.x + newSr.dx, ir.x + ir.dx));
    float uy1 = std::max(oldSr.y + oldSr.dy, std::max(newSr.y + newSr.dy, ir.y + ir.dy));
    const float kMargin = 10.0f; // handles + pen width, px
    Rect rc{(int)floorf(ux0 - kMargin), (int)floorf(uy0 - kMargin),
            (int)ceilf(ux1 + kMargin) - (int)floorf(ux0 - kMargin),
            (int)ceilf(uy1 + kMargin) - (int)floorf(uy0 - kMargin)};
    HwndInvalidateRect(win->hwndCanvas, rc, false);
}

void EditToolsImageDragEnd(MainWindow* win) {
    auto& st = win->editTools;
    if (st.dragEditIdx < 0 || st.dragEditIdx >= st.imageEdits.len) {
        return;
    }
    auto& ed = st.imageEdits[st.dragEditIdx];
    ed.changed = !EditRectEq(ed.bbox, ed.content);
    if (ed.changed) {
        logf("[edit] image edit pending: page %d idx %d bbox %.1f,%.1f %.1fx%.1f\n", ed.pageNo, ed.imageIdx, ed.bbox.x,
             ed.bbox.y, ed.bbox.dx, ed.bbox.dy);
    }
    st.dragEditIdx = -1;
    st.dragEdge = 0;
}

// lazily extract the source image once per edit (owned by the edit; used by
// the live preview and by Apply)
static RenderedBitmap* EditGetPreviewBmp(DisplayModel* dm, MainWindow::EditToolsState::PageImageEdit& ed);

void EditToolsPaintImageOverlays(MainWindow* win, DisplayModel* dm, HDC hdc) {
    auto& st = win->editTools;
    if (!dm || st.imageEdits.len == 0) {
        return;
    }
    Gdiplus::Graphics gs(hdc);
    static Gdiplus::Pen selPen(Gdiplus::Color(255, 0, 120, 255), 1.6f);
    static Gdiplus::Pen pendPen(Gdiplus::Color(255, 0, 120, 255), 1.0f);
    static Gdiplus::SolidBrush handleBrush(Gdiplus::Color(255, 255, 255, 255));
    static Gdiplus::Pen handlePen(Gdiplus::Color(255, 0, 0, 0), 1.0f);
    static Gdiplus::SolidBrush dimBrush(Gdiplus::Color(110, 0, 0, 0));
    for (int i = 0; i < st.imageEdits.len; i++) {
        auto& ed = st.imageEdits[i];
        bool isSel = (i == st.selEditIdx);
        RectF sr = EditBBoxScreenRect(dm, ed);
        if (isSel && ed.crop) {
            // marquee crop: dim the image area outside the bbox
            RectF ir = EditImageScreenRect(dm, ed);
            auto fillIf = [&](float x, float y, float dx, float dy) {
                if (dx > 0 && dy > 0) {
                    gs.FillRectangle(&dimBrush, x, y, dx, dy);
                }
            };
            fillIf(ir.x, ir.y, ir.dx, sr.y - ir.y);                             // above
            fillIf(ir.x, sr.y + sr.dy, ir.dx, (ir.y + ir.dy) - (sr.y + sr.dy)); // below
            fillIf(ir.x, sr.y, sr.x - ir.x, sr.dy);                             // left
            fillIf(sr.x + sr.dx, sr.y, (ir.x + ir.dx) - (sr.x + sr.dx), sr.dy); // right
        } else if (isSel) {
            // live resize preview: dim the whole original image area, then
            // draw the CONTENT pixels stretched into the current bbox — what
            // the page will look like after Apply
            RectF ir = EditImageScreenRect(dm, ed);
            if (ir.dx > 0 && ir.dy > 0) {
                gs.FillRectangle(&dimBrush, ir.x, ir.y, ir.dx, ir.dy);
            }
            RenderedBitmap* pv = EditGetPreviewBmp(dm, ed);
            if (pv && pv->GetBitmap()) {
                Gdiplus::Bitmap src(pv->GetBitmap(), nullptr);
                int imgW = (int)src.GetWidth();
                int imgH = (int)src.GetHeight();
                if (imgW > 0 && imgH > 0 && ed.imageRect.dx > 0 && ed.imageRect.dy > 0 && ed.content.dx > 0 &&
                    ed.content.dy > 0) {
                    double sx = (double)imgW / ed.imageRect.dx;
                    double sy = (double)imgH / ed.imageRect.dy;
                    double csx = (double)(ed.content.x - ed.imageRect.x) * sx;
                    double csy = (double)(ed.content.y - ed.imageRect.y) * sy;
                    double csw = (double)ed.content.dx * sx;
                    double csh = (double)ed.content.dy * sy;
                    gs.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                    Gdiplus::RectF dest(sr.x, sr.y, sr.dx, sr.dy);
                    gs.DrawImage(&src, dest, (float)csx, (float)csy, (float)csw, (float)csh, Gdiplus::UnitPixel);
                }
            }
        }
        gs.DrawRectangle(isSel ? &selPen : &pendPen, sr.x, sr.y, sr.dx, sr.dy);
        if (isSel) {
            const float hs = 4.0f;
            float cx[3] = {sr.x, sr.x + sr.dx / 2, sr.x + sr.dx};
            float cy[3] = {sr.y, sr.y + sr.dy / 2, sr.y + sr.dy};
            for (int xi = 0; xi < 3; xi++) {
                for (int yi = 0; yi < 3; yi++) {
                    if (xi == 1 && yi == 1) {
                        continue; // no center handle
                    }
                    float hx = cx[xi] - hs;
                    float hy = cy[yi] - hs;
                    float hw = 2 * hs;
                    float hh = 2 * hs;
                    if (ed.crop) {
                        // crop: square handles (marquee convention)
                        gs.FillRectangle(&handleBrush, hx, hy, hw, hh);
                        gs.DrawRectangle(&handlePen, hx, hy, hw, hh);
                    } else {
                        // resize: round handles
                        gs.FillEllipse(&handleBrush, hx, hy, hw, hh);
                        gs.DrawEllipse(&handlePen, hx, hy, hw, hh);
                    }
                }
            }
        }
    }
}

// find the IPageElement of an image by (pageNo, imageIdx)
static IPageElement* EditFindImageElement(DisplayModel* dm, int pageNo, int imageIdx) {
    EngineBase* engine = dm->GetEngine();
    EngineMupdf* em = AsEngineMupdf(engine);
    if (!em) {
        return nullptr;
    }
    Vec<IPageElement*> els = engine->GetElements(pageNo);
    for (IPageElement* el : els) {
        if (!el || !el->Is(kindPageElementImage)) {
            continue;
        }
        int pg = 0, idx = 0;
        if (em->GetImageElementCoords(el, &pg, &idx) && idx == imageIdx) {
            return el;
        }
    }
    return nullptr;
}

// GDI+ png writer that PRESERVES alpha (the compose canvas is 32bppARGB)
static bool EditSaveArgbBitmapAsPng(Gdiplus::Bitmap* bmp, const WCHAR* path) {
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) {
        return false;
    }
    char* buf = (char*)malloc(size);
    if (!buf) {
        return false;
    }
    auto* codecs = (Gdiplus::ImageCodecInfo*)buf;
    if (num == 0 || size < num * sizeof(Gdiplus::ImageCodecInfo)) {
        free(buf);
        return false;
    }
    if (Gdiplus::GetImageEncoders(num, size, codecs) != Gdiplus::Ok) {
        free(buf);
        return false;
    }
    CLSID pngClsid{};
    bool found = false;
    for (UINT i = 0; i < num; i++) {
        if (0 == lstrcmpW(codecs[i].MimeType, L"image/png")) {
            pngClsid = codecs[i].Clsid;
            found = true;
            break;
        }
    }
    free(buf);
    if (!found) {
        return false;
    }
    return bmp->Save(path, &pngClsid, nullptr) == Gdiplus::Ok;
}

// lazily extract the source image once per edit (owned by the edit; used by
// the live preview and by Apply)
static RenderedBitmap* EditGetPreviewBmp(DisplayModel* dm, MainWindow::EditToolsState::PageImageEdit& ed) {
    if (ed.preview) {
        return ed.preview;
    }
    IPageElement* el = EditFindImageElement(dm, ed.pageNo, ed.imageIdx);
    if (!el) {
        return nullptr;
    }
    ed.preview = dm->GetEngine()->GetImageForPageElement(el);
    return ed.preview;
}

// write every pending edit back into the PDF as an IMAGE-DATA swap
// (ReplaceImageByIdx): the visible content pixels are drawn into the new
// bbox on a transparent canvas with the ORIGINAL image size, so the page
// footprint never changes and no annotation is created. After a successful
// apply the edit stays selected with content = bbox, so the bounding box
// keeps matching the visible image and further edits compose on top of it.
static void EditApplyImageEdits(MainWindow* win) {
    auto& st = win->editTools;
    EditSyncTab(win);
    DisplayModel* dm = win->AsFixed();
    EngineMupdf* em = dm ? AsEngineMupdf(dm->GetEngine()) : nullptr;
    if (!em || st.imageEdits.len == 0) {
        EditNotify(win, _TRA("No image edits to apply"));
        return;
    }
    int applied = 0;
    for (int i = 0; i < st.imageEdits.len; i++) {
        auto& ed = st.imageEdits[i];
        if (!ed.changed) {
            continue;
        }
        RenderedBitmap* rbmp = EditGetPreviewBmp(dm, ed);
        if (!rbmp) {
            logf("[edit] apply: could not extract image page %d idx %d\n", ed.pageNo, ed.imageIdx);
            continue;
        }
        Gdiplus::Bitmap src(rbmp->GetBitmap(), nullptr);
        int imgW = (int)src.GetWidth();
        int imgH = (int)src.GetHeight();
        if (imgW <= 0 || imgH <= 0 || ed.imageRect.dx <= 0 || ed.imageRect.dy <= 0 || ed.content.dx <= 0 ||
            ed.content.dy <= 0 || ed.bbox.dx <= 0 || ed.bbox.dy <= 0) {
            continue;
        }
        double sx = (double)imgW / ed.imageRect.dx;
        double sy = (double)imgH / ed.imageRect.dy;
        // content sub-rect in source pixels (what the user sees now)
        int csx = std::max(0, (int)((double)(ed.content.x - ed.imageRect.x) * sx + 0.5));
        int csy = std::max(0, (int)((double)(ed.content.y - ed.imageRect.y) * sy + 0.5));
        int csw = (int)((double)ed.content.dx * sx + 0.5);
        int csh = (int)((double)ed.content.dy * sy + 0.5);
        if (csx + csw > imgW) {
            csw = imgW - csx;
        }
        if (csy + csh > imgH) {
            csh = imgH - csy;
        }
        if (csw < 1 || csh < 1) {
            continue;
        }

        // full path: move the draw rect in the content stream (lets the image
        // grow past its original area — the tool's purpose), then swap the
        // data for exactly the content pixels (no padding, no scaling)
        if (em->SetImageDrawRect(ed.pageNo, ed.imageIdx, ed.bbox)) {
            Gdiplus::Bitmap out(csw, csh, PixelFormat32bppARGB);
            Gdiplus::Graphics g(&out);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            Gdiplus::RectF dest(0, 0, (float)csw, (float)csh);
            g.DrawImage(&src, dest, (float)csx, (float)csy, (float)csw, (float)csh, Gdiplus::UnitPixel);
            WCHAR tmpDir[MAX_PATH + 1]{};
            WCHAR tmpPath[MAX_PATH + 64]{};
            GetTempPathW(dimof(tmpDir), tmpDir);
            if (swprintf_s(tmpPath, L"%stumatrapdf-imgapply-%u-%d.png", tmpDir, GetCurrentProcessId(), i) >= 0 &&
                EditSaveArgbBitmapAsPng(&out, tmpPath)) {
                TempStr path8 = ToUtf8Temp(WStr(tmpPath));
                int r = em->ReplaceImageByIdx(ed.pageNo, ed.imageIdx, path8);
                logf("[edit] apply: page %d idx %d -> %s (draw rect moved)\n", ed.pageNo, ed.imageIdx,
                     r > 0 ? StrL("replaced") : StrL("FAILED"));
                DeleteFileW(tmpPath);
                if (r > 0) {
                    applied++;
                    ed.imageRect = ed.bbox;
                    ed.content = ed.bbox;
                    ed.changed = false;
                    delete ed.preview;
                    ed.preview = nullptr;
                    continue;
                }
                logf("[edit] apply: draw rect moved but image swap failed page %d idx %d — falling back\n", ed.pageNo,
                     ed.imageIdx);
            }
        }

        // fallback (rotated page / nested form / ambiguous ops): clamp the bbox
        // to the original footprint and compose the content onto a transparent
        // canvas with the ORIGINAL image size (page footprint unchanged)
        float fx0 = std::max(ed.bbox.x, ed.imageRect.x);
        float fy0 = std::max(ed.bbox.y, ed.imageRect.y);
        float fx1 = std::min(ed.bbox.x + ed.bbox.dx, ed.imageRect.x + ed.imageRect.dx);
        float fy1 = std::min(ed.bbox.y + ed.bbox.dy, ed.imageRect.y + ed.imageRect.dy);
        if (fx1 <= fx0 || fy1 <= fy0) {
            logf("[edit] apply: bbox outside footprint and draw rect locked page %d idx %d\n", ed.pageNo, ed.imageIdx);
            continue;
        }
        RectF fb{fx0, fy0, fx1 - fx0, fy1 - fy0};
        int dx = (int)((double)(fb.x - ed.imageRect.x) * sx + 0.5);
        int dy = (int)((double)(fb.y - ed.imageRect.y) * sy + 0.5);
        int dw = (int)((double)fb.dx * sx + 0.5);
        int dh = (int)((double)fb.dy * sy + 0.5);
        if (dw < 1 || dh < 1) {
            continue;
        }
        Gdiplus::Bitmap canvas(imgW, imgH, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&canvas);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        Gdiplus::RectF dest((float)dx, (float)dy, (float)dw, (float)dh);
        g.DrawImage(&src, dest, (float)csx, (float)csy, (float)csw, (float)csh, Gdiplus::UnitPixel);
        WCHAR tmpDir[MAX_PATH + 1]{};
        WCHAR tmpPath[MAX_PATH + 64]{};
        GetTempPathW(dimof(tmpDir), tmpDir);
        if (swprintf_s(tmpPath, L"%stumatrapdf-imgapply-%u-%d.png", tmpDir, GetCurrentProcessId(), i) < 0) {
            continue;
        }
        if (!EditSaveArgbBitmapAsPng(&canvas, tmpPath)) {
            logf("[edit] apply: could not write temp png\n");
            continue;
        }
        TempStr path8 = ToUtf8Temp(WStr(tmpPath));
        int r = em->ReplaceImageByIdx(ed.pageNo, ed.imageIdx, path8);
        logf("[edit] apply: page %d idx %d -> %s (clamped to footprint)\n", ed.pageNo, ed.imageIdx,
             r > 0 ? StrL("replaced") : StrL("FAILED"));
        DeleteFileW(tmpPath);
        if (r > 0) {
            applied++;
            // keep the selection on the applied result: content = the clamped
            // region so the bbox tracks the visible image across edits
            ed.content = fb;
            ed.changed = false;
            delete ed.preview;
            ed.preview = nullptr;
        }
    }
    if (applied > 0) {
        em->modifiedAnnotations = true;          // same red "unsaved changes" marker as a new highlight
        ToolbarUpdateStateForWindow(win, false); // lights the tab dot + Save marker
        gRenderCache->FreeForDisplayModel(dm);
        MainWindowRerender(win);
        EditNotify(win, _TRA("Image edits applied (save the PDF to keep them)"));
    } else {
        EditNotify(win, _TRA("No image edits to apply"));
    }
    logf("[edit] applied %d image edit(s); save the PDF (File > Save) to persist\n", applied);
}

static void HandleCmdEditImageCrop(MainWindow* win) {
    win->editTools.cropMode = !win->editTools.cropMode;
    HWND tb = win->editTools.hwndToolbarEdit;
    if (tb) {
        WORD state = TBSTATE_ENABLED | (win->editTools.cropMode ? TBSTATE_CHECKED : 0);
        SendMessageW(tb, TB_SETSTATE, CmdEditImageCrop, MAKELONG(state, 0));
    }
    logf("[edit] crop mode %s\n", win->editTools.cropMode ? StrL("ON") : StrL("OFF"));
    HwndInvalidate(win->hwndCanvas);
}

// select the image the context-menu Crop/Resize was clicked on (menu path)
void EditToolsSelectImageFromMenu(MainWindow* win, int pageNo, int imageIdx, RectF imageRect, bool cropMode) {
    if (!win->editTools.on) {
        HwndSendCommand(win->hwndFrame, CmdEditToolsToggle); // synchronous
    }
    if (!win->editTools.on) {
        return; // couldn't activate (e.g. markdown doc)
    }
    auto& st = win->editTools;
    if (cropMode && !st.cropMode) {
        HandleCmdEditImageCrop(win);
    }
    EditSyncTab(win);
    int found = -1;
    for (int i = 0; i < st.imageEdits.len; i++) {
        if (st.imageEdits[i].pageNo == pageNo && st.imageEdits[i].imageIdx == imageIdx) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        MainWindow::EditToolsState::PageImageEdit ed{};
        ed.pageNo = pageNo;
        ed.imageIdx = imageIdx;
        ed.imageRect = imageRect;
        ed.content = imageRect;
        ed.bbox = imageRect;
        ed.crop = cropMode;
        st.imageEdits.Append(ed);
        found = st.imageEdits.len - 1;
    }
    EditRememberTabPath(win);
    st.selEditIdx = found;
    logf("[edit] image selected from menu: page %d idx %d crop=%d\n", pageNo, imageIdx, (int)cropMode);
    HwndInvalidate(win->hwndCanvas);
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
        EditToolsClearImageEdits(win);
        win->editTools.cropMode = false;
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
        case CmdEditImageCrop:
            if (!win->editTools.on) {
                return true;
            }
            HandleCmdEditImageCrop(win);
            return true;
        case CmdEditImageApply:
            if (!win->editTools.on) {
                return true;
            }
            EditApplyImageEdits(win);
            return true;
        default:
            break;
    }
    return false;
}
