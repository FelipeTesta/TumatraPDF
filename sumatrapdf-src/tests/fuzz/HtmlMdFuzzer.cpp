// libFuzzer harness for the mupdf html/markdown pipeline (html-doc layout +
// the vendored cmark-gfm markdown parser), built by
// cmd/build-fuzz-harness-mupdf.ts with clang-cl /fsanitize=address,fuzzer.
// Dispatch on the first byte: '<' opens the input as HTML, anything else as
// Markdown. Fresh fz_context per input (mupdf keeps global caches, so
// per-iteration context is the only clean reset).
#include <cstdint>
#include <cstring>

extern "C" {
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/document.h"
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > (1u << 20)) {
        return 0;
    }
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        return 0;
    }

    fz_stream* stream = nullptr;
    fz_document* doc = nullptr;
    fz_page* page = nullptr;
    // inputs starting with '<' are html; everything else goes through the
    // markdown handler (cmark-gfm)
    const char* magic = (data[0] == '<') ? ".html" : ".md";

    fz_try(ctx) {
        stream = fz_open_memory(ctx, data, size);
        doc = fz_open_document_with_stream(ctx, magic, stream);
        int n = fz_count_pages(ctx, doc);
        if (n > 0) {
            page = fz_load_page(ctx, doc, 0);
            fz_bound_page(ctx, page);
        }
    }
    fz_catch(ctx) {
        // parse/layout errors are the expected outcome for mutated inputs
    }

    fz_drop_page(ctx, page);
    fz_drop_document(ctx, doc);
    fz_drop_stream(ctx, stream);
    fz_drop_context(ctx);
    return 0;
}
