/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Archive.h"
#if OS_WIN
#include "base/ScopedWin.h"
#endif
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#if OS_WIN
#include "base/Win.h"
#endif
#include "base/Timer.h"

extern "C" {
#include <mupdf/pdf.h>
#if defined(_WIN32)
#include <mupdf/helpers/pkcs7-windows.h>
#endif
#include <mupdf/fitz/device.h>
#include <mupdf/fitz/path.h>
#include <mupdf/fitz/geometry.h>
}

#include "Annotation.h"
#include "DocProperties.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineMupdf.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "Settings.h"
#include "DisplayModel.h"
#include "ArchVector.h"
#include "SumatraLog.h"

// Maximum pages to cache (arbitrary large limit)
static const int kMaxCachedPages = 10000;

// Per-page vector segment cache - use pointers to avoid static array of template issues
static Vec<ArchVecSeg>* g_archSegsCache[kMaxCachedPages] = {nullptr};
static bool g_archCacheValid[kMaxCachedPages] = {false};

// Path walker state for collecting stroke segments
struct ArchPathWalkState {
    fz_context* ctx;
    fz_matrix ctm;
    Vec<ArchVecSeg>* segs;
    fz_point lastPt;
    bool haveLastPt;
};

static void ArchWalkMoveto(fz_context* ctx, void* arg, float x, float y) {
    auto* state = (ArchPathWalkState*)arg;
    fz_point pt = fz_transform_point_xy(x, y, state->ctm);
    state->lastPt = pt;
    state->haveLastPt = true;
}

static void ArchWalkLineto(fz_context* ctx, void* arg, float x, float y) {
    auto* state = (ArchPathWalkState*)arg;
    fz_point pt = fz_transform_point_xy(x, y, state->ctm);
    if (state->haveLastPt) {
        ArchVecSeg seg;
        seg.x0 = state->lastPt.x;
        seg.y0 = state->lastPt.y;
        seg.x1 = pt.x;
        seg.y1 = pt.y;
        state->segs->Append(seg);
    }
    state->lastPt = pt;
    state->haveLastPt = true;
}

static void ArchWalkCurveto(fz_context* ctx, void* arg, float x1, float y1, float x2, float y2, float x3, float y3) {
    auto* state = (ArchPathWalkState*)arg;
    // Approximate curve by its endpoints (architecture drawings use straight lines mostly)
    fz_point pt3 = fz_transform_point_xy(x3, y3, state->ctm);
    if (state->haveLastPt) {
        ArchVecSeg seg;
        seg.x0 = state->lastPt.x;
        seg.y0 = state->lastPt.y;
        seg.x1 = pt3.x;
        seg.y1 = pt3.y;
        state->segs->Append(seg);
    }
    state->lastPt = pt3;
    state->haveLastPt = true;
}

static void ArchWalkClosepath(fz_context* ctx, void* arg) {
    auto* state = (ArchPathWalkState*)arg;
    // Closepath just connects back to the start of the subpath
    // The next moveto will start a new subpath
    state->haveLastPt = false;
}

static const fz_path_walker kArchPathWalker = {
    ArchWalkMoveto, ArchWalkLineto, ArchWalkCurveto, ArchWalkClosepath,
    nullptr, // quadto
    nullptr, // curvetov
    nullptr, // curvetoy
    nullptr, // rectto
};

// Custom device for extracting stroke path segments
typedef struct {
    fz_device super;
    Vec<ArchVecSeg>* segs;
} fz_arch_vector_device;

static void ArchVectorStrokePath(fz_context* ctx, fz_device* dev, const fz_path* path, const fz_stroke_state* stroke,
                                 fz_matrix ctm, fz_colorspace*, const float*, float, fz_color_params) {
    fz_arch_vector_device* d = (fz_arch_vector_device*)dev;
    ArchPathWalkState state{ctx, ctm, d->segs, {0, 0}, false};
    fz_walk_path(ctx, path, &kArchPathWalker, &state);
}

// Passthrough stubs for other device ops (we only care about stroke_path)
static void ArchVectorFillPath(fz_context*, fz_device*, const fz_path*, int, fz_matrix, fz_colorspace*, const float*,
                               float, fz_color_params) {}
static void ArchVectorClipPath(fz_context*, fz_device*, const fz_path*, int, fz_matrix, fz_rect) {}
static void ArchVectorClipStrokePath(fz_context*, fz_device*, const fz_path*, const fz_stroke_state*, fz_matrix,
                                     fz_rect) {}
static void ArchVectorFillText(fz_context*, fz_device*, const fz_text*, fz_matrix, fz_colorspace*, const float*, float,
                               fz_color_params) {}
static void ArchVectorStrokeText(fz_context*, fz_device*, const fz_text*, const fz_stroke_state*, fz_matrix,
                                 fz_colorspace*, const float*, float, fz_color_params) {}
