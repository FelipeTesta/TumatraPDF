/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "Tabs.h"
#include "Theme.h"
#include "Darkmodelib.h"

constexpr int kPadding = 8;

// T2 data-loss fix: when set, UpdateActiveGroupAutoSave skips syncing current
// window tabs into the active group. Used by ActivateTabGroupInWindow so the
// destination group's tabFiles are not clobbered by the close-tab hook.
static bool sSuppressGroupAutoSave = false;

void SuppressGroupAutoSave(bool suppress) {
    sSuppressGroupAutoSave = suppress;
}

enum class TabGroupDialogMode {
    Save,
    Open,
};

struct TabGroupsListBoxModel : ListBoxModel {
    Vec<TabGroup*> groups;

    void Reload() {
        groups.Reset();
        auto* g = gGlobalPrefs->tabGroups;
        if (g) {
            for (auto* tg : *g) {
                groups.Append(tg);
            }
        }
    }

    int ItemsCount() override { return len(groups); }

    Str Item(int i) override { return groups[i]->name; }

    int TabCount(int i) {
        auto* tf = groups[i]->tabFiles;
        return tf ? len(*tf) : 0;
    }
};

struct TabGroupsWnd : Wnd {
    ~TabGroupsWnd() override;

    HFONT font = nullptr;
    HWND hwndParent = nullptr;
    Edit* editName = nullptr;
    ListBox* listBox = nullptr;
    TabGroupsListBoxModel* model = nullptr;
    Button* btnOk = nullptr;
    Button* btnDelete = nullptr;
    Button* btnCancel = nullptr;
    TabGroupDialogMode mode = TabGroupDialogMode::Save;
    MainWindow* win = nullptr;

    bool Create(MainWindow* winIn, TabGroupDialogMode modeIn);
    void LayoutToClient();
    void UpdateTheme();
    void SaveTabGroup();
    void OpenTabGroup();
    void DeleteTabGroup();
    void UpdateDeleteButton();
    void OnCancel();
    void OnOk();
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;
    bool PreTranslateMessage(MSG& msg) override;
    void ScheduleDelete();
};

static Vec<TabGroupsWnd*> gTabGroupsWnds;

TabGroupsWnd::~TabGroupsWnd() = default;

static void DeleteTabGroupsWndInstance(TabGroupsWnd* w) {
    delete w;
}

void TabGroupsWnd::ScheduleDelete() {
    auto fn = MkFunc0<TabGroupsWnd>(DeleteTabGroupsWndInstance, this);
    uitask::Post(fn, "SafeDeleteTabGroupsWnd");
}

static void PopulateListBox(TabGroupsWnd* w) {
    w->model->Reload();
    w->listBox->SetModel(w->model);
}

void TabGroupsWnd::LayoutToClient() {
    if (!layout || !hwnd) {
        return;
    }
    Rect rc = HwndClientRect(hwnd);
    Constraints bc = Tight({rc.dx, rc.dy});
    layout->Layout(bc);
    layout->SetBounds({0, 0, rc.dx, rc.dy});
}

void TabGroupsWnd::SaveTabGroup() {
    if (!editName) {
        return;
    }
    TempStr name = editName->GetTextTemp();
    if (str::IsEmptyOrWhiteSpace(name)) {
        return;
    }

    auto* group = AllocStruct<TabGroup>();
    group->name = str::Dup(name);
    group->tabFiles = new Vec<TabFile*>();

    for (WindowTab* tab : win->Tabs()) {
        if (tab->IsAboutTab()) {
            continue;
        }
        if (!tab->filePath) {
            continue;
        }
        auto* tf = AllocStruct<TabFile>();
        str::ReplaceWithCopy(&tf->path, tab->filePath);
        group->tabFiles->Append(tf);
    }

    if (!gGlobalPrefs->tabGroups) {
        gGlobalPrefs->tabGroups = new Vec<TabGroup*>();
    }
    gGlobalPrefs->tabGroups->Append(group);
    SaveSettings();
    Close();
}

// T1: live auto-sync API

TabGroup* FindTabGroup(const char* name) {
    if (!name || !*name || !gGlobalPrefs->tabGroups) {
        return nullptr;
    }
    for (auto* g : *gGlobalPrefs->tabGroups) {
        if (g && g->name && str::EqI(Str(g->name), Str(name))) {
            return g;
        }
    }
    return nullptr;
}

