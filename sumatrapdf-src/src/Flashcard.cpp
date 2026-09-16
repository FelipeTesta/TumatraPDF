/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"

extern "C" {
#include <mupdf/pdf.h>
}

#include "Annotation.h"
#include "DocProperties.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineMupdf.h"
#include "AppTools.h"
#include "Flashcard.h"

// Stable card key: FNV-1a hash of page + quantized bounds. MuPDF can renumber
// annotation objects on save (pdf_to_num is not stable), so we key study state
// by position instead. Bounds are quantized to 0.01 page units so the key does
// not change across zoom levels.
static u64 FlashcardKey(int pageNo, const RectF& b) {
    u64 h = 14695981039346656037ULL; // FNV-1a offset basis
    auto mix = [&](u64 v) {
        h ^= v;
        h *= 1099511628211ULL;
    };
    mix((u64)(u32)pageNo);
    mix((u64)(i32)(int)(b.x * 100.0f));
    mix((u64)(i32)(int)(b.y * 100.0f));
    mix((u64)(i32)(int)(b.dx * 100.0f));
    mix((u64)(i32)(int)(b.dy * 100.0f));
    return h;
}

// Iterate all annotations on all pages and extract flashcards.
// Flashcards are Highlight annotations with author "TumatraPDF-Flashcard"
// whose contents start with "Q: "
Vec<Flashcard> FlashcardLoadFromDocument(EngineMupdf* engine) {
    logf("[fc] FlashcardLoadFromDocument - scanning document annotations\n");
    Vec<Flashcard> result;
    if (!engine || !engine->pdfdoc) {
        return result;
    }

    fz_context* ctx = engine->Ctx();
    ScopedRecursiveMutex cs(&engine->docLock);

    int pageCount = fz_count_pages(ctx, engine->_doc);
    for (int pageIdx = 0; pageIdx < pageCount; pageIdx++) {
        fz_page* page = fz_load_page(ctx, engine->_doc, pageIdx);
        if (!page) {
            continue;
        }
        pdf_page* pdfpage = pdf_page_from_fz_page(ctx, page);
        if (!pdfpage) {
            fz_drop_page(ctx, page);
            continue;
        }

        for (pdf_annot* annot = pdf_first_annot(ctx, pdfpage); annot; annot = pdf_next_annot(ctx, annot)) {
            int annotType = pdf_annot_type(ctx, annot);
            if (annotType != PDF_ANNOT_HIGHLIGHT) {
                continue;
            }

            // Only load annotations authored by our flashcard system
            const char* author = pdf_annot_author(ctx, annot);
            if (!author || !str::StartsWith(Str(author), StrL("TumatraPDF-Flashcard"))) {
                continue;
            }

            Flashcard card;
            card.annotId = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
            card.pageNo = pageIdx + 1; // 1-based
            fz_rect rect = pdf_annot_rect(ctx, annot);
            card.bounds = RectF(PointF(rect.x0, rect.y0), PointF(rect.x1, rect.y1));
            // Cloze is positional: the highlight rect is the mask over existing PDF
            // text, so no content is stored (no duplicated text in the document).
            card.key = FlashcardKey(card.pageNo, card.bounds);

            result.Append(card);
        }

        fz_drop_page(ctx, page);
    }

    logf("[fc] FlashcardLoadFromDocument - found %d flashcards\n", len(result));
    return result;
}

// Compute MD5 hash of filePath and return %APPDATA%\SumatraPDF\FlashcardStudy\<md5>.json
TempStr FlashcardStudyPath(const char* filePath) {
    logf("[fc] FlashcardStudyPath - computing for %s\n", Str(filePath));
    if (!filePath) {
        return {};
    }

    Str pathStr(filePath);
    u8 digest[16];
    CalcMD5Digest(pathStr, digest);
    TempStr md5Hex = str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));

    TempStr dir = GetPathInAppDataDirTemp(StrL("FlashcardStudy"));
    return path::JoinTemp(dir, fmt("%s.json", md5Hex));
}

static void WriteJsonValue(FILE* f, const FlashcardStudyState& state) {
    fprintf(f,
            "      {\n"
            "        \"rating\": %d,\n"
            "        \"interval\": %d,\n"
            "        \"easeFactor\": %.4f,\n"
            "        \"lastReviewedAt\": %lld,\n"
            "        \"nextReviewAt\": %lld,\n"
            "        \"reviewCount\": %d\n"
            "      }",
            state.rating, state.interval, state.easeFactor, state.lastReviewedAt, state.nextReviewAt,
            state.reviewCount);
}

