/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Flashcard_h
#define Flashcard_h

// Forward declarations
class EngineMupdf;
struct MainWindow;

// Flashcard — represents a card saved as a PDF FreeText annotation
// Cards are stored INSIDE the PDF (portable), study state is external
struct Flashcard {
    int annotId = -1;       // MuPDF annotation ID (unique per document)
    int pageNo = -1;        // 1-based page number
    RectF bounds;           // selection rect in PAGE coordinates (PDF points)
    Str text;               // "Q: ..."
    Str tip;                // optional hint text (from "T: " in annotation)
};

// FlashcardStudyState — per-card study progress (stored in external JSON)
struct FlashcardStudyState {
    int rating = 0;         // last rating 1-4 (0 = new/unseen)
    int interval = 0;       // days until next review
    float easeFactor = 2.5f; // SM-2 ease factor
    i64 lastReviewedAt = 0; // timestamp (ms since epoch)
    i64 nextReviewAt = 0;   // timestamp
    int reviewCount = 0;    // total reviews
};

// FlashcardStudyDoc — study states for all cards in a document
struct FlashcardStudyDoc {
    int version = 1;
    struct StateEntry {
        int annotId;
        FlashcardStudyState state;
    };
    Vec<StateEntry> states;
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

// FlashcardToolbar (in FlashcardToolbar.cpp)
void FlashcardToolbarCreate(MainWindow* win);
void FlashcardToolbarDestroy(MainWindow* win);
void FlashcardToolbarUpdateCount(MainWindow* win);

// FlashcardSidebar (in FlashcardSidebar.cpp)
void FlashcardSidebarCreate(MainWindow* win);
void FlashcardSidebarDestroy(MainWindow* win);
void FlashcardSidebarToggle(MainWindow* win);
void FlashcardSidebarPopulate(MainWindow* win);

#endif