// rebuild group->tabFiles from win->Tabs() in order; deep-copies paths.
// Caller is responsible for SaveSettings().
void SyncGroupFromWindow(const char* name, MainWindow* win) {
    if (!name || !*name || !win) {
        return;
    }
    TabGroup* group = FindTabGroup(name);
    if (!group) {
        group = AllocStruct<TabGroup>();
        str::ReplaceWithCopy(&group->name, Str(name));
        group->tabFiles = new Vec<TabFile*>();
        if (!gGlobalPrefs->tabGroups) {
            gGlobalPrefs->tabGroups = new Vec<TabGroup*>();
        }
        gGlobalPrefs->tabGroups->Append(group);
    }
    // free old tabFiles
    if (group->tabFiles) {
        for (auto* tf : *group->tabFiles) {
            str::Free(tf->path);
            free(tf);
        }
        delete group->tabFiles;
    }
    group->tabFiles = new Vec<TabFile*>();
    for (WindowTab* tab : win->Tabs()) {
        if (tab->IsAboutTab()) {
            continue;
        }
        if (!tab->filePath.s) {
            continue;
        }
        auto* tf = AllocStruct<TabFile>();
        str::ReplaceWithCopy(&tf->path, tab->filePath);
        group->tabFiles->Append(tf);
    }
}

void SetActiveTabGroup(MainWindow* win, const char* name) {
    if (!win) {
        return;
    }
    str::ReplaceWithCopy(&gGlobalPrefs->activeTabGroup, Str(name ? name : ""));
    if (name && *name) {
        // only sync when there are document tabs to record — empty sessions stay empty
        bool hasFileTab = false;
        for (WindowTab* tab : win->Tabs()) {
            if (!tab->IsAboutTab()) {
                hasFileTab = true;
                break;
            }
        }
        if (hasFileTab) {
            SyncGroupFromWindow(name, win);
        }
    }
    SaveSettings();
    RefreshTabGroupButton(win);
}

// guard chain: shutdown / empty active / drag in progress → skip.
// then sync + persist the activeTabGroup pointer (SaveSettings).
void UpdateActiveGroupAutoSave(MainWindow* win) {
    if (!win || !win->tabsCtrl) {
        return;
    }
    // (a) shutting down: gGlobalPrefs is being torn down
    if (!gGlobalPrefs) {
        return;
    }
    // (b1) activation in progress: suppress auto-sync so the destination
    // group's tabFiles are not clobbered by close-tab hooks.
    if (sSuppressGroupAutoSave) {
        return;
    }
    // (b) empty active group → unsynced, nothing to save
    if (!gGlobalPrefs->activeTabGroup.s || !*gGlobalPrefs->activeTabGroup.s) {
        return;
    }
    // genuine last-tab close: sync EMPTY (0 files) — empty sessions are first-class
    // (d) drag/migration in progress → skip, will fire on drop
    if (win->tabsCtrl->draggingTab) {
        return;
    }
    SyncGroupFromWindow(gGlobalPrefs->activeTabGroup.s, win);
    SaveSettings();
}