// Save flashcard study state to external JSON file
void FlashcardStudySave(const char* filePath, const FlashcardStudyDoc& doc) {
    logf("[fc] FlashcardStudySave - saving to %s\n", Str(filePath));
    if (!filePath) {
        return;
    }

    TempStr path = FlashcardStudyPath(filePath);
    if (!path) {
        return;
    }

    // Ensure directory exists
    TempStr dir = path::GetDirTemp(Str(path));
    if (dir) {
        dir::CreateAll(Str(dir));
    }

    FILE* f = fopen(path.s, "wb");
    if (!f) {
        logf("[fc] FlashcardStudySave - ERROR: failed to open file %s\n", Str(filePath));
        logf("FlashcardStudySave: failed to open '%s' for writing\n", path);
        return;
    }

    fprintf(f,
            "{\n"
            "  \"version\": %d,\n"
            "  \"states\": {\n",
            doc.version);

    bool first = true;
    for (int i = 0; i < doc.states.len; i++) {
        auto& entry = doc.states.els[i];
        if (!first) {
            fprintf(f, ",\n");
        }
        first = false;
        fprintf(f, "    \"%llu\": ", (unsigned long long)entry.key);
        WriteJsonValue(f, entry.state);
    }

    fprintf(f,
            "\n  }\n"
            "}\n");

    logf("[fc] FlashcardStudySave - saved %d entries\n", len(doc.states));

    fclose(f);
}

// Simple JSON parser for the flashcard study file.
// Only handles the specific format we write.
static bool ParseJsonObject(Str json, Vec<FlashcardStudyDoc::StateEntry>* outStates) {
    // Find "states": {
    Str statesKey = StrL("\"states\"");
    int statesIdx = str::IndexOf(json, statesKey);
    if (statesIdx < 0) {
        return false;
    }
    const char* statesStart = json.s + statesIdx;
    Str afterStates = Str(statesStart + len(statesKey));
    str::TrimWSInPlace(afterStates, str::TrimOpt::Left);
    if (!afterStates || afterStates.s[0] != ':') {
        return false;
    }
    afterStates = Str(afterStates.s + 1);
    str::TrimWSInPlace(afterStates, str::TrimOpt::Left);
    if (!afterStates || afterStates.s[0] != '{') {
        return false;
    }
    afterStates = Str(afterStates.s + 1);

    // Helper: find index of any char in set
    auto findAnyChar = [](Str s, Str chars) -> int {
        for (int i = 0; i < len(s); i++) {
            if (str::IndexOfChar(chars, s.s[i]) >= 0) {
                return i;
            }
        }
        return -1;
    };

    // Parse key-value pairs: "annotId": { ... }
    while (afterStates && afterStates.s[0] != '}') {
        str::TrimWSInPlace(afterStates, str::TrimOpt::Left);
        if (!afterStates || afterStates.s[0] != '"') {
            break;
        }

        // Parse key (annotId)
        Str keyStart = Str(afterStates.s + 1);
        int keyEndIdx = str::IndexOf(keyStart, StrL("\""));
        if (keyEndIdx < 0) {
            break;
        }
        Str keyStr = Str(keyStart.s, keyEndIdx);
        u64 key = (u64)ParseInt64(keyStr);

        // Find the value object
        Str afterKey = Str(keyStart.s + keyEndIdx + 1);
        str::TrimWSInPlace(afterKey, str::TrimOpt::Left);
        if (!afterKey || afterKey.s[0] != ':') {
            break;
        }
        afterKey = Str(afterKey.s + 1);
        str::TrimWSInPlace(afterKey, str::TrimOpt::Left);
        if (!afterKey || afterKey.s[0] != '{') {
            break;
        }

        // Find matching '}' for the value object
        int braceCount = 1;
        Str valueStart = Str(afterKey.s + 1);
        Str valueEnd = valueStart;
        while (valueEnd && valueEnd.s[0] && braceCount > 0) {
            if (valueEnd.s[0] == '{') {
                braceCount++;
            } else if (valueEnd.s[0] == '}') {
                braceCount--;
            }
            valueEnd = Str(valueEnd.s + 1);
        }
        if (braceCount != 0) {
            break;
        }
        // valueEnd points past the closing '}', so step back
        valueEnd = Str(valueEnd.s - 1);
        Str valueJson = Str(valueStart.s, (int)(valueEnd.s - valueStart.s));

        // Parse the state fields
        FlashcardStudyState state = {};
        auto getField = [&](Str fieldName, auto& out, auto parser) {
            int fieldIdx = str::IndexOf(valueJson, fieldName);
            if (fieldIdx < 0) return;
            Str afterField = Str(valueJson.s + fieldIdx + len(fieldName));
            str::TrimWSInPlace(afterField, str::TrimOpt::Left);
            if (!afterField || afterField.s[0] != ':') return;
            afterField = Str(afterField.s + 1);
            str::TrimWSInPlace(afterField, str::TrimOpt::Left);
            int valEndIdx = findAnyChar(afterField, StrL(",\n}"));
            if (valEndIdx < 0) return;
            Str valStr = Str(afterField.s, valEndIdx);
            str::TrimWSInPlace(valStr, str::TrimOpt::Both);
            out = parser(valStr);
        };

        getField(StrL("rating"), state.rating, [](Str s) { return ParseInt(s); });
        getField(StrL("interval"), state.interval, [](Str s) { return ParseInt(s); });
        getField(StrL("easeFactor"), state.easeFactor, [](Str s) {
            char* end;
            float val = strtof(s.s, &end);
            return val;
        });
        getField(StrL("lastReviewedAt"), state.lastReviewedAt, [](Str s) { return ParseInt64(s); });
        getField(StrL("nextReviewAt"), state.nextReviewAt, [](Str s) { return ParseInt64(s); });
        getField(StrL("reviewCount"), state.reviewCount, [](Str s) { return ParseInt(s); });

        FlashcardStudyDoc::StateEntry entry;
        entry.key = key;
        entry.state = state;
        outStates->Append(entry);

        str::TrimWSInPlace(valueEnd, str::TrimOpt::Left);
        if (!valueEnd || valueEnd.s[0] != ',') {
            break;
        }
        afterStates = Str(valueEnd.s + 1);
    }

    return true;
}

