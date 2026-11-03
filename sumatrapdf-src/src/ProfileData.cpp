/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

// Profile Memory core (FASE F1): single-file user-data store
// "tumatraPDF-data_1.json" in the profile dir (user-configured
// GlobalPrefs::profileDataDir, else the per-exe app-data dir). It aggregates
// every user-data category — per-book SM-2 study states (imported from the
// legacy <md5>.json files in the flashcard study dir), per-day reading and
// study stats (imported from the two stats sidecars), and per-document time
// counters (pulled from gFileHistory at save) — so export / sync / import can
// treat ONE portable unit. The legacy files keep being written as before and
// act as import / legacy views; the profile is the authoritative superset.
//
// Day entries are attributed per machine ("dateKey": {"machine": sec}) so two
// machines sharing this file through a cloud-synced folder merge exactly:
// each machine replaces only its own rows, and merged getters sum other
// machines' rows on top of the live local module. JSON is hand-written /
// hand-parsed (no library), in the same style as Flashcard.cpp; writes are
// atomic (temp file + MoveFileExW replace) so a crash or a Drive-sync stall
// mid-write can never leave a truncated file behind.

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"

#include "AppTools.h"
#include "Settings.h"
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "Flashcard.h"
#include "FileHistory.h"
#include "ReadingStats.h"
#include "FlashcardStats.h"
#include "ProfileData.h"

// set by -for-testing cmd-line flag (SumatraConfig.cpp); never import during
// test runs to avoid touching live user app-data
extern bool gForTesting;

// the number in the file name is the format version (schema changes bump it)
// NOTE: must be a literal macro — StrL/compile-time length needs a literal
#define kProfileDataFileName StrL("tumatraPDF-data_1.json")
static const int kProfileDataVersion = 1;

// POD day row: one calendar day bucketed by the machine that earned it
struct ProfDayMachine {
    int dateKey = 0; // YYYYMMDD
    int sec = 0;
    int cards = 0;           // study rows only (0 for reading rows)
    char* machine = nullptr; // owned (str::Dup, freed by ProfFreeDays)
};

// per-document counters pulled from gFileHistory at save; docPath is the key
struct ProfDocEntry {
    char* docPath = nullptr; // owned
    int readingSec = 0;
    int fcStudySec = 0;
    int fcCards = 0;
};

// One book's study states inside the profile (strings owned by the profile)
struct ProfileBook {
    char* streamKey = nullptr; // owned: md5-of-PDF-path hex (legacy key)
    char* docName = nullptr;   // owned: base name of the book
    char* docPath = nullptr;   // owned: full path (sync marker)
    FlashcardStudyDoc study;
};

static struct {
    Vec<ProfileBook> books;
    Vec<ProfDayMachine> readingDaily;
    Vec<ProfDayMachine> studyDaily;
    Vec<ProfDocEntry> perDoc;
    i64 lastWriteTs = 0;
    bool loaded = false;
    bool dirty = false;
} gProfile;

static char* gProfileThisMachine = nullptr; // owned, set at first load
static bool gCloudRevertPending = false;    // newest backup slot is strictly newer than primary

static void ProfSaveInner();
static void ProfMaybeBackup();
static void ProfPullHistory();
static void ProfileDataEnsureLoaded();

// forward: used by ProfMergeConflictedCopies before their definitions
static ProfileBook* ProfFindBook(Str keyHex);
static void ProfParseDaySection(Str json, const char* section, Vec<ProfDayMachine>& out);
static bool ProfDayPairPresent(Vec<ProfDayMachine>& v, int dateKey, Str machine);
static void ProfMergeConflictedCopies();

// forward: backup constants/slot dir are defined later (rolling-backups section)
static const int kProfBackupSlots = 9;
static const i64 kProfBackupSlotSec = 8 * 3600;
static TempStr ProfBackupDirTemp();
static i64 ProfSlotLastWriteTs(int slotIdx);

// forward: F7 integrity checksum helpers
static TempStr ProfReadChecksumTemp(Str json);
static TempStr ProfBlankChecksumTemp(Str json);
static TempStr ProfComputeChecksumTemp(Str json);
static TempStr ProfSubstChecksumTemp(Str json, Str hex);

// forward: F7 export/import — ProfCopyDirTree defined later (rolling-backups section)
static int ProfCopyDirTree(Str srcDir, TempStr dstDir);

// ---- small helpers ----

TempStr ProfileDataPathTemp() {
    Str custom = gGlobalPrefs->profileDataDir;
    if (custom && len(custom) > 0) {
        return path::JoinTemp(Str(custom), kProfileDataFileName);
    }
    return GetPathInAppDataDirTemp(kProfileDataFileName);
}

// single parse of the primary profile: version, books, day counts, lastWriteTs.
// false when the primary file does not exist.
bool ProfileDataStats(int* version, int* books, int* readingDays, int* studyDays, i64* lastWriteTs) {
    TempStr path = ProfileDataPathTemp();
    if (!file::Exists(Str(path))) {
        return false;
    }
    ProfileDataEnsureLoaded();
    if (version) {
        *version = kProfileDataVersion;
    }
    if (books) {
        *books = (int)len(gProfile.books);
    }
    if (readingDays) {
        *readingDays = (int)len(gProfile.readingDaily);
    }
    if (studyDays) {
        *studyDays = (int)len(gProfile.studyDaily);
    }
    if (lastWriteTs) {
        *lastWriteTs = gProfile.lastWriteTs;
    }
    return true;
}

// this machine's name (attribution key when two machines share the file)
static char* ProfMachineNameDup() {
    WCHAR buf[64]{};
    DWORD n = dimof(buf);
    if (!GetComputerNameW(buf, &n) || n == 0) {
        return str::Dup(StrL("?")).s;
    }
    return str::Dup(ToUtf8Temp(WStr(buf, (int)n))).s;
}

static bool ProfIsThisMachine(Str machine) {
    return gProfileThisMachine && str::Eq(machine, gProfileThisMachine);
}

// md5-of-path hex key, same derivation as FlashcardStudyPath
static TempStr ProfBookKeyTemp(const char* pdfPath) {
    u8 digest[16];
    CalcMD5Digest(Str(pdfPath), digest);
    return str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));
}

static void ProfFreeBook(ProfileBook& b) {
    ::free(b.streamKey);
    ::free(b.docName);
    ::free(b.docPath);
    b.study.states.Clear();
}

static void ProfFreeBooks(Vec<ProfileBook>& books) {
    for (int i = 0; i < len(books); i++) {
        ProfFreeBook(books[i]);
    }
    books.Clear();
}

static void ProfFreeDays(Vec<ProfDayMachine>& v) {
    for (int i = 0; i < len(v); i++) {
        ::free(v[i].machine);
    }
    v.Clear();
}

static void ProfFreeDocs(Vec<ProfDocEntry>& v) {
    for (int i = 0; i < len(v); i++) {
        ::free(v[i].docPath);
    }
    v.Clear();
}

// ---- hand-rolled JSON parsing (same style as Flashcard.cpp) ----
// The profile wraps the legacy per-book JSON shape verbatim ({"states": {...}})
// so the exported FlashcardStudy parse helpers work on raw slices.

// "name" as a JSON key token (\"name\") in the temp arena — fmt() rejects raw
// char* args, so build it here
// "name" wrapped as a JSON key token ("name") in the temp arena — fmt()
// rejects raw char* args, so build it here
static TempStr ProfQuotedNameTemp(const char* name) {
    str::Builder sb(32);
    sb.AppendChar('"');
    sb.Append(Str(name));
    sb.AppendChar('"');
    return sb.TakeStr();
}

// interior of the '{' ... matching-'}' block whose opening brace is at s[0],
// skipping over string literals (a "}" inside a string never closes). Empty
// Str when s[0] != '{' or the block is unterminated.
static Str ProfBlockInterior(Str s) {
    if (!s || !s.s || s.s[0] != '{') {
        return Str();
    }
    int depth = 0;
    bool inStr = false;
    for (int i = 0; i < len(s); i++) {
        char c = s.s[i];
        if (inStr) {
            if (c == '\\' && i + 1 < len(s)) {
                i++; // skip the escaped char
            } else if (c == '"') {
                inStr = false;
            }
            continue;
        }
        if (c == '"') {
            inStr = true;
        } else if (c == '{') {
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0) {
                return Str(s.s + 1, i - 1);
            }
        }
    }
    return Str();
}

// interior object of the '{' value of "name" in json (skips ws + colon).
// Empty when absent or the value is not an object.
static Str ProfSectionInterior(Str json, const char* name) {
    if (!json.s) {
        return Str();
    }
    TempStr quoted = ProfQuotedNameTemp(name);
    int idx = str::IndexOf(json, quoted);
    if (idx < 0) {
        return Str();
    }
    Str after = Str(json.s + idx + len(quoted));
    str::TrimWSInPlace(after, str::TrimOpt::Left);
    if (!after || after.s[0] != ':') {
        return Str();
    }
    after = Str(after.s + 1);
    str::TrimWSInPlace(after, str::TrimOpt::Left);
    return ProfBlockInterior(after);
}

// value of "\"name\": <n>" where <n> is a JSON number; def when absent/malformed
static i64 ProfNumField(Str json, const char* name, i64 def) {
    if (!json.s) {
        return def;
    }
    TempStr quoted = ProfQuotedNameTemp(name);
    int idx = str::IndexOf(json, quoted);
    if (idx < 0) {
        return def;
    }
    Str after = Str(json.s + idx + len(quoted));
    str::TrimWSInPlace(after, str::TrimOpt::Left);
    if (!after || after.s[0] != ':') {
        return def;
    }
    after = Str(after.s + 1);
    str::TrimWSInPlace(after, str::TrimOpt::Left);
    if (!after.s || len(after) == 0) {
        return def;
    }
    char buf[32]{};
    int n = std::min(len(after), (int)(dimof(buf) - 1));
    memcpy(buf, after.s, (size_t)n);
    char* end = nullptr;
    long long v = _strtoi64(buf, &end, 10);
    if (end == buf) {
        return def;
    }
    return v;
}