void TabGroupsWnd::OpenTabGroup() {
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    if (sel < 0) {
        return;
    }
    auto* groups = gGlobalPrefs->tabGroups;
    if (!groups || sel >= len(*groups)) {
        return;
    }
    TabGroup* group = (*groups)[sel];

    // reuse current window if it has no files open (only about tab or empty)
    MainWindow* targetMain = win;
    bool hasFiles = false;
    for (WindowTab* tab : targetMain->Tabs()) {
        if (!tab->IsAboutTab()) {
            hasFiles = true;
            break;
        }
    }
    MainWindow* targetWin = hasFiles ? CreateAndShowMainWindow(nullptr) : targetMain;
    if (!targetWin) {
        return;
    }
    bool first = true;
    for (TabFile* tf : *group->tabFiles) {
        if (!tf->path.s) {
            continue;
        }
        LoadArgs args(tf->path, targetWin);
        if (!first) {
            args.forceReuse = false;
        }
        LoadDocument(&args);
        first = false;
    }
    // post WM_CLOSE instead of DestroyWindow so we return from the
    // listbox double-click callback before the dialog is torn down
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

static void FreeTabGroup(TabGroup* group) {
    if (!group) {
        return;
    }
    str::Free(group->name);
    if (group->tabFiles) {
        for (auto* tf : *group->tabFiles) {
            str::Free(tf->path);
            free(tf);
        }
        delete group->tabFiles;
    }
    free(group);
}

void TabGroupsWnd::UpdateDeleteButton() {
    if (!btnDelete) {
        return;
    }
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    btnDelete->SetIsEnabled(sel >= 0);
}

void TabGroupsWnd::DeleteTabGroup() {
    int sel = listBox ? listBox->GetCurrentSelection() : -1;
    if (sel < 0) {
        return;
    }
    auto* groups = gGlobalPrefs->tabGroups;
    if (!groups || sel >= len(*groups)) {
        return;
    }
    TabGroup* group = (*groups)[sel];
    groups->Remove(group);
    FreeTabGroup(group);
    SaveSettings();
    PopulateListBox(this);
    UpdateDeleteButton();
}

static void DrawTabGroupItem(TabGroupsWnd* w, ListBox::DrawItemEvent* ev) {
    if (ev->itemIndex < 0 || ev->itemIndex >= w->model->ItemsCount()) {
        return;
    }

    HDC hdc = ev->hdc;
    Rect rc = ev->itemRect;
    ListBox* lb = ev->listBox;

    COLORREF colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    COLORREF colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    SetBkColor(hdc, colBg);
    HdcFillRectWithBkColor(hdc, rc);

    SetTextColor(hdc, colText);
    SetBkMode(hdc, TRANSPARENT);

    HFONT oldFont = nullptr;
    if (lb->font) {
        oldFont = SelectFont(hdc, lb->font);
    }

    int padX = DpiScale(lb->hwnd, 4);
    rc.x += padX;
    rc.dx -= 2 * padX;

    // draw group name on the left
    Str name = w->model->Item(ev->itemIndex);
    uint fmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT;
    HdcDrawText(hdc, name, rc, fmt);

    // draw tab count on the right
    int nTabs = w->model->TabCount(ev->itemIndex);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d tabs", nTabs);
    COLORREF rightCol = AccentColor(colText, 80);
    SetTextColor(hdc, rightCol);
    fmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RIGHT;
    HdcDrawText(hdc, Str(buf), rc, fmt);

    if (oldFont) {
        SelectFont(hdc, oldFont);
    }
}

static void OnListDoubleClick(TabGroupsWnd* w) {
    if (w->mode == TabGroupDialogMode::Open) {
        w->OpenTabGroup();
    } else {
        int sel = w->listBox ? w->listBox->GetCurrentSelection() : -1;
        if (sel >= 0 && w->editName) {
            auto* groups = gGlobalPrefs->tabGroups;
            if (groups && sel < len(*groups)) {
                w->editName->SetText((*groups)[sel]->name);
                w->editName->SelectAll();
                HwndSetFocus(w->editName->hwnd);
            }
        }
    }
}

void TabGroupsWnd::UpdateTheme() {
    COLORREF colBg = ThemeWindowControlBackgroundColor();
    COLORREF colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    auto setColors = [&](Wnd* c) {
        if (c) {
            c->SetColors(colTxt, colBg);
        }
    };
    setColors(editName);
    setColors(listBox);
    setColors(btnOk);
    setColors(btnDelete);
    setColors(btnCancel);
    if (UseDarkModeLib()) {
        dmlib::setDarkWndSafe(hwnd);
        dmlib::setWindowEraseBgSubclass(hwnd);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void TabGroupsWnd::OnCancel() {
    Close();
}

void TabGroupsWnd::OnOk() {
    if (mode == TabGroupDialogMode::Save) {
        SaveTabGroup();
    } else {
        OpenTabGroup();
    }
}

LRESULT TabGroupsWnd::WndProc(HWND hwndIn, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SIZE) {
        LayoutToClient();
        return 0;
    }
    return WndProcDefault(hwndIn, msg, wp, lp);
}

bool TabGroupsWnd::PreTranslateMessage(MSG& msg) {
    if (!hwnd) {
        return false;
    }
    if (msg.hwnd != hwnd && !IsChild(hwnd, msg.hwnd)) {
        return false;
    }
    if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && editName && msg.hwnd == editName->hwnd &&
        mode == TabGroupDialogMode::Save) {
        TempStr name = editName->GetTextTemp();
        if (!str::IsEmptyOrWhiteSpace(name)) {
            SaveTabGroup();
            return true;
        }
    }
    if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
        OnCancel();
        return true;
    }
    return false;
}

