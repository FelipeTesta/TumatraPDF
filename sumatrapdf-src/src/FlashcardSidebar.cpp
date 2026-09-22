/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/LabelWithCloseWnd.h"
#include "Settings.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "AppSettings.h"
#include "Theme.h"
#include "resource.h"
#include "Flashcard.h"
#include "FlashcardSidebar.h"

struct ListaTreeModel : TreeModel {
    ~ListaTreeModel() override { DeleteVecMembers(items); }

    TreeItem Root() override { return (TreeItem)rootItem; }

    Str Text(TreeItem ti) override {
        auto* item = (ListaTreeItem*)ti;
        return item->text;
    }

    TreeItem Parent(TreeItem ti) override {
        auto* item = (ListaTreeItem*)ti;
        return (TreeItem)item->parent;
    }

    int ChildCount(TreeItem ti) override {
        auto* item = (ListaTreeItem*)ti;
        if (!item) return 0;
        return len(item->children);
    }

    TreeItem ChildAt(TreeItem ti, int idx) override {
        auto* item = (ListaTreeItem*)ti;
        return (TreeItem)item->children[idx];
    }

    bool IsExpanded(TreeItem ti) override {
        auto* item = (ListaTreeItem*)ti;
        return item->isExpanded;
    }

    bool IsChecked(TreeItem /*ti*/) override { return false; }

    void SetHandle(TreeItem ti, HTREEITEM hItem) override {
        ReportIf(ti < 0);
        auto* item = (ListaTreeItem*)ti;
        item->hItem = hItem;
    }

    HTREEITEM GetHandle(TreeItem ti) override {
        ReportIf(ti < 0);
        auto* item = (ListaTreeItem*)ti;
        return item->hItem;
    }

    struct ListaTreeItem {
        Str text;
        int cardIdx = -1;
        ListaTreeItem* parent = nullptr;
        Vec<ListaTreeItem*> children;
        HTREEITEM hItem = nullptr;
        bool isExpanded = false;

        ~ListaTreeItem() {
            str::Free(text);
            DeleteVecMembers(children);
        }
    };

    ListaTreeItem* rootItem = new ListaTreeItem();
    Vec<ListaTreeItem*> items;
};

static WNDPROC gWndProcListaBox = nullptr;

static void LayoutListaContainer(MainWindow* win) {
    if (!win || !win->flashcard.listaLayout || !win->flashcard.hwndListaBox) {
        return;
    }
    Rect rc = HwndClientRect(win->flashcard.hwndListaBox);
    if (rc.IsEmpty()) {
        return;
    }
    win->flashcard.listaLayout->Layout(Tight(Size{rc.dx, rc.dy}));
    win->flashcard.listaLayout->SetBounds(Rect{0, 0, rc.dx, rc.dy});
}

static LRESULT CALLBACK WndProcListaBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        return CallWindowProc(gWndProcListaBox, hwnd, msg, wp, lp);
    }

    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    switch (msg) {
        case WM_SIZE:
            LayoutListaContainer(win);
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == IDC_FAV_LABEL_WITH_CLOSE) {
                FlashcardSidebarToggle(win);
            }
            break;
    }
    return CallWindowProc(gWndProcListaBox, hwnd, msg, wp, lp);
}

static void ListaTreeSelectionChanged(TreeView::SelectionChangedEvent* args) {
    MainWindow* win = FindMainWindowByHwnd(args->treeView->hwnd);
    if (!win) return;

    TreeView* treeView = args->treeView;
    TreeItem ti = treeView->GetSelection();
    if (ti == 0) return;

    auto* model = (ListaTreeModel*)treeView->treeModel;
    auto* item = (ListaTreeModel::ListaTreeItem*)ti;
    if (!model || item->cardIdx < 0) return;

    int cardIdx = item->cardIdx;
    if (cardIdx >= len(win->flashcard.cards)) return;

    Flashcard& card = win->flashcard.cards[cardIdx];
    WindowTab* tab = win->CurrentTab();
    if (!tab) return;

    DisplayModel* dm = tab->AsFixed();
    if (!dm) return;

    RectF vr;
    int vPage = dm->PhysicalToVirtualForRect(card.pageNo, card.bounds, &vr);
    Rect screenRect = dm->CvtToScreen(vPage, vr);
    dm->ScrollScreenToRect(vPage, screenRect);
}