// unescape a raw JSON string token (object keys) into an owned heap string
static char* ProfUnescapeKeyDup(Str rawKey) {
    str::Builder sb;
    for (int i = 0; i < len(rawKey); i++) {
        char c = rawKey.s[i];
        if (c == '\\' && i + 1 < len(rawKey)) {
            i++;
            char e = rawKey.s[i];
            if (e == 'n') {
                c = '\n';
            } else if (e == 't') {
                c = '\t';
            } else if (e != 'u') {
                c = e; // covers \" and \\ (unicode escapes don't occur here)
            } else {
                c = '?';
            }
        }
        sb.AppendChar(c);
    }
    // heap-own the result (temp builder lives in the arena)
    return str::Dup(sb.TakeStr()).s;
}

// walk "key": <value> members of an object interior. Values are either an
// object (cb gets objInterior non-empty) or a number token (valueStr holds its
// digits) or a string token (raw, unescaped by the callback as needed)
template <typename T>
static void ProfForEachMember(Str interior, T cb) {
    if (!interior.s) {
        return;
    }
    const char* end = interior.s + len(interior);
    const char* p = interior.s;
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r' || *p == ',')) {
            p++;
        }
        if (p >= end || *p != '"') {
            break;
        }
        const char* keyStart = p + 1;
        const char* keyEnd = keyStart;
        while (keyEnd < end && *keyEnd != '"') {
            keyEnd++;
        }
        if (keyEnd >= end) {
            break;
        }
        Str rawKey(keyStart, (int)(keyEnd - keyStart));
        p = keyEnd + 1;
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r' || *p == ':')) {
            p++;
        }
        if (p >= end) {
            break;
        }
        if (*p == '{') {
            Str val(p, (int)(end - p));
            Str objInterior = ProfBlockInterior(val);
            if (!objInterior.s) {
                break;
            }
            p = objInterior.s + len(objInterior) + 1; // past the closing '}'
            cb(rawKey, StrL(""), objInterior, true);
        } else {
            const char* vEnd = p;
            while (vEnd < end && *vEnd != ',') {
                vEnd++;
            }
            Str value(p, (int)(vEnd - p));
            cb(rawKey, value, Str(), false);
            p = vEnd;
        }
    }
}

// ---- ensure loaded / legacy import ----

// books section: "\"md5Key\": { \"docName\": ..., \"states\": { legacy shape } }"
static void ProfParseBooks(Str json, Vec<ProfileBook>& out) {
    Str sec = ProfSectionInterior(json, "books");
    if (!sec) {
        return;
    }
    ProfForEachMember(sec, [&](Str rawKey, Str valueTok, Str interior, bool isObj) {
        if (!isObj) {
            return;
        }
        ProfileBook b;
        b.streamKey = ProfUnescapeKeyDup(rawKey);
        b.docName = FlashcardStudyParseJsonStringFieldDup(interior, StrL("docName"));
        b.docPath = FlashcardStudyParseJsonStringFieldDup(interior, StrL("docPath"));
        // states: the legacy parser finds "states" inside any JSON text
        FlashcardStudyParseJsonInto(interior, b.study);
        out.Append(b);
    });
}

// legacy sidecar line format: "YYYYMMDD <sec>" (reading) and
// "YYYYMMDD <sec> <cards>" (study); attributed to this machine on import
static void ProfImportSidecar(Str text, Vec<ProfDayMachine>& out, bool isStudy) {
    StrVec lines;
    Split(&lines, text, StrL("\n"), true);
    char* machine = ProfMachineNameDup();
    Str machineStr = Str(machine);
    for (Str line : lines) {
        if (len(line) < 9) {
            continue;
        }
        char buf[64]{};
        int n = std::min(len(line), (int)(dimof(buf) - 1));
        memcpy(buf, line.s, (size_t)n);
        char* p1 = nullptr;
        long dateKeyL = strtol(buf, &p1, 10);
        long secL = 0;
        long cardsL = 0;
        int fields = 0;
        if (p1 != buf) {
            fields = 1;
            char* p2 = p1;
            secL = strtol(p2, &p2, 10);
            if (p2 != p1) {
                fields = 2;
                if (isStudy) {
                    char* p3 = p2;
                    cardsL = strtol(p3, &p3, 10);
                    if (p3 != p2) {
                        fields = 3;
                    }
                }
            }
        }
        if (fields >= 2 && dateKeyL > 19000101 && secL >= 0) {
            ProfDayMachine e;
            e.dateKey = (int)dateKeyL;
            e.sec = (int)secL;
            e.cards = (fields == 3) ? (int)cardsL : 0;
            e.machine = str::Dup(machineStr).s;
            out.Append(e);
        }
    }
    ::free(machine);
}

// pull per-book study JSONs from a specific legacy study dir into outBooks.
// Never deletes anything in dir. Appends without key dedup (caller dedups).
static void ProfImportBooksFromDir(Vec<ProfileBook>& outBooks, TempStr dir) {
    if (!dir) {
        return;
    }
    TempStr pattern = path::JoinTemp(Str(dir), StrL("*.json"));
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pattern))), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        TempStr file = path::JoinTemp(Str(dir), ToUtf8Temp(WStr(fd.cFileName)));
        Str json = file::ReadFile(Str(file));
        if (!json) {
            continue;
        }
        ProfileBook b;
        b.docName = FlashcardStudyParseJsonStringFieldDup(json, StrL("docName"));
        b.docPath = FlashcardStudyParseJsonStringFieldDup(json, StrL("docPath"));
        FlashcardStudyParseJsonInto(json, b.study);
        if (b.docPath) {
            b.streamKey = str::Dup(ProfBookKeyTemp(b.docPath)).s;
        }
        if (len(b.study.states) > 0 || b.docName) {
            outBooks.Append(b);
        } else {
            ProfFreeBook(b); // scratch/legacy file with no usable data
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void ProfImportLegacy() {
    ProfImportBooksFromDir(gProfile.books, FlashcardStudyDir());
    TempStr settingsPath = GetSettingsPathTemp();
    TempStr sideDir = path::GetDirTemp(Str(settingsPath));
    TempStr rcPath = path::JoinTemp(Str(sideDir), StrL("reading-stats.txt"));
    Str rcText = file::ReadFile(Str(rcPath));
    if (rcText) {
        ProfImportSidecar(rcText, gProfile.readingDaily, false);
    }
    TempStr fcPath = path::JoinTemp(Str(sideDir), StrL("flashcard-stats.txt"));
    Str fcText = file::ReadFile(Str(fcPath));
    if (fcText) {
        ProfImportSidecar(fcText, gProfile.studyDaily, true);
    }
    logf("[profile] legacy import: %d book(s), %d reading day(s), %d study day(s)\n", len(gProfile.books),
         len(gProfile.readingDaily), len(gProfile.studyDaily));
}

// F5: one-time silent Portable→Install import. Runs only when the Install
// build has no profile data of its own and the portable exe-adjacent location
// holds data worth adopting (primary json, legacy study books, or non-empty
// sidecars). Reads from the portable side only — never deletes anything there.
// Installed side: books via ProfImportBooksFromDir(portable study dir),
// sidecars via ProfImportSidecar(portable sidecar text), primary json via
// ProfParseBooks-style struct load. Target is empty → set wholesale.
static void ProfImportPortableInstall() {
    if (gForTesting) {
        logf("[profile] portable import: skipped (-for-testing)\n");
        return;
    }
    if (IsRunningInPortableMode()) {
        return;
    }
    Str custom = gGlobalPrefs->profileDataDir;
    if (custom && len(custom) > 0) {
        return; // user chose a custom profile dir — not the default install path
    }

    // ---- portable exe-adjacent location ----
    TempStr portableDir = GetSelfExeDirTemp();
    TempStr portablePrimaryPath = path::JoinTemp(Str(portableDir), kProfileDataFileName);
    Str portablePrimary = file::ReadFile(Str(portablePrimaryPath));
    bool hasPortablePrimary = portablePrimary && ProfBlockInterior(portablePrimary);

    // portable legacy study books: <exeDir>\FlashcardStudy\*.json
    TempStr portableStudyDir = path::JoinTemp(Str(portableDir), StrL("FlashcardStudy"));
    bool hasPortableBooks = false;
    {
        TempStr pat = path::JoinTemp(Str(portableStudyDir), StrL("*.json"));
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pat))), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }
                hasPortableBooks = true;
                break;
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }

    // portable sidecars live next to the portable exe (settings dir == exe dir)
    TempStr portableRcPath = path::JoinTemp(Str(portableDir), StrL("reading-stats.txt"));
    TempStr portableFcPath = path::JoinTemp(Str(portableDir), StrL("flashcard-stats.txt"));
    Str rcText = file::ReadFile(Str(portableRcPath));
    Str fcText = file::ReadFile(Str(portableFcPath));
    bool hasPortableSidecars = (rcText && len(rcText) > 0) || (fcText && len(fcText) > 0);

    if (!hasPortablePrimary && !hasPortableBooks && !hasPortableSidecars) {
        return; // nothing to adopt
    }

    // ---- installed profile must be empty ----
    TempStr installedPath = ProfileDataPathTemp();
    Str installedJson = file::ReadFile(Str(installedPath));
    if (installedJson && ProfBlockInterior(installedJson)) {
        return; // installed primary already exists — don't clobber
    }
    bool installedHasBooks = false;
    {
        TempStr installedStudyDir = FlashcardStudyDir();
        TempStr pat = path::JoinTemp(Str(installedStudyDir), StrL("*.json"));
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pat))), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }
                installedHasBooks = true;
                break;
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    if (installedHasBooks) {
        return; // installed already has legacy books — don't clobber
    }

    // ---- adopt from portable (target empty → set wholesale) ----
    int importedBooks = 0;
    int importedReadingDays = 0;
    int importedStudyDays = 0;

    if (hasPortablePrimary) {
        ProfParseBooks(portablePrimary, gProfile.books);
        ProfParseDaySection(portablePrimary, "readingStats", gProfile.readingDaily);
        ProfParseDaySection(portablePrimary, "fcStats", gProfile.studyDaily);
        gProfile.dirty = true;
    }
    ProfImportBooksFromDir(gProfile.books, portableStudyDir);
    if (rcText) {
        ProfImportSidecar(rcText, gProfile.readingDaily, false);
    }
    if (fcText) {
        ProfImportSidecar(fcText, gProfile.studyDaily, true);
    }

    importedBooks = (int)len(gProfile.books);
    importedReadingDays = (int)len(gProfile.readingDaily);
    importedStudyDays = (int)len(gProfile.studyDaily);

    if (gProfile.dirty) {
        ProfileDataSaveNow();
    }

    logf("[profile] portable import: %d book(s), %d reading day(s), %d study day(s)\n", importedBooks,
         importedReadingDays, importedStudyDays);
}