static void TeardownTabGroupsWnd(TabGroupsWnd* w) {
    if (!w || gTabGroupsWnds.Find(w) < 0) {
        return;
    }
    gTabGroupsWnds.Remove(w);
    w->model = nullptr;
    w->ScheduleDelete();
}

static void OnTabGroupsClose(Wnd::CloseEvent* ev) {
    TeardownTabGroupsWnd((TabGroupsWnd*)ev->e->self);
}

static void OnTabGroupsDestroy(Wnd::DestroyEvent* ev) {
    TeardownTabGroupsWnd((TabGroupsWnd*)ev->e->self);
}

bool TabGroupsWnd::Create(MainWindow* winIn, TabGroupDialogMode modeIn) {
    win = winIn;
    mode = modeIn;
    hwndParent = win->hwndFrame;
    bool isRtl = IsUIRtl();

    Str titleStr = (mode == TabGroupDialogMode::Save) ? Str(_TRA("Save Tab Group")) : Str(_TRA("Restore Tab Group"));
    {
        CreateCustomArgs args;
        args.title = titleStr;
        args.visible = false;
        args.style = WS_OVERLAPPEDWINDOW;
        args.font = font;
        args.isRtl = isRtl;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    if (mode == TabGroupDialogMode::Save) {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.withBorder = true;
        args.isRtl = isRtl;
        int groupNum = 1;
        if (gGlobalPrefs->tabGroups) {
            groupNum = len(*gGlobalPrefs->tabGroups) + 1;
        }
        TempStr defaultName = fmt("group #%d", groupNum);
        args.text = defaultName;
        editName = new Edit();
        editName->Create(args);
        auto* editPad = new Padding(editName, DpiScaledInsets(hwnd, 0, 0, kPadding, 0));
        vbox->AddChild(editPad);
    }

    {
        ListBox::CreateArgs lbArgs;
        lbArgs.parent = hwnd;
        lbArgs.font = font;
        lbArgs.isRtl = isRtl;
        listBox = new ListBox();
        listBox->onDrawItem = MkFunc1(DrawTabGroupItem, this);
        listBox->onSelectionChanged = MkMethod0<TabGroupsWnd, &TabGroupsWnd::UpdateDeleteButton>(this);
        listBox->onDoubleClick = MkFunc0(OnListDoubleClick, this);
        listBox->Create(lbArgs);
        model = new TabGroupsListBoxModel();
        PopulateListBox(this);
        vbox->AddChild(listBox, 1);
    }

    {
        auto* btnRow = new HBox();
        btnRow->alignMain = MainAxisAlign::MainEnd;
        btnRow->alignCross = CrossAxisAlign::CrossCenter;

        btnCancel = CreateButton(hwnd, _TRA("Cancel"), MkMethod0<TabGroupsWnd, &TabGroupsWnd::OnCancel>(this), isRtl);
        btnRow->AddChild(btnCancel);
        btnDelete =
            CreateButton(hwnd, _TRA("Delete"), MkMethod0<TabGroupsWnd, &TabGroupsWnd::DeleteTabGroup>(this), isRtl);
        btnDelete->SetIsEnabled(false);
        btnDelete->SetInsetsPt(0, 0, 0, 4);
        btnRow->AddChild(btnDelete);
        Str okText = (mode == TabGroupDialogMode::Save) ? Str(_TRA("Save")) : Str(_TRA("Restore"));
        btnOk = CreateButton(hwnd, okText, MkMethod0<TabGroupsWnd, &TabGroupsWnd::OnOk>(this), isRtl);
        btnOk->SetInsetsPt(0, 0, 0, 4);
        btnRow->AddChild(btnOk);
        vbox->AddChild(new Padding(btnRow, DpiScaledInsets(hwnd, kPadding, 0, 0, 0)));
    }

    layout = new Padding(vbox, DpiScaledInsets(hwnd, kPadding, kPadding));

    int winW = DpiScale(hwnd, 400);
    int winH = DpiScale(hwnd, 350);
    SetWindowPos(hwnd, nullptr, 0, 0, winW, winH, SWP_NOMOVE | SWP_NOZORDER);
    LayoutToClient();
    HwndCenterDialog(hwnd, hwndParent);
    HwndEnsureOnScreen(hwnd);
    UpdateTheme();
    UpdateDeleteButton();
    SetIsVisible(true);
    if (editName) {
        editName->SelectAll();
        HwndSetFocus(editName->hwnd);
    }
    return true;
}

static void ShowTabGroupsDialog(MainWindow* win, TabGroupDialogMode mode) {
    for (auto* w : gTabGroupsWnds) {
        if (w->win == win && w->mode == mode) {
            if (w->hwnd && IsWindow(w->hwnd)) {
                BringWindowToTop(w->hwnd);
                return;
            }
            TeardownTabGroupsWnd(w);
            break;
        }
    }

    auto* wnd = new TabGroupsWnd();
    wnd->font = GetAppFont(win->hwndFrame);
    wnd->onClose = MkFunc1Void<Wnd::CloseEvent*>(OnTabGroupsClose);
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnTabGroupsDestroy);
    if (!wnd->Create(win, mode)) {
        delete wnd;
        return;
    }
    gTabGroupsWnds.Append(wnd);
}

