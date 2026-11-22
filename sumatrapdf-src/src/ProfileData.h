/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef ProfileData_h
#define ProfileData_h

#include "Flashcard.h"

// Single-file "Profile Memory" store: <root>\tumatraPDF-data_1.json
// consolidates every user-data category (study states, reading/study daily
// stats, per-document info) so it can be synced, backed up and imported as one
// unit. The legacy per-book *.json files and the two stats sidecars keep being
// written as before (dual write) and are imported here.
// storage layout (s64-R9o): everything lives under ONE data root —
// ProfileDataGetDataRootTemp(). "<root>\stats\" holds the two sidecars,
// "<root>\study\" the per-book *.json files (only while the legacy
// flashcardStudyDir override is unset; a set override keeps resolving per-book
// files there). Migration of legacy sidecars (next to the settings file) into
// <root>\stats is copy-only on load; the legacy files are never deleted.
// Day entries carry per-machine attribution so two machines sharing the file
// through a synced folder sum exactly, with no double counting.

void ProfileDataUpdateStudyDoc(const char* pdfPath, const FlashcardStudyDoc& doc);
void ProfileDataMergeStudyDoc(const char* pdfPath, const FlashcardStudyDoc& doc);
bool ProfileDataGetStudyDoc(const char* pdfPath, FlashcardStudyDoc& out);
// drop every book's study states from the profile
void ProfileDataClearStudyDocs();
// rebuild the books from the legacy study dir (adopt/resync/recover path)
void ProfileDataReimportStudyFiles();

// register this machine's reading/study day numbers (replaces the row that
// carries this machine name; rows of other machines stay untouched)
void ProfileDataUpsertReadingDay(int dateKey, int sec);
void ProfileDataUpsertStudyDay(int dateKey, int sec, int cards);

// list profile books for the Config flashcard list (merged with json-derived
// rows by the caller); fills docName/docPath/totalCards/dueCount/lastReviewedAt
// from the in-memory profile, syncStatus from PDF presence. Owned strings freed
// by ConfigFreeDocs (same as json-derived rows).
int ProfileDataListBooks(Vec<FlashcardStudyDocInfo>& out);
// remove one book row (and its perDoc section) keyed by full pdfPath; dirty +
// save via the normal cadence. false when the book is not in the profile.
bool ProfileDataDeleteBook(const char* pdfPath);

// incremental live ticks (per document, keyed by full pdfPath); mark dirty +
// debounced save handled by the existing flush cadence / shutdown, not per tick
void ProfileDataAddReadingSec(const char* pdfPath, int sec);
void ProfileDataAddFlashcardSec(const char* pdfPath, int sec);
void ProfileDataAddFlashcardCard(const char* pdfPath);

// per-session (TabGroup) study stats; book-independent sidecar, seeded once via backfill
void ProfileDataFlushSessionsIfNeeded(); // flush only if dirty > 0

struct SessionStatEntry {
    Str name; // points into ProfileData's store, valid until the process flushes/loads
    int readSec = 0;
    int fcSec = 0;
    int fcCards = 0;
    u32 lastUsedTs = 0; // unix seconds of the last recorded activity; 0 = unknown (old sidecar)
};
void ProfileDataAddSessionReadingSec(Str group, int sec);
void ProfileDataAddSessionStudySec(Str group, int sec);
void ProfileDataAddSessionCard(Str group);
void ProfileDataGetSessionStats(Vec<SessionStatEntry>& out); // appends one entry per recorded session
void ProfileDataRenameSession(Str oldName, Str newName);     // rename the key; no-op if old absent/new empty/old==new
// day getters merged across machines: this machine's live numbers come from
// the owning stats module (ReadingStats* / FlashcardStats*), other machines'
// numbers from the profile
int ProfileDataReadingDaySec(int dateKey);
int ProfileDataReadingTotalSec();
int ProfileDataStudyDaySec(int dateKey);
int ProfileDataStudyDayCards(int dateKey);
int ProfileDataStudyTotalSec();
int ProfileDataStudyTotalCards();

// the ONE data-root resolver (s64-R9o): returns the absolute dir (no trailing
// slash) under which ALL user study data lives — tumatraPDF-data_1.json,
// stats\ sidecars, study\ per-book JSONs (when flashcardStudyDir is unset),
// backup\ rolling slots. profileDataDir advanced setting wins; else the
// per-exe app-data dir. Callers path::JoinTemp() onto it; spaces documented
// in ProfileData.cpp.
TempStr ProfileDataGetDataRootTemp();
// atomic save of tumatraPDF-data_1.json (books, day entries with the current
// perDoc info pulled from gFileHistory) + cheap rolling backup guard
void ProfileDataSaveNow();
TempStr ProfileDataPathTemp();
bool ProfileDataStats(int* version, int* books, int* readingDays, int* studyDays, i64* lastWriteTs);
int ProfileDataBackupCount();
TempStr ProfileDataBackupStampTemp(int slotIdx);
bool ProfileDataRecoverBackupSlot(int slotIdx);
bool ProfileDataConsumeCloudRevertPending();
i64 ProfileDataCloudRevertPrimaryTs();
i64 ProfileDataCloudRevertNewestBackupTs();
int ProfileDataCloudRevertNewestBackupSlot();

// F7: export / import. Export copies primary + backup/ + stats/ + study/ to destDir.
// overwriteExisting=false: refuse when dest primary exists.
// overwriteExisting=true: overwrite dest primary atomically.
// Import: replace=true replaces primary (atomic), replace=false merges per-item.
// Import validates source checksum before any modification (if present).
bool ProfileDataExportToDir(const char* destDir, bool overwriteExisting = false);
bool ProfileDataImportFromJson(const char* srcJsonPath, bool replace);

// A6: return human-readable status of the LAST startup data migration (F4 + F5).
// Safe to call before/after load; never crashes.
// Returns a BORROWED Str view; callers must NOT free it.
TempStr ProfileDataLastMigrationStatusTemp();

#endif