// Startup reconcile: adopt day keys from the legacy sidecars that are absent
// from the profile. Never overwrites existing rows — zero double-counting.
// Running twice admits 0/0/0 the second time (idempotent).
//
// Books are deliberately NOT imported here: ProfImportBooksFromDir appends
// without key dedup, so calling it again would double already-present books.
// Books merge on first contact via ProfileDataMergeStudyDoc instead.
static void ProfReconcileSidecars() {
    TempStr settingsPath = GetSettingsPathTemp();
    TempStr sideDir = path::GetDirTemp(Str(settingsPath));
    int addedBooks = 0;
    int addedReadingDays = 0;
    int addedStudyDays = 0;

    // reading sidecar: "YYYYMMDD <sec>"
    TempStr rcPath = path::JoinTemp(Str(sideDir), StrL("reading-stats.txt"));
    Str rcText = file::ReadFile(Str(rcPath));
    if (rcText) {
        StrVec lines;
        Split(&lines, rcText, StrL("\n"), true);
        char* machine = ProfMachineNameDup();
        Str machineStr = Str(machine);
        for (Str line : lines) {
            if (len(line) < 9) {
                continue;
            }
            char buf[64]{};
            int n = std::min(len(line), (int)(dimof(buf) - 1));
            memcpy(buf, line.s, (size_t)n);
            char* p1 = nullptr;
            long dateKeyL = strtol(buf, &p1, 10);
            long secL = 0;
            if (p1 != buf) {
                char* p2 = p1;
                secL = strtol(p2, &p2, 10);
            }
            if (dateKeyL > 19000101 && secL >= 0) {
                int dateKey = (int)dateKeyL;
                bool have = false;
                for (int i = 0; i < len(gProfile.readingDaily); i++) {
                    if (gProfile.readingDaily[i].dateKey == dateKey &&
                        str::EqI(Str(gProfile.readingDaily[i].machine), machineStr)) {
                        have = true;
                        break;
                    }
                }
                if (!have) {
                    ProfDayMachine e;
                    e.dateKey = dateKey;
                    e.sec = (int)secL;
                    e.cards = 0;
                    e.machine = str::Dup(machineStr).s;
                    gProfile.readingDaily.Append(e);
                    addedReadingDays++;
                    gProfile.dirty = true;
                }
            }
        }
        ::free(machine);
    }

    // study sidecar: "YYYYMMDD <sec> <cards>"
    TempStr fcPath = path::JoinTemp(Str(sideDir), StrL("flashcard-stats.txt"));
    Str fcText = file::ReadFile(Str(fcPath));
    if (fcText) {
        StrVec lines;
        Split(&lines, fcText, StrL("\n"), true);
        char* machine = ProfMachineNameDup();
        Str machineStr = Str(machine);
        for (Str line : lines) {
            if (len(line) < 9) {
                continue;
            }
            char buf[64]{};
            int n = std::min(len(line), (int)(dimof(buf) - 1));
            memcpy(buf, line.s, (size_t)n);
            char* p1 = nullptr;
            long dateKeyL = strtol(buf, &p1, 10);
            long secL = 0;
            long cardsL = 0;
            if (p1 != buf) {
                char* p2 = p1;
                secL = strtol(p2, &p2, 10);
                if (p2 != p1) {
                    char* p3 = p2;
                    cardsL = strtol(p3, &p3, 10);
                }
            }
            if (dateKeyL > 19000101 && secL >= 0) {
                int dateKey = (int)dateKeyL;
                bool have = false;
                for (int i = 0; i < len(gProfile.studyDaily); i++) {
                    if (gProfile.studyDaily[i].dateKey == dateKey &&
                        str::EqI(Str(gProfile.studyDaily[i].machine), machineStr)) {
                        have = true;
                        break;
                    }
                }
                if (!have) {
                    ProfDayMachine e;
                    e.dateKey = dateKey;
                    e.sec = (int)secL;
                    e.cards = (int)cardsL;
                    e.machine = str::Dup(machineStr).s;
                    gProfile.studyDaily.Append(e);
                    addedStudyDays++;
                    gProfile.dirty = true;
                }
            }
        }
        ::free(machine);
    }

    logf("[profile] reconcile: +%d book(s), +%d reading day(s), +%d study day(s)\n", addedBooks, addedReadingDays,
         addedStudyDays);
}

// Google Drive may drop conflicted copies next to the primary:
// tumatraPDF-data_1 (1).json, tumatraPDF-data_1 (2).json, ... (pattern:
// <base> (N).json). Merge them into the primary without ever overwriting:
// adopt only book keys and (dateKey, machine) day rows absent from primary.
// Conflicted copies are never deleted / renamed / moved. Idempotent (2nd run
// adopts 0).
static bool ProfDayPairPresent(Vec<ProfDayMachine>& v, int dateKey, Str machine) {
    for (int i = 0; i < len(v); i++) {
        if (v[i].dateKey == dateKey && str::EqI(Str(v[i].machine), machine)) {
            return true;
        }
    }
    return false;
}

static void ProfMergeConflictedCopies() {
    TempStr path = ProfileDataPathTemp();
    TempStr dir = path::GetDirTemp(Str(path));
    TempStr pattern = path::JoinTemp(Str(dir), StrL("tumatraPDF-data_*.json"));
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pattern))), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    int addedBooks = 0;
    int addedDayRows = 0;
    int copies = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        TempStr fileName = path::GetBaseNameTemp(ToUtf8Temp(WStr(fd.cFileName)));
        if (!fileName || str::Eq(fileName, kProfileDataFileName)) {
            continue; // primary itself
        }
        copies++;
        TempStr filePath = path::JoinTemp(Str(dir), ToUtf8Temp(WStr(fd.cFileName)));
        Str copyJson = file::ReadFile(Str(filePath));
        if (!copyJson || !ProfBlockInterior(copyJson)) {
            logf("[profile] conflicted-copy %s unparseable, skipped\n", Str(fileName));
            continue;
        }
        // books: adopt keys absent from primary (never overwrite existing rows)
        Str booksSec = ProfSectionInterior(copyJson, "books");
        if (booksSec) {
            ProfForEachMember(booksSec, [&](Str rawKey, Str valueTok, Str interior, bool isObj) {
                if (!isObj) {
                    return;
                }
                if (ProfFindBook(rawKey)) {
                    return;
                }
                ProfileBook b;
                b.streamKey = ProfUnescapeKeyDup(rawKey);
                b.docName = FlashcardStudyParseJsonStringFieldDup(interior, StrL("docName"));
                b.docPath = FlashcardStudyParseJsonStringFieldDup(interior, StrL("docPath"));
                FlashcardStudyParseJsonInto(interior, b.study);
                gProfile.books.Append(b);
                addedBooks++;
                gProfile.dirty = true;
            });
        }
        // day rows: adopt (dateKey, machine) pairs absent from primary sections
        for (const char* secName : {"readingStats", "fcStats"}) {
            Vec<ProfDayMachine> tempDays;
            ProfParseDaySection(copyJson, secName, tempDays);
            Vec<ProfDayMachine>& target = (secName[0] == 'r') ? gProfile.readingDaily : gProfile.studyDaily;
            for (int i = 0; i < len(tempDays); i++) {
                ProfDayMachine& e = tempDays[i];
                if (!ProfDayPairPresent(target, e.dateKey, Str(e.machine))) {
                    target.Append(e);
                    e.machine = nullptr; // ownership transferred to primary
                    addedDayRows++;
                    gProfile.dirty = true;
                }
            }
            ProfFreeDays(tempDays);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    logf("[profile] conflicted-copy merge: +%d book(s), +%d day row(s) from %d copy/copies\n", addedBooks, addedDayRows,
         copies);
}

// ---- F7: export / import ----

