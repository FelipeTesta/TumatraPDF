/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Flashcard_h
#define Flashcard_h

// Forward declarations
class EngineMupdf;
class EngineBase;
struct Annotation;
struct MainWindow;
struct WindowTab;

// Flashcard — represents a card saved as a PDF Highlight annotation.
// Cards are occlusion masks over existing PDF text (cloze): the highlight rect
// hides the text underneath in study mode. No content is duplicated in the PDF.
struct Flashcard {
    u64 key = 0;      // stable id: hash(pageNo + bounds) — survives PDF save (annotId is not stable)
    int annotId = -1; // MuPDF annotation object number (unstable across save; display only)
    int pageNo = -1;  // 1-based page number
    RectF bounds;     // selection rect in PAGE coordinates (PDF points) = the cloze mask
    // Per quad-point subrects (PAGE coords): a multi-line selection has one quad per
    // line, so the mask covers exactly the highlighted text, not the union bbox.
    Vec<RectF> rects;
};

// FlashcardStudyState — per-card study progress (stored in external JSON)
struct FlashcardStudyState {
    int rating = 0;          // last rating 1-4 (0 = new/unseen)
    int interval = 0;        // days until next review
    float easeFactor = 2.5f; // SM-2 ease factor
    i64 lastReviewedAt = 0;  // timestamp (ms since epoch)
    i64 nextReviewAt = 0;    // timestamp
    int reviewCount = 0;     // total reviews
};

// FlashcardStudyDoc — study states for all cards in a document
struct FlashcardStudyDoc {
    int version = 1;
    struct StateEntry {
        u64 key; // matches Flashcard::key
        FlashcardStudyState state;
    };
    Vec<StateEntry> states;
};

// One card in the study session queue: the tab (document) it comes from +
// the card's index in that tab's cards vec. Cross-document sessions carry
// entries from several tabs; single-doc sessions carry only the current tab.
struct FlashcardQueueEntry {
    WindowTab* tab = nullptr;
    int cardIdx = -1;
};

// FlashcardTabState — per-document flashcard data (lives in WindowTab).
// Cards/study states/filter belong to the document, so they must follow the
// tab: switching tabs must not keep the previous book's cards painted over
// the new document, and a cross-document session needs each book's data
// separately.
struct FlashcardTabState {
    Vec<Flashcard> cards;       // cards of this document
    FlashcardStudyDoc studyDoc; // review states of this document
    // page filter expression, e.g. "1-15;20-25;-22-23;" (add/remove ranges);
    // empty = no filter (all pages)
    Str filterExpr;
    bool filterEnabled = true; // ON/OFF switch (keeps the expression)
    bool cardsLoaded = false;  // lazy: true once FlashcardLoadFromDocument ran
};

// FcListRow — backing data for one row of the Lista panel (the label string
// itself lives in the listbox item; this carries the numeric per-card state)
struct FcListRow {
    int cardIdx = -1;
    int pageNo = -1;
    int rating = 0;       // last rating 1-4 (0 = new)
    int interval = 0;     // SM-2 interval in days
    i64 nextReviewAt = 0; // timestamp (ms)
};

// Load flashcards from PDF annotations in the current document
Vec<Flashcard> FlashcardLoadFromDocument(EngineMupdf* engine);

// Create a flashcard Highlight annotation for the given PAGE-space rects
// (author/color/opacity per the flashcard recipe). Used by CmdFlashcardAdd
// and the import merge.
Annotation* FlashcardCreateHighlightAnnot(EngineBase* engine, int pageNo, Vec<RectF>& rects);

// Import the NEW flashcards (Highlight annots) from another copy of the SAME
// document into ours, merging with existing cards (duplicate = same page +
// same mask within 1pt). Study history is NOT imported — new cards are "new".
// Returns the number of cards created.
int FlashcardImportFromPdf(EngineMupdf* dstEngine, Vec<Flashcard>& existing, const char* srcPath);

// Save flashcard study state to external JSON file
void FlashcardStudySave(const char* filePath, const FlashcardStudyDoc& doc);

// Load flashcard study state from external JSON file
FlashcardStudyDoc FlashcardStudyLoad(const char* filePath);

// Manual RESYNC: re-point an existing study JSON (old md5 file) at a new PDF
// path (book renamed on disk). Returns false when the JSON can't be read.
bool FlashcardStudyResync(const char* jsonPath, const char* newPdfPath);

// Restore the newest study-history backup slot over the study dir; returns
// the number of restored files (0 = no backup found)
int FlashcardStudyRecoverBackup();

