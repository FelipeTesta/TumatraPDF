# Flashcard System Implementation Plan for TumatraPDF

## 1. Architecture Overview

### 1.1 Core Principle

| Component | Storage | Portable? |
|---|---|---|
| **Cards** (text, position, page) | **Inside PDF** as `FreeText` annotation | ✅ Yes — via PDF |
| **Study state** (rating, interval, ease) | **External JSON** per user | ❌ No — personal |

Flashcards are standard PDF annotations. Share the PDF → cards travel with it.
Study progress stays on the local machine (per-user, per-document).

### 1.2 Card Types

| Type | Description | Annotation Type | Status |
|---|---|---|---|
| **Text Flashcard** | Selected text → Q/A card | `FreeText` | MVP |
| **Label/Tip** | Hint text visible over covered area | Custom overlay (v2) | Deferred |
| **Image Occlusion** | Solid rectangle covering non-selectable content | `Square` annotation | Deferred |

### 1.3 Data Model — Flashcard (MVP)

```cpp
// Flashcard — stored as PDF annotation
struct Flashcard {
    int id;                    // unique per-document (sequential, from annotation index)
    int pageNo;                // page number (1-based)
    RectF bounds;              // selection rect in PAGE coordinates (PDF points)
    Str text;                  // extracted text (question content)
    Str question;              // "Q:" portion (user-editable)
    Str answer;                // "A:" portion (user-editable)
    u64 createdAt;             // timestamp (ms since epoch)
};

// StudyState — stored in external JSON per user
struct FlashcardStudyState {
    int rating;                // last rating 1-4 (0 = new/unseen)
    int interval;              // days until next review
    float easeFactor;          // SM-2 ease factor, starts 2.5
    u64 lastReviewedAt;        // timestamp
    u64 nextReviewAt;          // timestamp
    int reviewCount;           // total reviews
};
```

### 1.4 Storage Strategy

#### Cards (inside PDF)
- `AnnotationType::FreeText` with `Author = "TumatraPDF-Flashcard"`
- `Contents` field: `"Q: question text\nA: answer text"` (or JSON for complex data)
- `bounds`: selection rectangle in page coordinates
- Saved automatically via `pdf_save_document()` (incremental save)
- **Portable**: Share PDF → cards visible in any PDF viewer

#### Study State (external)
- Location: `%APPDATA%\SumatraPDF\FlashcardStudy\<MD5-of-filepath>.json`
- Format:
```json
{
  "version": 1,
  "states": {
    "1": { "rating": 3, "interval": 6, "easeFactor": 2.5, "lastReviewedAt": 1725500000000, "nextReviewAt": 1726000000000, "reviewCount": 5 },
    "2": { "rating": 1, "interval": 0, "easeFactor": 2.3, "lastReviewedAt": 1725400000000, "nextReviewAt": 0, "reviewCount": 2 }
  }
}
```
- Key = annotation ID (from MuPDF)
- Loaded on document open, saved after each rating

### 1.5 UI Components

| Component | Pattern | Purpose |
|---|---|---|
| **Flashcard Button** (selection toolbar) | Like Highlight button | Create flashcard from text selection |
| **Keyboard Shortcut** (`S` key) | Like `A` for Highlight | Create flashcard from text selection |
| **Study Button** (main toolbar) | Toggle study mode on/off | Start/stop studying |
| **Secondary Toolbar** (below main) | Like `Toolbar2` (Arch Tools) | Study controls: Back, Filters, Lista |
| **Sidebar Panel** (left) | Like TOC/Favorites | Card list with IDs, click to navigate |
| **Highlight Rendering** | Extends `PaintSelection` | Subtle gray when reading; solid when studying |

### 1.6 State Struct (add to `MainWindow.h`)

