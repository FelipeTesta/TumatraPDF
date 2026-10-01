/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"

extern "C" {
#include <mupdf/pdf.h>
}

#include "Annotation.h"
#include "Settings.h"
#include "GlobalPrefs.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "DocProperties.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineMupdf.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "WindowTab.h"
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
// Flashcards are Highlight annotations with author "TumatraPDF-Flashcard".
// Cloze is positional: the highlight quad-points are the mask over existing
// PDF text and the annotation content stays empty (no text duplication).
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

            // A single corrupt annotation must not abort the whole scan (an
            // uncaught fz_throw here crashes the app). pdf_annot_rect() must NOT
            // be used: markup annotations are excluded from rect_subtypes and
            // throw "Highlight annotations have no Rect property". pdf_bound_annot()
            // computes the bbox from quad-points and works for all types.
            Flashcard card;
            bool ok = false;
            fz_try(ctx) {
                card.annotId = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
                card.pageNo = pageIdx + 1; // 1-based
                fz_rect rect = pdf_bound_annot(ctx, annot);
                card.bounds = RectF(PointF(rect.x0, rect.y0), PointF(rect.x1, rect.y1));
                // Per-quad subrects so a multi-line cloze masks exactly the
                // highlighted text (not the union bbox). pdf_annot_quad_point
                // ALREADY applies the page transform (cropbox + rotation) —
                // same space as pdf_bound_annot. Do NOT transform again: the
                // former extra pageCtm here double-transformed the rects and
                // displaced the mask on pages whose ctm isn't identity
                // (cropbox offset / rotation), while the native highlight
                // rendered correctly.
                int nQuads = pdf_annot_quad_point_count(ctx, annot);
                for (int i = 0; i < nQuads; i++) {
                    fz_quad q = pdf_annot_quad_point(ctx, annot, i);
                    fz_rect qr = fz_rect_from_quad(q);
                    card.rects.Append(RectF(PointF(qr.x0, qr.y0), PointF(qr.x1, qr.y1)));
                }
                if (len(card.rects) == 0) {
                    card.rects.Append(card.bounds);
                }
                ok = true;
            }
            fz_catch(ctx) {
                fz_report_error(ctx);
            }
            if (!ok) {
                continue;
            }
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

// Create a flashcard Highlight annotation covering the given PAGE-space rects:
// cloze is positional (content stays empty), author TumatraPDF-Flashcard, gray
// 0.5 color, 0.4 opacity — the exact recipe of CmdFlashcardAdd, extracted so
// the import merge can create cards the same way.
Annotation* FlashcardCreateHighlightAnnot(EngineBase* engine, int pageNo, Vec<RectF>& rects) {
    if (!engine || len(rects) == 0) {
        return nullptr;
    }
    AnnotCreateArgs args{};
    args.annotType = AnnotationType::Highlight;
    Str content; // empty: the highlight rect IS the card (no text duplication)
    args.content = content;
    args.setContentToSelection = false;
    Annotation* annot = EngineMupdfCreateAnnotation(engine, pageNo, PointF{}, &args);
    if (!annot) {
        logf("[fc] CreateHighlightAnnot - ERROR: EngineMupdfCreateAnnotation failed (page %d)\n", pageNo);
        return nullptr;
    }
    SetQuadPointsAsRect(annot, rects);
    annot->bounds = GetBounds(annot);
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf) {
        return annot;
    }
    fz_context* ctx = epdf->Ctx();
    ScopedRecursiveMutex cs(&epdf->docLock);
    fz_try(ctx) {
        pdf_set_annot_author(ctx, annot->pdfannot, CStrTemp(StrL("TumatraPDF-Flashcard")));
        float gray[3] = {0.5f, 0.5f, 0.5f};
        pdf_set_annot_color(ctx, annot->pdfannot, 3, gray);
        pdf_set_annot_opacity(ctx, annot->pdfannot, 0.4f);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return annot;
}

