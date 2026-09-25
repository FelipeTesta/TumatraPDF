/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Flashcard_h
#define Flashcard_h

// Forward declarations
class EngineMupdf;
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

// Load flashcards from PDF annotations in the current document
Vec<Flashcard> FlashcardLoadFromDocument(EngineMupdf* engine);

// Save flashcard study state to external JSON file
void FlashcardStudySave(const char* filePath, const FlashcardStudyDoc& doc);

// Load flashcard study state from external JSON file
FlashcardStudyDoc FlashcardStudyLoad(const char* filePath);

// SM-2 algorithm: compute next interval and ease factor
void FlashcardSm2Update(FlashcardStudyState& state, int rating);

// Get the JSON file path for a document's flashcard study state
TempStr FlashcardStudyPath(const char* filePath);

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

// Clean History dialog result: 0 = cancelled (X/Esc), 1 = clear the
// CURRENT book's review history (2s hold), 2 = clear ALL books (5s hold)
enum FlashcardCleanResult {
    kFlashcardCleanCancelled = 0,
    kFlashcardCleanCurrentBook = 1,
    kFlashcardCleanAllBooks = 2,
};

// FlashcardToolbar (in FlashcardToolbar.cpp)
void FlashcardToolbarCreate(MainWindow* win);
void FlashcardToolbarDestroy(MainWindow* win);
void FlashcardToolbarUpdateCount(MainWindow* win);
void FlashcardToolbarUpdateState(MainWindow* win);
// Clean History confirmation dialog: hold-to-confirm buttons ("Clear current
// book" 2s, "Clear ALL books" 5s, draining line under each). X/Esc cancels.
// Returns a FlashcardCleanResult.
int FlashcardCleanHistoryDialog(HWND hwndParent);
// Study Order options dialog (sequential/random + new-cards position);
// applies instantly on each option click
void FlashcardOrderOptionsDialog(MainWindow* win);
// Study Filter dialog: page-set expression input + bookmark mirror with
// checkboxes (chapter ranges inject into the expression) + Filters ON/OFF +
// Clear (2s hold) + cross-document session checkbox
void FlashcardFilterOptionsDialog(MainWindow* win);
// Delete every FlashcardStudy/*.json in the app-data dir (ALL books' review
// histories). Returns the number of files deleted.
int FlashcardDeleteAllStudyFiles();

// Commands_Flashcard.cpp: rebuild the study order after order/position
// settings changed (restarts from the first card when a session is active)
void FlashcardApplyStudyOrder(MainWindow* win);

// FlashcardSidebar (in FlashcardSidebar.cpp)
void FlashcardSidebarCreate(MainWindow* win);
void FlashcardSidebarDestroy(MainWindow* win);
void FlashcardSidebarToggle(MainWindow* win);
void FlashcardSidebarPopulate(MainWindow* win);

#endif