// Load flashcard study state from external JSON file
FlashcardStudyDoc FlashcardStudyLoad(const char* filePath) {
    logf("[fc] FlashcardStudyLoad - loading from %s\n", Str(filePath));
    FlashcardStudyDoc doc;
    doc.version = 1;

    if (!filePath) {
        return doc;
    }

    TempStr studyPath = FlashcardStudyPath(filePath);
    if (!studyPath) {
        return doc;
    }

    Str json = file::ReadFile(Str(studyPath));
    if (!json) {
        return doc;
    }

    auto findAnyChar = [](Str s, Str chars) -> int {
        for (int i = 0; i < len(s); i++) {
            if (str::IndexOfChar(chars, s.s[i]) >= 0) {
                return i;
            }
        }
        return -1;
    };

    // Try to extract version
    int verIdx = str::IndexOf(json, StrL("\"version\""));
    if (verIdx >= 0) {
        Str afterVer = Str(json.s + verIdx + len(StrL("\"version\"")));
        str::TrimWSInPlace(afterVer, str::TrimOpt::Left);
        if (afterVer && afterVer.s[0] == ':') {
            afterVer = Str(afterVer.s + 1);
            str::TrimWSInPlace(afterVer, str::TrimOpt::Left);
            int verEndIdx = findAnyChar(afterVer, StrL(",\n}"));
            if (verEndIdx >= 0) {
                Str verStr = Str(afterVer.s, verEndIdx);
                str::TrimWSInPlace(verStr, str::TrimOpt::Both);
                doc.version = ParseInt(verStr);
            }
        }
    }

    ParseJsonObject(json, &doc.states);

    logf("[fc] FlashcardStudyLoad - loaded %d entries\n", len(doc.states));
    return doc;
}

// SM-2 algorithm (lite version) for spaced repetition
// rating: 1=Again, 2=Hard, 3=Good, 4=Easy
void FlashcardSm2Update(FlashcardStudyState& state, int rating) {
    logf("[fc] FlashcardSm2Update - rating=%d interval=%d ease=%.2f\n", rating, state.interval, state.easeFactor);
    i64 now = (i64)time(nullptr) * 1000; // milliseconds since epoch

    state.rating = rating;
    state.lastReviewedAt = now;
    state.reviewCount++;

    if (rating == 1) { // Again
        state.interval = 0;
        state.easeFactor = std::max(1.3f, state.easeFactor - 0.2f);
    } else if (rating == 2) { // Hard
        state.interval = std::max(1, (int)(state.interval * 1.2f));
        state.easeFactor = std::max(1.3f, state.easeFactor - 0.15f);
    } else if (rating == 3) { // Good
        if (state.interval == 0) {
            state.interval = 1;
        } else {
            state.interval = std::max(1, (int)(state.interval * state.easeFactor));
        }
        // easeFactor unchanged
    } else if (rating == 4) { // Easy
        if (state.interval == 0) {
            state.interval = 1;
        } else {
            state.interval = std::max(1, (int)(state.interval * state.easeFactor * 1.3f));
        }
        state.easeFactor += 0.15f;
    }

    // Clamp ease factor to the SM-2 range [1.3, 2.5]
    state.easeFactor = std::max(1.3f, std::min(2.5f, state.easeFactor));

    state.nextReviewAt = now + (i64)state.interval * 86400000LL; // interval days * 24*60*60*1000

    logf("[fc] FlashcardSm2Update - new interval=%d ease=%.2f\n", state.interval, state.easeFactor);
}

#include "Flashcard.h"