```cpp
struct FlashcardState {
    bool on = false;                    // flashcard mode active
    bool studyMode = false;             // true when actively studying
    bool revealMode = false;            // true when answer is revealed (Space pressed)
    int currentCardIdx = -1;            // index in study order
    Vec<Flashcard> cards;               // loaded from PDF annotations
    Vec<int> studyOrder;                // shuffled indices for random mode
    int studyModeType = 0;              // 0 = In Order, 1 = Random
    int filterPageFrom = -1;            // filter: start page (-1 = all)
    int filterPageTo = -1;              // filter: end page (-1 = all)
    // Secondary toolbar
    HWND hwndReBarFlashcard = nullptr;
    HWND hwndToolbarFlashcard = nullptr;
    // Sidebar
    HWND hwndFlashcardBox = nullptr;
    ILayout* flashcardLayout = nullptr;
};
```

---

## 2. Study Flow

### 2.1 Complete Study Session

```
1. User clicks "Flashcard" button → flashcard mode ON
   → Secondary toolbar appears (Study | Back | Filters | Lista)
   → Subtle gray highlights appear on all cards

2. User clicks "Study" button → studyMode = true
   → First card highlighted SOLID (black if invert=off, gray if invert=on)
   → Text is COVERED (solid highlight over text)

3. User presses SPACE or ENTER → revealMode = true
   → Highlight becomes SUBTLE → text is VISIBLE
   → User reads the answer

4. User presses 1-4 → rate card
   → 1 = Again (reset), 2 = Hard, 3 = Good, 4 = Easy
   → SM-2 algorithm updates interval/ease
   → Study state saved to external JSON
   → Auto-advance to NEXT card (highlight solid)

5. If no more cards → study session ends
   → Show summary (optional)

6. User clicks "Back" → undo last rating
   → Revert to previous card
   → Restore previous study state
```

### 2.2 Back/Undo Button

- **Purpose**: Undo last rating, go back to previous card
- **Implementation**: Maintain a stack of `(cardIdx, previousRating, previousState)`
- On "Back": pop from stack, restore card to pre-rating state, re-highlight
- **Limitation**: Can only go back within current session (not across sessions)

### 2.3 Filters Button (v1 — Basic)

- **Purpose**: Restrict which cards are shown in study mode
- **UI**: Simple dialog with "From page" and "To page" inputs
- **Behavior**: Filters `studyOrder` to only include cards in the page range
- **Future**: More filters (by rating, by date, by tags)

### 2.4 Lista Button (Sidebar)

- **Purpose**: Show list of all cards, click to navigate
- **UI**: Left sidebar panel (like TOC/Favorites)
- **Content**: List of card IDs with page numbers and preview text
- **Click**: Navigate to card position, highlight it temporarily
- **Layout**: Reuse `ILayout` pattern from `TableOfContents.h`

---

## 3. File Plan

### 3.1 New Files to Create

| File | Purpose |
|------|---------|
| `src/Flashcard.h` | Flashcard struct, study state, storage logic |
| `src/Flashcard.cpp` | Load/save study state (JSON), CRUD operations, SM-2 algorithm |
| `src/FlashcardToolbar.cpp` | Secondary toolbar creation, button handlers |
| `src/FlashcardSidebar.cpp` | Sidebar panel UI (card list, navigation) |
| `src/Commands_Flashcard.h` | Command handler declarations |
| `src/Commands_Flashcard.cpp` | Command implementations |

### 3.2 Files to Modify

| File | Changes |
|------|---------|
| `src/MainWindow.h` | Add `FlashcardState flashcard;` struct |
| `src/Toolbar.h` | Declare `CreateFlashcardToolbar`, `DestroyFlashcardToolbar` |
| `src/Toolbar.cpp` | Add Study toggle button to main toolbar; implement toolbar2-style creation |
| `src/ToolbarLayout.h/cpp` | Add slot specs for flashcard toolbar controls |
| `src/Commands.h` | Add new command IDs (generated via `cmd/gen-commands.ts`) |
| `src/Commands.cpp` | Add command names, register handlers |
| `src/CommandAvailability.cpp` | Enable flashcard commands when document loaded |
| `src/SumatraPDF.cpp` | Handle CmdFlashcardAdd in WndProc (like CmdCreateAnnotHighlight); create/destroy toolbar on study toggle |
| `src/SelectionToolbar.cpp` | Add `{CmdFlashcardAdd, "Flashcard"}` to `gCandidateButtons[]` |
| `src/Accelerators.cpp` | Add `S` → `CmdFlashcardAdd` shortcut |
| `src/Canvas.cpp` | Render flashcard highlights in `OnPaintDocument` |
| `src/Annotation.h/cpp` | Add flashcard-specific helpers (create, identify, query) |
| `src/EngineMupdf.cpp` | Add flashcard annotation creation/deletion helpers |
| `src/EditAnnotations.cpp` | Filter flashcard annotations from regular ones |