static void ArchVectorClipText(fz_context*, fz_device*, const fz_text*, fz_matrix, fz_rect) {}
static void ArchVectorClipStrokeText(fz_context*, fz_device*, const fz_text*, const fz_stroke_state*, fz_matrix,
                                     fz_rect) {}
static void ArchVectorIgnoreText(fz_context*, fz_device*, const fz_text*, fz_matrix) {}
static void ArchVectorFillShade(fz_context*, fz_device*, fz_shade*, fz_matrix, float, fz_color_params) {}
static void ArchVectorFillImage(fz_context*, fz_device*, fz_image*, fz_matrix, float, fz_color_params) {}
static void ArchVectorFillImageMask(fz_context*, fz_device*, fz_image*, fz_matrix, fz_colorspace*, const float*, float,
                                    fz_color_params) {}
static void ArchVectorClipImageMask(fz_context*, fz_device*, fz_image*, fz_matrix, fz_rect) {}
static void ArchVectorPopClip(fz_context*, fz_device*) {}
static void ArchVectorBeginMask(fz_context*, fz_device*, fz_rect, int, fz_colorspace*, const float*, fz_color_params) {}
static void ArchVectorEndMask(fz_context*, fz_device*, fz_function*) {}
static void ArchVectorBeginGroup(fz_context*, fz_device*, fz_rect, fz_colorspace*, int, int, int, float) {}
static void ArchVectorEndGroup(fz_context*, fz_device*) {}
static int ArchVectorBeginTile(fz_context*, fz_device*, fz_rect, fz_rect, float, float, fz_matrix, int, int) {
    return 0;
}
static void ArchVectorEndTile(fz_context*, fz_device*) {}

static fz_device* FzNewArchVectorDevice(fz_context* ctx, Vec<ArchVecSeg>* segs) {
    fz_arch_vector_device* dev = fz_new_derived_device(ctx, fz_arch_vector_device);
    dev->super.stroke_path = ArchVectorStrokePath;
    dev->super.fill_path = ArchVectorFillPath;
    dev->super.clip_path = ArchVectorClipPath;
    dev->super.clip_stroke_path = ArchVectorClipStrokePath;
    dev->super.fill_text = ArchVectorFillText;
    dev->super.stroke_text = ArchVectorStrokeText;
    dev->super.clip_text = ArchVectorClipText;
    dev->super.clip_stroke_text = ArchVectorClipStrokeText;
    dev->super.ignore_text = ArchVectorIgnoreText;
    dev->super.fill_shade = ArchVectorFillShade;
    dev->super.fill_image = ArchVectorFillImage;
    dev->super.fill_image_mask = ArchVectorFillImageMask;
    dev->super.clip_image_mask = ArchVectorClipImageMask;
    dev->super.pop_clip = ArchVectorPopClip;
    dev->super.begin_mask = ArchVectorBeginMask;
    dev->super.end_mask = ArchVectorEndMask;
    dev->super.begin_group = ArchVectorBeginGroup;
    dev->super.end_group = ArchVectorEndGroup;
    dev->super.begin_tile = ArchVectorBeginTile;
    dev->super.end_tile = ArchVectorEndTile;
    dev->segs = segs;
    return &dev->super;
}

