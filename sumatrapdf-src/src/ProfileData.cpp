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
    bool loaded = false;
    bool dirty = false;
} gProfile;

static char* gProfileThisMachine = nullptr; // owned, set at first load

static void ProfSaveInner();
static void ProfMaybeBackup();
static void ProfPullHistory();

// ---- small helpers ----

static TempStr ProfileDataPathTemp() {
    Str custom = gGlobalPrefs->profileDataDir;
    if (custom && len(custom) > 0) {
        return path::JoinTemp(Str(custom), kProfileDataFileName);
    }
    return GetPathInAppDataDirTemp(kProfileDataFileName);
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

// one-time import: the profile file does not exist yet, so pull the per-book
// study JSONs and both sidecars out of their legacy homes
static void ProfImportBooksFromDir(Vec<ProfileBook>& outBooks) {
    TempStr dir = FlashcardStudyDir();
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
    ProfImportBooksFromDir(gProfile.books);
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

static void ProfileDataEnsureLoaded() {
    if (gProfile.loaded) {
        return;
    }
    gProfile.loaded = true;
    gProfileThisMachine = ProfMachineNameDup();
    TempStr path = ProfileDataPathTemp();
    logf("[profile] loading %s\n", Str(path));
    Str json = file::ReadFile(Str(path));
    if (!json) {
        ProfImportLegacy();
        gProfile.dirty = true;
        // materialize immediately: the import IS the profile's birth; waiting
        // for the next save cadence would lose it on a crash
        ProfPullHistory();
        ProfSaveInner();
        ProfMaybeBackup();
        gProfile.dirty = false;
        return;
    }
    ProfParseBooks(json, gProfile.books);
    ProfParseDaySection(json, "readingStats", gProfile.readingDaily);
    ProfParseDaySection(json, "fcStats", gProfile.studyDaily);
    ProfParseInfoSection(json, gProfile.perDoc);
    logf("[profile] loaded: %d book(s), %d reading day(s), %d study day(s)\n", len(gProfile.books),
         len(gProfile.readingDaily), len(gProfile.studyDaily));
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
    ProfImportBooksFromDir(gProfile.books);
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

static const int kProfBackupSlots = 9;
static const i64 kProfBackupSlotSec = 8 * 3600;

static TempStr ProfBackupDirTemp() {
    TempStr path = ProfileDataPathTemp();
    TempStr dir = path::GetDirTemp(Str(path));
    return path::JoinTemp(Str(dir), StrL("backup"));
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