// Export primary + backup/ subtree to destDir. Create dest recursively.
// Fail (false) if primary missing or dest primary already exists (caller decides UX).
bool ProfileDataExportToDir(const char* destDir) {
    if (!destDir || len(Str(destDir)) == 0) {
        return false;
    }
    TempStr primarySrc = ProfileDataPathTemp();
    if (!file::Exists(Str(primarySrc))) {
        logf("[profile] export: primary missing, nothing to export\n");
        return false;
    }
    TempStr destPrimary = path::JoinTemp(Str(destDir), kProfileDataFileName);
    if (file::Exists(Str(destPrimary))) {
        logf("[profile] export: dest primary already exists '%s', refusing\n", Str(destPrimary));
        return false;
    }
    if (!dir::CreateAll(Str(destDir))) {
        logf("[profile] export: failed to create dest dir '%s'\n", Str(destDir));
        return false;
    }
    if (!file::Copy(Str(destPrimary), Str(primarySrc), true)) {
        logf("[profile] export: failed to copy primary to '%s'\n", Str(destPrimary));
        return false;
    }
    TempStr backupSrc = path::JoinTemp(ProfBackupDirTemp(), StrL("")); // backup/ dir
    int copiedJson = 1;
    int copiedBackup = 0;
    if (file::Exists(Str(backupSrc))) {
        copiedBackup = ProfCopyDirTree(Str(backupSrc), path::JoinTemp(Str(destDir), StrL("backup")));
    }
    logf("[profile] export: %d json file(s), %d backup file(s) -> '%s'\n", copiedJson, copiedBackup, Str(destDir));
    return true;
}

// Import profile JSON from srcJsonPath.
// replace=true: move current primary to <profileDir>\tumatraPDF-data_1.json.pre-import,
//               copy src → primary, reload profile.
// replace=false: merge newest-wins-adopt-absent (book keys + day-row pairs), dirty+save.
// src must parse (ProfBlockInterior). Old-format files without checksum are fine — pure parse adopt.
// Guard gForTesting outside (UI layer), not here.
bool ProfileDataImportFromJson(const char* srcJsonPath, bool replace) {
    if (!srcJsonPath || len(Str(srcJsonPath)) == 0) {
        return false;
    }
    Str srcJson = file::ReadFile(Str(srcJsonPath));
    if (!srcJson || !ProfBlockInterior(srcJson)) {
        logf("[profile] import: src unparseable '%s'\n", Str(srcJsonPath));
        return false;
    }
    TempStr primaryDst = ProfileDataPathTemp();
    TempStr profileDir = path::GetDirTemp(Str(primaryDst));
    if (replace) {
        // move current primary to .pre-import (overwrite old .pre-import)
        TempStr preImport = path::JoinTemp(Str(profileDir), StrL("tumatraPDF-data_1.json.pre-import"));
        if (file::Exists(Str(primaryDst))) {
            if (!file::Copy(Str(preImport), Str(primaryDst), true)) {
                logf("[profile] import: failed to pre-import backup '%s'\n", Str(preImport));
                return false;
            }
        }
        // copy src → primary (keep source)
        if (!file::Copy(Str(primaryDst), Str(srcJson), true)) {
            logf("[profile] import: failed to copy src to primary '%s'\n", Str(primaryDst));
            return false;
        }
        // drop in-memory state, reload from primary
        gProfile.loaded = false;
        ProfFreeBooks(gProfile.books);
        ProfFreeDays(gProfile.readingDaily);
        ProfFreeDays(gProfile.studyDaily);
        ProfFreeDocs(gProfile.perDoc);
        ::free(gProfileThisMachine);
        gProfileThisMachine = nullptr;
        ProfileDataEnsureLoaded();
        logf("[profile] import: replaced primary from '%s', reloaded\n", Str(srcJsonPath));
        return true;
    }
    // merge: adopt absent book keys + absent (dateKey, machine) day-row pairs
    int addedBooks = 0;
    int addedDayRows = 0;
    // books
    Str booksSec = ProfSectionInterior(srcJson, "books");
    if (booksSec) {
        ProfForEachMember(booksSec, [&](Str rawKey, Str valueTok, Str interior, bool isObj) {
            if (!isObj) {
                return;
            }
            if (ProfFindBook(rawKey)) {
                return;
            }
            ProfileBook b;
            b.streamKey = ProfUnescapeKeyDup(rawKey);
            b.docName = FlashcardStudyParseJsonStringFieldDup(interior, StrL("docName"));
            b.docPath = FlashcardStudyParseJsonStringFieldDup(interior, StrL("docPath"));
            FlashcardStudyParseJsonInto(interior, b.study);
            gProfile.books.Append(b);
            addedBooks++;
            gProfile.dirty = true;
        });
    }
    // day rows: readingStats + fcStats
    for (const char* secName : {"readingStats", "fcStats"}) {
        Vec<ProfDayMachine> tempDays;
        ProfParseDaySection(srcJson, secName, tempDays);
        Vec<ProfDayMachine>& target = (secName[0] == 'r') ? gProfile.readingDaily : gProfile.studyDaily;
        for (int i = 0; i < len(tempDays); i++) {
            ProfDayMachine& e = tempDays[i];
            bool have = false;
            for (int j = 0; j < len(target); j++) {
                if (target[j].dateKey == e.dateKey && str::EqI(Str(target[j].machine), Str(e.machine))) {
                    have = true;
                    break;
                }
            }
            if (!have) {
                target.Append(e);
                e.machine = nullptr; // ownership transferred
                addedDayRows++;
                gProfile.dirty = true;
            }
        }
        ProfFreeDays(tempDays);
    }
    if (gProfile.dirty) {
        ProfileDataSaveNow();
    }
    logf("[profile] import: merge from '%s': +%d book(s), +%d day row(s)\n", Str(srcJsonPath), addedBooks,
         addedDayRows);
    return true;
}

static void ProfParseDaySection(Str json, const char* section, Vec<ProfDayMachine>& out) {
    Str sec = ProfSectionInterior(json, section);
    if (!sec) {
        return;
    }
    // one member per day: "YYYYMMDD": { machine rows }
    ProfForEachMember(sec, [&](Str rawKey, Str valueTok, Str dayInterior, bool isObj) {
        if (!isObj) {
            return;
        }
        char kbuf[24]{};
        int n = std::min(len(rawKey), (int)(dimof(kbuf) - 1));
        memcpy(kbuf, rawKey.s, (size_t)n);
        i64 dateKey = _strtoi64(kbuf, nullptr, 10);
        if (dateKey < 19000101) {
            return;
        }
        // day rows: "machine": number (reading) | {"sec","cards"} (study)
        ProfForEachMember(dayInterior, [&](Str machKey, Str valTok, Str rowInterior, bool isRowObj) {
            ProfDayMachine e;
            e.dateKey = (int)dateKey;
            e.machine = ProfUnescapeKeyDup(machKey);
            if (isRowObj) {
                e.sec = (int)ProfNumField(rowInterior, "sec", 0);
                e.cards = (int)ProfNumField(rowInterior, "cards", 0);
            } else {
                char vbuf[32]{};
                int vn = std::min(len(valTok), (int)(dimof(vbuf) - 1));
                memcpy(vbuf, valTok.s, (size_t)vn);
                e.sec = (int)_strtoi64(vbuf, nullptr, 10);
            }
            out.Append(e);
        });
    });
}

// perDoc section: "\"<docPath>\": {\"readingSec\": n, \"fcStudySec\": n, \"fcCards\": n}"
static void ProfParseInfoSection(Str json, Vec<ProfDocEntry>& out) {
    Str sec = ProfSectionInterior(json, "perDoc");
    if (!sec) {
        return;
    }
    ProfForEachMember(sec, [&](Str rawKey, Str valueTok, Str interior, bool isObj) {
        if (!isObj) {
            return;
        }
        ProfDocEntry e;
        e.docPath = ProfUnescapeKeyDup(rawKey);
        e.readingSec = (int)ProfNumField(interior, "readingSec", 0);
        e.fcStudySec = (int)ProfNumField(interior, "fcStudySec", 0);
        e.fcCards = (int)ProfNumField(interior, "fcCards", 0);
        out.Append(e);
    });
}

// ---- F7: integrity checksum for tumatraPDF-data_1.json ----
// Checksum = MD5 hex of the file content with the checksum value blanked to "".
// On write: build content with "checksum": "", hash it, then substitute the real hash.
// On load: blank the stored checksum value, re-hash, compare. Mismatch => corrupt.
// Absent/empty checksum token (old-format files) => skip verify, log once.

// Extract the checksum hex string from JSON text. Empty Str when absent/malformed.
static TempStr ProfReadChecksumTemp(Str json) {
    TempStr quoted = ProfQuotedNameTemp("checksum");
    int idx = str::IndexOf(json, quoted);
    if (idx < 0) {
        return Str();
    }
    Str after = Str(json.s + idx + len(quoted));
    str::TrimWSInPlace(after, str::TrimOpt::Left);
    if (!after || after.s[0] != ':') {
        return Str();
    }
    after = Str(after.s + 1);
    str::TrimWSInPlace(after, str::TrimOpt::Left);
    if (!after || after.s[0] != '"') {
        return Str();
    }
    const char* p = after.s + 1;
    const char* end = after.s + len(after);
    while (p < end && *p != '"') {
        p++;
    }
    if (p >= end) {
        return Str();
    }
    int n = (int)(p - after.s - 1);
    char buf[65]{};
    n = std::min(n, 64);
    memcpy(buf, after.s + 1, (size_t)n);
    return Str(buf, n);
}

