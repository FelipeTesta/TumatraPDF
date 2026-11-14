/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#include "base/Base.h"
#include "SumatraLog.h"
#include "Commands_ArchTools.h"

#include "Flashcard.h"
#include "Settings.h"
#include "GlobalPrefs.h"
#include "Commands.h"
#include "MainWindow.h"
#include "Toolbar.h"
#include "DocController.h"
#include "DocProperties.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "DisplayModel.h"

#include "Annotation.h"

#include "Notifications.h"

#include "Translations.h"

extern "C" {
#include <mupdf/pdf.h>
}
#include "EngineMupdf.h"

// Forward declarations — these are defined in other translation units
void ShowArchScaleDialog(MainWindow* win);

// Cheap relative ms timestamp for [arch] debug logs (ms since first call in this TU).
static u64 ArchElapsedMs() {
    static u64 base = 0;
    u64 now = (u64)GetTickCount64();
    if (base == 0) base = now;
    return now - base;
}

// Idempotency guard window for rapid double-dispatch of CmdArchAlign.
// A second toggle within this window, with no canvas interaction in between,
// is treated as a spurious re-post (e.g. Edit toolbar re-posting WM_COMMAND)
// and ignored so a single click leaves align ON. A legit fast OFF->ON within
// the window would be swallowed — acceptable tradeoff to kill the phantom
// 5->0 flip seen on real machines (gap can exceed 100ms).
constexpr u64 kAlignToggleGuardMs = 500;
void UpdateToolbar2State(MainWindow*);
void SetToolbarButtonCheckedState(MainWindow* win, int cmdId, bool isChecked);
bool RelayoutFrame(MainWindow* win, bool updateToolbars, int sidebarDx);
void ScheduleRepaint(MainWindow* win, int delay);
bool IsCurrentDocMarkdown(MainWindow* win);
// Discard any active arch-tools geometry preview (defined below).
static void DiscardArchPreview(MainWindow* win);

// HandleCmdArchScale — open floating scale dialog
void HandleCmdArchScale(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int oldMode = win->archTools.toolMode;
    int pageNo = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    auto& ps = win->archTools.GetScaleForPage(pageNo);
    LogInfo("[arch +%llums] CmdArchScale: page %d, opening scale dialog", (unsigned long long)ArchElapsedMs(), pageNo);
    if (!ps.scaleLineDefined) {
        win->archTools.toolMode = 1;
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_CROSS));
        }
    }
    // s64-R2: discard any uncommitted arch-tools preview (align or page crop) on mode switch
    DiscardArchPreview(win);
    ShowArchScaleDialog(win);
}

// HandleCmdArchMeasure — toggle measure mode
void HandleCmdArchMeasure(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int oldMode = win->archTools.toolMode;
    if (win->archTools.toolMode == 2) {
        LogInfo("[arch +%llums] CmdArchMeasure OFF toolMode=%d -> 0", (unsigned long long)ArchElapsedMs(),
                win->archTools.toolMode);
        win->archTools.toolMode = 0;
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
        }
    } else {
        win->archTools.toolMode = 2; // Measure mode
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_CROSS));
        }
    }
    // s64-R2: discard any uncommitted arch-tools preview on mode switch
    DiscardArchPreview(win);
    LogInfo("[arch +%llums] CmdArchMeasure: archToolMode=%d -> %d", (unsigned long long)ArchElapsedMs(), oldMode,
            win->archTools.toolMode);
    UpdateToolbar2State(win);
}

// HandleCmdArchMeasureRadius — toggle radius mode: 3 clicks (chord start,
// chord end, arc bulge point) draw an arc through the 3 points and show its
// radius in the current unit
void HandleCmdArchMeasureRadius(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int oldMode = win->archTools.toolMode;
    win->archTools.toolMode = (win->archTools.toolMode == 3) ? 0 : 3;
    win->archTools.dragLine = 0;
    if (win->hwndCanvas) {
        SetCursor(LoadCursor(nullptr, win->archTools.toolMode ? IDC_CROSS : IDC_ARROW));
    }
    // s64-R2: discard any uncommitted arch-tools preview on mode switch
    DiscardArchPreview(win);
    LogInfo("[arch +%llums] CmdArchMeasureRadius: archToolMode=%d -> %d", (unsigned long long)ArchElapsedMs(), oldMode,
            win->archTools.toolMode);
    UpdateToolbar2State(win);
}

