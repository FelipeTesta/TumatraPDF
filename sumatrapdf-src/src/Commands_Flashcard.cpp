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
#include "Selection.h"
#include "Flashcard.h"
#include "Commands_Flashcard.h"

static void AddUniquePageNo(Vec<int>& pageNos, int pageNo) {
    for (int i = 0; i < len(pageNos); i++) {
        if (pageNos[i] == pageNo) return;
    }
    pageNos.Append(pageNo);
}

bool HandleCommandFlashcard(MainWindow* win, int cmd) {
    switch (cmd) {
    case CmdFlashcardToggle: {
        win->flashcard.on = !win->flashcard.on;
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
        }
        MainWindowRerender(win);
        ToolbarUpdateStateForWindow(win, true);
        return true;
    }
    case CmdFlashcardAdd: {
        WindowTab* tab = win->CurrentTab();
        if (!tab) return true;
        DisplayModel* dm = tab->AsFixed();
        if (!dm) return true;
        EngineBase* engine = dm->GetEngine();
        if (!engine || !EngineSupportsAnnotations(engine)) return true;
        if (!win->selection.showSelection || !tab->selectionOnPage) return true;

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
        win->flashcard.studyMode = !win->flashcard.studyMode;
        win->flashcard.revealMode = false;
        win->flashcard.currentCardIdx = -1;
        if (win->flashcard.studyMode) {
            // Build study order: all card indices
            win->flashcard.studyOrder.Reset();
            for (int i = 0; i < len(win->flashcard.cards); i++) {
                win->flashcard.studyOrder.Append(i);
            }
            // Navigate to first card
            if (len(win->flashcard.studyOrder) > 0) {
                win->flashcard.currentCardIdx = 0;
                // TODO: navigate to card position
            }
        }
        MainWindowRerender(win);
        return true;
    }
    case CmdFlashcardReveal: {
        if (win->flashcard.studyMode && !win->flashcard.revealMode) {
            win->flashcard.revealMode = true;
            MainWindowRerender(win);
        }
        return true;
    }
    case CmdFlashcardRate1:
    case CmdFlashcardRate2:
    case CmdFlashcardRate3:
    case CmdFlashcardRate4: {
        if (!win->flashcard.studyMode || win->flashcard.currentCardIdx < 0) return true;
        int rating = cmd - CmdFlashcardRate1 + 1;
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
        }
        MainWindowRerender(win);
        return true;
    }
    case CmdFlashcardBack: {
        if (win->flashcard.studyMode && win->flashcard.currentCardIdx > 0) {
            win->flashcard.revealMode = false;
            win->flashcard.currentCardIdx--;
            MainWindowRerender(win);
        }
        return true;
    }
    case CmdFlashcardLista:
        logf("Flashcard: Lista sidebar (v2)\n");
        return true;
    case CmdFlashcardNext:
        // Not used in auto-advance study mode
        return true;
    default:
        return false;
    }
}