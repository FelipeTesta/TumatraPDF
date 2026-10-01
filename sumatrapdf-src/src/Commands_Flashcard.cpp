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
#include "Tabs.h"
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

// xorshift128+ generator: better distribution than rand() (and rand()%n is
// modulo-biased). Seeded once at first use; good enough for shuffling the
// study queue.
static u64 sXorshift128State[2] = {0, 0};

static u64 Xorshift128Next() {
    u64* s = sXorshift128State;
    if (s[0] == 0 && s[1] == 0) {
        u64 t = (u64)time(nullptr) ^ ((u64)GetTickCount64() << 21);
        s[0] = t ? t : 88172645463325252ULL;
        s[1] = t ^ 0x9E3779B97F4A7C15ULL;
    }
    u64 x = s[0];
    u64 y = s[1];
    s[0] = y;
    x ^= x << 23;
    x ^= x >> 17;
    x ^= y ^ (y >> 26);
    s[1] = x;
    return x + y;
}

template <typename T>
static void ShuffleVecInPlace(Vec<T>& v) {
    for (int i = len(v) - 1; i > 0; i--) {
        int j = (int)(Xorshift128Next() % ((u64)i + 1));
        T tmp = v[i];
        v[i] = v[j];
        v[j] = tmp;
    }
}

// Lazy-load a tab's flashcard cards + study states. Returns true when the tab
// is a loaded PDF document (cards ready to use, even if it has 0 cards);
// false for non-document tabs, non-PDF engines or not-yet-loaded tabs.
bool FlashcardEnsureTabCards(WindowTab* tab) {
    if (!tab || tab->IsNonDocumentTab()) {
        return false;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return false;
    }
    if (!tab->flashcard.cardsLoaded) {
        auto* engine = AsEngineMupdf(dm->GetEngine());
        if (!engine) {
            return false;
        }
        tab->flashcard.cards = FlashcardLoadFromDocument(engine);
        tab->flashcard.cardsLoaded = true;
        if (tab->filePath) {
            tab->flashcard.studyDoc = FlashcardStudyLoad(tab->filePath.s);
        }
        logf("[fc] EnsureTabCards - '%s': loaded %d cards, %d study states\n", FlashcardTabLogName(tab),
             len(tab->flashcard.cards), len(tab->flashcard.studyDoc.states));
    }
    return true;
}

// Index of a tab inside win->tabs, -1 when the tab is no longer in this
// window (closed, or dragged out to another window — global sessions only
// count tabs that live in THIS window)
static int FindTabIndex(MainWindow* win, WindowTab* tab) {
    auto tabs = win->Tabs();
    for (int i = 0; i < len(tabs); i++) {
        if (tabs[i] == tab) {
            return i;
        }
    }
    return -1;
}

// Collect this tab's new/due cards into the group vectors, applying the tab's
// own page filter (each book keeps its own filter — a global session respects
// every book's filter independently).
static void CollectTabCards(MainWindow* win, WindowTab* tab, i64 now, Vec<FlashcardQueueEntry>& newCards,
                            Vec<FlashcardQueueEntry>& dueCards) {
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return;
    }
    int pageCount = dm->PageCount();
    // evaluate the page filter once per tab: pages[i-1] = 1 when included
    Vec<u8> pages;
    bool filtered = false;
    if (tab->flashcard.filterEnabled && tab->flashcard.filterExpr) {
        filtered = FlashcardFilterEval(tab->flashcard.filterExpr, pageCount, pages);
        logf("[fc] CollectTabCards - '%s': filter '%s' -> %s\n", FlashcardTabLogName(tab), tab->flashcard.filterExpr,
             filtered ? StrL("active") : StrL("inactive (empty/invalid)"));
    }
    int nBefore = len(newCards) + len(dueCards);
    for (int i = 0; i < len(tab->flashcard.cards); i++) {
        Flashcard& card = tab->flashcard.cards[i];
        if (filtered && (card.pageNo < 1 || card.pageNo > len(pages) || !pages[card.pageNo - 1])) {
            continue;
        }
        // SRS: include only cards that are new (never rated) or due for review
        bool isNew = true;
        bool isDue = false;
        for (int j = 0; j < len(tab->flashcard.studyDoc.states); j++) {
            if (tab->flashcard.studyDoc.states[j].key == card.key) {
                const FlashcardStudyState& s = tab->flashcard.studyDoc.states[j].state;
                isNew = (s.rating == 0);
                isDue = (!isNew && s.nextReviewAt <= now);
                break;
            }
        }
        FlashcardQueueEntry entry;
        entry.tab = tab;
        entry.cardIdx = i;
        if (isNew) {
            newCards.Append(entry);
        } else if (isDue) {
            dueCards.Append(entry);
        }
    }
    logf("[fc] CollectTabCards - '%s': %d cards pass (of %d)\n", FlashcardTabLogName(tab),
         len(newCards) + len(dueCards) - nBefore, len(tab->flashcard.cards));
}