void FlashcardSidebarCreate(MainWindow* win) {
    if (win->flashcard.hwndListaBox) return;
    logf("[fc] FlashcardSidebarCreate - creating sidebar popup\n");

    HMODULE h = GetModuleHandleW(nullptr);
    DWORD dwStyle = WS_POPUP | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    int dx = DpiScale(win->hwndFrame, 280);
    int dy = DpiScale(win->hwndFrame, 400);

    win->flashcard.hwndListaBox = CreateWindowExW(WS_EX_TOOLWINDOW, WC_STATIC, L"Flashcards", dwStyle, 0, 0, dx, dy,
                                                  win->hwndFrame, nullptr, h, nullptr);
    if (!win->flashcard.hwndListaBox) return;

    auto* l = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = win->flashcard.hwndListaBox;
        args.cmdId = IDC_FAV_LABEL_WITH_CLOSE;
        args.font = GetAppSidebarLabelFont(win->hwndFrame);
        args.isRtl = IsUIRtl();
        l->Create(args);
    }
    l->SetPaddingXY(2, 2);
    l->SetText(_TRA("Flashcards"));

    auto* treeView = new TreeView();
    TreeView::CreateArgs targs;
    targs.parent = win->flashcard.hwndListaBox;
    targs.font = GetAppTreeFont(win->hwndFrame);
    targs.fullRowSelect = true;
    targs.isRtl = IsUIRtl();
    treeView->Create(targs);
    ReportIf(!treeView->hwnd);

    win->flashcard.listaTreeView = treeView;

    auto* model = new ListaTreeModel();
    treeView->SetTreeModel(model);
    treeView->onSelectionChanged = MkFunc1Void(ListaTreeSelectionChanged);

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(l);
    vbox->AddChild(new Spacer(0, 2));
    vbox->AddChild(treeView, 1);
    win->flashcard.listaLayout = vbox;

    if (nullptr == gWndProcListaBox) {
        gWndProcListaBox = (WNDPROC)GetWindowLongPtr(win->flashcard.hwndListaBox, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->flashcard.hwndListaBox, GWLP_WNDPROC, (LONG_PTR)WndProcListaBox);

    RECT rcFrame;
    GetWindowRect(win->hwndFrame, &rcFrame);
    SetWindowPos(win->flashcard.hwndListaBox, HWND_TOP, rcFrame.left, rcFrame.top + 100, dx, dy, SWP_NOACTIVATE);
    ShowWindow(win->flashcard.hwndListaBox, SW_SHOWNOACTIVATE);

    UpdateControlsColors(win);
}

void FlashcardSidebarDestroy(MainWindow* win) {
    if (!win->flashcard.hwndListaBox) return;

    logf("[fc] FlashcardSidebarDestroy - destroying sidebar popup\n");

    if (win->flashcard.listaTreeView) {
        delete win->flashcard.listaTreeView->treeModel;
        win->flashcard.listaTreeView->treeModel = nullptr;
        delete win->flashcard.listaTreeView;
        win->flashcard.listaTreeView = nullptr;
    }
    if (win->flashcard.listaLayout) {
        delete win->flashcard.listaLayout;
        win->flashcard.listaLayout = nullptr;
    }

    DestroyWindow(win->flashcard.hwndListaBox);
    win->flashcard.hwndListaBox = nullptr;
}

void FlashcardSidebarToggle(MainWindow* win) {
    logf("[fc] FlashcardSidebarToggle\n");
    if (win->flashcard.hwndListaBox) {
        bool visible = IsWindowVisible(win->flashcard.hwndListaBox);
        if (visible) {
            ShowWindow(win->flashcard.hwndListaBox, SW_HIDE);
        } else {
            RECT rcFrame;
            GetWindowRect(win->hwndFrame, &rcFrame);
            SetWindowPos(win->flashcard.hwndListaBox, HWND_TOP, rcFrame.left, rcFrame.top + 100, 0, 0,
                         SWP_NOACTIVATE | SWP_NOSIZE);
            ShowWindow(win->flashcard.hwndListaBox, SW_SHOWNOACTIVATE);
            FlashcardSidebarPopulate(win);
        }
    } else {
        FlashcardSidebarCreate(win);
        FlashcardSidebarPopulate(win);
    }
}

void FlashcardSidebarPopulate(MainWindow* win) {
    if (!win->flashcard.hwndListaBox || !win->flashcard.listaTreeView) return;

    logf("[fc] FlashcardSidebarPopulate - adding %d cards to list\n", len(win->flashcard.cards));

    TreeView* treeView = win->flashcard.listaTreeView;
    auto* model = (ListaTreeModel*)treeView->treeModel;
    if (!model) return;

    model->rootItem->children.Reset();
    DeleteVecMembers(model->items);
    model->items.Reset();

    for (int i = 0; i < len(win->flashcard.cards); i++) {
        Flashcard& card = win->flashcard.cards[i];
        auto* item = new ListaTreeModel::ListaTreeItem();
        item->text = fmt("Card %d â€” Page %d", i + 1, card.pageNo);
        item->cardIdx = i;
        item->parent = model->rootItem;
        model->rootItem->children.Append(item);
        model->items.Append(item);
    }

    treeView->Clear();
    treeView->SetTreeModel(model);
    treeView->ExpandAll();
}