// Import the NEW flashcards from another copy of the SAME document (e.g. a
// study partner's PDF): loads the friend's card annotations, skips the ones
// we already have (same page + same mask, 1pt tolerance — identical text
// selections produce identical quads) and creates the missing ones in our
// document. Study HISTORY is not touched: imported cards are "new" cards.
// Returns the number of cards created.
int FlashcardImportFromPdf(EngineMupdf* dstEngine, Vec<Flashcard>& existing, const char* srcPath) {
    if (!dstEngine || !srcPath) {
        return 0;
    }
    EngineBase* srcBase = CreateEngineMupdfFromFile(Str(srcPath), FileType::PDF, 0, nullptr);
    if (!srcBase) {
        logf("[fc] Import - ERROR: cannot open '%s'\n", Str(srcPath));
        return 0;
    }
    EngineMupdf* srcEngine = AsEngineMupdf(srcBase);
    if (!srcEngine || !srcEngine->pdfdoc) {
        logf("[fc] Import - ERROR: '%s' is not a PDF (engine mismatch)\n", Str(srcPath));
        srcBase->Release();
        return 0;
    }
    Vec<Flashcard> theirs = FlashcardLoadFromDocument(srcEngine);
    logf("[fc] Import - source '%s' has %d card(s)\n", Str(srcPath), len(theirs));
    int imported = 0;
    int dupes = 0;
    // 'near' is a legacy windows.h macro — name it nearF
    auto nearF = [](float a, float b) {
        float d = a - b;
        return d < 0 ? -d : d;
    };
    for (int i = 0; i < len(theirs); i++) {
        Flashcard& tc = theirs[i];
        // duplicate when one of OUR cards on the same page has (nearly) the
        // same mask — the friend highlighted the same text we already have
        bool dup = false;
        for (int j = 0; j < len(existing); j++) {
            const Flashcard& ec = existing[j];
            if (ec.pageNo != tc.pageNo) {
                continue;
            }
            if (nearF(ec.bounds.x, tc.bounds.x) < 1.0f && nearF(ec.bounds.y, tc.bounds.y) < 1.0f &&
                nearF(ec.bounds.dx, tc.bounds.dx) < 1.0f && nearF(ec.bounds.dy, tc.bounds.dy) < 1.0f) {
                dup = true;
                break;
            }
        }
        if (dup) {
            dupes++;
            continue;
        }
        Annotation* annot = FlashcardCreateHighlightAnnot(dstEngine, tc.pageNo, tc.rects);
        if (annot) {
            imported++;
        } else {
            logf("[fc] Import - ERROR: failed to create card on page %d\n", tc.pageNo);
        }
    }
    srcBase->Release();
    logf("[fc] Import - imported %d new card(s), skipped %d duplicate(s)\n", imported, dupes);
    return imported;
}

// Dry-run half of FlashcardImportFromPdf: counts how many of the source's
// flashcard-tagged highlights we DON'T have yet (same dedupe rule: same page
// + bounds within 1pt) without creating anything. *srcCountOut gets the
// source's total card count. Used by the Import window's comparison table.
int FlashcardImportDiffCount(Vec<Flashcard>& existing, const char* srcPath, int* srcCountOut) {
    if (srcCountOut) {
        *srcCountOut = 0;
    }
    if (!srcPath) {
        return 0;
    }
    EngineBase* srcBase = CreateEngineMupdfFromFile(Str(srcPath), FileType::PDF, 0, nullptr);
    if (!srcBase) {
        logf("[fc] Import diff - ERROR: cannot open '%s'\n", Str(srcPath));
        return -1;
    }
    EngineMupdf* srcEngine = AsEngineMupdf(srcBase);
    if (!srcEngine || !srcEngine->pdfdoc) {
        logf("[fc] Import diff - ERROR: '%s' is not a PDF (engine mismatch)\n", Str(srcPath));
        srcBase->Release();
        return -1;
    }
    Vec<Flashcard> theirs = FlashcardLoadFromDocument(srcEngine);
    int srcTotal = len(theirs);
    if (srcCountOut) {
        *srcCountOut = srcTotal;
    }
    logf("[fc] Import diff - source '%s' has %d card(s)\n", Str(srcPath), srcTotal);
    auto nearF = [](float a, float b) {
        float d = a - b;
        return d < 0 ? -d : d;
    };
    int diff = 0;
    for (int i = 0; i < srcTotal; i++) {
        Flashcard& tc = theirs[i];
        bool dup = false;
        for (int j = 0; j < len(existing); j++) {
            const Flashcard& ec = existing[j];
            if (ec.pageNo != tc.pageNo) {
                continue;
            }
            if (nearF(ec.bounds.x, tc.bounds.x) < 1.0f && nearF(ec.bounds.y, tc.bounds.y) < 1.0f &&
                nearF(ec.bounds.dx, tc.bounds.dx) < 1.0f && nearF(ec.bounds.dy, tc.bounds.dy) < 1.0f) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            diff++;
        }
    }
    srcBase->Release();
    logf("[fc] Import diff - %d new card(s) vs current document\n", diff);
    return diff;
}