// HandleCmdArchRefLine — toggle reference-line mode: 2 clicks place a subtle
// continuous guide line that spans the page through both points
void HandleCmdArchRefLine(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int oldMode = win->archTools.toolMode;
    win->archTools.toolMode = (win->archTools.toolMode == 4) ? 0 : 4;
    win->archTools.dragLine = 0;
    if (win->hwndCanvas) {
        SetCursor(LoadCursor(nullptr, win->archTools.toolMode ? IDC_CROSS : IDC_ARROW));
    }
    // s64-R2: discard any uncommitted arch-tools preview on mode switch
    DiscardArchPreview(win);
    LogInfo("[arch +%llums] CmdArchRefLine: archToolMode=%d -> %d", (unsigned long long)ArchElapsedMs(), oldMode,
            win->archTools.toolMode);
    UpdateToolbar2State(win);
}

// HandleCmdArchAlign — toggle align mode (toolMode 5): user draws a 2-point
// line; the tool decides the nearer axis (Shift snaps H/V), computes the
// rotation to make the line axis-aligned and the page crop rect (mediabox
// rotated so the line is axis-aligned, mediabox W:H preserved). Ephemeral:
// not persisted, cleared on toggle-off / ESC / doc close.
void HandleCmdArchAlign(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int oldMode = win->archTools.toolMode;
    LogInfo("[arch +%llums] CmdArchAlign ENTRY toolMode=%d", (unsigned long long)ArchElapsedMs(), oldMode);

    // Idempotency guard: a second CmdArchAlign within kAlignToggleGuardMs of the first,
    // with no canvas interaction in between, is a rapid double-dispatch
    // (e.g. the Edit toolbar check button re-posting WM_COMMAND). Ignore it
    // so a single click leaves align ON (toolMode stays 5) instead of
    // instantly flipping 5 -> 0.
    u64 nowMs = (u64)GetTickCount64();
    if (oldMode == 5 && nowMs - win->archTools.alignLastToggleMs < kAlignToggleGuardMs &&
        win->archTools.alignCanvasInteractMs <= win->archTools.alignLastToggleMs) {
        LogInfo("[arch +%llums] CmdArchAlign: rapid duplicate dispatch ignored (toolMode stays 5)",
                (unsigned long long)ArchElapsedMs());
        // Resync the Edit toolbar checked state so the button stays ON visually.
        ToolbarUpdateStateForWindow(win, false);
        return;
    }

    // s64-R2: if switching away from align mode (5 -> 0), discard any uncommitted preview.
    // Note: toolMode is still `oldMode` here (toggle happens below), so checking
    // `win->archTools.toolMode != 5` would never be true. Rely on oldMode only.
    if (oldMode == 5) {
        DiscardArchPreview(win);
    }

    win->archTools.toolMode = (win->archTools.toolMode == 5) ? 0 : 5;
    win->archTools.dragLine = 0;
    win->archTools.alignLastToggleMs = nowMs;
    win->archTools.alignCanvasInteractMs = 0; // new toggle cycle: no canvas interaction yet
    if (win->archTools.toolMode == 0) {
        win->archTools.alignLine = MainWindow::AlignLine{};
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
        }
    } else {
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_CROSS));
        }
    }
    LogInfo("[arch +%llums] CmdArchAlign EXIT toolMode=%d (was=%d)", (unsigned long long)ArchElapsedMs(),
            win->archTools.toolMode, oldMode);
    UpdateToolbar2State(win);
    // Crop Align button is on Edit toolbar — sync its checked state via
    // ToolbarUpdateStateForWindow (UpdateToolbar2State only syncs Arch toolbar)
    ToolbarUpdateStateForWindow(win, false);
}

