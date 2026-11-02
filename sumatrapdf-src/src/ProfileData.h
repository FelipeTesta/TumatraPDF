/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef ProfileData_h
#define ProfileData_h

struct FlashcardStudyDoc;

// Single-file "Profile Memory" store: <profileDataDir>\tumatraPDF-data_1.json
// consolidates every user-data category (study states, reading/study daily
// stats, per-document info) so it can be synced, backed up and imported as one
// unit. The legacy per-book *.json files and the two stats sidecars keep being
// written as before (dual write) and are imported here.
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

// day getters merged across machines: this machine's live numbers come from
// the owning stats module (ReadingStats* / FlashcardStats*), other machines'
// numbers from the profile
int ProfileDataReadingDaySec(int dateKey);
int ProfileDataReadingTotalSec();
int ProfileDataStudyDaySec(int dateKey);
int ProfileDataStudyDayCards(int dateKey);
int ProfileDataStudyTotalSec();
int ProfileDataStudyTotalCards();

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

// F7: export / import. Export copies primary + backup/ subtree to destDir.
// Import replaces primary or merges (newest-wins-adopt-absent) into live profile.
bool ProfileDataExportToDir(const char* destDir);
bool ProfileDataImportFromJson(const char* srcJsonPath, bool replace);

#endif