static void BuildFilteredStudyOrder(MainWindow* win) {
    win->flashcard.studyOrder.Reset();
    i64 now = (i64)time(nullptr) * 1000;
    // collect new (never rated) and due cards SEPARATELY so the NewCardsPosition
    // setting can order them relative to each other
    Vec<FlashcardQueueEntry> newCards;
    Vec<FlashcardQueueEntry> dueCards;
    int nTabsUsed = 0;
    if (win->flashcard.crossDocSession) {
        // global session: all PDF tabs of THIS window (other windows never count)
        auto tabs = win->Tabs();
        for (WindowTab* tab : tabs) {
            if (FlashcardEnsureTabCards(tab)) {
                nTabsUsed++;
                CollectTabCards(win, tab, now, newCards, dueCards);
            }
        }
    } else {
        WindowTab* tab = win->CurrentTab();
        if (tab && FlashcardEnsureTabCards(tab)) {
            nTabsUsed = 1;
            CollectTabCards(win, tab, now, newCards, dueCards);
        }
    }
    // random order (user setting): shuffle WITHIN each group, so the
    // new-cards position keeps its meaning (position = group order,
    // random = inside-group order)
    if (gGlobalPrefs->flashcardSettings.randomOrder) {
        ShuffleVecInPlace(newCards);
        ShuffleVecInPlace(dueCards);
    }
    // combine per NewCardsPosition: 0 = new first, 1 = new last,
    // 2 = mixed (proportional interleave, like Anki's "mix")
    int pos = gGlobalPrefs->flashcardSettings.newCardsPosition;
    int nNew = len(newCards);
    int nDue = len(dueCards);
    if (pos == 0) {
        win->flashcard.studyOrder.Append(newCards);
        win->flashcard.studyOrder.Append(dueCards);
    } else if (pos == 1) {
        win->flashcard.studyOrder.Append(dueCards);
        win->flashcard.studyOrder.Append(newCards);
    } else {
        int total = nNew + nDue;
        int iNew = 0, iDue = 0;
        for (int k = 0; k < total; k++) {
            // proportional stepping: take a new card when its "share" of the
            // queue has not been consumed yet; else the next due card
            if (iNew < nNew && (iDue >= nDue || (k * nNew) / total >= iNew)) {
                win->flashcard.studyOrder.Append(newCards[iNew++]);
            } else {
                win->flashcard.studyOrder.Append(dueCards[iDue++]);
            }
        }
    }
    TempStr posStr = pos == 0 ? StrL("newFirst") : (pos == 1 ? StrL("newLast") : StrL("mixed"));
    logf("[fc] BuildFilteredStudyOrder - %d cards (scope=%s, tabsUsed=%d, order=%s, new=%s, nNew=%d, nDue=%d)\n",
         len(win->flashcard.studyOrder), win->flashcard.crossDocSession ? StrL("global-session") : StrL("current-doc"),
         nTabsUsed, gGlobalPrefs->flashcardSettings.randomOrder ? StrL("random") : StrL("sequential"), posStr, nNew,
         nDue);
}

// Called after study-order settings changed (Order dialog), after the page
// filter changed (Filter dialog) or after the study scope toggled: rebuild
// the queue and restart from its first card (centered) when a session is
// active, so Next / advance / back stay coherent with the configuration
static void FlashcardNavigateToEntry(MainWindow* win, const FlashcardQueueEntry& e, bool centerVertically);
void FlashcardApplyStudyOrder(MainWindow* win) {
    if (win->flashcard.studyMode) {
        BuildFilteredStudyOrder(win);
        win->flashcard.revealMode = false;
        if (len(win->flashcard.studyOrder) > 0) {
            win->flashcard.currentCardIdx = 0;
            FlashcardNavigateToEntry(win, win->flashcard.studyOrder[0], true);
        } else {
            win->flashcard.currentCardIdx = -1;
        }
        MainWindowRerender(win);
    }
    FlashcardToolbarUpdateState(win);
}