// SM-2 algorithm: compute next interval and ease factor
void FlashcardSm2Update(FlashcardStudyState& state, int rating);

// Get the JSON file path for a document's flashcard study state
TempStr FlashcardStudyPath(const char* filePath);

// Directory where the study-history JSONs live: the flashcardSettings.studyDir
// setting when set (e.g. a cloud-synced folder for backup), else the per-exe
// portable app-data "FlashcardStudy" dir. Never empty.
TempStr FlashcardStudyDir();

// One document's study-history summary as listed in the Config window
struct FlashcardStudyDocInfo {
    char* fileName = nullptr; // owned (str::Dup): JSON file name inside the study dir
    char* docName = nullptr;  // owned (str::Dup): display name saved in the JSON ("" for legacy files)
    int totalCards = 0;       // entries in the JSON (cards that have study state)
    int dueCount = 0;         // entries with nextReviewAt <= now
    i64 lastReviewedAt = 0;   // most recent review timestamp (ms), 0 = never
};

// Scan the study dir and summarize every *.json into out (sorted by
// lastReviewedAt, most recent first). Returns the number of entries.
int FlashcardStudyListDocs(Vec<FlashcardStudyDocInfo>& out);

// Evaluate a page-filter expression into a per-page membership vector
// (pages[i-1] = 1 when page i is included). Returns false when the
// expression is empty/garbage (caller treats that as "all pages").
bool FlashcardFilterEval(Str expr, int pageCount, Vec<u8>& pages);

// Lazy-load a tab's cards + study states; true when the tab is a loaded PDF
bool FlashcardEnsureTabCards(WindowTab* tab);

// Stable per-tab name for [fc] logs: displayName is empty for cmdline-loaded
// docs until the UI assigns it — falls back to the file's base name
TempStr FlashcardTabLogName(WindowTab* tab);

// Called from LoadModelIntoTab: refresh flashcard state after a tab switch
void FlashcardOnTabChanged(MainWindow* win);

// FlashcardToolbar (in FlashcardToolbar.cpp)
void FlashcardToolbarCreate(MainWindow* win);
void FlashcardToolbarDestroy(MainWindow* win);
void FlashcardToolbarUpdateCount(MainWindow* win);
void FlashcardToolbarUpdateState(MainWindow* win);
// Config window (Config toolbar button): study-history folder picker (📂 —
// e.g. a Google Drive folder so histories stay backed up), per-book history
// list (|PDF|cards|due|last review|) and the clear actions — "clear the
// SELECTED book" (2s hold) and "clear ALL books" (5s hold). Actions execute
// inside the dialog (it owns the selection). X/Esc closes.
void FlashcardConfigDialog(MainWindow* win);
// Study Order options dialog (sequential/random + new-cards position);
// applies instantly on each option click
void FlashcardOrderOptionsDialog(MainWindow* win);
// Study Filter dialog: page-set expression input + bookmark mirror with
// checkboxes (chapter ranges inject into the expression) + Filters ON/OFF +
// Clear (2s hold) + cross-document session checkbox
void FlashcardFilterOptionsDialog(MainWindow* win);
// Delete every study-history JSON in the study dir (ALL books' review
// histories). Returns the number of files deleted.
int FlashcardDeleteAllStudyFiles();

// Move every study-history JSON from one study dir to another (called when the
// user changes the study folder in the Config window). Files already present
// in the target are KEPT (not overwritten — e.g. a cloud-synced folder may
// hold a copy from another machine); MOVEFILE_COPY_ALLOWED makes the move
// work across volumes (local disk → cloud drive). Returns the number of files
// moved.
int FlashcardStudyMigrateFiles(Str fromDir, Str toDir);

// Commands_Flashcard.cpp: rebuild the study order after order/position
// settings changed (restarts from the first card when a session is active)
void FlashcardApplyStudyOrder(MainWindow* win);
// Commands_Flashcard.cpp: stop the active study session (queue, index,
// modes); per-tab caches are kept
void FlashcardStopSession(MainWindow* win);

// FlashcardSidebar (in FlashcardSidebar.cpp)
void FlashcardSidebarCreate(MainWindow* win);
void FlashcardSidebarDestroy(MainWindow* win);
void FlashcardSidebarToggle(MainWindow* win);
void FlashcardSidebarPopulate(MainWindow* win);
// re-lay out + repaint the Lista panel after RelayoutFrame moved it
void RelayoutFlashcardListPanel(MainWindow* win);
// scroll the current tab to a card (used by the Lista panel's click)
void FlashcardNavigateToCardInCurrentTab(MainWindow* win, int cardIdx);

#endif