// Directory where the study-history JSONs are stored: the user-configured
// flashcardSettings.studyDir when set (e.g. a Google Drive folder so the
// histories are backed up / synced between machines), else the per-exe
// portable app-data "FlashcardStudy" dir. Changing the setting MIGRATES the
// existing files to the new dir (see FlashcardStudyMigrateFiles; files
// already in the target are kept).
TempStr FlashcardStudyDir() {
    Str custom = gGlobalPrefs->flashcardStudyDir;
    if (custom && len(custom) > 0) {
        return (TempStr)custom.s;
    }
    return GetPathInAppDataDirTemp(StrL("FlashcardStudy"));
}

// Compute MD5 hash of filePath and return <study dir>\<md5>.json
TempStr FlashcardStudyPath(const char* filePath) {
    logf("[fc] FlashcardStudyPath - computing for %s\n", Str(filePath));
    if (!filePath) {
        return {};
    }

    Str pathStr(filePath);
    u8 digest[16];
    CalcMD5Digest(pathStr, digest);
    TempStr md5Hex = str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));

    TempStr dir = FlashcardStudyDir();
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

// Write s as a JSON string literal (escape backslash, quote and control
// chars) so a book name with quotes round-trips safely
static void WriteJsonStringLiteral(FILE* f, Str s) {
    fputc('"', f);
    for (int i = 0; i < len(s); i++) {
        char c = s.s[i];
        if (c == '\\' || c == '"') {
            fputc('\\', f);
        }
        unsigned char u = (unsigned char)c;
        if (u < 0x20) {
            fprintf(f, "\\u%04x", u);
        } else {
            fputc(c, f);
        }
    }
    fputc('"', f);
}

// Local rolling backups of the study dir: full copies of every *.json in
// backup\<slot>\ (1d / 3d / 7d). Refreshed after each successful save when the
// snapshot is older than the slot's age (cheap stamp-file guard, so a normal
// study session costs 3 tiny reads per rating, one copy burst per day).
static const struct {
    const char* slot;
    int ageDays;
} kFcBackupSlots[] = {
    {"1d", 1},
    {"3d", 3},
    {"7d", 7},
};

// copy every *.json of the study dir into backup\<slot>\ (after wiping the
// slot) and stamp it with the current time
static void FlashcardStudyBackupRefreshSlot(Str dir, Str slotDir) {
    dir::CreateAll(Str(slotDir));
    WIN32_FIND_DATAW fd{};
    // wipe the previous snapshot
    TempStr wipePat = fmt("%s\\*.json", Str(slotDir));
    HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(wipePat))), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            TempStr old = fmt("%s\\%s", Str(slotDir), ToUtf8Temp(WStr(fd.cFileName)));
            DeleteFileW(CWStrTemp(ToWStrTemp(Str(old))));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    // copy the current files in
    int copied = 0;
    TempStr pat = fmt("%s\\*.json", Str(dir));
    h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pat))), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue; // backup\ itself is never matched here
            }
            TempStr src = fmt("%s\\%s", Str(dir), ToUtf8Temp(WStr(fd.cFileName)));
            TempStr dst = fmt("%s\\%s", Str(slotDir), ToUtf8Temp(WStr(fd.cFileName)));
            if (file::Copy(Str(dst), Str(src), false)) {
                copied++;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
}

