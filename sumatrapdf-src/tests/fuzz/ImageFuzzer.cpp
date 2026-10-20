// libFuzzer harness for the mupdf image decoder pipeline, built by
// cmd/build-fuzz-harness-mupdf.ts (second target). Decodes arbitrary bytes as
// an image (png/jpg/jpx/jbig2/webp/bmp/gif/tiff/... — whichever decoder accepts
// the magic) and renders it into a pixmap, all under ASan.
#include <cstdint>

extern "C" {
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/image.h"
#include "mupdf/fitz/pixmap.h"
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > (2u << 20)) {
        return 0;
    }
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        return 0;
    }
    fz_buffer* buf = nullptr;
    fz_image* img = nullptr;
    fz_pixmap* pix = nullptr;
    int gotImage = 0;
    fz_try(ctx) {
        buf = fz_new_buffer_from_copied_data(ctx, data, size);
        img = fz_new_image_from_buffer(ctx, buf);
        gotImage = 1;
    }
    fz_catch(ctx) {}
    if (!gotImage) {
        // the image call consumed the buffer only on success
        fz_drop_buffer(ctx, buf);
        fz_drop_context(ctx);
        return 0;
    }
    // clamp the decoded pixmap: a mutated header can claim enormous dimensions
    if (img->w <= 0 || img->h <= 0 || (long long)img->w * img->h > 64LL * 1024 * 1024) {
        fz_drop_image(ctx, img);
        fz_drop_context(ctx);
        return 0;
    }
    fz_try(ctx) {
        pix = fz_get_pixmap_from_image(ctx, img, nullptr, nullptr, nullptr, nullptr);
    }
    fz_catch(ctx) {}
    fz_drop_pixmap(ctx, pix);
    fz_drop_image(ctx, img);
    fz_drop_context(ctx);
    return 0;
}