void ShowSaveTabGroupDialog(MainWindow* win) {
    ShowTabGroupsDialog(win, TabGroupDialogMode::Save);
}

void ShowOpenTabGroupDialog(MainWindow* win) {
    ShowTabGroupsDialog(win, TabGroupDialogMode::Open);
}

// Tab system T3: activate a group in a specific window — set active, close
// current document tabs, then load the group's files into that window.
// Two-phase to prevent data loss:
//   1. Suppress UpdateActiveGroupAutoSave so closing current tabs does NOT
//      overwrite the destination group's tabFiles with the current book.
//   2. Set activeTabGroup directly (not via SetActiveTabGroup, which calls
//      SyncGroupFromWindow and would clobber the group's file list before
//      load).
void ActivateTabGroupInWindow(MainWindow* win, const char* name) {
    if (!name || !*name || !win) {
        return;
    }
    TabGroup* g = FindTabGroup(name);
    if (!g) {
        return;
    }
    sSuppressGroupAutoSave = true;
    // snapshot destination group's files BEFORE any mutation (async loads may
    // fire auto-save hooks that would clobber the group we're loading)
    Vec<Str> snapshot;
    for (TabFile* tf : *g->tabFiles) {
        if (tf->path.s) {
            snapshot.Append(Str(tf->path));
        }
    }
    // set the active pointer without syncing current tabs into the group
    str::ReplaceWithCopy(&gGlobalPrefs->activeTabGroup, Str(name));
    logf("[tabs] activate '%s' close-loop start %d ms\n", Str(name), (int)GetTickCount());
    // close tabs not in destination group (keep already-open dest tabs to
    // preserve scroll position and avoid rebuild churn)
    auto tabs = win->Tabs();
    for (int i = len(tabs) - 1; i >= 0; i--) {
        WindowTab* tab = tabs[i];
        if (!tab || tab->IsNonDocumentTab()) {
            continue;
        }
        bool keep = false;
        for (Str s : snapshot) {
            if (str::EqI(tab->filePath, s)) {
                keep = true;
                break;
            }
        }
        if (keep) {
            continue;
        }
        CloseTab(tab, false);
    }
    logf("[tabs] activate '%s' close-loop done %d ms\n", Str(name), (int)GetTickCount());
    // load group files lazily: first file sync (selected immediately), rest lazy
    bool first = true;
    for (TabFile* tf : *g->tabFiles) {
        if (!tf->path.s) {
            continue;
        }
        // skip files already open in this window
        bool alreadyOpen = false;
        for (WindowTab* t : win->Tabs()) {
            if (!t->IsNonDocumentTab() && str::EqI(t->filePath, tf->path)) {
                alreadyOpen = true;
                break;
            }
        }
        if (alreadyOpen) {
            continue;
        }
        LoadArgs args(tf->path, win);
        args.lazyLoad = !first; // first file sync so selection shows content at once
        if (!first) {
            args.forceReuse = false;
        }
        LoadDocument(&args);
        first = false;
    }
    TabsSelect(win, 0);
    logf("[tabs] activate '%s' load-loop done %d ms\n", Str(name), (int)GetTickCount());
    sSuppressGroupAutoSave = false;
    SaveSettings();
    RefreshTabGroupButton(win);
}
