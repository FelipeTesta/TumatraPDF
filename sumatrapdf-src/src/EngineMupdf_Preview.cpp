/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

extern "C" {
#include <mupdf/pdf.h>
}

#include "Annotation.h"
#include "DocProperties.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineMupdf.h"
#include "Settings.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "SumatraPDF.h"

// Apply a preview geometry edit in-memory for visual feedback (s64-R2).
// Does NOT add to pendingGeomEdits — this is for preview only.
// Stores the edit in previewGeomEdit for later promote/discard.
bool EngineMupdf::ApplyPreviewGeometry(int pageNo, float angleDeg, RectF cropRectPage, DisplayModel* dm) {
    if (!pdfdoc || pageNo < 1 || pageNo > pageCount) {
        return false;
    }
    fz_context* ctx = Ctx();
    bool ok = false;

    // Capture pristine state at FIRST preview apply for this page.
    PagePristineState* pristine = nullptr;
    for (PagePristineState& ps : pristinePageStates) {
        if (ps.pageNo == pageNo) {
            pristine = &ps;
            break;
        }
    }
    if (!pristine) {
        PagePristineState ps;
        // We need to load the page to capture pristine state
        FzPageInfo* pageInfo = pages[pageNo - 1];
        if (pageInfo) {
            if (!pageInfo->page) {
                fz_try(ctx) {
                    pageInfo->page = fz_load_page(ctx, _doc, pageNo - 1);
                }
                fz_catch(ctx) {
                    fz_report_error(ctx);
                }
            }
            if (pageInfo->page) {
                pdf_page* pdfpage = pdf_page_from_fz_page(ctx, pageInfo->page);
                if (pdfpage) {
                    pdf_obj* pageObj = pdfpage->obj;
                    if (pageObj) {
                        ps.pageNo = pageNo;
                        ps.pageDict = pageObj;
                        pdf_keep_obj(ctx, ps.pageDict);
                        ps.contents = pdf_page_contents(ctx, pdfpage);
                        if (ps.contents) pdf_keep_obj(ctx, ps.contents);
                        ps.resources = pdf_page_resources(ctx, pdfpage);
                        if (ps.resources) pdf_keep_obj(ctx, ps.resources);
                        ps.mediaBox = pdf_bound_page(ctx, pdfpage, FZ_MEDIA_BOX);
                        ps.cropBox = pdf_bound_page(ctx, pdfpage, FZ_CROP_BOX);
                        pdf_obj* rotateObj = pdf_dict_gets(ctx, pageObj, "Rotate");
                        if (rotateObj && pdf_is_int(ctx, rotateObj)) {
                            ps.rotate = pdf_to_int(ctx, rotateObj);
                        } else {
                            ps.rotate = 0;
                        }
                        ps.hasState = true;
                        pristinePageStates.Append(ps);
                        pristine = &pristinePageStates[len(pristinePageStates) - 1];
                    }
                }
            }
        }
    }

    // Apply the geometry edit to the live PDF (same logic as ApplyPendingGeometryEditsNow)
    // Variables needed after the fz_try block:
    bool is90Multiple = false;
    fz_rect zeroOriginBox = fz_make_rect(0, 0, 0, 0);
    {
        ScopedRecursiveMutex scope(&docLock);
        fz_try(ctx) {
            FzPageInfo* pageInfo = pages[pageNo - 1];
            if (!pageInfo) {
                ok = false;
            } else {
                if (!pageInfo->page) {
                    fz_try(ctx) {
                        pageInfo->page = fz_load_page(ctx, _doc, pageNo - 1);
                    }
                    fz_catch(ctx) {
                        fz_report_error(ctx);
                    }
                }
                if (!pageInfo->page) {
                    ok = false;
                } else {
                    pdf_page* pdfpage = pdf_page_from_fz_page(ctx, pageInfo->page);
                    if (!pdfpage) {
                        ok = false;
                    } else {
                        pdf_obj* pageObj = pdfpage->obj;
                        if (!pageObj) {
                            ok = false;
                        } else {
                            // Get current rotation from /Rotate key (default 0)
                            int currentRotate = 0;
                            pdf_obj* rotateObj = pdf_dict_gets(ctx, pageObj, "Rotate");
                            if (rotateObj && pdf_is_int(ctx, rotateObj)) {
                                currentRotate = pdf_to_int(ctx, rotateObj);
                            }

                            float rem = fmodf(fabsf(angleDeg), 90.0f);
                            is90Multiple = (rem < 0.01f || rem > 89.99f);

                            if (is90Multiple) {
                                // R2 path
                                int rotateDelta = (int)lroundf(angleDeg / 90.0f) * 90;
                                int newRotate = (currentRotate + rotateDelta) % 360;
                                if (newRotate < 0) newRotate += 360;
                                pdf_dict_put_int(ctx, pageObj, PDF_NAME(Rotate), newRotate);

                                fz_rect cropBox = ToFzRect(cropRectPage);
                                if (rotateDelta == 90 || rotateDelta == -270) {
                                    fz_rect mb = pdf_bound_page(ctx, pdfpage, FZ_MEDIA_BOX);
                                    float cx = (mb.x0 + mb.x1) / 2;
                                    float cy = (mb.y0 + mb.y1) / 2;
                                    float x = cropBox.x0 - cx;
                                    float y = cropBox.y0 - cy;
                                    float w = cropBox.x1 - cropBox.x0;
                                    float h = cropBox.y1 - cropBox.y0;
                                    cropBox.x0 = cx - y - h;
                                    cropBox.y0 = cy + x;
                                    cropBox.x1 = cropBox.x0 + h;
                                    cropBox.y1 = cropBox.y0 + w;
                                } else if (rotateDelta == 180 || rotateDelta == -180) {
                                    fz_rect mb = pdf_bound_page(ctx, pdfpage, FZ_MEDIA_BOX);
                                    float cx = (mb.x0 + mb.x1) / 2;
                                    float cy = (mb.y0 + mb.y1) / 2;
                                    float x = cropBox.x0 - cx;
                                    float y = cropBox.y0 - cy;
                                    float w = cropBox.x1 - cropBox.x0;
                                    float h = cropBox.y1 - cropBox.y0;
                                    cropBox.x0 = cx - x - w;
                                    cropBox.y0 = cy - y - h;
                                    cropBox.x1 = cropBox.x0 + w;
                                    cropBox.y1 = cropBox.y0 + h;
                                } else if (rotateDelta == 270 || rotateDelta == -90) {
                                    fz_rect mb = pdf_bound_page(ctx, pdfpage, FZ_MEDIA_BOX);
                                    float cx = (mb.x0 + mb.x1) / 2;
                                    float cy = (mb.y0 + mb.y1) / 2;
                                    float x = cropBox.x0 - cx;
                                    float y = cropBox.y0 - cy;
                                    float w = cropBox.x1 - cropBox.x0;
                                    float h = cropBox.y1 - cropBox.y0;
                                    cropBox.x0 = cx + y;
                                    cropBox.y0 = cy - x - w;
                                    cropBox.x1 = cropBox.x0 + h;
                                    cropBox.y1 = cropBox.y0 + w;
                                }
                                pdf_set_page_box(ctx, pdfpage, FZ_CROP_BOX, cropBox);
                            } else {
                                // R3 path - arbitrary angle via Form XObject
                                pdf_obj* boxObj = pdf_dict_get_inheritable(ctx, pageObj, PDF_NAME(CropBox));
                                if (!boxObj || !pdf_is_array(ctx, boxObj)) {
                                    boxObj = pdf_dict_get_inheritable(ctx, pageObj, PDF_NAME(MediaBox));
                                }
                                fz_rect cbPdf = boxObj ? pdf_to_rect(ctx, boxObj) : fz_make_rect(0, 0, 612, 792);
                                float cbX0 = fz_min(cbPdf.x0, cbPdf.x1);
                                float cbY1 = fz_max(cbPdf.y0, cbPdf.y1);

                                fz_rect cropRect = ToFzRect(cropRectPage);
                                float cropX0 = cropRect.x0;
                                float cropY0 = cropRect.y0;
                                float cropW = cropRect.x1 - cropRect.x0;
                                float cropH = cropRect.y1 - cropRect.y0;
                                float cropY1 = cropRect.y1;

                                float x0_pdf = cbX0 + cropX0;
                                float y0_pdf = cbY1 - cropY1;
                                float Cx_pdf = cbX0 + (cropX0 + cropRect.x1) / 2;
                                float Cy_pdf = cbY1 - (cropY0 + cropY1) / 2;

                                float cx = cropX0 + cropW / 2;
                                float cy = cropY0 + cropH / 2;
                                float rad = angleDeg * (float)M_PI / 180.0f;
                                float cosA = cosf(rad);
                                float sinA = sinf(rad);

                                auto rotPt = [&](float x, float y) -> fz_point {
                                    float dx = x - cx;
                                    float dy = y - cy;
                                    return fz_make_point(cx + cosA * dx - sinA * dy, cy + sinA * dx + cosA * dy);
                                };
                                fz_point c0 = rotPt(cropX0, cropY0);
                                fz_point c1 = rotPt(cropX0 + cropW, cropY0);
                                fz_point c2 = rotPt(cropX0 + cropW, cropY0 + cropH);
                                fz_point c3 = rotPt(cropX0, cropY0 + cropH);

                                float rx0 = fz_min(fz_min(c0.x, c1.x), fz_min(c2.x, c3.x));
                                float ry0 = fz_min(fz_min(c0.y, c1.y), fz_min(c2.y, c3.y));
                                float rx1 = fz_max(fz_max(c0.x, c1.x), fz_max(c2.x, c3.x));
                                float ry1 = fz_max(fz_max(c0.y, c1.y), fz_max(c2.y, c3.y));
                                fz_rect rotatedBBox = fz_make_rect(rx0, ry0, rx1, ry1);

                                zeroOriginBox = fz_make_rect(0, 0, rotatedBBox.x1 - rotatedBBox.x0,
                                                             rotatedBBox.y1 - rotatedBBox.y0);

                                str::Builder xobjContent;
                                xobjContent.Append("q\n");
                                xobjContent.Append(fmt("1 0 0 1 %g %g cm\n", -x0_pdf, -y0_pdf));

                                pdf_obj* origContents = pdf_page_contents(ctx, pdfpage);
                                if (origContents) {
                                    pdf_keep_obj(ctx, origContents);
                                    fz_buffer* buf = nullptr;
                                    fz_stream* cstm = pdf_open_contents_stream(ctx, pdfdoc, origContents);
                                    if (cstm) {
                                        buf = fz_read_all(ctx, cstm, 1024);
                                        fz_drop_stream(ctx, cstm);
                                    }
                                    if (buf) {
                                        unsigned char* data = nullptr;
                                        size_t len = fz_buffer_storage(ctx, buf, &data);
                                        if (data && len > 0) {
                                            xobjContent.Append(Str((const char*)data, (int)len));
                                        }
                                        fz_drop_buffer(ctx, buf);
                                    }
                                    pdf_drop_obj(ctx, origContents);
                                }

                                xobjContent.Append("\nQ\n");

                                Str xobjStr = xobjContent.TakeStr();
                                fz_buffer* xobjBuf =
                                    fz_new_buffer_from_copied_data(ctx, (const unsigned char*)xobjStr.s, xobjStr.len);

                                fz_rect xobjBBox = fz_make_rect(0, 0, cropW, cropH);

                                fz_matrix matrix;
                                matrix.a = cosA;
                                matrix.b = sinA;
                                matrix.c = -sinA;
                                matrix.d = cosA;
                                float dpx = x0_pdf - Cx_pdf;
                                float dpy = y0_pdf - Cy_pdf;
                                matrix.e = zeroOriginBox.x1 / 2 + cosA * dpx - sinA * dpy;
                                matrix.f = zeroOriginBox.y1 / 2 + sinA * dpx + cosA * dpy;

                                pdf_obj* pageRes = pdf_page_resources(ctx, pdfpage);
                                if (pageRes) pdf_keep_obj(ctx, pageRes);

                                pdf_obj* xobj = pdf_new_xobject(ctx, pdfdoc, xobjBBox, matrix, pageRes, xobjBuf);
                                fz_drop_buffer(ctx, xobjBuf);
                                if (pageRes) pdf_drop_obj(ctx, pageRes);
                                if (!xobj) {
                                    ok = false;
                                } else {
                                    static int xobjCounter = 0;
                                    char xobjName[32];
                                    snprintf(xobjName, sizeof(xobjName), "Fm%d", xobjCounter++);

                                    str::Builder pageContent;
                                    pageContent.Append("q\n");
                                    pageContent.Append(fmt("/%s Do\n", Str(xobjName)));
                                    pageContent.Append("Q\n");

                                    Str pageContentStr = pageContent.TakeStr();
                                    fz_buffer* pageContentBuf = fz_new_buffer_from_copied_data(
                                        ctx, (const unsigned char*)pageContentStr.s, pageContentStr.len);

                                    pdf_obj* newContents =
                                        pdf_add_object_drop(ctx, pdfdoc, pdf_new_dict(ctx, pdfdoc, 1));
                                    pdf_update_stream(ctx, pdfdoc, newContents, pageContentBuf, 0);
                                    fz_drop_buffer(ctx, pageContentBuf);

                                    pdf_obj* newRes = pdf_new_dict(ctx, pdfdoc, 2);
                                    if (pageRes) {
                                        pageRes = pdf_page_resources(ctx, pdfpage);
                                        if (pageRes) pdf_keep_obj(ctx, pageRes);
                                    }
                                    if (pageRes) {
                                        int n = pdf_dict_len(ctx, pageRes);
                                        for (int i = 0; i < n; i++) {
                                            pdf_obj* key = pdf_dict_get_key(ctx, pageRes, i);
                                            pdf_obj* val = pdf_dict_get_val(ctx, pageRes, i);
                                            pdf_keep_obj(ctx, key);
                                            pdf_keep_obj(ctx, val);
                                            pdf_dict_put(ctx, newRes, key, val);
                                            pdf_drop_obj(ctx, key);
                                            pdf_drop_obj(ctx, val);
                                        }
                                        pdf_drop_obj(ctx, pageRes);
                                    }
                                    pdf_obj* xobjDict = pdf_new_dict(ctx, pdfdoc, 1);
                                    pdf_obj* nameObj = pdf_new_name(ctx, xobjName);
                                    pdf_keep_obj(ctx, xobj);
                                    pdf_dict_put(ctx, xobjDict, nameObj, xobj);
                                    pdf_drop_obj(ctx, nameObj);
                                    pdf_drop_obj(ctx, xobj);
                                    pdf_obj* xobjKey = pdf_new_name(ctx, "XObject");
                                    pdf_dict_put(ctx, newRes, xobjKey, xobjDict);
                                    pdf_drop_obj(ctx, xobjKey);
                                    pdf_drop_obj(ctx, xobjDict);

                                    pdf_dict_put_drop(ctx, pageObj, PDF_NAME(Contents), newContents);
                                    pdf_dict_put_drop(ctx, pageObj, PDF_NAME(Resources), newRes);
                                    pdf_dict_put_rect(ctx, pageObj, PDF_NAME(MediaBox), zeroOriginBox);
                                    pdf_dict_put_rect(ctx, pageObj, PDF_NAME(CropBox), zeroOriginBox);
                                    ok = true;
                                }
                            }
                        }
                    }
                }
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            ok = false;
        }
    }

    if (!ok) {
        return false;
    }

    // Invalidate caches (same as ApplyPendingGeometryEditsNow pass 2)
    {
        ScopedRecursiveMutex ps(&pagesLock);
        ScopedMutex rl(&renderLock);
        FzPageInfo* pageInfo = pages[pageNo - 1];
        if (pageInfo) {
            if (!is90Multiple) { // R3 changes mediabox
                pageInfo->mediabox = ToRectF(zeroOriginBox);
            }
            if (pageInfo->page) {
                fz_drop_page(ctx, pageInfo->page);
                pageInfo->page = nullptr;
            }
            if (pageInfo->displayList) {
                fz_drop_display_list(ctx, pageInfo->displayList);
                pageInfo->displayList = nullptr;
            }
            pageInfo->elementsNeedRebuilding = true;
        }
    }

    // Store as preview (not in pendingGeomEdits)
    previewGeomEdit.pageNo = pageNo;
    previewGeomEdit.angleDeg = angleDeg;
    previewGeomEdit.cropRectPage = cropRectPage;
    // For R3, newMediabox is set; for R2, it's empty
    if (!is90Multiple) {
        previewGeomEdit.newMediabox = ToRectF(zeroOriginBox);
    } else {
        previewGeomEdit.newMediabox = RectF();
    }
    hasPreviewGeomEdit = true;
    appliedInMemory = true; // live doc has been modified

    // Invalidate render cache and relayout
    if (dm && gRenderCache) {
        gRenderCache->FreeForDisplayModel(dm);
    }
    if (dm) {
        dm->BuildPagesInfo();
        dm->RelayoutKeepingView();
    }

    return true;
}

// Promote the preview edit to pendingGeomEdits (commit for save) (s64-R2).
bool EngineMupdf::PromotePreviewToPending(int pageNo) {
    if (!hasPreviewGeomEdit || previewGeomEdit.pageNo != pageNo) {
        return false;
    }
    // Add to pendingGeomEdits (replacing any existing entry for this page)
    for (PageGeomEdit& e : pendingGeomEdits) {
        if (e.pageNo == pageNo) {
            e = previewGeomEdit;
            modifiedPageGeometry = true;
            return true;
        }
    }
    pendingGeomEdits.Append(previewGeomEdit);
    modifiedPageGeometry = true;
    return true;
}

// Discard the preview edit and restore pristine geometry (s64-R2).
bool EngineMupdf::DiscardPreview(int pageNo, DisplayModel* dm) {
    if (!hasPreviewGeomEdit || previewGeomEdit.pageNo != pageNo) {
        return false;
    }
    // Restore pristine state (same as RestorePageGeometryForAlign)
    bool ok = RestorePageGeometry(pageNo, dm);
    if (ok) {
        hasPreviewGeomEdit = false;
        previewGeomEdit = PageGeomEdit{};
    }
    return ok;
}

// Restore preview if it exists and is not committed (called before save) (s64-R2).
bool EngineMupdf::RestorePreviewIfNeeded(DisplayModel* dm) {
    if (hasPreviewGeomEdit) {
        // Check if this preview is already in pendingGeomEdits (promoted)
        bool inPending = false;
        for (const PageGeomEdit& e : pendingGeomEdits) {
            if (e.pageNo == previewGeomEdit.pageNo && e.angleDeg == previewGeomEdit.angleDeg) {
                inPending = true;
                break;
            }
        }
        if (!inPending) {
            // Preview exists but not committed — discard it before save
            DiscardPreview(previewGeomEdit.pageNo, dm);
        }
    }
    return true;
}

// Helper: restore page geometry to pristine state (s64: RestorePageGeometry).
// Moved from Canvas.cpp to EngineMupdf for reuse.
bool EngineMupdf::RestorePageGeometry(int pageNo, DisplayModel* dm) {
    if (!pdfdoc || pageNo < 1 || pageNo > pageCount) {
        return false;
    }
    fz_context* ctx = Ctx();
    bool ok = false;

    // Find pristine state for this page
    PagePristineState* pristine = nullptr;
    for (PagePristineState& ps : pristinePageStates) {
        if (ps.pageNo == pageNo) {
            pristine = &ps;
            break;
        }
    }
    if (!pristine || !pristine->hasState) {
        return false; // No pristine state captured
    }

    // Pass 1: restore PDF objects under docLock
    {
        ScopedRecursiveMutex scope(&docLock);
        fz_try(ctx) {
            FzPageInfo* pageInfo = pages[pageNo - 1];
            if (!pageInfo || !pageInfo->page) {
                fz_try(ctx) {
                    pageInfo->page = fz_load_page(ctx, _doc, pageNo - 1);
                }
                fz_catch(ctx) {
                    fz_report_error(ctx);
                }
            }
            if (pageInfo && pageInfo->page) {
                pdf_page* pdfpage = pdf_page_from_fz_page(ctx, pageInfo->page);
                if (pdfpage) {
                    pdf_obj* pageObj = pdfpage->obj;
                    if (pageObj) {
                        if (pristine->contents) {
                            pdf_dict_put(ctx, pageObj, PDF_NAME(Contents), pristine->contents);
                        }
                        if (pristine->resources) {
                            pdf_dict_put(ctx, pageObj, PDF_NAME(Resources), pristine->resources);
                        }
                        pdf_dict_put_rect(ctx, pageObj, PDF_NAME(MediaBox), pristine->mediaBox);
                        pdf_dict_put_rect(ctx, pageObj, PDF_NAME(CropBox), pristine->cropBox);
                        pdf_dict_put_int(ctx, pageObj, PDF_NAME(Rotate), pristine->rotate);
                        ok = true;
                    }
                }
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            ok = false;
        }
    }

    if (!ok) {
        return false;
    }

    // Pass 2: invalidate caches under pagesLock+renderLock
    {
        ScopedRecursiveMutex ps(&pagesLock);
        ScopedMutex rl(&renderLock);
        FzPageInfo* pageInfo = pages[pageNo - 1];
        if (pageInfo) {
            pageInfo->mediabox = ToRectF(pristine->mediaBox);
            if (pageInfo->page) {
                fz_drop_page(ctx, pageInfo->page);
                pageInfo->page = nullptr;
            }
            if (pageInfo->displayList) {
                fz_drop_display_list(ctx, pageInfo->displayList);
                pageInfo->displayList = nullptr;
            }
            pageInfo->elementsNeedRebuilding = true;
        }
    }

    // Clear the pending geometry edit for this page
    for (int i = 0; i < len(pendingGeomEdits); i++) {
        if (pendingGeomEdits[i].pageNo == pageNo) {
            pendingGeomEdits.RemoveAtFast(i);
            break;
        }
    }
    bool anyLeft = false;
    for (const PageGeomEdit& e : pendingGeomEdits) {
        if (e.pageNo > 0) {
            anyLeft = true;
            break;
        }
    }
    modifiedPageGeometry = anyLeft;
    appliedInMemory = anyLeft;

    // Invalidate render cache and relayout
    if (dm && gRenderCache) {
        gRenderCache->FreeForDisplayModel(dm);
    }
    if (dm) {
        dm->BuildPagesInfo();
        dm->RelayoutKeepingView();
    }

    return true;
}