// HandleCmdArchAlignApply — commit the current arch-tools preview for save (s64-R2).
// For align (toolMode 5): promotes the in-memory preview (already created by
// FinalizeAlign) to pendingGeomEdits. For page crop (toolMode 6): first turns the
// live bounding box into a preview via ApplyPreviewGeometry(pageNo, 0, cropBboxPage),
// then promotes it — so the Edit-bar Apply button works for both arch tools.
// Enabled only when a preview/box is active.
void HandleCmdArchAlignApply(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    DisplayModel* dm = win->AsFixed();
    if (!dm) return;
    EngineMupdf* em = AsEngineMupdf(dm->GetEngine());
    if (!em) return;

    int pageNo = dm->CurrentPageNo();
    if (!dm->ValidPageNo(pageNo)) return;

    // Page crop (toolMode 6): create the preview from the live bbox, then commit.
    if (win->archTools.toolMode == 6 && win->archTools.cropBboxDefined) {
        bool rotatedCrop = (win->archTools.cropAngle != 0.0f);
        if (em->ApplyPreviewGeometry(pageNo, win->archTools.cropAngle, win->archTools.cropBboxPage, dm, rotatedCrop)) {
            if (em->PromotePreviewToPending(pageNo)) {
                em->hasPreviewGeomEdit = false;
                em->modifiedPageGeometry = true;
                em->previewGeomEdit = EngineMupdf::PageGeomEdit{};
                win->archTools.cropBboxPage = RectF{};
                win->archTools.cropBboxDefined = false;
                win->archTools.cropDragState = 0;
                win->archTools.cropAngle = 0.0f; // angle consumed by the apply
                win->archTools.cropRotating = false;
                win->archTools.toolMode = 0; // box consumed: the page IS the result
                if (win->hwndCanvas) {
                    SetCursor(LoadCursor(nullptr, IDC_ARROW));
                }
                LogInfo("[arch +%llums] CmdArchAlignApply: crop bbox applied + promoted for page %d",
                        (unsigned long long)ArchElapsedMs(), pageNo);
                ToolbarUpdateStateForWindow(win, false);
            }
        } else {
            // Page has engine annotations — reject silently to avoid annotation coordinate drift.
            // Show UI notification.
            if (pageNo >= 1 && pageNo <= em->pageCount) {
                FzPageInfo* pageInfo = em->pages[pageNo - 1];
                if (pageInfo && pageInfo->annotsLoaded) {
                    bool hasAnnot = false;
                    for (auto* annot : pageInfo->annotations) {
                        if (annot && annot->pdfannot) {
                            hasAnnot = true;
                            break;
                        }
                    }
                    for (auto* widget : pageInfo->widgets) {
                        if (widget && widget->pdfannot) {
                            hasAnnot = true;
                            break;
                        }
                    }
                    if (hasAnnot) {
                        NotificationCreateArgs nargs;
                        nargs.hwndParent = win->hwndCanvas;
                        nargs.groupId = kNotifActionResponse;
                        nargs.warning = true;
                        nargs.timeoutMs = 5000;
                        nargs.delayInMs = 500;
                        nargs.noClose = true;
                        Str fmtStr = _TRA("Crop Align: page has annotations and cannot be realigned");
                        nargs.msg = fmt(fmtStr.s);
                        ShowNotification(nargs);
                    }
                }
            }
        }
        return;
    }

    // Align (toolMode 5): promote the preview FinalizeAlign already created.
    if (em->hasPreviewGeomEdit && em->previewGeomEdit.pageNo == pageNo) {
        if (em->PromotePreviewToPending(pageNo)) {
            em->hasPreviewGeomEdit = false;
            em->modifiedPageGeometry = true;
            em->previewGeomEdit = EngineMupdf::PageGeomEdit{};
            LogInfo("[arch +%llums] CmdArchAlignApply: promoted preview to pending for page %d",
                    (unsigned long long)ArchElapsedMs(), pageNo);
            ToolbarUpdateStateForWindow(win, false);
        }
    }
}

