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

// Iterate all annotations on all pages and extract flashcards.
// Flashcards are FreeText annotations whose contents start with "Q: "
Vec<Flashcard> FlashcardLoadFromDocument(EngineMupdf* engine) {
    logf("FC: FlashcardLoadFromDocument - scanning document annotations\n");
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
            if (annotType != PDF_ANNOT_FREE_TEXT) {
                continue;
            }

            const char* contents = pdf_annot_contents(ctx, annot);
            if (!contents || !str::StartsWith(Str(contents), StrL("Q: "))) {
                continue;
            }

            Flashcard card;
            card.annotId = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
            card.pageNo = pageIdx + 1; // 1-based
            fz_rect rect = pdf_annot_rect(ctx, annot);
            card.bounds = RectF(PointF(rect.x0, rect.y0), PointF(rect.x1, rect.y1));
            card.text = str::DupTemp(Str(contents));
            card.tip = {};

            // Parse optional tip from annotation content: "Q: question\nT: tip"
            Str fullText = card.text;
            Str tipMarker = StrL("\nT: ");
            int tipPos = str::IndexOf(fullText, tipMarker);
            if (tipPos >= 0) {
                card.tip = str::Dup(fullText.s + tipPos + 4); // skip "\nT: "
                card.text = str::DupTemp(Str(fullText.s, tipPos)); // trim to just "Q: ..."
            }

            result.Append(card);
        }

        fz_drop_page(ctx, page);
    }

    logf("FC: FlashcardLoadFromDocument - found %d flashcards\n", len(result));
    return result;
}

// Compute MD5 hash of filePath and return %APPDATA%\SumatraPDF\FlashcardStudy\<md5>.json
TempStr FlashcardStudyPath(const char* filePath) {
    logf("FC: FlashcardStudyPath - computing for %s\n", Str(filePath));
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
    logf("FC: FlashcardStudySave - saving to %s\n", Str(filePath));
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
        logf("FC: FlashcardStudySave - ERROR: failed to open file %s\n", Str(filePath));
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
        fprintf(f, "    \"%d\": ", entry.annotId);
        WriteJsonValue(f, entry.state);
    }

    fprintf(f,
            "\n  }\n"
            "}\n");

    logf("FC: FlashcardStudySave - saved %d entries\n", len(doc.states));

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
        int annotId = ParseInt(keyStr);

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

        getField(StrL("rating"), state.rating, [](Str s) {
            return ParseInt(s);
        });
        getField(StrL("interval"), state.interval, [](Str s) {
            return ParseInt(s);
        });
        getField(StrL("easeFactor"), state.easeFactor, [](Str s) {
            char* end; float val = strtof(s.s, &end); return val;
        });
        getField(StrL("lastReviewedAt"), state.lastReviewedAt, [](Str s) {
            return ParseInt64(s);
        });
        getField(StrL("nextReviewAt"), state.nextReviewAt, [](Str s) {
            return ParseInt64(s);
        });
        getField(StrL("reviewCount"), state.reviewCount, [](Str s) {
            return ParseInt(s);
        });

        FlashcardStudyDoc::StateEntry entry;
        entry.annotId = annotId;
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
    logf("FC: FlashcardStudyLoad - loading from %s\n", Str(filePath));
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

    logf("FC: FlashcardStudyLoad - loaded %d entries\n", len(doc.states));
    return doc;
}

// SM-2 algorithm (lite version) for spaced repetition
// rating: 1=Again, 2=Hard, 3=Good, 4=Easy
void FlashcardSm2Update(FlashcardStudyState& state, int rating) {
    logf("FC: FlashcardSm2Update - rating=%d interval=%d ease=%.2f\n", rating, state.interval, state.easeFactor);
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

    state.nextReviewAt = now + (i64)state.interval * 86400000LL; // interval days * 24*60*60*1000

    logf("FC: FlashcardSm2Update - new interval=%d ease=%.2f\n", state.interval, state.easeFactor);
}

#include "Flashcard.h"