---

## 4. Command Plan

Run `bun cmd/gen-commands.ts` after adding to `cmd/gen-commands.ts`:

### 4.1 Commands

| Command ID | Name | Purpose |
|------------|------|---------|
| `CmdFlashcardAdd` | Add selection as flashcard | Create new card from text selection (like Highlight) |
| `CmdFlashcardStudy` | Toggle study mode | Start/stop studying (solid highlights) |
| `CmdFlashcardReveal` | Reveal answer | Space/Enter — show text behind highlight |
| `CmdFlashcardRate1` | Rate "Again" | 1 key — reset, stay on card |
| `CmdFlashcardRate2` | Rate "Hard" | 2 key — short interval |
| `CmdFlashcardRate3` | Rate "Good" | 3 key — normal interval |
| `CmdFlashcardRate4` | Rate "Easy" | 4 key — long interval |
| `CmdFlashcardBack` | Undo last rating | Go back to previous card |
| `CmdFlashcardFilters` | Show filters dialog | Restrict cards by page range |
| `CmdFlashcardList` | Toggle sidebar | Show/hide card list panel |
| `CmdFlashcardClearAll` | Delete all flashcards | Remove all cards from document |

### 4.2 Creation Flow (like Highlight)

**Selection Toolbar Button** — add to `gCandidateButtons[]` in `SelectionToolbar.cpp`:
```cpp
{CmdFlashcardAdd, "Flashcard"},
```
This adds a "Flashcard" button to the popup toolbar that appears when text is selected.

**Keyboard Shortcut** — `S` key (available, only `Ctrl+S` = SaveAs is used):
- Add `CmdFlashcardAdd` → `S` in `Accelerators.cpp`

**Handler** — in `SumatraPDF.cpp`, similar to `CmdCreateAnnotHighlight`:
```cpp
case CmdFlashcardAdd: {
    AnnotCreateArgs args{AnnotationType::FreeText};
    args.content = fmt("Q: %s\nA: ", selectedText);
    args.col = PdfColor(0, 0, 0);        // border: black
    args.bgCol = PdfColor(255, 255, 200); // fill: light yellow
    args.opacity = 80;
    args.textSize = 12;
    args.borderWidth = 1;
    lastCreatedAnnot = MakeAnnotationsFromSelection(tab, &args);
    SetAuthor(lastCreatedAnnot, "TumatraPDF-Flashcard");
    // Save PDF + empty study state
} break;
```

### 4.3 Study Mode Shortcuts (during study)

- `Space` / `Enter` — Reveal answer
- `1-4` — Rate card (auto-advance)
- `Backspace` — Undo last rating (Back)
- `F` — Toggle flashcard mode
- `L` — Toggle sidebar list

---

## 5. Study Algorithm (SM-2 Lite)

```cpp
void UpdateCardStudyState(FlashcardStudyState* state, int rating) {
    if (rating < 3) { // Again or Hard
        state->interval = (rating == 1) ? 0 : 1; // reset or 1 day
        state->easeFactor -= 0.2; // penalty
    } else { // Good or Easy
        if (state->interval == 0) state->interval = 1;
        else if (state->interval == 1) state->interval = 6;
        else state->interval = (int)(state->interval * state->easeFactor);
    }
    
    // Ease factor adjustment
    state->easeFactor += (0.1 - (3 - rating) * (0.08 + 0.02));
    if (state->easeFactor < 1.3) state->easeFactor = 1.3;
    
    state->rating = rating;
    state->lastReviewedAt = NowMs();
    state->nextReviewAt = state->lastReviewedAt + state->interval * 86400000;
    state->reviewCount++;
}
```