// Discard any active arch-tools geometry preview (align or page crop) and clear
// the ephemeral overlay state so a mode switch leaves a pristine canvas. Safe to
// call when nothing is active (no-op).
static void DiscardArchPreview(MainWindow* win) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        win->archTools.alignLine = MainWindow::AlignLine{};
        win->archTools.cropBboxPage = RectF{};
        win->archTools.cropBboxDefined = false;
        win->archTools.cropDragState = 0;
        win->archTools.cropAngle = 0.0f;
        win->archTools.cropRotating = false;
        return;
    }
    EngineMupdf* em = AsEngineMupdf(dm->GetEngine());
    if (em && em->hasPreviewGeomEdit) {
        int pageNo = dm->CurrentPageNo();
        if (dm->ValidPageNo(pageNo)) {
            em->DiscardPreview(pageNo, dm);
            LogInfo("[arch +%llums] DiscardArchPreview: reverted preview for page %d",
                    (unsigned long long)ArchElapsedMs(), pageNo);
        }
    }
    win->archTools.alignLine = MainWindow::AlignLine{};
    win->archTools.cropBboxPage = RectF{};
    win->archTools.cropBboxDefined = false;
    win->archTools.cropDragState = 0;
    win->archTools.cropResizeHandle = 0;
    win->archTools.cropAngle = 0.0f;
    win->archTools.cropRotating = false;
}

// HandleCmdArchPageCrop — toggle page-crop mode (toolMode 6): image-editor-style
// bounding box over the current page. On activation the box is the full page
// mediabox (fitz page space, y-down). Drag the 8 handles or the interior to
// resize/move; Apply commits via HandleCmdArchAlignApply; tool switch / ESC
// discards back to pristine.
void HandleCmdArchPageCrop(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int oldMode = win->archTools.toolMode;
    LogInfo("[arch +%llums] CmdArchPageCrop ENTRY toolMode=%d", (unsigned long long)ArchElapsedMs(), oldMode);

    // Discard any other active preview when switching into/out of crop mode.
    if (oldMode != 6) {
        DiscardArchPreview(win);
    }

    win->archTools.toolMode = (win->archTools.toolMode == 6) ? 0 : 6;
    win->archTools.cropDragState = 0;
    win->archTools.cropResizeHandle = 0;
    if (win->archTools.toolMode == 0) {
        // Turning off: revert any uncommitted crop preview, clear box, arrow cursor.
        DiscardArchPreview(win);
        win->archTools.on = false; // leave arch tools as user found them
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
        }
    } else {
        // Turning on: overlay = full page bounds (fitz page space, y-down).
        // NOTE: do NOT set archTools.on — page crop lives on the Edit bar which
        // shares its toolbar row slot with Arch Tools; they're mutually exclusive.
        // The canvas paint block is gated to also run for toolMode==6 (see OnPaintDocument).
        DisplayModel* dm = win->AsFixed();
        if (dm && dm->GetEngine() && dm->ValidPageNo(dm->CurrentPageNo())) {
            RectF mb = dm->GetEngine()->PageMediabox(dm->CurrentPageNo());
            if (mb.dx > 0.0f && mb.dy > 0.0f) {
                win->archTools.cropBboxPage = mb;
                win->archTools.cropBboxDefined = true;
            }
        }
        if (win->hwndCanvas) {
            SetCursor(LoadCursor(nullptr, IDC_CROSS));
        }
    }
    LogInfo("[arch +%llums] CmdArchPageCrop EXIT toolMode=%d (was=%d)", (unsigned long long)ArchElapsedMs(),
            win->archTools.toolMode, oldMode);
    UpdateToolbar2State(win);
    ToolbarUpdateStateForWindow(win, false);
}

// HandleCmdArchClear — clear current page's measurements AND its scale line
// (the green line + calibration persist otherwise and "Clean lines" would
// appear to do nothing about the most visible line on the page)
void HandleCmdArchClear(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int pageNo = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    int count = 0;
    Vec<ArchMeasurement> remaining;
    for (int i = 0; i < len(win->archTools.measurements); i++) {
        if (win->archTools.measurements[i].pageNo == pageNo) {
            count++;
        } else {
            remaining.Append(win->archTools.measurements[i]);
        }
    }
    win->archTools.measurements = remaining;
    auto& ps = win->archTools.GetScaleForPage(pageNo);
    bool hadScale = ps.scaleSet || ps.scaleLineDefined;
    ps.scaleSet = false;
    ps.scaleLineDefined = false;
    ps.scaleFactor = 0.0f;
    ps.scaleLineP1x = ps.scaleLineP1y = 0.0f;
    ps.scaleLineP2x = ps.scaleLineP2y = 0.0f;
    LogInfo("[arch +%llums] CmdArchClear: cleared %d measurements (scale reset: %d) on page %d",
            (unsigned long long)ArchElapsedMs(), count, hadScale ? 1 : 0, pageNo);
    ScheduleRepaint(win, 0);
}