static void FlashcardStudyMaybeBackup() {
    TempStr dir = FlashcardStudyDir();
    i64 now = (i64)time(nullptr) * 1000;
    for (auto& s : kFcBackupSlots) {
        TempStr slotDir = fmt("%s\\backup\\%s", Str(dir), Str(s.slot));
        TempStr stampPath = fmt("%s\\stamp.txt", Str(slotDir));
        i64 last = 0;
        Str stamp = file::ReadFile(Str(stampPath));
        if (stamp) {
            last = ParseInt(stamp);
        }
        if (last > 0 && now - last < (i64)s.ageDays * 24 * 60 * 60 * 1000) {
            continue; // snapshot still fresh enough
        }
        FlashcardStudyBackupRefreshSlot(dir, Str(slotDir));
        TempStr txt = fmt("%lld", (long long)now);
        file::WriteFile(Str(stampPath), Str(txt));
        logf("[fc] Backup - slot '%s' refreshed\n", Str(s.slot));
    }
}

// restore the NEWEST backup slot that has files (tries 1d -> 3d -> 7d) over
// the study dir. Returns the number of restored files (0 = no backup found).
// Called by the Config window's "Restore Backup" hold action.
int FlashcardStudyRecoverBackup() {
    TempStr dir = FlashcardStudyDir();
    for (auto& s : kFcBackupSlots) {
        TempStr slotDir = fmt("%s\\backup\\%s", Str(dir), Str(s.slot));
        int restored = 0;
        WIN32_FIND_DATAW fd{};
        TempStr pat = fmt("%s\\*.json", Str(slotDir));
        HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pat))), &fd);
        if (h == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            TempStr src = fmt("%s\\%s", Str(slotDir), ToUtf8Temp(WStr(fd.cFileName)));
            TempStr dst = fmt("%s\\%s", Str(dir), ToUtf8Temp(WStr(fd.cFileName)));
            if (file::Copy(Str(dst), Str(src), false)) {
                restored++;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        if (restored > 0) {
            logf("[fc] Backup - recovered %d file(s) from slot '%s'\n", restored, Str(s.slot));
            return restored;
        }
    }
    logf("[fc] Backup - recover: no backup slot has files\n");
    return 0;
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

    // atomic write: serialize into a temp file next to the target, then
    // replace in one move — a crash / Drive-sync stall mid-write can never
    // leave a truncated JSON behind
    TempStr tmpPath = str::JoinTemp(path, StrL(".tmp"));
    FILE* f = fopen(tmpPath.s, "wb");
    if (!f) {
        logf("[fc] FlashcardStudySave - ERROR: failed to open file %s\n", Str(filePath));
        logf("FlashcardStudySave: failed to open '%s' for writing\n", tmpPath);
        return;
    }

    // docName (base name of the book) lets the Config window list books by
    // name; docPath (full path) powers the sync markers (moved/renamed
    // detection) and is re-validated by Re-check in that window
    fprintf(f, "{\n  \"version\": %d,\n  \"docName\": ", doc.version);
    WriteJsonStringLiteral(f, path::GetBaseNameTemp(Str(filePath)));
    fprintf(f, ",\n  \"docPath\": ");
    WriteJsonStringLiteral(f, filePath);
    fprintf(f, ",\n  \"states\": {\n");

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

    fclose(f);

    // MOVEFILE_REPLACE_EXISTING makes the swap single-step; fall back to
    // remove+rename if the replace fails (e.g. antivirus holding a handle)
    WCHAR* tmpW = CWStrTemp(ToWStrTemp(tmpPath));
    WCHAR* dstW = CWStrTemp(ToWStrTemp(Str(path)));
    BOOL ok = MoveFileExW(tmpW, dstW, MOVEFILE_REPLACE_EXISTING);
    if (!ok) {
        remove(path.s);
        ok = rename(tmpPath.s, path.s) == 0;
    }
    if (!ok) {
        logf("[fc] FlashcardStudySave - ERROR: failed to replace '%s'\n", Str(path));
        return;
    }

    logf("[fc] FlashcardStudySave - saved %d entries\n", len(doc.states));

    // rolling local backups (1d / 3d / 7d slots) — cheap stamp guard inside
    FlashcardStudyMaybeBackup();
}

