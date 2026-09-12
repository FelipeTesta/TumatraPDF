/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "Commands.h"
#include "Settings.h"
#include "GlobalPrefs.h"
#include "Translations.h"
#include "Annotation.h"
#include "DocProperties.h"
#include "TreeModel.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
extern "C" {
#include <mupdf/pdf.h>
}
#include "EngineMupdf.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Toolbar.h"
#include "SumatraDialogs.h"
#include "AppSettings.h"
#include "Selection.h"
#include "Flashcard.h"
#include "FlashcardSidebar.h"
#include "Commands_Flashcard.h"

static void AddUniquePageNo(Vec<int>& pageNos, int pageNo) {
    for (int i = 0; i < len(pageNos); i++) {
        if (pageNos[i] == pageNo) return;
    }
    pageNos.Append(pageNo);
}

static void BuildFilteredStudyOrder(MainWindow* win) {
    win->flashcard.studyOrder.Reset();
    for (int i = 0; i < len(win->flashcard.cards); i++) {
        Flashcard& card = win->flashcard.cards[i];
        int from = win->flashcard.filterPageFrom;
        int to = win->flashcard.filterPageTo;
        bool passFilter = true;
        if (from > 0 && card.pageNo < from) passFilter = false;
        if (to > 0 && card.pageNo > to) passFilter = false;
        if (passFilter) {
            win->flashcard.studyOrder.Append(i);
        }
    }
    logf("FC: BuildFilteredStudyOrder - %d cards in filter range (from=%d, to=%d)\n",
         len(win->flashcard.studyOrder), win->flashcard.filterPageFrom, win->flashcard.filterPageTo);
}

static void HandleFlashcardRate(MainWindow* win, int rating) {
    int cardIdx = win->flashcard.studyOrder[win->flashcard.currentCardIdx];
    Flashcard& card = win->flashcard.cards[cardIdx];

    // Find or create study state for this card
    FlashcardStudyState state = {};
    for (int i = 0; i < len(win->flashcard.studyDoc.states); i++) {
        if (win->flashcard.studyDoc.states[i].annotId == card.annotId) {
            state = win->flashcard.studyDoc.states[i].state;
            break;
        }
    }
    FlashcardSm2Update(state, rating);

    // Save/update study state
    bool found = false;
    for (int i = 0; i < len(win->flashcard.studyDoc.states); i++) {
        if (win->flashcard.studyDoc.states[i].annotId == card.annotId) {
            win->flashcard.studyDoc.states[i].state = state;
            found = true;
            break;
        }
    }
    if (!found) {
        FlashcardStudyDoc::StateEntry entry;
        entry.annotId = card.annotId;
        entry.state = state;
        win->flashcard.studyDoc.states.Append(entry);
    }

    // Save to disk
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->filePath) {
        FlashcardStudySave(tab->filePath.s, win->flashcard.studyDoc);
    }

    // Advance to next card
    win->flashcard.revealMode = false;
    win->flashcard.currentCardIdx++;
    if (win->flashcard.currentCardIdx >= len(win->flashcard.studyOrder)) {
        win->flashcard.studyMode = false;
        win->flashcard.currentCardIdx = -1;
    } else {
        // Navigate to next card position
        int nextCardIdx = win->flashcard.studyOrder[win->flashcard.currentCardIdx];
        Flashcard& nextCard = win->flashcard.cards[nextCardIdx];
        WindowTab* tab2 = win->CurrentTab();
        if (tab2) {
            DisplayModel* dm2 = tab2->AsFixed();
            if (dm2) {
                Rect screenRect = dm2->CvtToScreen(nextCard.pageNo, nextCard.bounds);
                dm2->ScrollScreenToRect(nextCard.pageNo, screenRect);
            }
        }
    }
    MainWindowRerender(win);
}