Vec<ArchVecSeg> ArchExtractPageSegments(EngineMupdf* engine, int pageNo) {
    if (pageNo < 1 || pageNo > kMaxCachedPages) {
        return Vec<ArchVecSeg>();
    }

    int idx = pageNo - 1;
    if (g_archCacheValid[idx] && g_archSegsCache[idx]) {
        return *g_archSegsCache[idx];
    }

    auto* ctx = engine->Ctx();
    if (!ctx) {
        return Vec<ArchVecSeg>();
    }

    // Get the page info (loads page if needed)
    FzPageInfo* pageInfo = engine->GetFzPageInfo(pageNo, false, nullptr);
    if (!pageInfo || !pageInfo->page) {
        return Vec<ArchVecSeg>();
    }

    fz_page* page = pageInfo->page;
    Vec<ArchVecSeg> segs;

    fz_device* dev = nullptr;
    fz_var(dev);
    fz_try(ctx) {
        dev = FzNewArchVectorDevice(ctx, &segs);
        // Run page contents with identity CTM to get geometry in page space
        fz_run_page_contents(ctx, page, dev, fz_identity, nullptr);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        if (dev) {
            fz_drop_device(ctx, dev);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        segs.Clear();
    }

    // Cache the result
    if (!g_archSegsCache[idx]) {
        g_archSegsCache[idx] = new Vec<ArchVecSeg>();
    }
    *g_archSegsCache[idx] = segs;
    g_archCacheValid[idx] = true;

    // Debug log
    logf("[ArchVector] Page %d: extracted %d segments\n", pageNo, len(segs));

    return segs;
}

void ArchClearVectorCache() {
    for (int i = 0; i < kMaxCachedPages; i++) {
        if (g_archCacheValid[i] && g_archSegsCache[i]) {
            g_archSegsCache[i]->Clear();
            delete g_archSegsCache[i];
            g_archSegsCache[i] = nullptr;
            g_archCacheValid[i] = false;
        }
    }
    logf("[ArchVector] Cache cleared\n");
}

ArchSnapResult ArchSnapToVector(DisplayModel* dm, int pageNo, float screenX, float screenY, float thresholdPx) {
    ArchSnapResult result;
    result.snapped = false;

    if (!dm || !dm->engine || pageNo < 1) {
        result.pageX = 0;
        result.pageY = 0;
        return result;
    }

    // Convert screen point to page coordinates
    Point screenPt = {(int)screenX, (int)screenY};
    PointF pagePt = dm->CvtFromScreen(screenPt, pageNo);

    // Get cached segments
    EngineMupdf* engine = AsEngineMupdf(dm->engine);
    if (!engine) {
        result.pageX = pagePt.x;
        result.pageY = pagePt.y;
        return result;
    }

    Vec<ArchVecSeg> segs = ArchExtractPageSegments(engine, pageNo);
    if (len(segs) == 0) {
        result.pageX = pagePt.x;
        result.pageY = pagePt.y;
        return result;
    }

    // Convert threshold from screen pixels to page units
    // Get zoom for this page
    float zoom = 1.0f;
    if (dm->pagesInfo && pageNo <= dm->PageCount()) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (pi) {
            zoom = pi->zoomReal;
        }
    }
    if (zoom <= 0) zoom = 1.0f;

    // Page units per screen pixel = 1 / zoom
    float pageUnitsPerPx = 1.0f / zoom;
    float thresholdPage = thresholdPx * pageUnitsPerPx;

    // Find nearest endpoint or point-on-segment
    float bestDistSq = thresholdPage * thresholdPage;
    float bestX = pagePt.x;
    float bestY = pagePt.y;
    bool found = false;

    for (const ArchVecSeg& seg : segs) {
        // Check distance to endpoint 0
        float dx0 = seg.x0 - pagePt.x;
        float dy0 = seg.y0 - pagePt.y;
        float distSq0 = dx0 * dx0 + dy0 * dy0;
        if (distSq0 < bestDistSq) {
            bestDistSq = distSq0;
            bestX = seg.x0;
            bestY = seg.y0;
            found = true;
        }

        // Check distance to endpoint 1
        float dx1 = seg.x1 - pagePt.x;
        float dy1 = seg.y1 - pagePt.y;
        float distSq1 = dx1 * dx1 + dy1 * dy1;
        if (distSq1 < bestDistSq) {
            bestDistSq = distSq1;
            bestX = seg.x1;
            bestY = seg.y1;
            found = true;
        }

        // Check distance to segment (point-to-line-segment)
        float segDx = seg.x1 - seg.x0;
        float segDy = seg.y1 - seg.y0;
        float segLenSq = segDx * segDx + segDy * segDy;
        if (segLenSq > 0) {
            float t = ((pagePt.x - seg.x0) * segDx + (pagePt.y - seg.y0) * segDy) / segLenSq;
            if (t >= 0 && t <= 1) {
                float projX = seg.x0 + t * segDx;
                float projY = seg.y0 + t * segDy;
                float dx = projX - pagePt.x;
                float dy = projY - pagePt.y;
                float distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestX = projX;
                    bestY = projY;
                    found = true;
                }
            }
        }
    }

    result.snapped = found;
    result.pageX = bestX;
    result.pageY = bestY;
    return result;
}

bool ArchHitTestSegment(DisplayModel* dm, int pageNo, float pageX, float pageY, float thresholdPage, float* outX0,
                        float* outY0, float* outX1, float* outY1) {
    if (!dm || !dm->engine || pageNo < 1) {
        return false;
    }

    EngineMupdf* engine = AsEngineMupdf(dm->engine);
    if (!engine) {
        return false;
    }

    Vec<ArchVecSeg> segs = ArchExtractPageSegments(engine, pageNo);
    if (len(segs) == 0) {
        return false;
    }

    float bestDistSq = thresholdPage * thresholdPage;
    bool found = false;

    for (const ArchVecSeg& seg : segs) {
        // Check distance to segment (point-to-line-segment)
        float segDx = seg.x1 - seg.x0;
        float segDy = seg.y1 - seg.y0;
        float segLenSq = segDx * segDx + segDy * segDy;
        if (segLenSq > 0) {
            float t = ((pageX - seg.x0) * segDx + (pageY - seg.y0) * segDy) / segLenSq;
            if (t >= 0 && t <= 1) {
                float projX = seg.x0 + t * segDx;
                float projY = seg.y0 + t * segDy;
                float dx = projX - pageX;
                float dy = projY - pageY;
                float distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    *outX0 = seg.x0;
                    *outY0 = seg.y0;
                    *outX1 = seg.x1;
                    *outY1 = seg.y1;
                    found = true;
                }
            }
        }
    }

    return found;
}