// Delete every study-history JSON in the study dir — the review histories
// of ALL books. Called by the Config window's 5s-hold "clear ALL books"
// action. Returns the number of files deleted.
int FlashcardDeleteAllStudyFiles() {
    TempStr dir = FlashcardStudyDir();
    TempStr pattern = path::JoinTemp(dir, StrL("*.json"));
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pattern))), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    int deleted = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        TempStr file = path::JoinTemp(dir, ToUtf8Temp(WStr(fd.cFileName)));
        if (DeleteFileW(CWStrTemp(ToWStrTemp(Str(file))))) {
            deleted++;
        } else {
            logf("[fc] FlashcardDeleteAllStudyFiles - ERROR: failed to delete '%s'\n", Str(file));
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    logf("[fc] FlashcardDeleteAllStudyFiles - deleted %d study file(s)\n", deleted);
    return deleted;
}

// Move every study-history JSON from fromDir to toDir (Config window folder
// change). A file that already exists in toDir is KEPT there untouched (not
// overwritten) — a cloud-synced target dir may already hold the book's file
// from another machine, and the next FlashcardStudySave of an open book will
// write the up-to-date state there anyway. Cross-volume moves (local disk →
// cloud drive) need MOVEFILE_COPY_ALLOWED.
int FlashcardStudyMigrateFiles(Str fromDir, Str toDir) {
    if (!fromDir || !toDir || str::Eq(fromDir, toDir)) {
        return 0;
    }
    TempStr pattern = path::JoinTemp(Str(fromDir), StrL("*.json"));
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pattern))), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        logf("[fc] FlashcardStudyMigrateFiles - nothing to move from '%s'\n", Str(fromDir));
        return 0;
    }
    int moved = 0;
    int kept = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        TempStr src = path::JoinTemp(Str(fromDir), ToUtf8Temp(WStr(fd.cFileName)));
        TempStr dst = path::JoinTemp(Str(toDir), ToUtf8Temp(WStr(fd.cFileName)));
        if (GetFileAttributesW(CWStrTemp(ToWStrTemp(Str(dst)))) != INVALID_FILE_ATTRIBUTES) {
            kept++;
            logf("[fc] FlashcardStudyMigrateFiles - kept existing '%s' in target\n", Str(dst));
            continue;
        }
        if (MoveFileExW(CWStrTemp(ToWStrTemp(Str(src))), CWStrTemp(ToWStrTemp(Str(dst))),
                        MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
            moved++;
            logf("[fc] FlashcardStudyMigrateFiles - moved '%s' -> '%s'\n", Str(src), Str(dst));
        } else {
            logf("[fc] FlashcardStudyMigrateFiles - ERROR: move '%s' failed (lastError=%u)\n", Str(src),
                 (unsigned)GetLastError());
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    logf("[fc] FlashcardStudyMigrateFiles - moved %d, kept %d existing (from '%s' to '%s')\n", moved, kept,
         Str(fromDir), Str(toDir));
    return moved;
}

// Stable per-tab name for [fc] logs: displayName is empty for cmdline-loaded
// docs until the UI assigns it — falls back to the file's base name
TempStr FlashcardTabLogName(WindowTab* tab) {
    if (!tab) {
        return StrL("(no tab)");
    }
    if (tab->displayName) {
        return tab->displayName;
    }
    if (tab->filePath) {
        return path::GetBaseNameTemp(tab->filePath);
    }
    return StrL("(no name)");
}

// Evaluate a page-filter expression into a per-page membership vector
// (pages[p-1] = 1 when page p is included). Syntax: tokens separated by ';'.
// A token is "N" or "N-M" (add pages) with an optional leading '-' to REMOVE
// pages instead. Tokens apply in order, so a later removal carves holes in an
// earlier range: "1-15;20-25;-22-23;" = pages 1-15 plus 20-25, minus 22-23.
// Values are clamped to [1, pageCount]; out-of-range tokens are ignored.
// Returns false when no valid token was found (caller treats that as "all
// pages" — an empty or garbage expression never hides every card).
bool FlashcardFilterEval(Str expr, int pageCount, Vec<u8>& pages) {
    pages.Reset();
    for (int i = 0; i < pageCount; i++) {
        pages.Append(0);
    }
    if (pageCount <= 0) {
        return false;
    }
    bool anyValid = false;
    int nSkipped = 0;
    int i = 0;
    int n = len(expr);
    while (i < n) {
        // skip separators / whitespace before a token
        while (i < n && (expr.s[i] == ';' || expr.s[i] == ' ' || expr.s[i] == '\t')) {
            i++;
        }
        if (i >= n) {
            break;
        }
        bool remove = false;
        if (expr.s[i] == '-') {
            remove = true;
            i++;
        }
        int v = 0;
        bool hasDigit = false;
        while (i < n && expr.s[i] >= '0' && expr.s[i] <= '9') {
            v = v * 10 + (expr.s[i] - '0');
            hasDigit = true;
            i++;
        }
        if (!hasDigit) {
            // garbage between separators: skip to the next ';' and continue
            nSkipped++;
            while (i < n && expr.s[i] != ';') {
                i++;
            }
            continue;
        }
        int from = v;
        int to = v;
        // optional "-M" range end ('-' right after the first number)
        if (i < n && expr.s[i] == '-') {
            i++;
            int w = 0;
            hasDigit = false;
            while (i < n && expr.s[i] >= '0' && expr.s[i] <= '9') {
                w = w * 10 + (expr.s[i] - '0');
                hasDigit = true;
                i++;
            }
            if (hasDigit) {
                to = w;
            }
        }
        if (from > to) {
            int t = from;
            from = to;
            to = t;
        }
        int lo = std::max(1, from);
        int hi = std::min(pageCount, to);
        if (hi >= lo) {
            for (int p = lo; p <= hi; p++) {
                pages[p - 1] = remove ? (u8)0 : (u8)1;
            }
        }
        anyValid = true;
        // skip trailing junk until the next separator
        while (i < n && expr.s[i] != ';') {
            if (expr.s[i] != ' ' && expr.s[i] != '\t') {
                nSkipped++;
            }
            i++;
        }
    }
    logf("[fc] FlashcardFilterEval - '%s' pageCount=%d -> %s (%d skipped)\n", expr, pageCount,
         anyValid ? StrL("valid") : StrL("no valid token"), nSkipped);
    return anyValid;
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
            // skip the key's closing quote: we searched the field NAME only
            // ("rating"), so s[0] is the '"' that ends the key, not the ':'
            if (afterField && afterField.s[0] == '"') {
                afterField = Str(afterField.s + 1);
                str::TrimWSInPlace(afterField, str::TrimOpt::Left);
            }
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
static char* ParseJsonDocNameDup(Str json);
static Str FlashcardStudyAdoptByName(const char* filePath, Str dstPath);

// parse a study JSON (version + states) into doc; shared by StudyLoad and the
// manual resync
static void FlashcardStudyParseJson(Str json, FlashcardStudyDoc& doc) {
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
}

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
        // no history under this md5: the book may have been MOVED on disk
        // (path changed, base name kept). Try to ADOPT a study JSON saved
        // under the old path that carries this book's base name
        json = FlashcardStudyAdoptByName(filePath, Str(studyPath));
        if (!json) {
            return doc;
        }
    }

    FlashcardStudyParseJson(json, doc);

    logf("[fc] FlashcardStudyLoad - loaded %d entries\n", len(doc.states));
    return doc;
}

