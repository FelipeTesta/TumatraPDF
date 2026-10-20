// libFuzzer harness for the base text parsers, built by cmd/build-fuzz-harness.ts
// with clang-cl /fsanitize=address,fuzzer. Inputs starting with '{' go to
// json::Parse, everything else to ParseSquareTree, so one corpus exercises both.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "base/Base.h"
#include "base/JsonParser.h"
#include "base/SquareTreeParser.h"

// ReportIf() reports through this app-level hook; the harness provides a
// logging stub so the base objects link without the app.
void _uploadDebugReport(Str condStr, Str fileLine, bool isCrash, bool captureCallstack) {
    fprintf(stderr, "ReportIf: %.*s at %.*s\n", len(condStr), condStr.s, len(fileLine), fileLine.s);
}

// log()/loga() route into SumatraLog.cpp (app level); stderr stub for the harness
void log(Str s) {
    fprintf(stderr, "%.*s\n", len(s), s.s);
}

static void FuzzVisit(void*, json::Value* v) {
    // touch every field so the parser must produce consistent values
    (void)len(v->value);
    (void)v->type;
}

static void FuzzJson(Str s) {
    json::Parse(s, MkFunc1(FuzzVisit, (void*)nullptr));
}

static void FuzzSquareTree(Str s) {
    SquareTreeNode* root = ParseSquareTree(s);
    delete root;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > (1u << 20)) {
        return 0;
    }
    char* buf = (char*)malloc(size + 1);
    if (!buf) {
        return 0;
    }
    memcpy(buf, data, size);
    buf[size] = 0;
    Str s(buf, (int)size);
    if (buf[0] == '{') {
        FuzzJson(s);
    } else {
        FuzzSquareTree(s);
    }
    free(buf);
    return 0;
}