// Return a copy of json with the checksum value replaced by empty string "".
static TempStr ProfBlankChecksumTemp(Str json) {
    TempStr quoted = ProfQuotedNameTemp("checksum");
    int idx = str::IndexOf(json, quoted);
    if (idx < 0) {
        return Str(json); // no checksum field -> nothing to blank
    }
    Str after = Str(json.s + idx + len(quoted));
    str::TrimWSInPlace(after, str::TrimOpt::Left);
    if (!after || after.s[0] != ':') {
        return Str(json);
    }
    after = Str(after.s + 1);
    str::TrimWSInPlace(after, str::TrimOpt::Left);
    if (!after || after.s[0] != '"') {
        return Str(json);
    }
    const char* p = after.s + 1;
    const char* end = after.s + len(after);
    while (p < end && *p != '"') {
        p++;
    }
    if (p >= end) {
        return Str(json);
    }
    int valStart = (int)((after.s + 1) - json.s); // position of first char inside quotes
    int valEnd = (int)(p - json.s);               // position of closing quote
    str::Builder sb(len(json));
    sb.Append(Str(json.s, valStart)); // everything before the value
    sb.AppendChar('"');
    sb.AppendChar('"');                                          // empty value
    sb.Append(Str(json.s + valEnd + 1, len(json) - valEnd - 1)); // after closing quote
    return sb.TakeStr();
}

// Compute MD5 checksum of json with the checksum value blanked out.
static TempStr ProfComputeChecksumTemp(Str json) {
    TempStr blanked = ProfBlankChecksumTemp(json);
    if (!blanked) {
        return Str();
    }
    u8 digest[16];
    CalcMD5Digest(blanked, digest);
    return str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));
}

// Replace "checksum": "" with "checksum": "<hex>" in json text.
static TempStr ProfSubstChecksumTemp(Str json, Str hex) {
    TempStr quoted = ProfQuotedNameTemp("checksum");
    int idx = str::IndexOf(json, quoted);
    if (idx < 0) {
        return Str(json);
    }
    Str after = Str(json.s + idx + len(quoted));
    str::TrimWSInPlace(after, str::TrimOpt::Left);
    if (!after || after.s[0] != ':') {
        return Str(json);
    }
    after = Str(after.s + 1);
    str::TrimWSInPlace(after, str::TrimOpt::Left);
    if (!after || after.s[0] != '"') {
        return Str(json);
    }
    const char* p = after.s + 1;
    const char* end = after.s + len(after);
    while (p < end && *p != '"') {
        p++;
    }
    if (p >= end) {
        return Str(json);
    }
    int valStart = (int)((after.s + 1) - json.s); // position of first char inside quotes
    int valEnd = (int)(p - json.s);               // position of closing quote
    str::Builder sb(len(json));
    sb.Append(Str(json.s, valStart)); // everything up to opening quote
    sb.AppendChar('"');
    sb.Append(hex);
    sb.AppendChar('"');
    sb.Append(Str(json.s + valEnd + 1, len(json) - valEnd - 1)); // after closing quote
    return sb.TakeStr();
}

// ---- F4: profileDataDir setter — ensure dir exists + migrate on dir change ----

// recursively copy every file in srcDir into dstDir (dstDir is created).
// Never deletes anything in srcDir; files already in dstDir are kept
// (dontOverwrite). Returns number of files copied.
static int ProfCopyDirTree(Str srcDir, TempStr dstDir) {
    dir::CreateAll(Str(dstDir));
    int copied = 0;
    TempStr pat = path::JoinTemp(srcDir, StrL("*.*"));
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pat))), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        TempStr name = ToUtf8Temp(WStr(fd.cFileName));
        if (str::Eq(Str(name), StrL(".")) || str::Eq(Str(name), StrL(".."))) {
            continue;
        }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            copied += ProfCopyDirTree(path::JoinTemp(srcDir, name), path::JoinTemp(Str(dstDir), name));
            continue;
        }
        TempStr srcFile = path::JoinTemp(srcDir, name);
        TempStr dstFile = path::JoinTemp(Str(dstDir), name);
        if (file::Copy(Str(dstFile), Str(srcFile), true)) {
            copied++;
        } else {
            logf("[profile] F4: failed to copy '%s' -> '%s'\n", Str(srcFile), Str(dstFile));
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return copied;
}