// Extract a "field": "..." string value from a study JSON (unescape \" and
// \\). Returns nullptr when the field is missing (legacy files written before
// the field existed).
static char* ParseJsonStringFieldDup(Str json, Str fieldName) {
    TempStr quotedT = fmt("\"%s\"", fieldName);
    Str quoted = Str(quotedT);
    int idx = str::IndexOf(json, quoted);
    if (idx < 0) {
        return nullptr;
    }
    Str s = Str(json.s + idx + len(quoted));
    str::TrimWSInPlace(s, str::TrimOpt::Left);
    if (!s || s.s[0] != ':') {
        return nullptr;
    }
    s = Str(s.s + 1);
    str::TrimWSInPlace(s, str::TrimOpt::Left);
    if (!s || s.s[0] != '"') {
        return nullptr;
    }
    s = Str(s.s + 1);
    char buf[512];
    int n = 0;
    while (n < dimofi(buf) - 1 && s.s[0] && s.s[0] != '"') {
        char c = s.s[0];
        if (c == '\\' && len(s) > 1) {
            s = Str(s.s + 1);
            char e = s.s[0];
            if (e == 'n') {
                c = '\n';
            } else if (e == 't') {
                c = '\t';
            } else {
                c = e; // covers \" and \\ (other escapes copied as-is)
            }
        }
        buf[n++] = c;
        s = Str(s.s + 1);
    }
    buf[n] = 0;
    return str::Dup(Str(buf, n)).s;
}