---

## 6. Implementation Details

### 6.1 Creating a Flashcard

```cpp
// User selected text → clicked "Add to Flashcards"
void CmdFlashcardAdd(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = tab->AsFixed();
    EngineMupdf* engine = AsEngineMupdf(dm->GetEngine());
    
    // Get selection in page coordinates
    SelectionOnPage* sel = tab->selection.GetSelectionOnPage();
    if (!sel) return;
    
    int pageNo = sel->pageNo;
    RectF bounds = sel->GetRect();
    
    // Create FreeText annotation
    AnnotCreateArgs args = {};
    args.annotType = AnnotationType::FreeText;
    args.content = fmt("Q: %s\nA: ", sel->GetText()); // Question only, answer empty
    args.col = PdfColor(0, 0, 0);        // border: black
    args.bgCol = PdfColor(255, 255, 200); // fill: light yellow
    args.opacity = 80;
    args.textSize = 12;
    args.borderWidth = 1;
    
    // Set author to identify as flashcard
    Annotation* card = EngineMupdfCreateAnnotation(engine, pageNo, bounds, &args);
    if (card) {
        SetAuthor(card, "TumatraPDF-Flashcard");
        pdf_save_document(engine->pdf_doc, ...); // Save to PDF
        
        // Create empty study state
        FlashcardStudyState state = {};
        state.rating = 0;
        state.easeFactor = 2.5;
        FlashcardStudySave(win, card->id, &state);
        
        MainWindowRerender(win);
    }
}
```

### 6.2 Loading Cards from PDF

```cpp
// On document open
void FlashcardLoadFromPDF(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = tab->AsFixed();
    EngineMupdf* engine = AsEngineMupdf(dm->GetEngine());
    
    win->flashcard.cards.Clear();
    
    // Iterate all annotations, find flashcards by author
    for (int pageNo = 1; pageNo <= engine->pageCount; pageNo++) {
        pdf_page* page = pdf_load_page(engine->pdf_doc, pageNo - 1);
        pdf_annot* annot = pdf_first_annot(page);
        while (annot) {
            char* author = pdf_annot_author(annot);
            if (author && StrEq(author, "TumatraPDF-Flashcard")) {
                Flashcard card;
                card.id = pdf_annot_obj(annot)->i; // MuPDF object ID
                card.pageNo = pageNo;
                card.bounds = GetAnnotRect(annot); // page coords
                card.text = GetAnnotContents(annot);
                ParseQAPair(card.text, &card.question, &card.answer);
                card.createdAt = GetAnnotModificationDate(annot);
                win->flashcard.cards.Append(card);
            }
            annot = pdf_next_annot(annot);
        }
        pdf_drop_page(page);
    }
}
```

### 6.3 Highlight Rendering

```cpp
// In Canvas.cpp OnPaintDocument
void PaintFlashcardHighlights(MainWindow* win, HDC hdc) {
    if (!win->flashcard.on) return;
    
    for (int i = 0; i < win->flashcard.cards.Count(); i++) {
        Flashcard& card = win->flashcard.cards[i];
        RectF screenRect = dm->CvtToScreen(card.pageNo, card.bounds);
        
        bool isCurrentCard = (i == win->flashcard.currentCardIdx);
        bool isRevealed = win->flashcard.revealMode && isCurrentCard;
        
        COLORREF color;
        if (win->flashcard.studyMode && isCurrentCard && !isRevealed) {
            // Study mode: SOLID highlight (covers text)
            color = GetFlashcardStudyColor(win);
            alpha = 200;
        } else {
            // Reading mode: SUBTLE highlight
            color = GetFlashcardReadingColor(win);
            alpha = 30;
        }
        
        PaintTransparentRect(hdc, screenRect, color, alpha);
    }
}
```

