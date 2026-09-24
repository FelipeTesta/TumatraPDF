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
    // random order (user setting): shuffle the queue in place once, when it is
    // built. Next / rate-advance / back then all follow this shuffled order.
    if (gGlobalPrefs->flashcardSettings.randomOrder) {
        Vec<int>& order = win->flashcard.studyOrder;
        for (int i = len(order) - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            int tmp = order[i];
            order[i] = order[j];
            order[j] = tmp;
        }
    }
    logf("[fc] BuildFilteredStudyOrder - %d due cards (from=%d, to=%d, order=%s)\n", len(win->flashcard.studyOrder),
         win->flashcard.filterPageFrom, win->flashcard.filterPageTo,
         gGlobalPrefs->flashcardSettings.randomOrder ? StrL("random") : StrL("sequential"));
}

// Navigate the view to a study card. Under TC2 the display uses virtual pages,
// so route the physical card rect through PhysicalToVirtualForRect first (raw
// pageNo + CvtToScreen lands on the wrong page/column). When centerVertically
// is set, scroll so the card sits in the middle of the viewport (instant, no
// animation — ScrollYTo jumps); otherwise keep the current scroll position.
static void FlashcardNavigateToCard(MainWindow* win, int cardIdx, bool centerVertically) {
    if (cardIdx < 0 || cardIdx >= len(win->flashcard.cards)) {
        return;
    }
    Flashcard& card = win->flashcard.cards[cardIdx];
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return;
    }
    RectF vr;
    int vPage = dm->PhysicalToVirtualForRect(card.pageNo, card.bounds, &vr);
    if (!dm->ValidPageNo(vPage)) {
        return;
    }
    // CvtToScreen returns VIEWPORT-RELATIVE coords (it adds pageOnScreen,
    // which moves with the scroll). Same idiom as ScrollScreenToRect: compute
    // the delta from the current view and scroll by it (ScrollYBy clamps).
    Rect screenRect = dm->CvtToScreen(vPage, vr);
    if (!centerVertically) {
        return;
    }
    int dy = screenRect.y + screenRect.dy / 2 - dm->viewPort.dy / 2;
    logf("[fc] FlashcardNavigateToCard - card %d vPage=%d screen=(%d,%d %dx%d) vpdy=%d dy=%d\n", cardIdx, vPage,
         screenRect.x, screenRect.y, screenRect.dx, screenRect.dy, dm->viewPort.dy, dy);
    if (dy != 0) {
        dm->ScrollYBy(dy, false);
    }
}

static void HandleFlashcardRate(MainWindow* win, int rating) {
    int curIdx = win->flashcard.currentCardIdx;
    if (curIdx < 0 || curIdx >= len(win->flashcard.studyOrder)) {
        return; // defensive: no valid current card
    }
    int cardIdx = win->flashcard.studyOrder[curIdx];
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

    // Advance to next card: it arrives masked (front / pre-reveal state) and
    // vertically centered in the viewport
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
        FlashcardNavigateToCard(win, win->flashcard.studyOrder[win->flashcard.currentCardIdx], true);
    }
    MainWindowRerender(win);
}

bool HandleCommandFlashcard(MainWindow* win, int cmd) {
    switch (cmd) {
        case CmdFlashcardToggle: {
            logf("[fc] CmdFlashcardToggle - toggling flashcard mode\n");
            win->flashcard.on = !win->flashcard.on;
            // StrL only takes literals: wrap each branch separately, else the
            // ternary inside the macro computes the length from the wrong type
            logf("[fc] CmdFlashcardToggle - mode %s\n", win->flashcard.on ? StrL("ON") : StrL("OFF"));
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
                // Navigate to first card: masked (front state), vertically centered
                if (len(win->flashcard.studyOrder) > 0) {
                    win->flashcard.currentCardIdx = 0;
                    FlashcardNavigateToCard(win, win->flashcard.studyOrder[0], true);
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
                // Post-reveal back: the card arrives REVEALED (the answer was
                // already seen) and the scroll position is KEPT — no
                // auto-centering on the way back
                win->flashcard.currentCardIdx--;
                win->flashcard.revealMode = true;
                FlashcardToolbarUpdateState(win);
                MainWindowRerender(win);
            } else {
                logf("[fc] CmdFlashcardBack - no previous card\n");
            }
            return true;
        }
        case CmdFlashcardNext: {
            logf("[fc] CmdFlashcardNext - skipping to next card\n");
            if (!win->flashcard.studyMode) {
                return true;
            }
            int next = win->flashcard.currentCardIdx + 1;
            if (next >= len(win->flashcard.studyOrder)) {
                logf("[fc] CmdFlashcardNext - no next card\n");
                return true;
            }
            // skip WITHOUT revealing or rating: the next card arrives masked
            // (front state), vertically centered; no SM-2 update, no save
            win->flashcard.currentCardIdx = next;
            win->flashcard.revealMode = false;
            FlashcardToolbarUpdateState(win);
            FlashcardNavigateToCard(win, win->flashcard.studyOrder[next], true);
            MainWindowRerender(win);
            return true;
        }
        case CmdFlashcardOrderToggle: {
            gGlobalPrefs->flashcardSettings.randomOrder = !gGlobalPrefs->flashcardSettings.randomOrder;
            logf("[fc] CmdFlashcardOrderToggle - randomOrder=%s\n",
                 gGlobalPrefs->flashcardSettings.randomOrder ? StrL("ON") : StrL("OFF"));
            if (win->flashcard.studyMode) {
                // rebuild the queue in the new order and restart from its first
                // card, so Next / advance / back stay coherent with the order
                // the user configured
                BuildFilteredStudyOrder(win);
                win->flashcard.revealMode = false;
                if (len(win->flashcard.studyOrder) > 0) {
                    win->flashcard.currentCardIdx = 0;
                    FlashcardNavigateToCard(win, win->flashcard.studyOrder[0], true);
                } else {
                    win->flashcard.currentCardIdx = -1;
                }
                MainWindowRerender(win);
            }
            FlashcardToolbarUpdateState(win);
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
        case CmdFlashcardCleanHistory: {
            logf("[fc] CmdFlashcardCleanHistory - opening confirm dialog\n");
            WindowTab* tab = win->CurrentTab();
            if (!tab || !tab->filePath) {
                logf("[fc] CmdFlashcardCleanHistory - ERROR: no document loaded\n");
                return true;
            }
            // the dialog holds "Yes" for 2s (draining-line animation) before
            // confirming this destructive action
            if (FlashcardCleanHistoryDialog(win->hwndFrame)) {
                win->flashcard.studyDoc.states.Reset();
                // stop any active session: every card becomes "new" again
                win->flashcard.studyMode = false;
                win->flashcard.currentCardIdx = -1;
                win->flashcard.revealMode = false;
                FlashcardStudySave(tab->filePath.s, win->flashcard.studyDoc);
                FlashcardToolbarUpdateCount(win);
                FlashcardToolbarUpdateState(win);
                MainWindowRerender(win);
                logf("[fc] CmdFlashcardCleanHistory - review history cleared for this doc\n");
            } else {
                logf("[fc] CmdFlashcardCleanHistory - cancelled by user\n");
            }
            return true;
        }
        default:
            return false;
    }
}