static char* ParseJsonDocNameDup(Str json) {
    return ParseJsonStringFieldDup(json, StrL("docName"));
}

// RESYNC (automatic): no study JSON exists under filePath's md5, so the book
// was probably MOVED on disk (path changed, base name kept). Adopt the study
// JSON whose saved docName equals this book's base name: rename it to the
// current md5 path and hand its contents back to the caller. When several
// files match (same base name in different folders), the newest wins and all
// matches are logged.
static Str FlashcardStudyAdoptByName(const char* filePath, Str dstPath) {
    TempStr docName = path::GetBaseNameTemp(Str(filePath));
    if (!docName || !dstPath) {
        return nullptr;
    }
    TempStr dir = FlashcardStudyDir();
    TempStr pattern = path::JoinTemp(dir, StrL("*.json"));
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pattern))), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    TempStr bestFile = nullptr;
    FILETIME bestTime{};
    bool found = false;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        TempStr file = path::JoinTemp(dir, ToUtf8Temp(WStr(fd.cFileName)));
        Str json = file::ReadFile(Str(file));
        if (!json) {
            continue;
        }
        char* name = ParseJsonDocNameDup(json);
        if (!name) {
            continue;
        }
        bool match = str::EqI(Str(name), Str(docName));
        ::free(name);
        if (!match) {
            continue;
        }
        logf("[fc] StudyLoad - RESYNC candidate: '%s' matches '%s'\n", Str(file), Str(docName));
        if (!found || CompareFileTime(&fd.ftLastWriteTime, &bestTime) > 0) {
            found = true;
            bestTime = fd.ftLastWriteTime;
            bestFile = file;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!found) {
        return nullptr;
    }
    // rename the old-md5 file to the CURRENT md5 path, then read it back
    BOOL ok =
        MoveFileExW(CWStrTemp(ToWStrTemp(Str(bestFile))), CWStrTemp(ToWStrTemp(dstPath)), MOVEFILE_REPLACE_EXISTING);
    if (!ok) {
        logf("[fc] StudyLoad - RESYNC ERROR: failed to rename '%s' (lastError=%u)\n", Str(bestFile),
             (unsigned)GetLastError());
        return nullptr;
    }
    logf("[fc] StudyLoad - RESYNC: adopted '%s' for '%s' (book moved on disk)\n", Str(bestFile), Str(docName));
    // re-save under the CURRENT path so the JSON's docPath (the sync-marker
    // source) stops pointing at the old location
    {
        Str adoptedJson = file::ReadFile(dstPath);
        if (adoptedJson) {
            FlashcardStudyDoc adopted;
            FlashcardStudyParseJson(adoptedJson, adopted);
            FlashcardStudySave(filePath, adopted);
        }
    }
    return file::ReadFile(dstPath);
}