bool HandleCommandFlashcard(MainWindow* win, int cmd) {
    switch (cmd) {
    case CmdFlashcardToggle: {
        logf("FC: CmdFlashcardToggle - toggling flashcard mode\n");
        win->flashcard.on = !win->flashcard.on;
        logf("FC: CmdFlashcardToggle - mode %s\n", StrL(win->flashcard.on ? "ON" : "OFF"));
        if (win->flashcard.on) {
            FlashcardToolbarCreate(win);
            // Load flashcards from current document
            WindowTab* tab = win->CurrentTab();
            if (tab) {
                DisplayModel* dm = tab->AsFixed();
                if (dm) {
                    auto* engine = AsEngineMupdf(dm->GetEngine());
                    if (engine) {
                        win->flashcard.cards = FlashcardLoadFromDocument(engine);
                        logf("Flashcard: loaded %d cards\n", len(win->flashcard.cards));
                    }
                }
            }
        } else {
            FlashcardToolbarDestroy(win);
            win->flashcard.studyMode = false;
            win->flashcard.revealMode = false;
            win->flashcard.currentCardIdx = -1;
            win->flashcard.cards.Reset();
            win->flashcard.studyDoc.states.Reset();
            win->flashcard.studyOrder.Reset();
            win->flashcard.filterPageFrom = -1;
            win->flashcard.filterPageTo = -1;
        }
        MainWindowRerender(win);
        ToolbarUpdateStateForWindow(win, true);
        return true;
    }
    case CmdFlashcardAdd: {
        logf("FC: CmdFlashcardAdd - adding flashcard from selection\n");
        WindowTab* tab = win->CurrentTab();
        if (!tab) return true;
        DisplayModel* dm = tab->AsFixed();
        if (!dm) return true;
        EngineBase* engine = dm->GetEngine();
        if (!engine || !EngineSupportsAnnotations(engine)) return true;
        if (!win->selection.showSelection || !tab->selectionOnPage) {
            logf("FC: CmdFlashcardAdd - ERROR: no text selected or no engine\n");
            return true;
        }

        // Get selected text
        bool isTextOnly = false;
        TempStr selText = GetSelectedTextTemp(tab, StrL("\r\n"), isTextOnly);
        Str content = fmt("Q: %s", selText ? Str(selText.s) : StrL(""));

        // Create FreeText annotation
        AnnotCreateArgs args{};
        args.annotType = AnnotationType::FreeText;
        args.content = content;
        args.setContentToSelection = false;

        // Collect pages from selection
        Vec<SelectionOnPage>* s = tab->selectionOnPage;
        Vec<int> pageNos;
        for (auto& sel : *s) {
            if (!dm->ValidPageNo(sel.pageNo)) continue;
            AddUniquePageNo(pageNos, sel.pageNo);
        }

        Annotation* annot = nullptr;
        for (auto pageNo : pageNos) {
            Vec<RectF> rects;
            for (auto& sel : *s) {
                if (pageNo != sel.pageNo) continue;
                rects.Append(sel.rect);
            }
            annot = EngineMupdfCreateAnnotation(engine, pageNo, PointF{}, &args);
            if (annot) {
                SetQuadPointsAsRect(annot, rects);
                annot->bounds = GetBounds(annot);
            }
        }

        if (annot) {
            DeleteOldSelectionInfo(win, true);
            MainWindowRerender(win);
            ToolbarUpdateStateForWindow(win, true);
        }
        return true;
    }
    case CmdFlashcardStudy: {
        logf("FC: CmdFlashcardStudy - toggling study mode\n");
        win->flashcard.studyMode = !win->flashcard.studyMode;
        win->flashcard.revealMode = false;
        win->flashcard.currentCardIdx = -1;
        if (win->flashcard.studyMode) {
            logf("FC: CmdFlashcardStudy - study ON, %d cards\n", len(win->flashcard.studyOrder));
            // Build filtered study order
            BuildFilteredStudyOrder(win);
            // Navigate to first card
            if (len(win->flashcard.studyOrder) > 0) {
                win->flashcard.currentCardIdx = 0;
                int cardIdx = win->flashcard.studyOrder[0];
                Flashcard& card = win->flashcard.cards[cardIdx];
                WindowTab* tab = win->CurrentTab();
                if (tab) {
                    DisplayModel* dm = tab->AsFixed();
                    if (dm) {
                        Rect screenRect = dm->CvtToScreen(card.pageNo, card.bounds);
                        dm->ScrollScreenToRect(card.pageNo, screenRect);
                    }
                }
            }
        } else {
            logf("FC: CmdFlashcardStudy - study OFF\n");
        }
        MainWindowRerender(win);
        return true;
    }
    case CmdFlashcardReveal: {
        logf("FC: CmdFlashcardReveal - revealing answer for card %d\n", win->flashcard.currentCardIdx);
        if (win->flashcard.studyMode && !win->flashcard.revealMode) {
            win->flashcard.revealMode = true;
            MainWindowRerender(win);
        }
        return true;
    }
    case CmdFlashcardRate1: {
        logf("FC: CmdFlashcardRate1 (Again) - card %d\n", win->flashcard.currentCardIdx);
        if (!win->flashcard.studyMode || win->flashcard.currentCardIdx < 0) return true;
        HandleFlashcardRate(win, 1);
        return true;
    }
    case CmdFlashcardRate2: {
        logf("FC: CmdFlashcardRate2 (Hard) - card %d\n", win->flashcard.currentCardIdx);
        if (!win->flashcard.studyMode || win->flashcard.currentCardIdx < 0) return true;
        HandleFlashcardRate(win, 2);
        return true;
    }
    case CmdFlashcardRate3: {
        logf("FC: CmdFlashcardRate3 (Good) - card %d\n", win->flashcard.currentCardIdx);
        if (!win->flashcard.studyMode || win->flashcard.currentCardIdx < 0) return true;
        HandleFlashcardRate(win, 3);
        return true;
    }
    case CmdFlashcardRate4: {
        logf("FC: CmdFlashcardRate4 (Easy) - card %d\n", win->flashcard.currentCardIdx);
        if (!win->flashcard.studyMode || win->flashcard.currentCardIdx < 0) return true;
        HandleFlashcardRate(win, 4);
        return true;
    }
    case CmdFlashcardBack: {
        logf("FC: CmdFlashcardBack - going back\n");
        if (win->flashcard.studyMode && win->flashcard.currentCardIdx > 0) {
            win->flashcard.revealMode = false;
            win->flashcard.currentCardIdx--;
            // Navigate to previous card position
            int prevCardIdx = win->flashcard.studyOrder[win->flashcard.currentCardIdx];
            Flashcard& prevCard = win->flashcard.cards[prevCardIdx];
            WindowTab* tab3 = win->CurrentTab();
            if (tab3) {
                DisplayModel* dm3 = tab3->AsFixed();
                if (dm3) {
                    Rect screenRect = dm3->CvtToScreen(prevCard.pageNo, prevCard.bounds);
                    dm3->ScrollScreenToRect(prevCard.pageNo, screenRect);
                }
            }
            MainWindowRerender(win);
        } else {
            logf("FC: CmdFlashcardBack - no previous card\n");
        }
        return true;
    }
    case CmdFlashcardLista:
        logf("FC: CmdFlashcardLista - toggling sidebar\n");
        FlashcardSidebarToggle(win);
        return true;
    case CmdFlashcardNext:
        logf("FC: CmdFlashcardNext - not used (auto-advance)\n");
        return true;
    case CmdFlashcardFilter: {
        logf("FC: CmdFlashcardFilter - opening page filter dialog\n");
        // Get current page count
        int pageCount = 0;
        WindowTab* tab = win->CurrentTab();
        if (tab) {
            DisplayModel* dm = tab->AsFixed();
            if (dm) {
                pageCount = dm->PageCount();
            }
        }
        if (pageCount <= 0) {
            logf("FC: CmdFlashcardFilter - ERROR: no document loaded\n");
            return true;
        }
        // Show simple filter dialog using GoToPage pattern
        TempStr fromStr = nullptr;
        TempStr toStr = nullptr;
        if (win->flashcard.filterPageFrom > 0) {
            fromStr = fmt("%d", win->flashcard.filterPageFrom);
        }
        if (win->flashcard.filterPageTo > 0) {
            toStr = fmt("%d", win->flashcard.filterPageTo);
        }
        // Use two sequential GoToPage dialogs
        TempStr result1 = Dialog_GoToPage(win->hwndFrame, fromStr ? fromStr : StrL(""), pageCount, true);
        if (result1 && len(result1) > 0) {
            int from = ParseInt(result1);
            if (from >= 1 && from <= pageCount) {
                win->flashcard.filterPageFrom = from;
            }
        } else if (result1 && len(result1) == 0) {
            // Empty = clear filter from
            win->flashcard.filterPageFrom = -1;
        }
        TempStr result2 = Dialog_GoToPage(win->hwndFrame, toStr ? toStr : StrL(""), pageCount, true);
        if (result2 && len(result2) > 0) {
            int to = ParseInt(result2);
            if (to >= 1 && to <= pageCount) {
                win->flashcard.filterPageTo = to;
            }
        } else if (result2 && len(result2) == 0) {
            // Empty = clear filter to
            win->flashcard.filterPageTo = -1;
        }
        logf("FC: CmdFlashcardFilter - filter set: from=%d, to=%d\n",
             win->flashcard.filterPageFrom, win->flashcard.filterPageTo);
        return true;
    }
    default:
        return false;
    }
}