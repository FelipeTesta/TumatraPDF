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

bool RelayoutFrame(MainWindow* win, bool updateToolbars = true, int sidebarDx = -1);

static void AddUniquePageNo(Vec<int>& pageNos, int pageNo) {
    for (int i = 0; i < len(pageNos); i++) {
        if (pageNos[i] == pageNo) return;
    }
    pageNos.Append(pageNo);
}

static void BuildFilteredStudyOrder(MainWindow* win) {
    win->flashcard.studyOrder.Reset();
    i64 now = (i64)time(nullptr) * 1000;
    for (int i = 0; i < len(win->flashcard.cards); i++) {
        Flashcard& card = win->flashcard.cards[i];
        int from = win->flashcard.filterPageFrom;
        int to = win->flashcard.filterPageTo;
        bool passFilter = true;
        if (from > 0 && card.pageNo < from) passFilter = false;
        if (to > 0 && card.pageNo > to) passFilter = false;
        if (passFilter) {
            // SRS: include only cards that are new (never rated) or due for review
            bool isNew = true;
            bool isDue = false;
            for (int j = 0; j < len(win->flashcard.studyDoc.states); j++) {
                if (win->flashcard.studyDoc.states[j].key == card.key) {
                    const FlashcardStudyState& s = win->flashcard.studyDoc.states[j].state;
                    isNew = (s.rating == 0);
                    isDue = (!isNew && s.nextReviewAt <= now);
                    break;
                }
            }
            if (isNew || isDue) {
                win->flashcard.studyOrder.Append(i);
            }
        }
    }
    logf("[fc] BuildFilteredStudyOrder - %d due cards (from=%d, to=%d)\n", len(win->flashcard.studyOrder),
         win->flashcard.filterPageFrom, win->flashcard.filterPageTo);
}