// ensure the profile dir exists (mkdir + logf when created) and, when
// profileDataDir points at a folder other than the default app-data dir,
// migrate the existing profile (+ backup/ subtree) into it on first load.
// Only migrates when the new dir has no primary profile file
// (overwrite-vs-use-existing is an explicit deferred UX decision). Never
// deletes the source. Returns the (ensured) profile file path.
static TempStr ProfEnsureProfileDirAndMigrate() {
    TempStr newPath = ProfileDataPathTemp(); // <newDir>\tumatraPDF-data_1.json
    TempStr newDir = path::GetDirTemp(Str(newPath));
    TempStr oldDir = GetAppDataDirTemp(); // previous/default location
    if (!newDir || !oldDir || str::Eq(Str(newDir), Str(oldDir))) {
        // default app-data dir: nothing to migrate, just make sure it exists
        if (newDir && !dir::CreateAll(Str(newDir))) {
            logf("[profile] F4: could not create profile dir '%s'\n", Str(newDir));
        }
        return newPath;
    }
    bool created = !file::Exists(Str(newDir));
    if (created && !dir::CreateAll(Str(newDir))) {
        logf("[profile] F4: could not create profile dir '%s'\n", Str(newDir));
        return newPath;
    }
    if (created) {
        logf("[profile] F4: created profile dir '%s'\n", Str(newDir));
    }
    // new dir already has a primary profile -> keep it, no migration
    TempStr primaryDst = path::JoinTemp(Str(newDir), kProfileDataFileName);
    if (file::Exists(Str(primaryDst))) {
        logf("[profile] F4: new dir already has primary profile, keeping it (no migrate)\n");
        return newPath;
    }
    // old dir must hold data to migrate from
    TempStr primarySrc = path::JoinTemp(Str(oldDir), kProfileDataFileName);
    if (!file::Exists(Str(primarySrc))) {
        logf("[profile] F4: no primary profile in old dir '%s', nothing to migrate\n", Str(oldDir));
        return newPath;
    }
    // copy every tumatraPDF-data_*.json from oldDir to newDir (incl. conflicted copies)
    int copiedJson = 0;
    TempStr pat = path::JoinTemp(Str(oldDir), StrL("tumatraPDF-data_*.json"));
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(CWStrTemp(ToWStrTemp(Str(pat))), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            TempStr name = ToUtf8Temp(WStr(fd.cFileName));
            TempStr srcFile = path::JoinTemp(Str(oldDir), name);
            TempStr dstFile = path::JoinTemp(Str(newDir), name);
            if (file::Copy(Str(dstFile), Str(srcFile), true)) {
                copiedJson++;
            } else {
                logf("[profile] F4: failed to copy '%s' -> '%s'\n", Str(srcFile), Str(dstFile));
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    // copy the backup/ subtree (slot dirs with tumatraPDF-data_1.json + stamps)
    int copiedBackup = 0;
    TempStr oldBackup = path::JoinTemp(Str(oldDir), StrL("backup"));
    if (file::Exists(Str(oldBackup))) {
        copiedBackup = ProfCopyDirTree(Str(oldBackup), path::JoinTemp(Str(newDir), StrL("backup")));
    }
    logf("[profile] F4: migrated profile from '%s' to '%s' (%d json file(s), %d backup file(s); source kept)\n",
         Str(oldDir), Str(newDir), copiedJson, copiedBackup);
    return newPath;
}

static void ProfileDataEnsureLoaded() {
    if (gProfile.loaded) {
        return;
    }
    gProfile.loaded = true;
    gProfileThisMachine = ProfMachineNameDup();
    // F4: ensure the profile dir exists and, on a dir change, migrate the
    // existing profile (+ backup/) from the old location before reading the
    // primary file from the new location.
    TempStr path = ProfEnsureProfileDirAndMigrate();
    // F5: one-time Portable→Install import (after F4 migrate, before reading
    // primary / reconcile). Only runs when the installed profile is empty and
    // the portable exe-adjacent location has data worth adopting.
    ProfImportPortableInstall();
    logf("[profile] loading %s\n", Str(path));
    Str json = file::ReadFile(Str(path));
    bool primaryParsedOk = json && ProfBlockInterior(json);
    // F7: integrity checksum verification (only when checksum token present).
    // Absent/empty checksum token (old-format files) => skip verify, log once.
    if (primaryParsedOk) {
        TempStr storedChecksum = ProfReadChecksumTemp(json);
        if (len(storedChecksum) > 0) {
            TempStr expectedChecksum = ProfComputeChecksumTemp(json);
            if (!expectedChecksum || !str::Eq(Str(storedChecksum), Str(expectedChecksum))) {
                logf("[profile] checksum mismatch — primary corrupt (ts=%lld)\n",
                     (long long)ProfNumField(json, "lastWriteTs", 0));
                primaryParsedOk = false;
            }
        } else {
            logf("[profile] checksum absent — skipping verify (old-format file)\n");
        }
    }
    if (!primaryParsedOk) {
        // file missing or corrupt: try newest valid backup slot before legacy import
        bool recovered = false;
        for (int i = 0; i < kProfBackupSlots; i++) {
            TempStr slotDir = path::JoinTemp(ProfBackupDirTemp(), fmt("%02d", i));
            TempStr srcPath = path::JoinTemp(Str(slotDir), kProfileDataFileName);
            if (!file::Exists(Str(srcPath))) {
                continue;
            }
            Str slotJson = file::ReadFile(Str(srcPath));
            if (slotJson && ProfBlockInterior(slotJson)) {
                logf("[profile] main file corrupt — recovered from backup slot %d\n", i);
                json = slotJson;
                recovered = true;
                break;
            }
        }
        if (!recovered) {
            ProfImportLegacy();
            gProfile.dirty = true;
            ProfPullHistory();
            ProfSaveInner();
            ProfMaybeBackup();
            gProfile.dirty = false;
            ProfReconcileSidecars();
            ProfMergeConflictedCopies();
            return;
        }
    }
    // cloud-revert detection: newest backup slot strictly newer than the primary
    if (primaryParsedOk) {
        i64 primaryTs = ProfNumField(json, "lastWriteTs", 0);
        gProfile.lastWriteTs = primaryTs;
        i64 newestBackupTs = 0;
        for (int i = 0; i < kProfBackupSlots; i++) {
            i64 t = ProfSlotLastWriteTs(i);
            if (t > newestBackupTs) {
                newestBackupTs = t;
            }
        }
        if (newestBackupTs > primaryTs) {
            gCloudRevertPending = true;
            logf("[profile] cloud revert detected: primary ts=%lld older than backup ts=%lld\n", (long long)primaryTs,
                 (long long)newestBackupTs);
        }
    }
    ProfParseBooks(json, gProfile.books);
    ProfParseDaySection(json, "readingStats", gProfile.readingDaily);
    ProfParseDaySection(json, "fcStats", gProfile.studyDaily);
    ProfParseInfoSection(json, gProfile.perDoc);
    logf("[profile] loaded: %d book(s), %d reading day(s), %d study day(s)\n", len(gProfile.books),
         len(gProfile.readingDaily), len(gProfile.studyDaily));
    ProfReconcileSidecars();
    ProfMergeConflictedCopies();
}

// ---- merge / update books ----

static ProfileBook* ProfFindBook(Str keyHex) {
    if (!keyHex.s || len(keyHex) == 0) {
        return nullptr;
    }
    for (int i = 0; i < len(gProfile.books); i++) {
        if (str::EqI(Str(gProfile.books[i].streamKey), keyHex)) {
            return &gProfile.books[i];
        }
    }
    return nullptr;
}

// deep copy: never bit-copy the caller's Vec (it may keep appending in place)
static void ProfCopyStates(const FlashcardStudyDoc& src, FlashcardStudyDoc& dst) {
    dst.version = src.version;
    dst.states.Clear();
    for (int i = 0; i < len(src.states); i++) {
        dst.states.Append(src.states[i]);
    }
}

static void ProfSetBookIdentity(ProfileBook& b, const char* pdfPath) {
    ::free(b.docName);
    b.docName = str::Dup(path::GetBaseNameTemp(Str(pdfPath))).s;
    if (!*b.docName) {
        ::free(b.docName);
        b.docName = nullptr;
    }
    ::free(b.docPath);
    b.docPath = str::Dup(pdfPath).s;
}

static void ProfReplaceBookByKey(const char* pdfPath, const FlashcardStudyDoc& doc, TempStr keyT) {
    ProfileBook* b = ProfFindBook(keyT);
    if (!b) {
        ProfileBook nb;
        nb.streamKey = str::Dup(Str(keyT)).s;
        gProfile.books.Append(nb);
        b = &gProfile.books[len(gProfile.books) - 1];
    }
    ProfSetBookIdentity(*b, pdfPath);
    ProfCopyStates(doc, b->study);
    gProfile.dirty = true;
}

// the caller just SAVED this book's study states into the legacy storage; the
// profile row is a full replace (this doc is the fresh source of those states)
void ProfileDataUpdateStudyDoc(const char* pdfPath, const FlashcardStudyDoc& doc) {
    if (!pdfPath) {
        return;
    }
    ProfileDataEnsureLoaded();
    ProfReplaceBookByKey(pdfPath, doc, ProfBookKeyTemp(pdfPath));
    gProfile.dirty = true;
    ProfileDataSaveNow();
}

// the caller loaded a book from the legacy storage; newest-wins merge per
// state key so two machines' histories converge instead of clobbering each other
void ProfileDataMergeStudyDoc(const char* pdfPath, const FlashcardStudyDoc& doc) {
    if (!pdfPath) {
        return;
    }
    ProfileDataEnsureLoaded();
    TempStr keyT = ProfBookKeyTemp(pdfPath);
    ProfileBook* b = ProfFindBook(keyT);
    if (!b) {
        // first contact with this book: adopt fully (replace path)
        ProfReplaceBookByKey(pdfPath, doc, keyT);
        ProfileDataSaveNow();
        return;
    }
    for (int j = 0; j < len(doc.states); j++) {
        const FlashcardStudyDoc::StateEntry& se = doc.states[j];
        bool merged = false;
        for (int k = 0; k < len(b->study.states); k++) {
            if (b->study.states[k].key != se.key) {
                continue;
            }
            // newer review wins (equal timestamps → incoming, keeps parity)
            if (se.state.lastReviewedAt >= b->study.states[k].state.lastReviewedAt) {
                b->study.states[k].state = se.state;
                merged = true;
            }
            break;
        }
        if (!merged) {
            b->study.states.Append(se);
        }
    }
    ProfSetBookIdentity(*b, pdfPath);
    gProfile.dirty = true;
    ProfileDataSaveNow();
}

// hand the profile study states back to the book loader (legacy file absent)
bool ProfileDataGetStudyDoc(const char* pdfPath, FlashcardStudyDoc& out) {
    ProfileDataEnsureLoaded();
    if (!pdfPath) {
        return false;
    }
    TempStr keyT = ProfBookKeyTemp(pdfPath);
    ProfileBook* b = ProfFindBook(keyT);
    if (!b) {
        return false;
    }
    ProfCopyStates(b->study, out);
    return len(out.states) > 0;
}

void ProfileDataClearStudyDocs() {
    ProfileDataEnsureLoaded();
    ProfFreeBooks(gProfile.books);
    logf("[profile] cleared all book study states\n");
    gProfile.dirty = true;
    ProfileDataSaveNow();
}

// rebuild the books from the legacy dir (adopt/resync/recover path): REPLACE,
// not merge — the disk snapshot is a known point-in-time
void ProfileDataReimportStudyFiles() {
    ProfileDataEnsureLoaded();
    ProfFreeBooks(gProfile.books);
    ProfImportBooksFromDir(gProfile.books, FlashcardStudyDir());
    logf("[profile] reimported %d book(s) from legacy dir\n", len(gProfile.books));
    gProfile.dirty = true;
    ProfileDataSaveNow();
}

// ---- day rows ----

static bool ProfRowIsOtherMachine(const ProfDayMachine& e) {
    return !ProfIsThisMachine(Str(e.machine));
}

static int ProfSumDay(Vec<ProfDayMachine>& v, int dateKey, bool cards) {
    int sum = 0;
    for (int i = 0; i < len(v); i++) {
        const ProfDayMachine& e = v[i];
        if (e.dateKey == dateKey && ProfRowIsOtherMachine(e)) {
            sum += cards ? e.cards : e.sec;
        }
    }
    return sum;
}

static int ProfSumAll(Vec<ProfDayMachine>& v, bool cards) {
    int sum = 0;
    for (int i = 0; i < len(v); i++) {
        if (ProfRowIsOtherMachine(v[i])) {
            sum += cards ? v[i].cards : v[i].sec;
        }
    }
    return sum;
}

// replaces THIS machine's row wholesale: the caller passes the live module
// total for that day, not a delta
static void ProfUpsertDay(Vec<ProfDayMachine>& v, int dateKey, int sec, int cards) {
    if (sec < 0) {
        sec = 0;
    }
    if (cards < 0) {
        cards = 0;
    }
    for (int i = 0; i < len(v); i++) {
        ProfDayMachine& e = v[i];
        if (e.dateKey == dateKey && ProfIsThisMachine(Str(e.machine))) {
            e.sec = sec;
            e.cards = cards;
            gProfile.dirty = true;
            return;
        }
    }
    ProfDayMachine e;
    e.dateKey = dateKey;
    e.sec = sec;
    e.cards = cards;
    e.machine = str::Dup(Str(gProfileThisMachine)).s;
    v.Append(e);
    gProfile.dirty = true;
}

void ProfileDataUpsertReadingDay(int dateKey, int sec) {
    ProfileDataEnsureLoaded();
    ProfUpsertDay(gProfile.readingDaily, dateKey, sec, 0);
}

void ProfileDataUpsertStudyDay(int dateKey, int sec, int cards) {
    ProfileDataEnsureLoaded();
    ProfUpsertDay(gProfile.studyDaily, dateKey, sec, cards);
}

// merged = live local module + profile rows of OTHER machines (rows carrying
// this machine's name are skipped: the live module owns this machine's numbers)
int ProfileDataReadingDaySec(int dateKey) {
    ProfileDataEnsureLoaded();
    return ReadingStatsGetDaySec(dateKey) + ProfSumDay(gProfile.readingDaily, dateKey, false);
}

int ProfileDataReadingTotalSec() {
    ProfileDataEnsureLoaded();
    return ReadingStatsGetTotalSec() + ProfSumAll(gProfile.readingDaily, false);
}

int ProfileDataStudyDaySec(int dateKey) {
    ProfileDataEnsureLoaded();
    return FlashcardStatsGetDaySec(dateKey) + ProfSumDay(gProfile.studyDaily, dateKey, false);
}

int ProfileDataStudyDayCards(int dateKey) {
    ProfileDataEnsureLoaded();
    return FlashcardStatsGetDayCards(dateKey) + ProfSumDay(gProfile.studyDaily, dateKey, true);
}

int ProfileDataStudyTotalSec() {
    ProfileDataEnsureLoaded();
    return FlashcardStatsGetTotalSec() + ProfSumAll(gProfile.studyDaily, false);
}

int ProfileDataStudyTotalCards() {
    ProfileDataEnsureLoaded();
    return FlashcardStatsGetTotalCards() + ProfSumAll(gProfile.studyDaily, true);
}

// ---- save (atomic; mirrors FlashcardStudySave) ----

static void ProfWriteDaySection(FILE* f, const char* name, Vec<ProfDayMachine>& v) {
    fprintf(f, ",\n  \"%s\": {\n", name);
    bool firstDay = true;
    for (int i = 0; i < len(v); i++) {
        ProfDayMachine& e = v[i];
        if (!firstDay) {
            fprintf(f, ",\n");
        }
        firstDay = false;
        fprintf(f, "    \"%d\": {", e.dateKey);
        if (e.cards > 0) {
            fprintf(f, "\"%s\": {\"sec\": %d, \"cards\": %d}}", e.machine, e.sec, e.cards);
        } else {
            fprintf(f, "\"%s\": %d}", e.machine, e.sec);
        }
    }
    fprintf(f, "\n  }");
}

static void ProfEnsureFreshDocRow(const char* docPath, int readingSec, int fcStudySec, int fcCards) {
    if (!docPath || !*docPath) {
        return;
    }
    for (int i = 0; i < len(gProfile.perDoc); i++) {
        ProfDocEntry& e = gProfile.perDoc[i];
        if (str::EqI(Str(e.docPath), docPath)) {
            // take the CURRENT FileState numbers, not the max (a reset when a
            // file was removed must propagate)
            e.readingSec = readingSec;
            e.fcStudySec = fcStudySec;
            e.fcCards = fcCards;
            return;
        }
    }
    ProfDocEntry e;
    e.docPath = str::Dup(Str(docPath)).s;
    e.readingSec = readingSec;
    e.fcStudySec = fcStudySec;
    e.fcCards = fcCards;
    gProfile.perDoc.Append(e);
}

// YYYYMMDD for "today" in local time (bucket key; matches the stats sidecars)
static int ProfTodayDateKey() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return (int)st.wYear * 10000 + (int)st.wMonth * 100 + (int)st.wDay;
}

// increment one per-document counter row in gProfile.perDoc, keyed by docPath
// (full path, same key the perDoc snapshot uses). Each field clamped to >= 0.
static void ProfIncDocEntry(const char* docPath, int readingDelta, int fcStudyDelta, int fcCardsDelta) {
    if (!docPath || !*docPath) {
        return;
    }
    for (int i = 0; i < len(gProfile.perDoc); i++) {
        ProfDocEntry& e = gProfile.perDoc[i];
        if (str::EqI(Str(e.docPath), docPath)) {
            e.readingSec += readingDelta;
            e.fcStudySec += fcStudyDelta;
            e.fcCards += fcCardsDelta;
            if (e.readingSec < 0) {
                e.readingSec = 0;
            }
            if (e.fcStudySec < 0) {
                e.fcStudySec = 0;
            }
            if (e.fcCards < 0) {
                e.fcCards = 0;
            }
            gProfile.dirty = true;
            return;
        }
    }
    ProfDocEntry e;
    e.docPath = str::Dup(Str(docPath)).s;
    e.readingSec = readingDelta;
    e.fcStudySec = fcStudyDelta;
    e.fcCards = fcCardsDelta;
    gProfile.perDoc.Append(e);
    gProfile.dirty = true;
}

// current own-machine day value from a profile day section (dateKey + machine keyed)
static int ProfDayRead(const Vec<ProfDayMachine>& v, int dateKey, bool cards) {
    for (int i = 0; i < len(v); i++) {
        if (v[i].dateKey == dateKey && str::EqI(Str(v[i].machine), gProfileThisMachine)) {
            return cards ? v[i].cards : v[i].sec;
        }
    }
    return 0;
}

// incremental live ticks: per-document counters + today's own-machine day row.
// mark dirty only; save cadence is the existing flush / shutdown (not per tick).
void ProfileDataAddReadingSec(const char* pdfPath, int sec) {
    if (!pdfPath || !*pdfPath || sec <= 0) {
        return;
    }
    if (sec > 24 * 3600) {
        sec = 24 * 3600;
    }
    ProfileDataEnsureLoaded();
    ProfIncDocEntry(pdfPath, sec, 0, 0);
    int today = ProfTodayDateKey();
    ProfileDataUpsertReadingDay(today, ProfDayRead(gProfile.readingDaily, today, false) + sec);
}

void ProfileDataAddFlashcardSec(const char* pdfPath, int sec) {
    if (!pdfPath || !*pdfPath || sec <= 0) {
        return;
    }
    if (sec > 24 * 3600) {
        sec = 24 * 3600;
    }
    ProfileDataEnsureLoaded();
    ProfIncDocEntry(pdfPath, 0, sec, 0);
    int today = ProfTodayDateKey();
    ProfileDataUpsertStudyDay(today, ProfDayRead(gProfile.studyDaily, today, false) + sec,
                              ProfDayRead(gProfile.studyDaily, today, true));
}

void ProfileDataAddFlashcardCard(const char* pdfPath) {
    if (!pdfPath || !*pdfPath) {
        return;
    }
    ProfileDataEnsureLoaded();
    ProfIncDocEntry(pdfPath, 0, 0, 1);
    int today = ProfTodayDateKey();
    ProfileDataUpsertStudyDay(today, ProfDayRead(gProfile.studyDaily, today, false),
                              ProfDayRead(gProfile.studyDaily, today, true) + 1);
}

// list profile books for the Config flashcard list: docName/docPath from the
// profile, totalCards/dueCount/lastReviewedAt from the in-memory states,
// syncStatus from PDF presence on disk. Owned strings are freed by the caller
// (ConfigFreeDocs), same contract as json-derived rows.
int ProfileDataListBooks(Vec<FlashcardStudyDocInfo>& out) {
    ProfileDataEnsureLoaded();
    i64 now = (i64)time(nullptr) * 1000;
    for (int i = 0; i < len(gProfile.books); i++) {
        ProfileBook& b = gProfile.books[i];
        FlashcardStudyDocInfo info{};
        info.fileName = nullptr; // profile-only rows have no json file name
        info.docName = b.docName ? str::Dup(Str(b.docName)).s : nullptr;
        info.docPath = b.docPath ? str::Dup(Str(b.docPath)).s : nullptr;
        Vec<FlashcardStudyDoc::StateEntry>& states = b.study.states;
        info.totalCards = len(states);
        for (int j = 0; j < len(states); j++) {
            const FlashcardStudyState& st = states[j].state;
            if (st.rating != 0 && st.nextReviewAt <= now) {
                info.dueCount++;
            }
            if (st.lastReviewedAt > info.lastReviewedAt) {
                info.lastReviewedAt = st.lastReviewedAt;
            }
        }
        if (!info.docName) {
            info.docName = str::Dup(StrL("")).s;
        }
        if (info.docPath && len(info.docPath) > 0) {
            info.syncStatus = file::Exists(Str(info.docPath)) ? 1 : 2;
        }
        out.Append(info);
    }
    return len(out);
}

// remove one book row (and its perDoc section) keyed by full pdfPath; dirty +
// save via the normal cadence (not per call). false when the book is absent.
bool ProfileDataDeleteBook(const char* pdfPath) {
    if (!pdfPath || !*pdfPath) {
        return false;
    }
    ProfileDataEnsureLoaded();
    bool found = false;
    for (int i = 0; i < len(gProfile.books); i++) {
        if (gProfile.books[i].docPath && str::EqI(Str(gProfile.books[i].docPath), Str(pdfPath))) {
            ProfFreeBook(gProfile.books[i]);
            gProfile.books.RemoveAt(i);
            found = true;
            break;
        }
    }
    for (int i = 0; i < len(gProfile.perDoc); i++) {
        if (gProfile.perDoc[i].docPath && str::EqI(Str(gProfile.perDoc[i].docPath), Str(pdfPath))) {
            ::free(gProfile.perDoc[i].docPath);
            gProfile.perDoc.RemoveAt(i);
            break;
        }
    }
    if (found) {
        gProfile.dirty = true;
        ProfileDataSaveNow();
        logf("[profile] deleted book '%s'\n", Str(pdfPath));
    }
    return found;
}

// pull the per-document counters from gFileHistory (settings FileState stores)
static void ProfPullHistory() {
    for (int i = 0; i < gFileHistory.states->len; i++) {
        FileState* fs = gFileHistory.Get(i);
        if (!fs || !fs->filePath.s) {
            continue;
        }
        ProfEnsureFreshDocRow(fs->filePath.s, fs->readingTimeSec, fs->flashcardStudyTimeSec, fs->flashcardCardsDone);
    }
}

static void ProfSaveInner() {
    TempStr path = ProfileDataPathTemp();
    TempStr dir = path::GetDirTemp(Str(path));
    if (dir) {
        dir::CreateAll(Str(dir));
    }

    TempStr tmpPath = str::JoinTemp(Str(path), StrL(".tmp"));
    FILE* f = _wfopen(CWStrTemp(ToWStrTemp(Str(tmpPath))), L"wb");
    if (!f) {
        logf("[profile] save ERROR: failed to open '%s'\n", Str(tmpPath));
        return;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": %d,\n", kProfileDataVersion);
    fprintf(f, "  \"lastWriteTs\": %lld,\n", (long long)time(nullptr));
    fprintf(f, "  \"machine\": ");
    FlashcardStudyWriteJsonStringLit(f, gProfileThisMachine ? Str(gProfileThisMachine) : StrL("?"));
    fprintf(f, ",\n  \"checksum\": \"\"\n");

    // books (keeps the legacy "states" shape so the shared parser applies)
    fprintf(f, ",\n  \"books\": {");
    for (int i = 0; i < len(gProfile.books); i++) {
        ProfileBook& b = gProfile.books[i];
        if (i > 0) {
            fprintf(f, ",\n");
        }
        fprintf(f, "\n    \"%s\": {\n      \"docName\": ", b.streamKey);
        FlashcardStudyWriteJsonStringLit(f, b.docName ? Str(b.docName) : StrL(""));
        fprintf(f, ",\n      \"docPath\": ");
        FlashcardStudyWriteJsonStringLit(f, b.docPath ? Str(b.docPath) : StrL(""));
        fprintf(f, ",\n      \"states\": {");
        for (int j = 0; j < len(b.study.states); j++) {
            if (j > 0) {
                fprintf(f, ",\n");
            }
            fprintf(f, "\n        \"%llu\": ", (unsigned long long)b.study.states[j].key);
            FlashcardStudyWriteJsonValue(f, b.study.states[j].state);
        }
        fprintf(f, "\n      }\n    }");
    }
    fprintf(f, "\n  }");

    ProfWriteDaySection(f, "readingStats", gProfile.readingDaily);
    ProfWriteDaySection(f, "fcStats", gProfile.studyDaily);

    fprintf(f, ",\n  \"perDoc\": {");
    for (int i = 0; i < len(gProfile.perDoc); i++) {
        ProfDocEntry& e = gProfile.perDoc[i];
        if (i > 0) {
            fprintf(f, ",\n");
        }
        fprintf(f, "\n    ");
        FlashcardStudyWriteJsonStringLit(f, Str(e.docPath));
        fprintf(f, ": {\"readingSec\": %d, \"fcStudySec\": %d, \"fcCards\": %d}", e.readingSec, e.fcStudySec,
                e.fcCards);
    }
    fprintf(f, "\n  }\n}\n");

    fclose(f);

    // F7: compute integrity checksum over the file content with empty checksum
    // placeholder, then rewrite the temp file with the real checksum value.
    Str fileContent = file::ReadFile(Str(tmpPath));
    if (fileContent) {
        TempStr hexChecksum = ProfComputeChecksumTemp(fileContent);
        if (hexChecksum) {
            TempStr withChecksum = ProfSubstChecksumTemp(fileContent, hexChecksum);
            if (withChecksum) {
                file::WriteFile(Str(tmpPath), Str(withChecksum));
            }
        }
    }

    // MOVEFILE_REPLACE_EXISTING makes the swap single-step; fall back to
    // remove+rename if the replace fails (antivirus / Drive holding a handle)
    WCHAR* tmpW = CWStrTemp(ToWStrTemp(Str(tmpPath)));
    WCHAR* dstW = CWStrTemp(ToWStrTemp(Str(path)));
    BOOL ok = MoveFileExW(tmpW, dstW, MOVEFILE_REPLACE_EXISTING);
    if (!ok) {
        remove(path.s);
        ok = rename(tmpPath.s, path.s) == 0;
    }
    if (!ok) {
        logf("[profile] save ERROR: failed to replace '%s'\n", Str(path));
        return;
    }
    logf("[profile] saved %d book(s), %d reading day(s), %d study day(s)\n", len(gProfile.books),
         len(gProfile.readingDaily), len(gProfile.studyDaily));
}

void ProfileDataSaveNow() {
    ProfileDataEnsureLoaded();
    if (!gProfile.dirty) {
        return;
    }
    ProfPullHistory();
    ProfSaveInner();
    ProfMaybeBackup();
    gProfile.dirty = false;
}

// ---- rolling backups: 9 slots x 8h (generalized from the flashcards' 3x1d) ----

static TempStr ProfBackupDirTemp() {
    TempStr path = ProfileDataPathTemp();
    TempStr dir = path::GetDirTemp(Str(path));
    return path::JoinTemp(Str(dir), StrL("backup"));
}

// lastWriteTs recorded inside one backup slot's snapshot ("lastWriteTs": n),
// 0 when the slot is absent or unparseable
static i64 ProfSlotLastWriteTs(int slotIdx) {
    TempStr slotDir = path::JoinTemp(ProfBackupDirTemp(), fmt("%02d", slotIdx));
    TempStr srcPath = path::JoinTemp(Str(slotDir), kProfileDataFileName);
    if (!file::Exists(Str(srcPath))) {
        return 0;
    }
    Str slotJson = file::ReadFile(Str(srcPath));
    if (!slotJson || !ProfBlockInterior(slotJson)) {
        return 0;
    }
    return ProfNumField(slotJson, "lastWriteTs", 0);
}

// cheap guard: the slot for the current 8h period is refreshed when its stamp
// does not record this period yet (wall-clock derived so it survives reboots)
static void ProfMaybeBackup() {
    TempStr livePath = ProfileDataPathTemp();
    if (!file::Exists(Str(livePath))) {
        return;
    }
    time_t now = time(nullptr);
    i64 period = (i64)(now / kProfBackupSlotSec);
    int slotIdx = (int)(period % kProfBackupSlots);
    TempStr slotDir = path::JoinTemp(ProfBackupDirTemp(), fmt("%02d", slotIdx));
    TempStr stampPath = path::JoinTemp(Str(slotDir), StrL("stamp.txt"));
    if (file::Exists(Str(stampPath))) {
        Str old = file::ReadFile(Str(stampPath));
        if (old && ProfNumField(old, "period", -1) == period) {
            return;
        }
    }
    dir::CreateAll(Str(slotDir));
    TempStr dst = path::JoinTemp(Str(slotDir), kProfileDataFileName);
    file::Copy(Str(dst), Str(livePath), false);
    SYSTEMTIME st{};
    GetLocalTime(&st);
    TempStr stampBody = fmt("{\n  \"date\": \"%04d-%02d-%02d %02d:%02d\",\n  \"period\": %lld\n}\n", st.wYear,
                            st.wMonth, st.wDay, st.wHour, st.wMinute, period);
    TempStr tmpp = path::JoinTemp(Str(slotDir), StrL("stamp.tmp"));
    file::WriteFile(Str(tmpp), Str(stampBody));
    DeleteFileW(CWStrTemp(ToWStrTemp(Str(stampPath))));
    MoveFileExW(CWStrTemp(ToWStrTemp(Str(tmpp))), CWStrTemp(ToWStrTemp(Str(stampPath))), MOVEFILE_REPLACE_EXISTING);
    TempStr stampText = fmt("%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    logf("[profile] backup slot %02d refreshed (%s)\n", slotIdx, Str(stampText));
}

int ProfileDataBackupCount() {
    ProfileDataEnsureLoaded();
    int n = 0;
    for (int i = 0; i < kProfBackupSlots; i++) {
        TempStr slotDir = path::JoinTemp(ProfBackupDirTemp(), fmt("%02d", i));
        TempStr stampPath = path::JoinTemp(Str(slotDir), StrL("stamp.txt"));
        if (file::Exists(Str(stampPath))) {
            n++;
        }
    }
    return n;
}

// date+time stamp of one backup slot ("YYYY-MM-DD HH:MM"), nullptr when unused
TempStr ProfileDataBackupStampTemp(int slotIdx) {
    ProfileDataEnsureLoaded();
    if (slotIdx < 0 || slotIdx >= kProfBackupSlots) {
        return nullptr;
    }
    TempStr slotDir = path::JoinTemp(ProfBackupDirTemp(), fmt("%02d", slotIdx));
    TempStr stampPath = path::JoinTemp(Str(slotDir), StrL("stamp.txt"));
    Str body = file::ReadFile(Str(stampPath));
    if (!body) {
        return nullptr;
    }
    return FlashcardStudyParseJsonStringFieldDup(body, StrL("date"));
}

// restore one slot's snapshot over the live file and re-parse it
bool ProfileDataRecoverBackupSlot(int slotIdx) {
    ProfileDataEnsureLoaded();
    if (slotIdx < 0 || slotIdx >= kProfBackupSlots) {
        return false;
    }
    TempStr slotDir = path::JoinTemp(ProfBackupDirTemp(), fmt("%02d", slotIdx));
    TempStr srcPath = path::JoinTemp(Str(slotDir), kProfileDataFileName);
    TempStr dstPath = ProfileDataPathTemp();
    if (!file::Copy(Str(dstPath), Str(srcPath), false)) {
        return false;
    }
    // drop in-memory state so the next access re-parses the recovered snapshot
    gProfile.loaded = false;
    ProfFreeBooks(gProfile.books);
    ProfFreeDays(gProfile.readingDaily);
    ProfFreeDays(gProfile.studyDaily);
    ProfFreeDocs(gProfile.perDoc);
    ::free(gProfileThisMachine);
    gProfileThisMachine = nullptr;
    logf("[profile] recovered backup slot %02d\n", slotIdx);
    ProfileDataEnsureLoaded();
    ProfileDataSaveNow();
    return true;
}

// ---- cloud revert detection ----

// returns + clears the "newest backup is strictly newer than primary" flag
bool ProfileDataConsumeCloudRevertPending() {
    bool pending = gCloudRevertPending;
    gCloudRevertPending = false;
    return pending;
}

// primary file's lastWriteTs (unix seconds), 0 when not loaded/absent
i64 ProfileDataCloudRevertPrimaryTs() {
    ProfileDataEnsureLoaded();
    TempStr path = ProfileDataPathTemp();
    Str json = file::ReadFile(Str(path));
    if (!json || !ProfBlockInterior(json)) {
        return 0;
    }
    return ProfNumField(json, "lastWriteTs", 0);
}

// newest backup slot's lastWriteTs (unix seconds), 0 when none
i64 ProfileDataCloudRevertNewestBackupTs() {
    ProfileDataEnsureLoaded();
    i64 best = 0;
    for (int i = 0; i < kProfBackupSlots; i++) {
        i64 t = ProfSlotLastWriteTs(i);
        if (t > best) {
            best = t;
        }
    }
    return best;
}

// index of the newest backup slot, -1 when none
int ProfileDataCloudRevertNewestBackupSlot() {
    ProfileDataEnsureLoaded();
    int bestIdx = -1;
    i64 best = 0;
    for (int i = 0; i < kProfBackupSlots; i++) {
        i64 t = ProfSlotLastWriteTs(i);
        if (t > best) {
            best = t;
            bestIdx = i;
        }
    }
    return bestIdx;
}