// HandleCmdArchResetScale — reset only current page's scale
void HandleCmdArchResetScale(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int pageNo = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    auto& ps = win->archTools.GetScaleForPage(pageNo);
    ps.scaleSet = false;
    ps.scaleLineDefined = false;
    ps.scaleFactor = 0.0f;
    ps.scaleLineP1x = ps.scaleLineP1y = 0.0f;
    ps.scaleLineP2x = ps.scaleLineP2y = 0.0f;
    LogInfo("[arch +%llums] CmdArchResetScale: reset scale for page %d only", (unsigned long long)ArchElapsedMs(),
            pageNo);
    ScheduleRepaint(win, 0);
}

// HandleCmdArchToolsToggle — toggle arch tools on/off
void HandleCmdArchToolsToggle(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    if (IsCurrentDocMarkdown(win)) return;
    win->archTools.on = !win->archTools.on;
    if (win->archTools.on) {
        if (win->archTools.hwndReBar2) {
            ShowWindow(win->archTools.hwndReBar2, SW_SHOW);
        }
        // flashcards stay as they are: the toolbars are mutually exclusive
        // through DISABLED buttons / blocked commands (user request -
        // "inativar" beats auto-closing the other toolbar)
        // Stop the conflicting features (user request: arch tools and
        // two-columns/scan/autoscroll/trim are mutually exclusive). Post their
        // toggles: they run through FrameOnCommand, whose arch gate lets an
        // OFF through and blocks anything that would turn ON.
        auto* dmOn = win->AsFixed();
        if (win->autoScroll.active) {
            PostMessageW(win->hwndFrame, WM_COMMAND, CmdAutoScrollToggle, 0);
        }
        if (gGlobalPrefs->viewportCrop.enabled) {
            PostMessageW(win->hwndFrame, WM_COMMAND, CmdViewportCropToggle, 0);
        }
        if (dmOn && dmOn->viewportCropV2Enabled) {
            PostMessageW(win->hwndFrame, WM_COMMAND, CmdViewportCropV2Toggle, 0);
        }
        if (dmOn && dmOn->marginTrimEnabled) {
            PostMessageW(win->hwndFrame, WM_COMMAND, CmdMarginTrimToggle, 0);
        }
        UpdateToolbar2State(win);
    } else {
        if (win->archTools.hwndReBar2) {
            ShowWindow(win->archTools.hwndReBar2, SW_HIDE);
        }
        LogInfo("[arch +%llums] CmdArchToolsToggle OFF toolMode=%d -> 0", (unsigned long long)ArchElapsedMs(),
                win->archTools.toolMode);
        win->archTools.toolMode = 0;
        win->archTools.eraseMode = false;
        win->archTools.dragLine = 0;
        win->archTools.point1 = PointF{0, 0};
        win->archTools.point2 = PointF{0, 0};
        if (GetCapture() == win->hwndCanvas) {
            ReleaseCapture();
        }
        win->mouseAction = MouseAction::None;
    }
    // Explicitly set checked state for the toggle button (fixes underline drawing)
    SetToolbarButtonCheckedState(win, CmdArchToolsToggle, win->archTools.on);
    LogInfo("[arch +%llums] CmdArchToolsToggle: archToolsOn=%d", (unsigned long long)ArchElapsedMs(),
            win->archTools.on);
    // refresh button enabled states: the conflicting buttons gray out while
    // arch tools are on and come back when it's off (IsCmdEnabled)
    ToolbarUpdateStateForWindow(win, false);
    RelayoutFrame(win, true, -1);
    ScheduleRepaint(win, 0);
}

// HandleCmdArchCleanAll — clear ALL pages
void HandleCmdArchCleanAll(MainWindow* win) {
    if (!gGlobalPrefs->archToolsEnabled) return;
    int count = (int)len(win->archTools.measurements);
    win->archTools.measurements.Reset();
    win->archTools.scaleStates.Reset();
    win->archTools.toolMode = 0;
    if (win->hwndCanvas) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    LogInfo("[arch +%llums] CmdArchCleanAll: cleared %d measurements + all page scales",
            (unsigned long long)ArchElapsedMs(), count);
    UpdateToolbar2State(win);
    ScheduleRepaint(win, 0);
}