// Navigate the view to a study queue entry. When the card belongs to another
// tab (global session), switch to that tab first (TabsSelect — synchronous);
// the entry's tab may have been closed or dragged to another window, in
// which case the jump is skipped (the caller's queue rebuild drops it).
// Under TC2 the display uses virtual pages, so route the physical card rect
// through PhysicalToVirtualForRect first. When centerVertically is set,
// scroll so the card sits in the middle of the viewport (instant).
static void FlashcardNavigateToEntry(MainWindow* win, const FlashcardQueueEntry& e, bool centerVertically) {
    if (!e.tab || e.cardIdx < 0 || e.cardIdx >= len(e.tab->flashcard.cards)) {
        logf("[fc] NavigateToEntry - invalid entry (tab=%p cardIdx=%d)\n", (void*)e.tab, e.cardIdx);
        return;
    }
    if (win->CurrentTab() != e.tab) {
        int idx = FindTabIndex(win, e.tab);
        if (idx < 0) {
            logf("[fc] NavigateToEntry - entry tab no longer in this window, skipping\n");
            return;
        }
        logf("[fc] NavigateToEntry - global session: switching to tab %d ('%s')\n", idx, FlashcardTabLogName(e.tab));
        TabsSelect(win, idx);
    }
    DisplayModel* dm = e.tab->AsFixed();
    if (!dm) {
        return;
    }
    Flashcard& card = e.tab->flashcard.cards[e.cardIdx];
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
    logf("[fc] NavigateToEntry - card %d '%s' vPage=%d screen=(%d,%d %dx%d) vpdy=%d dy=%d\n", e.cardIdx,
         FlashcardTabLogName(e.tab), vPage, screenRect.x, screenRect.y, screenRect.dx, screenRect.dy, dm->viewPort.dy,
         dy);
    if (dy != 0) {
        dm->ScrollYBy(dy, false);
    }
}

// public wrapper for the Lista panel: navigate the CURRENT tab to one of its
// cards (tab switch is a no-op here — the list shows the current document)
void FlashcardNavigateToCardInCurrentTab(MainWindow* win, int cardIdx) {
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }
    FlashcardQueueEntry e{tab, cardIdx};
    logf("[fc] Lista - navigating to card %d of '%s'\n", cardIdx, FlashcardTabLogName(tab));
    FlashcardNavigateToEntry(win, e, true);
}

static void HandleFlashcardRate(MainWindow* win, int rating) {
    int curIdx = win->flashcard.currentCardIdx;
    if (curIdx < 0 || curIdx >= len(win->flashcard.studyOrder)) {
        return; // defensive: no valid current card
    }
    FlashcardQueueEntry e = win->flashcard.studyOrder[curIdx];
    if (!e.tab || e.cardIdx < 0 || e.cardIdx >= len(e.tab->flashcard.cards)) {
        return;
    }
    Flashcard& card = e.tab->flashcard.cards[e.cardIdx];

    // Find or create study state for this card in ITS OWN document's doc
    FlashcardStudyState state = {};
    for (int i = 0; i < len(e.tab->flashcard.studyDoc.states); i++) {
        if (e.tab->flashcard.studyDoc.states[i].key == card.key) {
            state = e.tab->flashcard.studyDoc.states[i].state;
            break;
        }
    }
    FlashcardSm2Update(state, rating);

    // Relearn: rating 1 (Again) reinserts this card at the end of the session
    // queue so it is presented again (matching Anki's relearning behavior).
    if (rating == 1) {
        win->flashcard.studyOrder.Append(e);
    }

    // Save/update study state in the card's document
    bool found = false;
    for (int i = 0; i < len(e.tab->flashcard.studyDoc.states); i++) {
        if (e.tab->flashcard.studyDoc.states[i].key == card.key) {
            e.tab->flashcard.studyDoc.states[i].state = state;
            found = true;
            break;
        }
    }
    if (!found) {
        FlashcardStudyDoc::StateEntry entry;
        entry.key = card.key;
        entry.state = state;
        e.tab->flashcard.studyDoc.states.Append(entry);
    }

    // Save to disk — the card's own book's JSON (per-book, keyed by MD5 of path)
    if (e.tab->filePath) {
        FlashcardStudySave(e.tab->filePath.s, e.tab->flashcard.studyDoc);
    }
    logf("[fc] HandleFlashcardRate - rated %d card %d of '%s'\n", rating, e.cardIdx, FlashcardTabLogName(e.tab));

    FlashcardToolbarUpdateCount(win);
    // the Lista panel's Due/Status columns changed for this card
    FlashcardSidebarPopulate(win);

    // Advance to next card: it arrives masked (front / pre-reveal state) and
    // vertically centered in the viewport (switching tabs when the next card
    // lives in another document)
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
        FlashcardNavigateToEntry(win, win->flashcard.studyOrder[win->flashcard.currentCardIdx], true);
    }
    MainWindowRerender(win);
}

// Stop the active study session (queue, index, modes) — per-tab caches
// (cards/states/filter) are kept: they belong to the documents.
void FlashcardStopSession(MainWindow* win) {
    win->flashcard.studyMode = false;
    win->flashcard.revealMode = false;
    win->flashcard.currentCardIdx = -1;
    win->flashcard.studyOrder.Reset();
}

