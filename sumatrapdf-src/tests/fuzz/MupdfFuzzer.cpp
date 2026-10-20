// libFuzzer harness for the mupdf document pipeline, built by
// cmd/build-fuzz-harness-mupdf.ts. Opens a fresh fz_context per input (mupdf
// keeps global caches, so per-iteration context is the only clean reset, the
// same pattern upstream mupdf's own fuzzers use), parses the bytes as a PDF
// through the stream API, loads and renders page 0. Parse errors are the
// expected outcome; the harness only reports memory-safety violations (ASan).
#include <cstdint>
#include <cstring>

#include "mupdf/fitz/context.h"
#include "mupdf/fitz/document.h"
#include "mupdf/fitz/pixmap.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > (4u << 20)) {
        return 0;
    }
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        return 0;
    }
    fz_stream* stream = fz_open_memory(ctx, data, size);
    fz_document* doc = nullptr;
    fz_page* page = nullptr;
    fz_pixmap* pix = nullptr;
    fz_device* dev = nullptr;
    fz_try(ctx) {
        doc = fz_open_document_with_stream(ctx, ".pdf", stream);
        int n = fz_count_pages(ctx, doc);
        if (n > 0) {
            page = fz_load_page(ctx, doc, 0);
            fz_rect bounds = fz_bound_page(ctx, page);
            // clamp: a malformed doc can claim a huge page box
            if (bounds.x1 - bounds.x0 > 1 && bounds.x1 - bounds.x0 < 20000 &&
                bounds.y1 - bounds.y0 > 1 && bounds.y1 - bounds.y0 < 20000) {
                fz_matrix ctm = fz_scale(0.25f, 0.25f);
                fz_rect scaled = fz_transform_rect(bounds, ctm);
                pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), fz_round_rect(scaled), 0, 1);
                dev = fz_new_draw_device(ctx, fz_identity, pix);
                fz_run_page(ctx, page, dev, fz_identity, nullptr);
            }
        }
    }
    fz_catch(ctx) {
        // parse/render errors are expected fuzz outcomes
    }
    // mupdf drop functions accept NULL, so the unset handles are no-ops
    fz_drop_device(ctx, dev);
    fz_drop_pixmap(ctx, pix);
    fz_drop_page(ctx, page);
    fz_drop_document(ctx, doc);
    fz_drop_stream(ctx, stream);
    fz_drop_context(ctx);
    return 0;
}
