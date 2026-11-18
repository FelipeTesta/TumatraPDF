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
#include "RenderCache.h"
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
#include "EditAnnotations.h"
#include "Flashcard.h"
#include "FlashcardSidebar.h"
#include "FlashcardStats.h"
#include "Canvas.h"   // kFlashcardStudyTimerID
#include "Theme.h"    // ThemeWindowBackgroundColor, DpiScale
#include "base/Dpi.h" // DpiScale
#include "Notifications.h"
#include "Commands_Flashcard.h"

bool RelayoutFrame(MainWindow* win, bool updateToolbars = true, int sidebarDx = -1);

static void AddUniquePageNo(Vec<int>& pageNos, int pageNo) {
    for (int i = 0; i < len(pageNos); i++) {
        if (pageNos[i] == pageNo) return;
    }
    pageNos.Append(pageNo);
}

// True when the current text selection re-creates an EXISTING flashcard:
// identical page set with each page's union-bounds within 1pt. Single-char
// differences shift the bounds by more than 1pt ("casca de cebola" allowed).
static bool FlashcardSelectionIsDuplicate(WindowTab* tab, const Vec<SelectionOnPage>* sel, const Vec<int>& pageNos) {
    if (!tab || !sel || len(pageNos) == 0) {
        return false;
    }
    FlashcardEnsureTabCards(tab);
    if (len(tab->flashcard.cards) == 0) {
        return false;
    }
    // per-page union bounds of the selection
    Vec<RectF> selByPage;
    Vec<int> selPageNos;
    auto UnionRectLocal = [](const RectF& a, const RectF& b) -> RectF {
        float left = a.x < b.x ? a.x : b.x;
        float top = a.y < b.y ? a.y : b.y;
        float rightA = a.x + a.dx;
        float rightB = b.x + b.dx;
        float bottomA = a.y + a.dy;
        float bottomB = b.y + b.dy;
        float right = rightA > rightB ? rightA : rightB;
        float bottom = bottomA > bottomB ? bottomA : bottomB;
        return RectF(left, top, right - left, bottom - top);
    };
    for (auto& s : *sel) {
        bool found = false;
        for (int i = 0; i < len(selPageNos); i++) {
            if (selPageNos[i] == s.pageNo) {
                selByPage[i] = UnionRectLocal(selByPage[i], s.rect);
                found = true;
                break;
            }
        }
        if (!found) {
            selPageNos.Append(s.pageNo);
            selByPage.Append(s.rect);
        }
    }
    for (int i = 0; i < len(selPageNos); i++) {
        if (!(selByPage[i].dx > 0.f && selByPage[i].dy > 0.f)) {
            return false; // selection not on this page? treat as non-duplicate… actually just skip this page
        }
    }
    for (const Flashcard& card : tab->flashcard.cards) {
        // card page list = primary + extraPages
        Vec<int> cardPages;
        Vec<RectF> cardRects;
        cardPages.Append(card.pageNo);
        cardRects.Append(card.bounds);
        for (const FlashcardPageGroup& g : card.extraPages) {
            cardPages.Append(g.pageNo);
            cardRects.Append(g.bounds);
        }
        if (len(cardPages) != len(selPageNos)) {
            continue;
        }
        bool allMatch = true;
        for (int i = 0; i < len(selPageNos); i++) {
            if (!FlashcardCardPageBoundsMatch(card, selPageNos[i], selByPage[i])) {
                allMatch = false;
                break;
            }
        }
        if (allMatch) {
            return true;
        }
    }
    return false;
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
    // The filter expression is typed against the page numbers the USER SEES.
    // Under two-column v2 those are VIRTUAL (physical page k renders as virtual
    // 2k-1 / 2k), so the filter is evaluated in that space. Cards store PHYSICAL
    // pages, so a card passes when either of its virtual columns is included.
    // v2 off: virtual == physical, behaviour unchanged.
    bool v2 = dm->IsViewportCropV2Active();
    int filterPageCount = v2 ? dm->VirtualPageCount() : pageCount;
    // evaluate the page filter once per tab: pages[i-1] = 1 when included
    Vec<u8> pages;
    bool filtered = false;
    if (tab->flashcard.filterEnabled && tab->flashcard.filterExpr) {
        filtered = FlashcardFilterEval(tab->flashcard.filterExpr, filterPageCount, pages);
        logf("[fc] CollectTabCards - '%s': filter '%s' -> %s (%s pages, %d)\n", FlashcardTabLogName(tab),
             tab->flashcard.filterExpr, filtered ? StrL("active") : StrL("inactive (empty/invalid)"),
             v2 ? StrL("virtual") : StrL("physical"), filterPageCount);
    }
    auto virtualPageInFilter = [&](int vpg) { return vpg >= 1 && vpg <= len(pages) && pages[vpg - 1]; };
    // a cross-page card passes the page filter when ANY of its pages is in it
    auto pageInFilter = [&](int physicalPageNo) {
        if (!v2) {
            return virtualPageInFilter(physicalPageNo);
        }
        int leftColumn = 2 * physicalPageNo - 1;
        return virtualPageInFilter(leftColumn) || virtualPageInFilter(leftColumn + 1);
    };
    int nBefore = len(newCards) + len(dueCards);
    for (int i = 0; i < len(tab->flashcard.cards); i++) {
        Flashcard& card = tab->flashcard.cards[i];
        if (filtered) {
            bool ok = pageInFilter(card.pageNo);
            for (int k = 0; !ok && k < len(card.extraPages); k++) {
                ok = pageInFilter(card.extraPages[k].pageNo);
            }
            if (!ok) {
                continue;
            }
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

// --- s64-R9p adaptive prewarmer (P1+P2) ---
// Phase 1: EWMA of per-advance card-switch cost (win->flashcard.cardSwitchMsEwma,
// updated in the FlashcardNavigateToEntry wrapper) drives a prefetch budget
// N = clamp(round(150/ewma), 0, tierCap). Silent degrade (N=0, no UI) when the
// machine is weak (tier 0), under memory pressure, or ewma > 400 ms.
// Phase 2: single BELOW_NORMAL worker thread with a mutex-protected FIFO
// queue (<=4 jobs in flight, <=2 tile requests, <=2 warm docs per batch). An
// epoch counter (Interlocked) is bumped on any invalidation — stale jobs
// no-op. Engine access ONLY via the locked GetFzPageInfo(loadQuick=true)
// (pagesLock + renderLock); never raw fz ctx / _doc. Tiles only for same-dm
// near-visible virtual pages via gRenderCache (self-synchronizing).
struct FcWarmJob {
    LONG epoch = 0;
    WindowTab* tab = nullptr;
    DisplayModel* dm = nullptr;
    EngineMupdf* engine = nullptr;
    int pageNo = -1; // physical page
    Vec<int> extraPages;
    bool wantTile = false;
    int vPage = -1; // virtual page (cache key for RequestRendering)
};

static volatile LONG gFcWarmEpoch = 1;
static volatile LONG gFcWarmActive = 0; // jobs executing right now
static CRITICAL_SECTION gFcWarmLock;
static bool gFcWarmLockInit = false;
static Vec<FcWarmJob> gFcWarmQueue;
static HANDLE gFcWarmThread = nullptr;
static HANDLE gFcWarmSignal = nullptr; // auto-reset: queue possibly non-empty
static bool gFcWarmStop = false;       // guarded by gFcWarmLock

static int FlashcardPrewarmTierCap() {
    static int cap = -1;
    if (cap < 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        u64 totalPhys = ms.ullTotalPhys;
        DWORD cores = si.dwNumberOfProcessors;
        u64 gb = 1024ull * 1024 * 1024;
        if (cores <= 2 || totalPhys < 4 * gb) {
            cap = 0; // weak machine: silent N=0
        } else if (cores <= 4 || totalPhys < 8 * gb) {
            cap = 1;
        } else {
            cap = 2; // default target N <= 2
        }
    }
    return cap;
}

static int FlashcardPrewarmBudget(MainWindow* win) {
    double ewma = win->flashcard.cardSwitchMsEwma;
    if (ewma > 400.0) {
        return 0; // switch too slow to benefit — silent degrade
    }
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms) && (ms.dwMemoryLoad >= 85 || ms.ullAvailPhys < 512ull * 1024 * 1024)) {
        return 0; // memory pressure — silent degrade
    }
    int cap = FlashcardPrewarmTierCap();
    double d = ewma < 1.0 ? 1.0 : ewma;
    int n = (int)(150.0 / d + 0.5);
    if (n < 0) {
        n = 0;
    }
    if (n > cap) {
        n = cap;
    }
    return n;
}

static DWORD WINAPI FlashcardPrewarmWorker(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    for (;;) {
        FcWarmJob job;
        bool haveJob = false;
        EnterCriticalSection(&gFcWarmLock);
        if (len(gFcWarmQueue) > 0) {
            job = gFcWarmQueue[0];
            gFcWarmQueue.RemoveAt(0);
            if (job.epoch == gFcWarmEpoch && gFcWarmActive < 4) {
                InterlockedIncrement(&gFcWarmActive);
                haveJob = true;
            }
        } else if (gFcWarmStop) {
            LeaveCriticalSection(&gFcWarmLock);
            break;
        }
        LeaveCriticalSection(&gFcWarmLock);
        if (!haveJob) {
            WaitForSingleObject(gFcWarmSignal, 40);
            continue;
        }
        if (job.engine) {
            job.engine->GetFzPageInfo(job.pageNo, true);
            int nExtra = len(job.extraPages);
            for (int k = 0; k < nExtra; k++) {
                job.engine->GetFzPageInfo(job.extraPages[k], true);
            }
        }
        if (job.wantTile && job.dm && gRenderCache) {
            if (job.epoch == gFcWarmEpoch && !job.dm->pauseRendering && job.dm->ValidPageNo(job.vPage)) {
                gRenderCache->RequestRendering(job.dm, job.vPage);
            }
        }
        InterlockedDecrement(&gFcWarmActive);
    }
    return 0;
}

void FlashcardPrewarmInvalidate() {
    InterlockedIncrement(&gFcWarmEpoch);
    if (!gFcWarmLockInit) {
        return;
    }
    EnterCriticalSection(&gFcWarmLock);
    int dropped = len(gFcWarmQueue);
    gFcWarmQueue.Reset();
    LeaveCriticalSection(&gFcWarmLock);
    // Wait for in-flight jobs to drain — but NEVER longer than ~50 ms with a
    // real deadline. Sleep(1) inherits the default 15.6ms timer granularity
    // (this process does not raise it), so a large iteration count here can
    // freeze the UI for seconds and trip Windows AppHangB1 (s64-R9r).
    DWORD deadline = GetTickCount() + 50;
    while (gFcWarmActive > 0 && (int)(GetTickCount() - deadline) < 0) {
        Sleep(1);
    }
    if (dropped > 0) {
        logf("[fc][perf] prewarm invalidate epoch=%d dropped=%d\n", (int)gFcWarmEpoch, dropped);
    }
}

static void FlashcardPrewarmEnsureWorker() {
    if (!gFcWarmLockInit) {
        InitializeCriticalSection(&gFcWarmLock);
        gFcWarmSignal = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        gFcWarmLockInit = true;
    }
    if (gFcWarmThread) {
        if (WaitForSingleObject(gFcWarmThread, 0) == WAIT_OBJECT_0) {
            CloseHandle(gFcWarmThread); // previous worker exited on its own
            gFcWarmThread = nullptr;
        } else {
            return;
        }
    }
    gFcWarmStop = false;
    gFcWarmThread = CreateThread(nullptr, 0, FlashcardPrewarmWorker, nullptr, 0, nullptr);
    if (!gFcWarmThread) {
        logf("[fc][perf] prewarm worker create FAILED - sync prefetch floor only\n");
    } else {
        logf("[fc][perf] prewarm worker started\n");
    }
}

static void FlashcardPrewarmShutdown() {
    if (!gFcWarmLockInit || !gFcWarmThread) {
        return;
    }
    FlashcardPrewarmInvalidate();
    EnterCriticalSection(&gFcWarmLock);
    gFcWarmStop = true;
    LeaveCriticalSection(&gFcWarmLock);
    SetEvent(gFcWarmSignal);
    if (WaitForSingleObject(gFcWarmThread, 500) == WAIT_TIMEOUT) {
        logf("[fc][perf] prewarm worker did not stop in 500ms - leaving it running\n");
        return; // never close the handle of a live thread
    }
    CloseHandle(gFcWarmThread);
    gFcWarmThread = nullptr;
    logf("[fc][perf] prewarm worker stopped\n");
}

static void FlashcardPrewarmEnqueue(MainWindow* win, int fromIdx) {
    win->flashcard.prewarmFilledForIdx = fromIdx;
    auto& order = win->flashcard.studyOrder;
    if (fromIdx < 0 || fromIdx + 1 >= len(order)) {
        logf("[fc][perf] prewarm skipped: no next card (fromIdx=%d orderLen=%d)\n", fromIdx, len(order));
        return;
    }
    // idle gate: never enqueue while revealing / dragging / rendering paused
    if (win->flashcard.revealMode || win->mouseAction != MouseAction::None) {
        logf("[fc][perf] prewarm skipped: busy (reveal=%d mouseAction=%d)\n", (int)win->flashcard.revealMode,
             (int)win->mouseAction);
        return;
    }
    FlashcardPrewarmEnsureWorker();
    double ewma = win->flashcard.cardSwitchMsEwma;
    int n = FlashcardPrewarmBudget(win);
    if (n <= 0) {
        logf("[fc][perf] prewarm skipped: n=0 (budget=0 ewma=%.0f cap=%d)\n", ewma, FlashcardPrewarmTierCap());
        // still log the epoch line so the probe can confirm the mechanism fired
        logf("[fc][perf] prewarm epoch=%d n=0 ewma=%.0fms\n", (int)gFcWarmEpoch, ewma);
        return;
    }
    int n2 = n + 2; // d2 window: a couple of entries beyond d1
    logf("[fc][perf] prewarm epoch=%d n=%d ewma=%.0fms\n", (int)gFcWarmEpoch, n, ewma);
    int tileReqs = 0, nWarmed = 0;
    WindowTab* warmed[2] = {nullptr, nullptr};
    for (int i = fromIdx + 1; i < len(order) && i <= fromIdx + n2; i++) {
        bool isD1 = i <= fromIdx + n;
        FlashcardQueueEntry& e = order[i];
        if (!e.tab || e.cardIdx < 0 || e.cardIdx >= len(e.tab->flashcard.cards)) {
            continue;
        }
        DisplayModel* dm = e.tab->AsFixed();
        auto* engine = dm ? AsEngineMupdf(dm->GetEngine()) : nullptr;
        if (!engine) {
            continue;
        }
        bool sameTab = e.tab == win->CurrentTab();
        // d2 (beyond budget) = other tabs only; same-tab pages are covered
        // by the sync prefetch floor + d1
        if (!isD1 && sameTab) {
            continue;
        }
        if (!isD1) {
            bool known = false;
            for (int w = 0; w < nWarmed; w++) {
                if (warmed[w] == e.tab) {
                    known = true;
                }
            }
            if (!known) {
                if (nWarmed >= 2) {
                    continue; // cap: <=2 warm docs per batch
                }
                warmed[nWarmed++] = e.tab;
            }
        }
        Flashcard& card = e.tab->flashcard.cards[e.cardIdx];
        FcWarmJob job;
        job.epoch = gFcWarmEpoch;
        job.tab = e.tab;
        job.dm = dm;
        job.engine = engine;
        job.pageNo = card.pageNo;
        int nExtra = len(card.extraPages);
        for (int k = 0; k < nExtra; k++) {
            job.extraPages.Append(card.extraPages[k].pageNo);
        }
        if (isD1 && sameTab && tileReqs < 2) {
            int vPage = dm->PhysicalToVirtualPage(card.pageNo, card.bounds);
            if (dm->ValidPageNo(vPage) && dm->PageVisibleNearby(vPage)) {
                job.wantTile = true;
                job.vPage = vPage;
                tileReqs++;
            }
        }
        EnterCriticalSection(&gFcWarmLock);
        if (len(gFcWarmQueue) + gFcWarmActive >= 4) {
            LeaveCriticalSection(&gFcWarmLock);
            logf("[fc][perf] prewarm queue full (epoch=%d), job dropped\n", (int)gFcWarmEpoch);
            break;
        }
        gFcWarmQueue.Append(job);
        LeaveCriticalSection(&gFcWarmLock);
        SetEvent(gFcWarmSignal);
    }
}

// PA3: warm the BITMAP cache (not only the fz page info) for the cards that
// come next, so advancing is a blit instead of a first-touch tile render.
// Only the next two entries: RenderCache aborts in-flight renders whose page is
// not "visible nearby" (current page +-1), so a deeper prefetch would be thrown
// away, and MAX_PAGE_REQUESTS is 8 - flooding the queue would starve the page
// the user is actually looking at. Adjacent pages are usually already covered
// by predictive rendering (gPredictiveRender, +-1/+-2); this pays off for the
// multi-page jumps of a study session.
static void FlashcardPrefetchRenderUpcoming(MainWindow* win, int fromIdx) {
    if (!gRenderCache) {
        return;
    }
    auto& order = win->flashcard.studyOrder;
    int done = 0;
    for (int i = fromIdx + 1; i < len(order) && done < 2; i++) {
        FlashcardQueueEntry& e = order[i];
        if (!e.tab || e.cardIdx < 0 || e.cardIdx >= len(e.tab->flashcard.cards)) {
            continue;
        }
        DisplayModel* dm = e.tab->AsFixed();
        if (!dm) {
            continue;
        }
        Flashcard& card = e.tab->flashcard.cards[e.cardIdx];
        int vPage = dm->PhysicalToVirtualPage(card.pageNo, card.bounds);
        if (!dm->ValidPageNo(vPage)) {
            continue;
        }
        float zoom = dm->GetZoomReal(vPage);
        if (zoom <= 0) {
            continue;
        }
        if (gRenderCache->Exists(dm, vPage, dm->GetRotation(), zoom)) {
            continue; // already blittable at this zoom
        }
        if (gRenderCache->IsRenderQueueFull()) {
            break;
        }
        gRenderCache->RequestRendering(dm, vPage);
        logf("[fc][perf] prefetch-render idx=%d card=%d page=%d (virtual %d) zoom=%.2f -> requested\n", i, e.cardIdx,
             card.pageNo, vPage, zoom);
        done++;
    }
}

// FC prefetch (user request: "fila pré-carregada dos próximos cards"): parse
// the PAGES of the next study-queue cards so advancing never pays a
// first-touch fz_load_page on the UI thread. loadQuick=true = page dict +
// annotations only (no stext extraction, no render): sub-ms when the page is
// already cached, a few ms for a first-touch parse. Also warms cross-page
// cards' secondary pages (they scroll into view on reveal).
static void FlashcardPrefetchUpcoming(MainWindow* win, int fromIdx, int count) {
    auto& order = win->flashcard.studyOrder;
    if (fromIdx < 0) {
        return;
    }
    int end = fromIdx + count;
    if (end > len(order)) {
        end = len(order);
    }
    for (int i = fromIdx; i < end; i++) {
        FlashcardQueueEntry& e = order[i];
        if (!e.tab || e.cardIdx < 0 || e.cardIdx >= len(e.tab->flashcard.cards)) {
            continue;
        }
        DisplayModel* dm = e.tab->AsFixed();
        auto* engine = dm ? AsEngineMupdf(dm->GetEngine()) : nullptr;
        if (!engine) {
            continue;
        }
        Flashcard& card = e.tab->flashcard.cards[e.cardIdx];
        u64 t0 = GetTickCount64();
        engine->GetFzPageInfo(card.pageNo, true);
        int nExtra = len(card.extraPages);
        for (int k = 0; k < nExtra; k++) {
            engine->GetFzPageInfo(card.extraPages[k].pageNo, true);
        }
        logf("[fc][perf] prefetch idx=%d card=%d page=%d extra=%d: %dms\n", i, e.cardIdx, card.pageNo, nExtra,
             (int)(GetTickCount64() - t0));
    }
    FlashcardPrefetchRenderUpcoming(win, fromIdx);
}

// Called after study-order settings changed (Order dialog), after the page
// filter changed (Filter dialog) or after the study scope toggled: rebuild
// the queue and restart from its first card (centered) when a session is
// active, so Next / advance / back stay coherent with the configuration
static void FlashcardNavigateToEntry(MainWindow* win, const FlashcardQueueEntry& e, bool centerVertically);
void FlashcardApplyStudyOrder(MainWindow* win) {
    FlashcardPrewarmInvalidate(); // order/filter rebuild: stale queued jobs no-op
    if (win->flashcard.studyMode) {
        BuildFilteredStudyOrder(win);
        win->flashcard.revealMode = false;
        if (len(win->flashcard.studyOrder) > 0) {
            win->flashcard.currentCardIdx = 0;
            FlashcardPrefetchUpcoming(win, 0, FlashcardPrewarmBudget(win));
            FlashcardPrewarmEnqueue(win, 0);
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
// s64-R9p: body of FlashcardNavigateToEntry; the wrapper below times the
// switch and feeds the EWMA (adaptive prewarmer budget signal).
static void FlashcardNavigateToEntryInner(MainWindow* win, const FlashcardQueueEntry& e, bool centerVertically) {
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
    // PA3 measurement: a cached tile set means this advance is a blit; cached=0
    // means the page is about to pay a first-touch tile render.
    if (gRenderCache) {
        float zoomCached = dm->GetZoomReal(vPage);
        bool cached = zoomCached > 0 && gRenderCache->Exists(dm, vPage, dm->GetRotation(), zoomCached);
        logf("[fc][perf] navigate: card=%d page=%d (virtual %d) cached=%d\n", e.cardIdx, card.pageNo, vPage,
             (int)cached);
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

static void FlashcardNavigateToEntry(MainWindow* win, const FlashcardQueueEntry& e, bool centerVertically) {
    u64 t0 = GetTickCount64();
    FlashcardNavigateToEntryInner(win, e, centerVertically);
    double dt = (double)(GetTickCount64() - t0);
    if (dt < 0.0) {
        dt = 0.0;
    }
    if (dt > 2000.0) {
        dt = 2000.0;
    }
    win->flashcard.cardSwitchMsEwma = 0.7 * win->flashcard.cardSwitchMsEwma + 0.3 * dt;
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
        FlashcardPrewarmInvalidate();
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

    // Save to disk — the card's own book's JSON (per-book, keyed by MD5 of path).
    // Prune first: states of cards whose highlight was deleted must not be
    // written back (FC-H3-left); a deleted cloze leaves no card behind.
    if (e.tab->filePath) {
        int pruned = FlashcardStudyPruneStates(e.tab->flashcard.studyDoc, e.tab->flashcard.cards);
        if (pruned > 0) {
            logf("[fc] HandleFlashcardRate - pruned %d state(s) of deleted cards in '%s'\n", pruned,
                 FlashcardTabLogName(e.tab));
        }
        FlashcardStudySave(e.tab->filePath.s, e.tab->flashcard.studyDoc);
    }
    // count the rated card for the stats (per-document FileState + today's
    // sidecar bucket — same stores the study-time seconds go to). Credit the
    // card's OWNING tab (e.tab), not the focused tab — see FlashcardStats.h.
    FlashcardStatsAddCard(e.tab);
    logf("[fc] HandleFlashcardRate - rated %d card %d of '%s'\n", rating, e.cardIdx, FlashcardTabLogName(e.tab));

    FlashcardToolbarUpdateCount(win);
    // the Lista panel's Due/Status columns changed for this card
    FlashcardSidebarPopulate(win);

    // Advance to next card: it arrives masked (front / pre-reveal state) and
    // vertically centered in the viewport (switching tabs when the next card
    // lives in another document)
    win->flashcard.revealMode = false;
    FlashcardToolbarUpdateState(win);
    // Advance to next card: reset the per-card stats accumulator so the next
    // card gets its own 60s cap, and stop the timer when the session ends.
    win->flashcard.studyTimeSec = 0.0f;
    win->flashcard.currentCardIdx++;
    if (win->flashcard.currentCardIdx >= len(win->flashcard.studyOrder)) {
        win->flashcard.studyMode = false;
        win->flashcard.currentCardIdx = -1;
        FlashcardStudyTimerStop(win);
        logf("[fc] HandleFlashcardRate - study session ended (all %d cards rated)\n", len(win->flashcard.studyOrder));
    } else {
        logf("[fc] HandleFlashcardRate - advancing to card %d/%d\n", win->flashcard.currentCardIdx + 1,
             len(win->flashcard.studyOrder));
        // prefetch the upcoming cards BEFORE navigating (page-parse only) so
        // the jump itself is instant
        FlashcardPrefetchUpcoming(win, win->flashcard.currentCardIdx, FlashcardPrewarmBudget(win));
        FlashcardNavigateToEntry(win, win->flashcard.studyOrder[win->flashcard.currentCardIdx], true);
        FlashcardPrewarmEnqueue(win, win->flashcard.currentCardIdx);
    }
    MainWindowRerender(win);
}

// Stop the active study session (queue, index, modes) — per-tab caches
// (cards/states/filter) are kept: they belong to the documents.
void FlashcardStopSession(MainWindow* win) {
    FlashcardPrewarmInvalidate();
    FlashcardPrewarmShutdown();
    win->flashcard.studyMode = false;
    win->flashcard.revealMode = false;
    win->flashcard.currentCardIdx = -1;
    win->flashcard.studyTimeSec = 0;
    win->flashcard.studyOrder.Reset();
    FlashcardStudyTimerStop(win);
}

// Called from LoadModelIntoTab: after a tab switch, refresh flashcard state —
// lazy-load the new tab's cards if flashcard mode is on (fixes masks from the
// previous book being painted over the new document) and refresh count/list.
void FlashcardOnTabChanged(MainWindow* win) {
    FlashcardPrewarmInvalidate(); // tab switch / doc reload: drop stale warm jobs
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

// Handle flashcard-related commands. Called from the main command dispatch;
// returns true if the command was handled.
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
                // arch tools stay as they are: the toolbars are mutually
                // exclusive through DISABLED buttons / blocked commands (user
                // request - "inativar" beats auto-closing the other toolbar)
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

            if (FlashcardSelectionIsDuplicate(tab, s, pageNos)) {
                logf("[fc] CmdFlashcardAdd - duplicate selection, not creating a flashcard\n");
                ShowTemporaryNotification(win->hwndCanvas, StrL("Already a flashcard"), 2200);
                return true;
            }

            Annotation* annot = nullptr;
            // Cross-page cloze: one highlight per page, but every page of the
            // same selection shares a group token (hashed from the first page's
            // first mask rect) so the loader re-groups them into ONE card that
            // masks all pages at once. Single-page selection: token 0.
            u64 groupToken = 0;
            if (len(pageNos) > 1) {
                int firstPage = pageNos[0];
                for (auto pageNo : pageNos) {
                    if (pageNo < firstPage) firstPage = pageNo;
                }
                Vec<RectF> firstRects;
                for (auto& sel : *s) {
                    if (sel.pageNo == firstPage) firstRects.Append(sel.rect);
                }
                groupToken = FlashcardGroupToken(firstPage, firstRects);
            }
            for (auto pageNo : pageNos) {
                Vec<RectF> rects;
                for (auto& sel : *s) {
                    if (pageNo != sel.pageNo) continue;
                    rects.Append(sel.rect);
                }
                annot = FlashcardCreateHighlightAnnot(engine, pageNo, rects, groupToken);
            }

            if (annot) {
                FlashcardPrewarmInvalidate(); // annot add: cards reload, warm jobs stale
                if (groupToken) {
                    logf("[fc] cross-page cloze: group token %08X spans %d page(s)\n", (u32)groupToken, len(pageNos));
                }
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
            win->flashcard.studyTimeSec = 0;
            if (win->flashcard.studyMode) {
                FlashcardPrewarmInvalidate(); // session (re)start: drop stale warm jobs
                FlashcardPrewarmEnsureWorker();
                if (gGlobalPrefs->flashcardPreloadSessionBooks) {
                    logf(
                        "[fc][perf] flashcardPreloadSessionBooks=on: lazy-tab background load not implemented - lazy "
                        "books load on first visit\n");
                }
                // Build filtered study order (respects scope + per-tab filters)
                BuildFilteredStudyOrder(win);
                // Navigate to first card: masked (front state), vertically centered
                if (len(win->flashcard.studyOrder) > 0) {
                    win->flashcard.currentCardIdx = 0;
                    FlashcardPrefetchUpcoming(win, 0, FlashcardPrewarmBudget(win));
                    FlashcardPrewarmEnqueue(win, 0);
                    FlashcardNavigateToEntry(win, win->flashcard.studyOrder[0], true);
                }
                FlashcardStudyTimerStart(win);
            } else {
                logf("[fc] CmdFlashcardStudy - study OFF\n");
                FlashcardPrewarmInvalidate();
                FlashcardPrewarmShutdown();
                FlashcardStudyTimerStop(win);
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
            FlashcardPrewarmEnqueue(win, next);
            MainWindowRerender(win);
            return true;
        }
        case CmdFlashcardDeleteActive: {
            logf("[fc] CmdFlashcardDeleteActive - deleting active study card\n");
            if (!win->flashcard.studyMode) {
                return true;
            }
            int curIdx = win->flashcard.currentCardIdx;
            if (curIdx < 0 || curIdx >= len(win->flashcard.studyOrder)) {
                return true;
            }
            FlashcardQueueEntry e = win->flashcard.studyOrder[curIdx];
            if (!e.tab || e.cardIdx < 0 || e.cardIdx >= len(e.tab->flashcard.cards)) {
                logf("[fc] CmdFlashcardDeleteActive - invalid queue entry (tab=%p cardIdx=%d)\n", (void*)e.tab,
                     e.cardIdx);
                return true;
            }
            DisplayModel* dm = e.tab->AsFixed();
            if (!dm) {
                return true;
            }
            EngineBase* engine = dm->GetEngine();
            if (!engine || !EngineSupportsAnnotations(engine)) {
                return true;
            }
            Flashcard& card = e.tab->flashcard.cards[e.cardIdx];
            int deletedIdx = e.cardIdx;
            // capture the page list before the cards vec is reloaded below
            Vec<int> pages;
            pages.Append(card.pageNo);
            for (const FlashcardPageGroup& g : card.extraPages) {
                pages.Append(g.pageNo);
            }

            // collect the card's flashcard annotations on the primary page and
            // every extraPage (cross-page group deleted as a unit)
            Vec<Annotation*> toDelete;
            for (int pageNo : pages) {
                Vec<Annotation*> annots;
                if (!EngineGetPageAnnotations(engine, pageNo, annots)) {
                    continue;
                }
                for (Annotation* a : annots) {
                    if (!a) {
                        continue;
                    }
                    Str author = Author(a);
                    if (!str::StartsWith(author, StrL("TumatraPDF-Flashcard"))) {
                        continue;
                    }
                    if (!FlashcardCardPageBoundsMatch(card, pageNo, a->bounds)) {
                        continue;
                    }
                    toDelete.Append(a);
                }
            }
            logf("[fc] CmdFlashcardDeleteActive - removing %d annotation(s) of card %d ('%s')\n", len(toDelete),
                 deletedIdx, FlashcardTabLogName(e.tab));
            for (Annotation* a : toDelete) {
                DeleteAnnotationAndUpdateUI(e.tab, a);
            }

            // reload the cards vec so the deleted card is gone everywhere;
            // warm-render jobs are stale after the mutation
            FlashcardPrewarmInvalidate();
            e.tab->flashcard.cards = FlashcardLoadFromDocument(AsEngineMupdf(engine));
            e.tab->flashcard.cardsLoaded = true;

            // prune SM-2 states of cards that no longer exist (the deleted
            // card's key is gone from the reloaded vec) and persist, like the
            // rate flow does
            if (e.tab->filePath) {
                int pruned = FlashcardStudyPruneStates(e.tab->flashcard.studyDoc, e.tab->flashcard.cards);
                if (pruned > 0) {
                    logf("[fc] CmdFlashcardDeleteActive - pruned %d state(s) of deleted cards in '%s'\n", pruned,
                         FlashcardTabLogName(e.tab));
                }
                FlashcardStudySave(e.tab->filePath.s, e.tab->flashcard.studyDoc);
            }

            // the cards vec shifted by one: drop every queued entry of the
            // deleted card (the current one + any relearn re-appends) and
            // shift down every later entry of this tab. Entries removed
            // before the current position move the "next" entry up, so
            // account for them.
            int removedBefore = 0;
            for (int i = len(win->flashcard.studyOrder) - 1; i >= 0; i--) {
                const FlashcardQueueEntry& ent = win->flashcard.studyOrder[i];
                if (ent.tab == e.tab && ent.cardIdx == deletedIdx) {
                    win->flashcard.studyOrder.RemoveAt(i);
                    if (i < curIdx) {
                        removedBefore++;
                    }
                }
            }
            curIdx -= removedBefore;
            for (FlashcardQueueEntry& ent : win->flashcard.studyOrder) {
                if (ent.tab == e.tab && ent.cardIdx > deletedIdx) {
                    ent.cardIdx--;
                }
            }

            FlashcardToolbarUpdateCount(win);
            // the Lista panel's rows changed (card gone, indices shifted)
            FlashcardSidebarPopulate(win);

            // same tail as HandleFlashcardRate: reset reveal/study time and
            // advance to the entry now at curIdx, or end the session
            win->flashcard.revealMode = false;
            win->flashcard.studyTimeSec = 0.0f;
            win->flashcard.currentCardIdx = curIdx;
            if (win->flashcard.currentCardIdx >= len(win->flashcard.studyOrder)) {
                win->flashcard.studyMode = false;
                win->flashcard.currentCardIdx = -1;
                FlashcardStudyTimerStop(win);
                logf("[fc] CmdFlashcardDeleteActive - study session ended (queue empty)\n");
            } else {
                logf("[fc] CmdFlashcardDeleteActive - advancing to card %d/%d\n", win->flashcard.currentCardIdx + 1,
                     len(win->flashcard.studyOrder));
                FlashcardPrefetchUpcoming(win, win->flashcard.currentCardIdx, FlashcardPrewarmBudget(win));
                FlashcardNavigateToEntry(win, win->flashcard.studyOrder[win->flashcard.currentCardIdx], true);
                FlashcardPrewarmEnqueue(win, win->flashcard.currentCardIdx);
            }
            FlashcardToolbarUpdateState(win);
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
        case CmdFlashcardStudyTimerToggle: {
            // Session timer checkbox: handler owns the state (BS_CHECKBOX,
            // not BS_AUTOCHECKBOX — R6-left lesson)
            win->flashcard.studyTimerEnabled = !win->flashcard.studyTimerEnabled;
            CheckDlgButton(win->flashcard.hwndToolbarFlashcard, CmdFlashcardStudyTimerToggle,
                           win->flashcard.studyTimerEnabled ? BST_CHECKED : BST_UNCHECKED);
            logf("[fc] StudyTimerToggle - session timer %s\n",
                 // StrL each branch separately: bare ternary decays to const
                 // char* (deleted FmtArg ctor)
                 win->flashcard.studyTimerEnabled ? StrL("ON") : StrL("OFF"));
            return true;
        }
        case CmdFlashcardStudyTimerEdit:
            // Minutes are read from the edit when Study starts
            // (FlashcardStudyTimerStart); nothing to do on the command itself.
            return true;
        default:
            return false;
    }
}

// Start the flashcard study timer (kFlashcardStudyTimerID on hwndCanvas).
// Called when study mode becomes active. Arms the session timer (if enabled
// in the toolbar) and shows the progress bar.
void FlashcardStudyTimerStart(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    // Read the configured minutes from the toolbar edit (keep the setting
    // current even if the user edited the box without pressing Enter)
    if (win->flashcard.hwndStudyTimerEdit) {
        WCHAR buf[16]{};
        GetWindowTextW(win->flashcard.hwndStudyTimerEdit, buf, (int)dimof(buf));
        int mins = _wtoi(buf);
        if (mins > 0 && mins <= 600) {
            win->flashcard.studyTimerMinutesSetting = (DWORD)mins;
        }
    }
    // USER_TIMER_MINIMUM = 10ms (100Hz) — same as auto-scroll continuous tick
    SetTimer(win->hwndCanvas, kFlashcardStudyTimerID, USER_TIMER_MINIMUM, nullptr);
    // Arm the session timer: effective countdown from the configured minutes
    win->flashcard.studyTimerMinutes = win->flashcard.studyTimerEnabled ? win->flashcard.studyTimerMinutesSetting : 0;
    win->flashcard.studyTimerStartTick = GetTickCount64();
    // Progress bar tracks the session timer (only when enabled)
    if (win->flashcard.studyTimerMinutes > 0) {
        FlashcardStudyTimerUIStart(win);
    }
    logf("[fc] StudyTimerStart - timer started (session timer %s, %u min)\n",
         // StrL only takes literals: wrap each branch separately, else the
         // ternary decays to const char* (deleted FmtArg ctor)
         win->flashcard.studyTimerMinutes > 0 ? StrL("ON") : StrL("OFF"), win->flashcard.studyTimerMinutes);
}

// Stop the flashcard study timer.
void FlashcardStudyTimerStop(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    KillTimer(win->hwndCanvas, kFlashcardStudyTimerID);
    FlashcardStudyTimerUIStop(win);
    win->flashcard.studyTimerMinutes = 0;
    // flush any fractional seconds accumulated in FlashcardStats
    FlashcardStatsOnSessionEnd();
    logf("[fc] StudyTimerStop - timer stopped\n");
}

// Flashcard study timer tick: called from Canvas.cpp when kFlashcardStudyTimerID fires.
// Two independent concerns:
//  1. STATS capture — credit time to the current card, capped at maxPerCardSec
//     (60s) per card. The cap only stops crediting; no visual feedback, no
//     beep, the user keeps studying.
//  2. SESSION timer — like the autoscroll timer: a user-configured countdown
//     (minutes) that limits the study session. On expiry: stop study mode,
//     beep + "∴" overlay (same as autoscroll).
void FlashcardStudyTick(MainWindow* win, HWND hwnd) {
    if (!win || !win->flashcard.studyMode) {
        return;
    }

    // --- 2. Session timer check (autoscroll pattern: wall-clock vs startTick) ---
    if (win->flashcard.studyTimerMinutes > 0 && win->flashcard.studyTimerStartTick > 0) {
        u64 elapsedMs = GetTickCount64() - win->flashcard.studyTimerStartTick;
        u64 totalMs = (u64)win->flashcard.studyTimerMinutes * 60 * 1000;
        if (elapsedMs >= totalMs) {
            logf("[fc] StudyTick - session timer expired (%u min), ending study session\n",
                 win->flashcard.studyTimerMinutes);
            // End the session (same as the Study toggle OFF path)
            FlashcardPrewarmInvalidate();
            FlashcardPrewarmShutdown();
            win->flashcard.studyMode = false;
            win->flashcard.revealMode = false;
            win->flashcard.currentCardIdx = -1;
            win->flashcard.studyTimeSec = 0.0f;
            win->flashcard.studyOrder.Reset();
            FlashcardStudyTimerStop(win);
            FlashcardToolbarUpdateState(win);
            MainWindowRerender(win);
            // Feedback: beep + "∴" overlay (same as autoscroll timer done)
            MessageBeep(MB_OK);
            FlashcardShowTimerDoneOverlay(win);
            return;
        }
    }

    // --- 1. Stats capture (per-card cap, NO visual feedback) ---
    // When revealMode is ON, the user is looking at the answer — pause capture
    if (win->flashcard.revealMode) {
        return;
    }
    int curIdx = win->flashcard.currentCardIdx;
    if (curIdx < 0 || curIdx >= len(win->flashcard.studyOrder)) {
        return; // no valid current card
    }
    FlashcardQueueEntry& e = win->flashcard.studyOrder[curIdx];
    if (!e.tab || e.cardIdx < 0 || e.cardIdx >= len(e.tab->flashcard.cards)) {
        return;
    }
    // Per-card cap: each card credits at most maxPerCardSec (60s) to the stats.
    // Past the cap we simply stop crediting — the session continues.
    // studyTimeSec is a WINDOW-level per-card accumulator (reset on advance/
    // rate), not per-doc attribution, so the cap stays window-level too.
    // Attribution: the owning tab is resolved from the queue entry `e`
    // (studyOrder[curIdx].tab) — correct even when the user Ctrl+Tab'd away
    // from the card's document mid-session. e.tab null/closed → skip the
    // tick entirely (no credit, no crash).
    if (win->flashcard.studyTimeSec < win->flashcard.maxPerCardSec) {
        win->flashcard.studyTimeSec += 0.01f; // 10ms per tick
        FlashcardStatsAddSeconds(e.tab, 0.01f);
    }
    // --- 3. Idle prewarm pump (s64-R9p): one-shot per advance; enqueue is
    // self-deduping via prewarmFilledForIdx ---
    if (win->flashcard.currentCardIdx != win->flashcard.prewarmFilledForIdx) {
        FlashcardPrewarmEnqueue(win, win->flashcard.currentCardIdx);
    }
}

// --- Study session-timer progress bar (bottom of screen) + "done" overlay ---

// Overlay window for the progress bar: a 2px silver line centered at the
// bottom that shrinks symmetrically toward the screen center over the
// session duration (totalMs), driven by the session startTick.
static HWND gFlashcardTimerUIHwnd = nullptr;
static constexpr UINT_PTR kFlashcardTimerUIPaintTimerId = 98;
static constexpr wchar_t kFlashcardTimerUIOverlayClass[] = L"TumatraFlashcardTimerUIOverlay";
// Session state shared with the paint callback (set at UIStart)
static u64 gFcSessTotalMs = 0;
static u64 gFcSessStartTick = 0;
// Position chosen at creation (fullscreen = bottom of screen; windowed = above taskbar)
static int gFcSessOverlayY = 0;

static LRESULT CALLBACK FlashcardTimerUIOverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH br = CreateSolidBrush(ThemeWindowBackgroundColor());
            FillRect((HDC)wp, &rc, br);
            DeleteObject(br);
            return 1;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int screenW = rc.right - rc.left;
            int lineY = rc.bottom - 2;
            int lineH = 2;
            // Remaining fraction of the session (1 -> 0)
            float remainingFrac = 1.0f;
            if (gFcSessTotalMs > 0) {
                u64 elapsedMs = GetTickCount64() - gFcSessStartTick;
                if (elapsedMs >= gFcSessTotalMs) {
                    remainingFrac = 0.0f;
                } else {
                    remainingFrac = 1.0f - (float)((double)elapsedMs / (double)gFcSessTotalMs);
                }
            }
            // ONE centered segment shrinking toward the center:
            // full width at start, nothing at expiry
            int centerX = (rc.left + rc.right) / 2;
            int currentHalfW = (int)(((float)screenW / 2.0f) * remainingFrac);
            if (currentHalfW < 0) {
                currentHalfW = 0;
            }
            COLORREF lineCol = ThemeWindowTextColor(); // TUMATRA-FORK(s64-R9l): was RGB(192,192,192)
            HBRUSH br = CreateSolidBrush(lineCol);
            RECT lineRect = {centerX - currentHalfW, lineY, centerX + currentHalfW, lineY + lineH};
            FillRect(hdc, &lineRect, br);
            DeleteObject(br);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (wp == kFlashcardTimerUIPaintTimerId) {
                // Repaint ~10x/sec to animate the shrink
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        case WM_DESTROY:
            gFlashcardTimerUIHwnd = nullptr;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterFlashcardTimerUIOverlayClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = FlashcardTimerUIOverlayProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kFlashcardTimerUIOverlayClass;
    wc.hbrBackground = nullptr; // WM_ERASEBKGND paints the theme surface
    RegisterClassExW(&wc);
}

// Start the progress-bar overlay for the study session timer.
void FlashcardStudyTimerUIStart(MainWindow* win) {
    if (!win || !win->hwndFrame) {
        return;
    }
    RegisterFlashcardTimerUIOverlayClass();
    // Session window for the paint callback
    gFcSessTotalMs = (u64)win->flashcard.studyTimerMinutes * 60 * 1000;
    gFcSessStartTick = win->flashcard.studyTimerStartTick;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int overlayH = DpiScale(win->hwndFrame, 8); // thin bar at bottom

    // Fullscreen: bottom of the screen. Windowed: above the Windows taskbar
    // (~48px up) so the line is not covered by it.
    WINDOWPLACEMENT wpl = {sizeof(wpl)};
    GetWindowPlacement(win->hwndFrame, &wpl);
    bool isFullscreen =
        (wpl.showCmd == SW_MAXIMIZE) && (wpl.rcNormalPosition.bottom - wpl.rcNormalPosition.top) >= screenH - 100;
    gFcSessOverlayY = isFullscreen ? (screenH - overlayH) : (screenH - overlayH - DpiScale(win->hwndFrame, 48));

    HWND hwndOverlay = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                                       kFlashcardTimerUIOverlayClass, L"", WS_POPUP | WS_VISIBLE, 0, gFcSessOverlayY,
                                       screenW, overlayH, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwndOverlay) {
        logf("[fc] TimerUIStart - failed to create overlay window\n");
        return;
    }
    gFlashcardTimerUIHwnd = hwndOverlay;
    ShowWindow(hwndOverlay, SW_SHOW);
    UpdateWindow(hwndOverlay);
    // Repaint timer (100ms) — the progress derives from the session clock
    SetTimer(hwndOverlay, kFlashcardTimerUIPaintTimerId, 100, nullptr);
    logf("[fc] TimerUIStart - progress bar started (fullscreen=%d, y=%d, %u min)\n", isFullscreen, gFcSessOverlayY,
         win->flashcard.studyTimerMinutes);
}

// Stop the progress-bar overlay.
void FlashcardStudyTimerUIStop(MainWindow* win) {
    (void)win;
    if (gFlashcardTimerUIHwnd) {
        DestroyWindow(gFlashcardTimerUIHwnd);
        gFlashcardTimerUIHwnd = nullptr;
    }
    logf("[fc] TimerUIStop - progress bar stopped\n");
}

// --- "Timer done" overlay ("∴" glyph for 3s, same as autoscroll) ---

static HWND gFlashcardTimerDoneHwnd = nullptr;
static constexpr UINT_PTR kFlashcardTimerDoneOverlayTimerId = 97;
static constexpr wchar_t kFlashcardTimerDoneOverlayClass[] = L"TumatraFlashcardTimerDoneOverlay";

static LRESULT CALLBACK FlashcardTimerDoneOverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH br = CreateSolidBrush(ThemeWindowBackgroundColor());
            FillRect((HDC)wp, &rc, br);
            DeleteObject(br);
            return 1;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, ThemeWindowTextColor());
            HFONT hFont = CreateFontW(DpiScale(hwnd, 52), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hOld = (HFONT)SelectObject(hdc, hFont);
            DrawTextW(hdc, L"\u2234", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOld);
            DeleteObject(hFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (wp == kFlashcardTimerDoneOverlayTimerId) {
                KillTimer(hwnd, kFlashcardTimerDoneOverlayTimerId);
                DestroyWindow(hwnd);
                gFlashcardTimerDoneHwnd = nullptr;
            }
            return 0;
        case WM_LBUTTONDOWN:
            KillTimer(hwnd, kFlashcardTimerDoneOverlayTimerId);
            DestroyWindow(hwnd);
            gFlashcardTimerDoneHwnd = nullptr;
            return 0;
        case WM_DESTROY:
            gFlashcardTimerDoneHwnd = nullptr;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterFlashcardTimerDoneOverlayClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = FlashcardTimerDoneOverlayProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kFlashcardTimerDoneOverlayClass;
    wc.hbrBackground = nullptr; // WM_ERASEBKGND paints the theme surface
    RegisterClassExW(&wc);
}

// Show the "timer done" overlay (centered "∴" glyph for 3s) when the study
// session timer expires. Same visuals as the autoscroll timer-done overlay.
void FlashcardShowTimerDoneOverlay(MainWindow* win) {
    if (gFlashcardTimerDoneHwnd) {
        KillTimer(gFlashcardTimerDoneHwnd, kFlashcardTimerDoneOverlayTimerId);
        DestroyWindow(gFlashcardTimerDoneHwnd);
        gFlashcardTimerDoneHwnd = nullptr;
    }
    RegisterFlashcardTimerDoneOverlayClass();
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int overlayW = DpiScale(win->hwndFrame, 200);
    int overlayH = DpiScale(win->hwndFrame, 80);
    int x = (screenW - overlayW) / 2;
    int y = screenH - overlayH - DpiScale(win->hwndFrame, 40);
    HWND hwndPopup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                     kFlashcardTimerDoneOverlayClass, L"", WS_POPUP | WS_VISIBLE, x, y, overlayW,
                                     overlayH, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwndPopup) {
        return;
    }
    gFlashcardTimerDoneHwnd = hwndPopup;
    ShowWindow(hwndPopup, SW_SHOW);
    UpdateWindow(hwndPopup);
    SetTimer(hwndPopup, kFlashcardTimerDoneOverlayTimerId, 3000, nullptr);
}