// Called from LoadModelIntoTab: after a tab switch, refresh flashcard state —
// lazy-load the new tab's cards if flashcard mode is on (fixes masks from the
// previous book being painted over the new document) and refresh count/list.
void FlashcardOnTabChanged(MainWindow* win) {
    if (!win->flashcard.on) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (tab && FlashcardEnsureTabCards(tab)) {
        logf("[fc] OnTabChanged - '%s': cards ready (%d cards)\n", FlashcardTabLogName(tab), len(tab->flashcard.cards));
    } else if (tab) {
        logf("[fc] OnTabChanged - '%s': skipped (not a loaded PDF)\n", FlashcardTabLogName(tab));
    } else {
        logf("[fc] OnTabChanged - no current tab\n");
    }
    FlashcardToolbarUpdateCount(win);
    FlashcardToolbarUpdateState(win);
    FlashcardSidebarPopulate(win);
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
                // Lazy-load the current tab's cards + study states
                WindowTab* tab = win->CurrentTab();
                if (tab) {
                    FlashcardEnsureTabCards(tab);
                    FlashcardToolbarUpdateCount(win);
                }
            } else {
                FlashcardToolbarDestroy(win);
                // sidebar is owned by flashcard mode: destroy it too, or it
                // stays floating with a stale list (FC-R3)
                FlashcardSidebarDestroy(win);
                RelayoutFrame(win, true, -1);
                FlashcardStopSession(win);
                logf("[fc] CmdFlashcardToggle - flashcard OFF, session cleared (per-tab caches kept)\n");
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

            // Cloze is positional: the highlight rect is the mask over existing
            // PDF text. No duplicated content is stored in the annotation
            // (content empty). Creation lives in FlashcardCreateHighlightAnnot
            // (shared with the import merge).

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
                annot = FlashcardCreateHighlightAnnot(engine, pageNo, rects);
            }

            if (annot) {
                DeleteOldSelectionInfo(win, true);
                // Reload cards so count/order reflect the new cloze immediately
                tab->flashcard.cards = FlashcardLoadFromDocument(AsEngineMupdf(engine));
                tab->flashcard.cardsLoaded = true;
                if (tab->filePath) {
                    tab->flashcard.studyDoc = FlashcardStudyLoad(tab->filePath.s);
                }
                MainWindowRerender(win);
                ToolbarUpdateStateForWindow(win, true);
                FlashcardToolbarUpdateCount(win);
                // keep an open sidebar's list in sync with the new card (FC-M2)
                FlashcardSidebarPopulate(win);
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
                // Build filtered study order (respects scope + per-tab filters)
                BuildFilteredStudyOrder(win);
                // Navigate to first card: masked (front state), vertically centered
                if (len(win->flashcard.studyOrder) > 0) {
                    win->flashcard.currentCardIdx = 0;
                    FlashcardNavigateToEntry(win, win->flashcard.studyOrder[0], true);
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
                // auto-centering on the way back (still switches tabs when the
                // previous card lives in another document)
                win->flashcard.currentCardIdx--;
                win->flashcard.revealMode = true;
                FlashcardNavigateToEntry(win, win->flashcard.studyOrder[win->flashcard.currentCardIdx], false);
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
            FlashcardNavigateToEntry(win, win->flashcard.studyOrder[next], true);
            MainWindowRerender(win);
            return true;
        }
        case CmdFlashcardOrderOptions: {
            logf("[fc] CmdFlashcardOrderOptions - opening order options dialog\n");
            FlashcardOrderOptionsDialog(win);
            return true;
        }
        case CmdFlashcardImport: {
            logf("[fc] CmdFlashcardImport - opening import window\n");
            FlashcardImportDialog(win);
            return true;
        }
        case CmdFlashcardSession: {
            logf("[fc] CmdFlashcardSession - opening session window\n");
            FlashcardSessionDialog(win);
            return true;
        }
        case CmdFlashcardLista:
            logf("[fc] CmdFlashcardLista - toggling sidebar\n");
            FlashcardSidebarToggle(win);
            return true;
        case CmdFlashcardFilter: {
            logf("[fc] CmdFlashcardFilter - opening filter dialog\n");
            WindowTab* tab = win->CurrentTab();
            if (!tab || !tab->AsFixed()) {
                logf("[fc] CmdFlashcardFilter - ERROR: no PDF document loaded\n");
                return true;
            }
            FlashcardFilterOptionsDialog(win);
            return true;
        }
        case CmdFlashcardConfig:
            logf("[fc] CmdFlashcardConfig - opening config window\n");
            // the window owns its list + selection, so the clear actions and
            // the folder change execute inside it (see FlashcardToolbar.cpp)
            FlashcardConfigDialog(win);
            return true;
        default:
            return false;
    }
}