// RESYNC (manual, from the Config window): re-point an existing study JSON
// (its old md5 file name) at a new PDF path — the classic case is a book
// RENAMED on disk, where even the base name changed and the automatic
// adopt-by-docName cannot find it. Parses the old file, Save() rewrites it
// under the new md5 with a fresh docName, then the old file is deleted.
bool FlashcardStudyResync(const char* jsonPath, const char* newPdfPath) {
    if (!jsonPath || !newPdfPath) {
        return false;
    }
    Str json = file::ReadFile(Str(jsonPath));
    if (!json) {
        logf("[fc] RESYNC ERROR: cannot read '%s'\n", Str(jsonPath));
        return false;
    }
    FlashcardStudyDoc doc;
    FlashcardStudyParseJson(json, doc);
    FlashcardStudySave(newPdfPath, doc);
    // delete the old file unless Save() already replaced it in place
    TempStr newPath = FlashcardStudyPath(newPdfPath);
    if (newPath && !str::Eq(Str(jsonPath), Str(newPath))) {
        DeleteFileW(CWStrTemp(ToWStrTemp(Str(jsonPath))));
    }
    logf("[fc] RESYNC: '%s' -> '%s' (%d states)\n", Str(jsonPath), Str(newPdfPath), len(doc.states));
    return true;
}

// Scan the study dir and summarize every *.json into out: name, card count,
// due count and last-review time. Sorted by lastReviewedAt (most recent
// first) so the Config window leads with the book the user is actually
// reviewing.
int FlashcardStudyListDocs(Vec<FlashcardStudyDocInfo>& out) {
    out.Reset();
    TempStr dir = FlashcardStudyDir();
    TempStr pattern = path::JoinTemp(dir, StrL("*.json"));
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pattern))), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        logf("[fc] FlashcardStudyListDocs - no study files in '%s'\n", Str(dir));
        return 0;
    }
    i64 now = (i64)time(nullptr) * 1000;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        TempStr file = path::JoinTemp(dir, ToUtf8Temp(WStr(fd.cFileName)));
        Str json = file::ReadFile(Str(file));
        if (!json) {
            continue;
        }
        FlashcardStudyDocInfo info{};
        info.fileName = str::Dup(ToUtf8Temp(WStr(fd.cFileName))).s;
        info.docName = ParseJsonDocNameDup(json);
        info.docPath = ParseJsonStringFieldDup(json, StrL("docPath"));
        Vec<FlashcardStudyDoc::StateEntry> states;
        ParseJsonObject(json, &states);
        info.totalCards = len(states);
        for (int i = 0; i < len(states); i++) {
            const FlashcardStudyState& st = states[i].state;
            if (st.rating != 0 && st.nextReviewAt <= now) {
                info.dueCount++;
            }
            if (st.lastReviewedAt > info.lastReviewedAt) {
                info.lastReviewedAt = st.lastReviewedAt;
            }
        }
        if (!info.docName) {
            // legacy JSON without docName: show the md5 base so the row is
            // still identifiable
            info.docName = str::Dup(Str(info.fileName ? info.fileName : "")).s;
        }
        // sync marker: 1 = PDF found at the saved path, 2 = missing
        // (moved/renamed), 0 = legacy file without docPath (unknown)
        if (info.docPath && len(info.docPath) > 0) {
            info.syncStatus = file::Exists(Str(info.docPath)) ? 1 : 2;
        }
        TempStr syncStr = info.syncStatus == 1 ? StrL("ok") : (info.syncStatus == 2 ? StrL("MISSING") : StrL("unknown"));
        logf("[fc] StudyListDocs - %s: name='%s' cards=%d due=%d last=%lld sync=%s\n",
             Str(info.fileName ? info.fileName : "?"), Str(info.docName ? info.docName : "?"), info.totalCards,
             info.dueCount, info.lastReviewedAt, Str(syncStr));
        out.Append(info);
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    // sort by lastReviewedAt desc (insertion sort: the list is small)
    for (int i = 1; i < len(out); i++) {
        FlashcardStudyDocInfo key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].lastReviewedAt < key.lastReviewedAt) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
    logf("[fc] FlashcardStudyListDocs - %d document(s) in '%s'\n", len(out), Str(dir));
    return len(out);
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