static void HandleFlashcardRate(MainWindow* win, int rating) {
    int cardIdx = win->flashcard.studyOrder[win->flashcard.currentCardIdx];
    Flashcard& card = win->flashcard.cards[cardIdx];

    // Find or create study state for this card
    FlashcardStudyState state = {};
    for (int i = 0; i < len(win->flashcard.studyDoc.states); i++) {
        if (win->flashcard.studyDoc.states[i].key == card.key) {
            state = win->flashcard.studyDoc.states[i].state;
            break;
        }
    }
    FlashcardSm2Update(state, rating);

    // Relearn: rating 1 (Again) reinserts this card at the end of the session
    // queue so it is presented again (matching Anki's relearning behavior).
    if (rating == 1) {
        win->flashcard.studyOrder.Append(cardIdx);
    }

    // Save/update study state
    bool found = false;
    for (int i = 0; i < len(win->flashcard.studyDoc.states); i++) {
        if (win->flashcard.studyDoc.states[i].key == card.key) {
            win->flashcard.studyDoc.states[i].state = state;
            found = true;
            break;
        }
    }
    if (!found) {
        FlashcardStudyDoc::StateEntry entry;
        entry.key = card.key;
        entry.state = state;
        win->flashcard.studyDoc.states.Append(entry);
    }

    // Save to disk
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->filePath) {
        FlashcardStudySave(tab->filePath.s, win->flashcard.studyDoc);
    }

    FlashcardToolbarUpdateCount(win);

    // Advance to next card
    win->flashcard.revealMode = false;
    FlashcardToolbarUpdateState(win);
    win->flashcard.currentCardIdx++;
    if (win->flashcard.currentCardIdx >= len(win->flashcard.studyOrder)) {
        win->flashcard.studyMode = false;
        win->flashcard.currentCardIdx = -1;
        logf("[fc] HandleFlashcardRate - study session ended (all %d cards rated)\n", len(win->flashcard.studyOrder));
    } else {
        logf("[fc] HandleFlashcardRate - advancing to card %d/%d\n", win->flashcard.currentCardIdx + 1,
             len(win->flashcard.studyOrder));
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
            logf("[fc] CmdFlashcardToggle - toggling flashcard mode\n");
            win->flashcard.on = !win->flashcard.on;
            logf("[fc] CmdFlashcardToggle - mode %s\n", StrL(win->flashcard.on ? "ON" : "OFF"));
            if (win->flashcard.on) {
                FlashcardToolbarCreate(win);
                RelayoutFrame(win, true, -1);
                // Close arch tools if open (mutually exclusive toolbars)
                if (win->archTools.on) {
                    win->archTools.on = false;
                    if (win->archTools.hwndReBar2) {
                        ShowWindow(win->archTools.hwndReBar2, SW_HIDE);
                    }
                    SetToolbarButtonCheckedState(win, CmdArchToolsToggle, false);
                }
                // Load flashcards from current document
                WindowTab* tab = win->CurrentTab();
                if (tab) {
                    DisplayModel* dm = tab->AsFixed();
                    if (dm) {
                        auto* engine = AsEngineMupdf(dm->GetEngine());
                        if (engine) {
                            win->flashcard.cards = FlashcardLoadFromDocument(engine);
                            logf("Flashcard: loaded %d cards\n", len(win->flashcard.cards));
                            // Load persisted study state so new/due counts are correct
                            if (tab->filePath) {
                                win->flashcard.studyDoc = FlashcardStudyLoad(tab->filePath.s);
                                logf("Flashcard: loaded %d study states\n", len(win->flashcard.studyDoc.states));
                            }
                            FlashcardToolbarUpdateCount(win);
                        }
                    }
                }
            } else {
                FlashcardToolbarDestroy(win);
                RelayoutFrame(win, true, -1);
                win->flashcard.studyMode = false;
                win->flashcard.revealMode = false;
                win->flashcard.currentCardIdx = -1;
                win->flashcard.cards.Reset();
                win->flashcard.studyDoc.states.Reset();
                win->flashcard.studyOrder.Reset();
                win->flashcard.filterPageFrom = -1;
                win->flashcard.filterPageTo = -1;
                logf("[fc] CmdFlashcardToggle - flashcard OFF, state cleared\n");
            }
            MainWindowRerender(win);
            ToolbarUpdateStateForWindow(win, true);
            return true;
        }
        case CmdFlashcardAdd: {
            logf("[fc] CmdFlashcardAdd - adding flashcard from selection\n");
            WindowTab* tab = win->CurrentTab();
            if (!tab) return true;
            DisplayModel* dm = tab->AsFixed();
            if (!dm) return true;
            EngineBase* engine = dm->GetEngine();
            if (!engine || !EngineSupportsAnnotations(engine)) return true;
            if (!win->selection.showSelection || !tab->selectionOnPage) {
                logf("[fc] CmdFlashcardAdd - ERROR: no text selected or no engine\n");
                return true;
            }

            // Cloze is positional: the highlight rect is the mask over existing PDF
            // text. No duplicated content is stored in the annotation (content empty).
            Str content;

            // Create Highlight annotation that covers the selected text
            AnnotCreateArgs args{};
            args.annotType = AnnotationType::Highlight;
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

                    // Set flashcard-specific properties: author, gray color, opacity
                    {
                        EngineMupdf* epdf = AsEngineMupdf(engine);
                        fz_context* ctx = epdf->Ctx();
                        ScopedRecursiveMutex cs(&epdf->docLock);
                        fz_try(ctx) {
                            pdf_set_annot_author(ctx, annot->pdfannot, CStrTemp(StrL("TumatraPDF-Flashcard")));
                            float gray[3] = {0.5f, 0.5f, 0.5f};
                            pdf_set_annot_color(ctx, annot->pdfannot, 3, gray);
                            pdf_set_annot_opacity(ctx, annot->pdfannot, 0.4f);
                        }
                        fz_catch(ctx) {
                            fz_report_error(ctx);
                        }
                    }
                }
            }

            if (annot) {
                DeleteOldSelectionInfo(win, true);
                // Reload cards so count/order reflect the new cloze immediately
                win->flashcard.cards = FlashcardLoadFromDocument(AsEngineMupdf(engine));
                if (tab->filePath) {
                    win->flashcard.studyDoc = FlashcardStudyLoad(tab->filePath.s);
                }
                MainWindowRerender(win);
                ToolbarUpdateStateForWindow(win, true);
                FlashcardToolbarUpdateCount(win);
                logf("[fc] Cloze card added (positional mask, no duplicated content), %d page(s) covered\n",
                     len(pageNos));
            }
            return true;
        }
        case CmdFlashcardStudy: {
            logf("[fc] CmdFlashcardStudy - toggling study mode\n");
            win->flashcard.studyMode = !win->flashcard.studyMode;
            win->flashcard.revealMode = false;
            win->flashcard.currentCardIdx = -1;
            if (win->flashcard.studyMode) {
                logf("[fc] CmdFlashcardStudy - study ON, %d cards\n", len(win->flashcard.studyOrder));
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
                            RectF vr;
                            int vPage = dm->PhysicalToVirtualForRect(card.pageNo, card.bounds, &vr);
                            Rect screenRect = dm->CvtToScreen(vPage, vr);
                            dm->ScrollScreenToRect(vPage, screenRect);
                        }
                    }
                }
            } else {
                logf("[fc] CmdFlashcardStudy - study OFF\n");
            }
            FlashcardToolbarUpdateState(win);
            MainWindowRerender(win);
            return true;
        }
        case CmdFlashcardReveal: {
            logf("[fc] CmdFlashcardReveal - revealing answer for card %d\n", win->flashcard.currentCardIdx);
            if (win->flashcard.studyMode && !win->flashcard.revealMode) {
                win->flashcard.revealMode = true;
                FlashcardToolbarUpdateState(win);
                MainWindowRerender(win);
            }
            return true;
        }
        case CmdFlashcardRate1: {
            logf("[fc] CmdFlashcardRate1 (Again) - card %d\n", win->flashcard.currentCardIdx);
            if (!win->flashcard.studyMode || win->flashcard.currentCardIdx < 0) return true;
            HandleFlashcardRate(win, 1);
            return true;
        }
        case CmdFlashcardRate2: {
            logf("[fc] CmdFlashcardRate2 (Hard) - card %d\n", win->flashcard.currentCardIdx);
            if (!win->flashcard.studyMode || win->flashcard.currentCardIdx < 0) return true;
            HandleFlashcardRate(win, 2);
            return true;
        }
        case CmdFlashcardRate3: {
            logf("[fc] CmdFlashcardRate3 (Good) - card %d\n", win->flashcard.currentCardIdx);
            if (!win->flashcard.studyMode || win->flashcard.currentCardIdx < 0) return true;
            HandleFlashcardRate(win, 3);
            return true;
        }
        case CmdFlashcardRate4: {
            logf("[fc] CmdFlashcardRate4 (Easy) - card %d\n", win->flashcard.currentCardIdx);
            if (!win->flashcard.studyMode || win->flashcard.currentCardIdx < 0) return true;
            HandleFlashcardRate(win, 4);
            return true;
        }
        case CmdFlashcardBack: {
            logf("[fc] CmdFlashcardBack - going back\n");
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
                logf("[fc] CmdFlashcardBack - no previous card\n");
            }
            return true;
        }
        case CmdFlashcardLista:
            logf("[fc] CmdFlashcardLista - toggling sidebar\n");
            FlashcardSidebarToggle(win);
            return true;
        case CmdFlashcardFilter: {
            logf("[fc] CmdFlashcardFilter - opening page filter dialog\n");
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
                logf("[fc] CmdFlashcardFilter - ERROR: no document loaded\n");
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
            logf("[fc] CmdFlashcardFilter - filter set: from=%d, to=%d\n", win->flashcard.filterPageFrom,
                 win->flashcard.filterPageTo);
            return true;
        }
        default:
            return false;
    }
}