---

## 7. Future Features (v2)

### 7.1 Label/Tip
- After creating a flashcard, option to add a "tip" text
- Tip is visible OVER the solid highlight (as overlay text)
- Helps user guess the answer without revealing fully
- Implementation: Custom overlay drawing in `PaintFlashcardHighlights`

### 7.2 Image Occlusion
- Instead of text selection, create a SOLID RECTANGLE annotation
- Covers any area of the document (images, diagrams, formulas)
- Uses `AnnotationType::Square` with solid fill
- Functional same as text flashcards: cover → reveal → rate
- **Creation flow**: User draws rectangle over image area → creates Square annotation with author="TumatraPDF-Flashcard-Occlusion"
- **Study flow**: Same as text cards — solid rectangle covers area, Space reveals (removes rectangle temporarily)

### 7.3 Advanced Filters
- Filter by page range, rating, date, tags
- Filter by card type (text vs image occlusion)
- Saved filter presets

### 7.4 Spaced Repetition Scheduling
- Show only "due" cards (nextReviewAt <= now)
- Deck statistics (new/learning/review counts)
- Custom intervals

---

## 8. MVP Scope

**Must have (MVP):**
1. ✅ "Flashcard" button in selection toolbar (like Highlight)
2. ✅ `S` keyboard shortcut to create flashcard from selection
3. ✅ Secondary toolbar: Back | Filters | Lista
4. ✅ Study mode toggle (main toolbar button)
5. ✅ Subtle gray highlight when reading; solid when studying
6. ✅ Study mode: Space reveals → 1-4 rates → auto-advance
7. ✅ Back button: undo last rating
8. ✅ Filters: basic page range filter
9. ✅ Lista: sidebar panel with card list, click to navigate
10. ✅ Cards saved as PDF annotations (portable)
11. ✅ Study state saved as external JSON (personal)

**Defer to v2:**
- Label/Tip overlay
- Image Occlusion
- Advanced filters (rating, date, tags)
- Spaced repetition scheduling (due cards only)
- Cross-document flashcard deck
- Export/import (Anki CSV)
- Sidebar stats/progress

---

## 9. Effort Estimate

| Component | Files | Est. Hours | Risk |
|-----------|-------|------------|------|
| Data model + PDF annotation storage | 2 | 3 | Low |
| Study state JSON persistence | 1 | 2 | Low |
| Selection toolbar button + S shortcut | 2 | 2 | Low (reuses existing) |
| Secondary toolbar | 2 | 5 | Medium |
| Highlight rendering | 1 | 3 | Low |
| Study mode logic (reveal/rate/back) | 1 | 4 | Medium |
| Filters (page range) | 1 | 2 | Low |
| Sidebar panel (card list) | 2 | 5 | Medium |
| Commands + wiring | 3 | 2 | Low |
| **Total MVP** | **~15** | **~28** | **Medium** |

---

## 10. Testing Checklist

- [ ] Toggle flashcard mode shows/hides secondary toolbar
- [ ] Text selection → "Add to Flashcards" creates FreeText annotation in PDF
- [ ] Flashcard annotation has correct author ("TumatraPDF-Flashcard")
- [ ] PDF save preserves flashcard annotations
- [ ] Reading mode: subtle gray highlights on all cards
- [ ] Study mode: solid highlight covers current card text
- [ ] Space/Enter reveals text (highlight becomes subtle)
- [ ] 1-4 keys rate card, auto-advance to next
- [ ] Back button undoes last rating
- [ ] Filters: restrict cards by page range
- [ ] Lista sidebar: lists all cards, click navigates to position
- [ ] Invert colors: highlights adapt (black↔gray)
- [ ] Study state persists across sessions (external JSON)
- [ ] Cards persist in PDF (survive close/reopen)
- [ ] Share PDF → another user sees same cards
- [ ] No memory leaks

---

*Plan updated 2026-09-10 based on user feedback. Cards = PDF annotations (portable), study state = external JSON (personal).*
