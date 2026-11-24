/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "AppTools.h"
#include "ProfileSyncWnd.h"
#include "ProfileData.h"
#include "ProfileBackupWnd.h"
#include "SumatraTest.h"
#include "SumatraPDF.h"
#include "DocController.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "Annotation.h"
extern "C" {
#include <mupdf/pdf.h>
}
#include "EngineMupdf.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "FileHistory.h"
#include "Favorites.h"
#include "SelectionTranslate.h"
#include "ImageSaveCropResize.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "Commands.h"
#include "FindWindow.h"
#include "Menu.h"
#include "Toolbar.h"
#include "LinkFollow.h"
#include "SelectTextKeyboard.h"
#include "HomePage.h"
#include "AIChatCommon.h"
#include "TabGroupsManage.h"
#include "MarkdownModel.h"
#include "wingui/BrowserDocView.h"
#include "AppSettings.h"
#include "Tabs.h"

// Silent add for -dbg-control tests (no name dialog, no settings flush).
static void AddFavoriteSilent(MainWindow* win, int pageNo) {
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath || !win->ctrl->ValidPageNo(pageNo)) {
        return;
    }
    Str path = tab->filePath;
    FileState* fs = gFileHistory.FindByPath(path);
    if (!fs) {
        fs = NewFileState(path);
        gFileHistory.Append(fs);
    }
    if (!fs->favorites) {
        return;
    }
    for (Favorite* fav : *fs->favorites) {
        if (fav->pageNo == pageNo) {
            return;
        }
    }
    TempStr pageLabel = win->ctrl->GetPageLabeTemp(pageNo);
    TempStr plainLabel = fmt("%d", pageNo);
    bool needsLabel = pageLabel && !str::Eq(plainLabel, pageLabel);
    Str pl = needsLabel ? pageLabel : Str{};
    fs->favorites->Append(NewFavorite(pageNo, {}, pl));
}

// Dev-only wrapper: ProfileDataPromptCloudRevert takes 2 args but MkFunc0
// supports only 1. Thin adapter so the control-pipe handler can fire it via
// uitask::Post on the UI thread.
static void TestLaunchCloudRevert(MainWindow* win) {
    if (win) {
        ProfileDataPromptCloudRevert(win, win->hwndFrame);
    }
}

// Drive favorites on the already-open document for tests/issue-3744.ts.
// action: "add" | "goto" | "next" | "prev" | "page". pageNo is used by add/goto.
static TempStr FavoriteNavResultTemp(Str action, int pageNo, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }

    if (str::EqI(action, "add")) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        AddFavoriteSilent(win, pageNo);
    } else if (str::EqI(action, "goto")) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        win->ctrl->GoToPage(pageNo, true);
    } else if (str::EqI(action, "next")) {
        GoToNextFavorite(win, true);
    } else if (str::EqI(action, "prev")) {
        GoToNextFavorite(win, false);
    } else if (str::EqI(action, "page")) {
        // report only
    } else {
        return finish(fmt("ERROR unknown-action action=%s", action), 1);
    }

    int cur = win->ctrl->CurrentPageNo();
    return finish(fmt("OK page=%d", cur), 0);
}

// ---------------------------------------------------------------------------
// Tab system T3 control-pipe test commands (for ad-hoc E2E session tests).
// Each drives the same statics the menu handlers call, so the control-pipe
// path exercises identical code to a user click.
// ---------------------------------------------------------------------------

static TempStr TabGroupActivateResultTemp(Str name, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) *exitCodeOut = code;
        return ToStrTemp(out);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    TabGroup* g = FindTabGroup(name.s);
    if (!g) {
        return finish(fmt("ERROR no-such-group name=%s", Str(name)), 1);
    }
    SetWindowActiveTabGroup(win, name.s);
    ActivateTabGroupInWindow(win, name.s);
    return finish(fmt("OK activated=%s tabs=%d", Str(name), len(*g->tabFiles)), 0);
}

static TempStr TabGroupListResultTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) *exitCodeOut = code;
        return ToStrTemp(out);
    };
    if (!gGlobalPrefs || !gGlobalPrefs->tabGroups) {
        return finish(StrL("OK groups=0"), 0);
    }
    out.Append(fmt("OK groups=%d", len(*gGlobalPrefs->tabGroups)));
    for (TabGroup* g : *gGlobalPrefs->tabGroups) {
        if (!g || !g->name) continue;
        int n = g->tabFiles ? len(*g->tabFiles) : 0;
        out.AppendChar(' ');
        out.Append(Str(g->name));
        out.AppendChar('=');
        out.Append(fmt("%d", n));
    }
    out.AppendChar('\n');
    // report activeTabGroup of the first window (gWindows[0]) so tests can verify it
    // (SaveSettings is a no-op under -for-testing; no focused-window helper exists)
    MainWindow* win = len(gWindows) ? gWindows[0] : nullptr;
    Str winActive = GetWindowActiveTabGroup(win);
    out.Append(fmt("active=%s", winActive.s ? winActive.s : StrL("")));
    out.AppendChar('\n');
    if (exitCodeOut) *exitCodeOut = 0;
    return ToStrTemp(out);
}

static TempStr TabGroupNewSessionResultTemp(Str name, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) *exitCodeOut = code;
        return ToStrTemp(out);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (len(name) == 0) {
        return finish(StrL("ERROR empty-name"), 1);
    }
    // new session = empty group: just set active pointer, do NOT sync current tabs
    SetWindowActiveTabGroup(win, name.s);
    RefreshTabGroupButton(win);
    // ensure the group exists in tabGroups (empty tabFiles) so TestTabGroupList sees it
    if (!FindTabGroup(name.s) && gGlobalPrefs->tabGroups) {
        auto* g = AllocStruct<TabGroup>();
        str::ReplaceWithCopy(&g->name, Str(name));
        g->tabFiles = new Vec<TabFile*>();
        gGlobalPrefs->tabGroups->Append(g);
    }
    SaveSettings();
    return finish(fmt("OK new-session=%s", Str(name)), 0);
}

static TempStr TabGroupOpenNewResultTemp(Str name, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) *exitCodeOut = code;
        return ToStrTemp(out);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    TabGroup* g = FindTabGroup(name.s);
    if (!g || !g->tabFiles || len(*g->tabFiles) == 0) {
        return finish(fmt("ERROR no-such-group name=%s", Str(name)), 1);
    }
    MainWindow* newWin = CreateAndShowMainWindow(nullptr);
    if (!newWin) {
        return finish(StrL("ERROR create-window-failed"), 1);
    }
    // NEW window gets that group; origin window keeps its own
    SetWindowActiveTabGroup(newWin, name.s);
    RefreshTabGroupButton(newWin);
    bool first = true;
    for (TabFile* tf : *g->tabFiles) {
        if (!tf->path.s) continue;
        LoadArgs args(tf->path, newWin);
        if (!first) args.forceReuse = false;
        LoadDocument(&args);
        first = false;
    }
    TabsSelect(newWin, 0);
    ShowMainWindow(newWin, WIN_STATE_NORMAL);
    SaveSettings();
    return finish(fmt("OK open-new=%s tabs=%d", Str(name), len(*g->tabFiles)), 0);
}

// Tab system T3: resolve a menu-cmd name string to the kTabGroupMenu* id the
// real TrackPopupMenu route would use, then post WM_COMMAND to the frame exactly
// as TrackPopupMenuEx dispatch does (HandleTabGroupMenuCmd).
// Supported strings:
//   "newsession"                    -> kTabGroupMenuNewSession (8900)
//   "openhere:<groupName>"          -> kTabGroupMenuOpenHereBase + index
//   "opennew:<groupName>"           -> kTabGroupMenuOpenNewBase + index
//   "rename:<groupName>:<newName>"  -> kTabGroupMenuRenameBase + index
//   "delete:<groupName>"            -> kTabGroupMenuDeleteBase + index
static int TabGroupMenuCmdResolveId(Str cmdStr) {
    if (!cmdStr) return 0;
    if (str::Eq(cmdStr, StrL("newsession"))) {
        return 8900; // kTabGroupMenuNewSession
    }
    // split on first ':' -> action
    int colon1 = str::IndexOfChar(cmdStr, ':');
    if (colon1 < 0) return 0;
    Str action = Str(cmdStr.s, colon1);
    Str rest = Str(cmdStr.s + colon1 + 1, len(cmdStr) - colon1 - 1);
    // split rest on second ':' if present -> groupName, newName (rename only)
    int colon2 = str::IndexOfChar(rest, ':');
    Str groupName, newName;
    if (colon2 < 0) {
        groupName = rest;
    } else {
        groupName = Str(rest.s, colon2);
        newName = Str(rest.s + colon2 + 1, len(rest) - colon2 - 1);
    }
    if (!gGlobalPrefs || !gGlobalPrefs->tabGroups) return 0;
    int idx = -1;
    for (int i = 0; i < len(*gGlobalPrefs->tabGroups); i++) {
        TabGroup* g = (*gGlobalPrefs->tabGroups)[i];
        if (g && g->name && str::EqI(g->name, groupName)) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return 0;
    if (str::EqI(action, StrL("openhere"))) return 8501 + idx;
    if (str::EqI(action, StrL("opennew"))) return 8601 + idx;
    if (str::EqI(action, StrL("rename"))) return 8701 + idx;
    if (str::EqI(action, StrL("delete"))) return 8801 + idx;
    return 0;
}

static TempStr TabGroupMenuCmdResultTemp(Str cmdStr, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) *exitCodeOut = code;
        return ToStrTemp(out);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    int menuId = TabGroupMenuCmdResolveId(cmdStr);
    if (menuId == 0) {
        return finish(fmt("ERROR bad-menu-cmd cmd=%s", cmdStr), 1);
    }
    logf("[tabs] menu cmd=%d (from %s)\n", menuId, Str(cmdStr));
    HandleTabGroupMenuCmd(win, menuId);
    return finish(fmt("OK menu-cmd=%d", menuId), 0);
}

// Tab system T3: open the new-session/rename dialog as the real menu route does.
// For rename, prefill with the existing group name. Under -for-testing the dialog
// auto-accepts (nameOut = "T1" or existing name).
// Tab system T3: dialog-open params replanted from the control pipe onto the
// main UI thread (see WM_COMMAND kTabGroupDialogTestCmd in SumatraPDF.cpp).
// Defined here (external linkage) so SumatraPDF.cpp can consume/free them.
bool gTestDialogIsRename = false;
Str gTestDialogName{}; // str::Dup'd; owned by pipe handler until WM_COMMAND frees it

static TempStr TabGroupDialogOpenResultTemp(Str name, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) *exitCodeOut = code;
        return ToStrTemp(out);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    gTestDialogIsRename = len(name) > 0;
    str::Free(gTestDialogName);
    gTestDialogName = str::Dup(name);
    // replant as WM_COMMAND on the frame so the modal dialog runs on the main
    // thread, not the pipe thread; reply immediately without waiting.
    PostMessageW(win->hwndFrame, WM_COMMAND, MAKEWPARAM(8950, 0), 0);
    return finish(StrL("OK posted"), 0);
}

enum class ControlCmd : u16 {
    Ping = 1,
    Quit = 2,
    TestSynctex = 10,
    TestSearch = 11,
    TestDest = 12,
    TestNamedDest = 13,
    TestChm = 14,
    TestSelectionTranslate = 15,
    TestTripleClickLineSelect = 16,
    TestContextMenuSelection = 17,
    TestGoToFindMatch = 18,
    // IDs 19-21 unused (reserved on the -dbg-control wire protocol; do not renumber).
    // Assign new test commands starting at 23.
    TestInverseSearch = 22,
    TestImageResizeArrowKey = 23,
    TestFindResultPageColumnClip = 24,
    TestFileKind = 25,
    TestScrollToLink = 26,
    TestI18nErrorString = 27,
    TestPageInfoOverlay = 28,
    TestGetToc = 29,
    TestPageLinks = 30,
    TestWindowStateDuringLoad = 31,
    TestTocNavigate = 32,
    TestMarkdownTocNavigate = 33,
    TestFavoriteNav = 34,
    TestToolbarButtons = 35,
    TestKeyboardLinkFollow = 36,
    TestFindResultsOrder = 37,
    TestClickClearsSelection = 38,
    TestRectSelectionDrag = 39,
    TestSelectTextKeyboard = 40,
    TestAIChat = 41,
    TestAIChatReplay = 42,
    TestMarkdownFollowLink = 43,
    TestHomeListRows = 44,
    TestContextMenu = 45,
    TestStatsToggleGraph = 46,
    TestCvtRoundTrip = 47,
    TestVirtWndHitTest = 48,
    TestTabGroupActivate = 49,
    TestTabGroupList = 50,
    TestTabGroupNewSession = 51,
    TestTabGroupOpenNew = 52,
    TestTabGroupMenuCmd = 53,
    TestTabGroupDialogOpen = 54,
    TestMarkdownAdapter = 55,
    TestProfileImportPrompt = 56,
    TestProfileCloudRevert = 57,
    TestGetEngineErrors = 58,
    TestBookmarkAdd = 59,
    TestBookmarkDirty = 60,
    TestBookmarkToc = 61,
    TestBookmarkSave = 62,
    TestBookmarkMove = 63,
    TestAlignQueue = 64,
    TestGeomSave = 65,
    TestAlignApply = 66,
    TestAlignCommit = 67,
    TestAlignDiscard = 68,
    TestGetPageNo = 69,
    TestTocToolbarButton = 70,
    TestBookmarkDelete = 71,
    TestRunCommand = 72,
    TestPageHold = 73,
    TestGetZoom = 74,
    // Manual multi-window test instrumentation (intentional, kept).
    // Drives/focuses/reads stats per window via the control pipe.
    // No automated test uses these hooks yet.
    TestNewWindow = 75,
    TestWindowStats = 76,
    TestOpenInWindow = 77,
    TestFocusWindow = 78,
    TestRunCommandInWindow = 79,
    // Manual profile sync test instrumentation (intentional, kept).
    // Drives profile sync actions via the control pipe.
    // No automated test uses these hooks yet.
    TestProfileSetFolder = 80,
    TestProfileExport = 81,
    TestProfileImport = 82,
    TestProfileStatus = 83,
    TestCommandEnabled = 84,
};

enum class ControlArgType : u16 {
    End = 0,
    Int32 = 1,
    Bytes = 2,
    String = 3,
    List = 4,
};

struct ControlArg {
    ControlArgType type = ControlArgType::End;
    i32 intVal = 0;
    u8* bytes = nullptr;
    u32 bytesLen = 0;
    Str str;
    Vec<ControlArg*>* list = nullptr;
};

static void DeleteControlArg(ControlArg* arg) {
    if (!arg) {
        return;
    }
    free(arg->bytes);
    str::FreePtr(&arg->str);
    if (arg->list) {
        for (ControlArg* el : *arg->list) {
            DeleteControlArg(el);
        }
        delete arg->list;
    }
    delete arg;
}

struct ControlRequest {
    u16 cmd = 0;
    u16 reqId = 0;
    Vec<ControlArg*> args;
    str::Builder results;
    HANDLE done = nullptr;
};

static void DeleteControlRequest(ControlRequest* req) {
    if (!req) {
        return;
    }
    for (ControlArg* arg : req->args) {
        DeleteControlArg(arg);
    }
    SafeCloseHandle(&req->done);
    delete req;
}

struct PacketReader {
    const u8* data = nullptr;
    size_t size = 0;
    size_t pos = 0;

    bool ReadU16(u16& v) {
        if (pos + 2 > size) {
            return false;
        }
        v = (u16)(data[pos] | (data[pos + 1] << 8));
        pos += 2;
        return true;
    }

    bool ReadU32(u32& v) {
        if (pos + 4 > size) {
            return false;
        }
        v = (u32)data[pos] | ((u32)data[pos + 1] << 8) | ((u32)data[pos + 2] << 16) | ((u32)data[pos + 3] << 24);
        pos += 4;
        return true;
    }

    bool ReadBytes(u8* dst, size_t n) {
        if (pos + n > size) {
            return false;
        }
        memcpy(dst, data + pos, n);
        pos += n;
        return true;
    }
};

static void AppendU16(str::Builder& s, u16 v) {
    u8 buf[2] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff)};
    s.Append(Str((char*)(buf), (int)(sizeof(buf))));
}

static void AppendU32(str::Builder& s, u32 v) {
    u8 buf[4] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff), (u8)((v >> 16) & 0xff), (u8)((v >> 24) & 0xff)};
    s.Append(Str((char*)(buf), (int)(sizeof(buf))));
}

static void AppendArgEnd(str::Builder& s) {
    AppendU16(s, (u16)ControlArgType::End);
}

static void AppendArgInt(str::Builder& s, i32 v) {
    AppendU16(s, (u16)ControlArgType::Int32);
    AppendU32(s, (u32)v);
}

static void AppendArgString(str::Builder& s, Str str) {
    if (!str) {
        str = StrL("");
    }
    size_t n = (size_t)str.len;
    AppendU16(s, (u16)ControlArgType::String);
    AppendU32(s, (u32)n);
    s.Append(str);
    s.AppendChar(0);
}

static bool ParseArg(PacketReader& r, ControlArg** argOut);

static bool ParseArgList(PacketReader& r, Vec<ControlArg*>* args, bool explicitCount, u16 count = 0) {
    for (u16 i = 0; !explicitCount || i < count; i++) {
        ControlArg* arg = nullptr;
        if (!ParseArg(r, &arg)) {
            return false;
        }
        if (!arg) {
            return !explicitCount;
        }
        args->Append(arg);
    }
    return true;
}

static bool ParseArg(PacketReader& r, ControlArg** argOut) {
    u16 typeRaw = 0;
    if (!r.ReadU16(typeRaw)) {
        return false;
    }
    ControlArgType type = (ControlArgType)typeRaw;
    if (type == ControlArgType::End) {
        *argOut = nullptr;
        return true;
    }

    ControlArg* arg = new ControlArg();
    arg->type = type;
    if (type == ControlArgType::Int32) {
        u32 v = 0;
        if (!r.ReadU32(v)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->intVal = (i32)v;
    } else if (type == ControlArgType::Bytes) {
        u32 n = 0;
        if (!r.ReadU32(n)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->bytes = AllocArray<u8>((int)n + 1);
        arg->bytesLen = n;
        if (!r.ReadBytes(arg->bytes, n)) {
            DeleteControlArg(arg);
            return false;
        }
    } else if (type == ControlArgType::String) {
        u32 n = 0;
        if (!r.ReadU32(n)) {
            DeleteControlArg(arg);
            return false;
        }
        char* strBuf = AllocArray<char>((int)n + 1);
        if (!r.ReadBytes((u8*)strBuf, n)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->str = Str(strBuf, (int)n);
        u8 zero = 1;
        if (!r.ReadBytes(&zero, 1) || zero != 0) {
            DeleteControlArg(arg);
            return false;
        }
    } else if (type == ControlArgType::List) {
        u16 count = 0;
        if (!r.ReadU16(count)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->list = new Vec<ControlArg*>();
        if (!ParseArgList(r, arg->list, true, count)) {
            DeleteControlArg(arg);
            return false;
        }
    } else {
        DeleteControlArg(arg);
        return false;
    }
    *argOut = arg;
    return true;
}

static ControlArg* ArgAt(ControlRequest* req, size_t idx, ControlArgType type) {
    if (idx >= (size_t)len(req->args)) {
        return nullptr;
    }
    ControlArg* arg = req->args[(int)idx];
    if (arg->type != type) {
        return nullptr;
    }
    return arg;
}

static Str StringArg(ControlRequest* req, size_t idx) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::String);
    return arg ? arg->str : Str{};
}

static bool IntArg(ControlRequest* req, size_t idx, i32& valOut) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::Int32);
    if (!arg) {
        return false;
    }
    valOut = arg->intVal;
    return true;
}

static bool FloatArg(ControlRequest* req, size_t idx, float& valOut) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::String);
    if (!arg || !arg->str) {
        return false;
    }
    // parse float from string
    char* end = nullptr;
    double d = strtod(arg->str.s, &end);
    if (end == arg->str.s || *end != '\0') {
        return false;
    }
    valOut = (float)d;
    return true;
}

static void AppendError(ControlRequest* req, Str msg) {
    req->results.Reset();
    AppendArgInt(req->results, -1);
    AppendArgString(req->results, msg);
    AppendArgEnd(req->results);
}

static void AppendTestResult(ControlRequest* req, int exitCode, Str result) {
    AppendArgInt(req->results, exitCode);
    AppendArgString(req->results, result);
    AppendArgEnd(req->results);
}

// Cross-thread marshal for Test* control-pipe handlers that touch UI HWNDs.
// TestToolbarButtons ran on the background control-pipe thread and issued
// TB_/TTM_* SendMessageW calls against comctl toolbars — a cross-thread race
// (LOG s36-az: WndProcToolbar fallthrough -> comctl32 write to dangling ptr;
// inter-thread SendMessage delivered mid-pump during comctl rebanding). This
// sends a synchronous message to the frame WndProc so fn(ctx) runs on the UI
// thread; the control thread blocks during the tiny call (acceptable, responses
// are small). Same-thread SendMessage on the UI thread = direct call, so the
// TB_/TTM_ calls inside ResultTemp are safe (no deadlock). The client already
// has timeouts (cmd/control.ts ControlClient.connect default 10s), so no
// server-side deadline guard is needed.
// NOTE: WM_APP + 0x430 is handled by a matching case in WndProcSumatraFrame
// (src/SumatraPDF.cpp) — keep the two values in sync.
static const UINT kUiThreadTestExecMsg = WM_APP + 0x430;

static int RunOnUIThread(MainWindow* win, int (*fn)(void* ctx), void* ctx) {
    if (!win || !win->hwndFrame || !fn) {
        return -1;
    }
    return (int)SendMessageW(win->hwndFrame, kUiThreadTestExecMsg, (WPARAM)fn, (LPARAM)ctx);
}

// Shuttled between control-pipe thread and UI thread for a marshalled handler.
// fn fills result + exitCode; RunOnUIThread returns exitCode to the caller.
struct UiTestCtx {
    Str result;
    int exitCode = -1;
};

static int ToolbarButtonsOnUiThread(void* p) {
    auto* c = (UiTestCtx*)p;
    c->result = ToolbarButtonsResultTemp(&c->exitCode);
    return c->exitCode;
}

// walk a TocItem chain and append "title|page=N|depth=D\n" lines (recursive).
static void AppendBookmarkToc(str::Builder& out, TocItem* item, int depth = 0) {
    for (; item; item = item->next) {
        if (item->title) {
            out.Append(fmt("%s|page=%d|depth=%d\n", item->title, item->pageNo, depth));
        }
        AppendBookmarkToc(out, item->child, depth + 1);
    }
}

static void ExecuteControlRequest(ControlRequest* req) {
    switch ((ControlCmd)req->cmd) {
        case ControlCmd::Ping:
            AppendArgString(req->results, "pong");
            AppendArgEnd(req->results);
            break;

        case ControlCmd::Quit:
            AppendArgInt(req->results, 0);
            AppendArgEnd(req->results);
            PostAppExit();
            break;

        case ControlCmd::TestSynctex: {
            i32 line = 0;
            Str pdf = StringArg(req, 0);
            Str src = StringArg(req, 1);
            if (!pdf || !src || !IntArg(req, 2, line)) {
                AppendError(req, "TestSynctex expects string pdf, string source, int line");
                break;
            }
            AppendTestResult(req, 0, SynctexResultTemp(pdf, src, line));
            break;
        }

        case ControlCmd::TestInverseSearch: {
            i32 page = 0, x = 0, y = 0;
            Str pdf = StringArg(req, 0);
            if (!pdf || !IntArg(req, 1, page) || !IntArg(req, 2, x) || !IntArg(req, 3, y)) {
                AppendError(req, "TestInverseSearch expects string pdf, int page, int x, int y");
                break;
            }
            AppendTestResult(req, 0, InverseSearchResultTemp(pdf, page, x, y));
            break;
        }

        case ControlCmd::TestSearch: {
            Str pdf = StringArg(req, 0);
            Str needle = StringArg(req, 1);
            Str password = StringArg(req, 2);
            if (!pdf || !needle) {
                AppendError(req, "TestSearch expects string pdf, string needle, optional string password");
                break;
            }
            if (!password && gCli) {
                password = gCli->password;
            }
            AppendTestResult(req, 0, SearchResultTemp(pdf, needle, password));
            break;
        }

        case ControlCmd::TestDest: {
            i32 destNo = 0;
            Str pdf = StringArg(req, 0);
            if (!pdf || !IntArg(req, 1, destNo)) {
                AppendError(req, "TestDest expects string pdf, int destinationNumber");
                break;
            }
            AppendTestResult(req, 0, DestResultTemp(pdf, destNo));
            break;
        }

        case ControlCmd::TestNamedDest: {
            Str pdf = StringArg(req, 0);
            Str name = StringArg(req, 1);
            if (!pdf || !name) {
                AppendError(req, "TestNamedDest expects string pdf, string name");
                break;
            }
            AppendTestResult(req, 0, NamedDestResultTemp(pdf, name));
            break;
        }

        case ControlCmd::TestChm: {
            Str chm = StringArg(req, 0);
            if (!chm) {
                AppendError(req, "TestChm expects string chmPath");
                break;
            }
            int exitCode = 0;
            Str res = ChmResultTemp(chm, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectionTranslate: {
            i32 backend = 0;
            Str srcLang = StringArg(req, 1);
            Str dstLang = StringArg(req, 2);
            Str text = StringArg(req, 3);
            if (!IntArg(req, 0, backend) || !srcLang || !dstLang || !text) {
                AppendError(req,
                            "TestSelectionTranslate expects int backend, string srcLang, string dstLang, string text");
                break;
            }
            int exitCode = 0;
            Str res = SelectionTranslateResultTemp(backend, srcLang, dstLang, text, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTripleClickLineSelect: {
            Str pdf = StringArg(req, 0);
            Str clickWord = StringArg(req, 1);
            Str expectedLine = StringArg(req, 2);
            if (!pdf || !clickWord || !expectedLine) {
                AppendError(req, "TestTripleClickLineSelect expects string pdf, string clickWord, string expectedLine");
                break;
            }
            int exitCode = 0;
            Str res = TripleClickLineSelectResultTemp(pdf, clickWord, expectedLine, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestContextMenuSelection: {
            Str word1 = StringArg(req, 0);
            Str word2 = StringArg(req, 1);
            Str cursorWord = StringArg(req, 2);
            if (!word1 || !word2 || !cursorWord) {
                AppendError(req, "TestContextMenuSelection expects string word1, string word2, string cursorWord");
                break;
            }
            int exitCode = 0;
            Str res = ContextMenuSelectionResultTemp(word1, word2, cursorWord, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGoToFindMatch: {
            Str word = StringArg(req, 0);
            Str typed = StringArg(req, 1);
            if (!word || !typed) {
                AppendError(req, "TestGoToFindMatch expects string word, string typed");
                break;
            }
            int exitCode = 0;
            Str res = GoToFindMatchResultTemp(word, typed, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestImageResizeArrowKey: {
            Str imagePath = StringArg(req, 0);
            if (!imagePath) {
                AppendError(req, "TestImageResizeArrowKey expects string imagePath");
                break;
            }
            int exitCode = 0;
            Str res = ImageResizeArrowKeyResultTemp(imagePath, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestClickClearsSelection: {
            Str word = StringArg(req, 0);
            if (!word) {
                AppendError(req, "TestClickClearsSelection expects string word");
                break;
            }
            int exitCode = 0;
            Str res = ClickClearsSelectionResultTemp(word, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestRectSelectionDrag: {
            Str word = StringArg(req, 0);
            if (!word) {
                AppendError(req, "TestRectSelectionDrag expects string word");
                break;
            }
            int exitCode = 0;
            Str res = RectSelectionDragResultTemp(word, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindResultsOrder: {
            Str term = StringArg(req, 0);
            if (!term) {
                AppendError(req, "TestFindResultsOrder expects string term, int startPage");
                break;
            }
            i32 startPage = 0;
            IntArg(req, 1, startPage);
            int exitCode = 0;
            Str res = FindResultsOrderResultTemp(term, startPage, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindResultPageColumnClip: {
            int exitCode = 0;
            Str res = FindResultPageColumnClipResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFileKind: {
            Str path = StringArg(req, 0);
            Str expectedKind = StringArg(req, 1);
            if (!path || !expectedKind) {
                AppendError(req, "TestFileKind expects string path, string expectedKind");
                break;
            }
            int exitCode = 0;
            Str res = FileKindResultTemp(path, expectedKind, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestScrollToLink: {
            i32 minDelta = 50;
            IntArg(req, 0, minDelta);
            int exitCode = 0;
            Str res = ScrollToLinkResultTemp(minDelta, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestI18nErrorString: {
            int exitCode = 0;
            Str res = I18nErrorStringResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageInfoOverlay: {
            Str pathTwo = StringArg(req, 0);
            Str pathOne = StringArg(req, 1);
            if (!pathTwo || !pathOne) {
                AppendError(req, "TestPageInfoOverlay expects string pathTwoPages, string pathOnePage");
                break;
            }
            int exitCode = 0;
            Str res = PageInfoOverlayResultTemp(pathTwo, pathOne, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGetToc: {
            Str path = StringArg(req, 0);
            if (!path) {
                AppendError(req, "TestGetToc expects string path");
                break;
            }
            int exitCode = 0;
            Str res = GetTocResultTemp(path, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageLinks: {
            Str path = StringArg(req, 0);
            i32 pageNo = 1;
            if (!path || !IntArg(req, 1, pageNo)) {
                AppendError(req, "TestPageLinks expects string path, int pageNo");
                break;
            }
            int exitCode = 0;
            Str res = PageLinksResultTemp(path, pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestWindowStateDuringLoad: {
            int exitCode = 0;
            Str res = WindowStateDuringLoadResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTocNavigate: {
            i32 destNo = 1;
            if (!IntArg(req, 0, destNo)) {
                AppendError(req, "TestTocNavigate expects int destNo (1-based)");
                break;
            }
            int exitCode = 0;
            Str res = TocNavigateResultTemp(destNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMarkdownTocNavigate: {
            i32 destNo = 0;
            i32 minScrollY = 1;
            if (!IntArg(req, 0, destNo) || !IntArg(req, 1, minScrollY)) {
                AppendError(req, "TestMarkdownTocNavigate expects int destNo, int minScrollY");
                break;
            }
            int exitCode = 0;
            Str res = MarkdownTocNavigateResultTemp(destNo, minScrollY, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMarkdownFollowLink: {
            Str href = StringArg(req, 0);
            i32 follow = 0;
            if (!IntArg(req, 1, follow)) {
                AppendError(req, "TestMarkdownFollowLink expects string href, int follow");
                break;
            }
            int exitCode = 0;
            Str res = MarkdownFollowLinkResultTemp(href, follow != 0, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestHomeListRows: {
            int exitCode = 0;
            Str res = HomeListRowsResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFavoriteNav: {
            Str action = StringArg(req, 0);
            i32 pageNo = 0;
            IntArg(req, 1, pageNo); // optional for next/prev/page
            if (!action) {
                AppendError(req, "TestFavoriteNav expects string action [, int pageNo]");
                break;
            }
            int exitCode = 0;
            Str res = FavoriteNavResultTemp(action, pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestToolbarButtons: {
            UiTestCtx ctx;
            MainWindow* win = len(gWindows) == 0 ? nullptr : gWindows[0];
            int code = RunOnUIThread(win, ToolbarButtonsOnUiThread, &ctx);
            Str res = ctx.result.len ? ctx.result : StrL("ERROR no-frame\n");
            if (code < 0) {
                code = 1;
                res = StrL("ERROR no-frame\n");
            }
            AppendTestResult(req, code, res);
            break;
        }

        case ControlCmd::TestContextMenu: {
            i32 x = 0, y = 0;
            IntArg(req, 0, x);
            IntArg(req, 1, y);
            int exitCode = 0;
            Str res = ContextMenuResultTemp(x, y, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestStatsToggleGraph: {
            int exitCode = 0;
            Str res = StatsToggleGraphResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestCvtRoundTrip: {
            int zoomPercent = 100;
            int sampleCount = 8;
            int seed = 42;
            IntArg(req, 0, zoomPercent);
            IntArg(req, 1, sampleCount);
            IntArg(req, 2, seed);
            int exitCode = 0;
            Str res = CvtRoundTripResultTemp(zoomPercent, sampleCount, seed, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestVirtWndHitTest: {
            int samplesPerWnd = 8;
            int seed = 42;
            IntArg(req, 0, samplesPerWnd);
            IntArg(req, 1, seed);
            int exitCode = 0;
            Str res = VirtWndHitTestResultTemp(samplesPerWnd, seed, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestKeyboardLinkFollow: {
            int exitCode = 0;
            Str res = KeyboardLinkFollowResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectTextKeyboard: {
            int exitCode = 0;
            Str res = SelectTextKeyboardResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAIChat: {
            i32 backend = 0;
            Str filePath = StringArg(req, 1);
            Str message = StringArg(req, 2);
            if (!IntArg(req, 0, backend) || !filePath || !message) {
                AppendError(req, "TestAIChat expects int backend, string filePath, string message");
                break;
            }
            int exitCode = 0;
            Str res = AIChatTestResultTemp(backend, filePath, message, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAIChatReplay: {
            Str userMsg = StringArg(req, 0);
            Str response = StringArg(req, 1);
            if (!userMsg || !response) {
                AppendError(req, "TestAIChatReplay expects string userMsg, string response");
                break;
            }
            int exitCode = 0;
            Str res = AIChatTestReplayResultTemp(userMsg, response, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTabGroupActivate: {
            Str name = StringArg(req, 0);
            int exitCode = 0;
            Str res = TabGroupActivateResultTemp(name, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTabGroupList: {
            int exitCode = 0;
            Str res = TabGroupListResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTabGroupNewSession: {
            Str name = StringArg(req, 0);
            int exitCode = 0;
            Str res = TabGroupNewSessionResultTemp(name, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTabGroupOpenNew: {
            Str name = StringArg(req, 0);
            int exitCode = 0;
            Str res = TabGroupOpenNewResultTemp(name, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTabGroupMenuCmd: {
            Str cmdStr = StringArg(req, 0);
            int exitCode = 0;
            Str res = TabGroupMenuCmdResultTemp(cmdStr, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTabGroupDialogOpen: {
            Str name = StringArg(req, 0);
            int exitCode = 0;
            Str res = TabGroupDialogOpenResultTemp(name, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMarkdownAdapter: {
            // Exercise the Markdown adapter chain: locate the first MainWindow
            // whose doc controller is a MarkdownModel with a live BrowserDocView,
            // then run a read-only, no-arg adapter query (GetScrollPos) off it.
            // If no markdown view is open, return an error marker shaped like
            // neighboring test handlers (AppendError => i32 -1 + message).
            MarkdownModel* mm = nullptr;
            for (MainWindow* w : gWindows) {
                if (w && w->ctrl) {
                    mm = w->ctrl->AsMarkdown();
                    if (mm && mm->docView) {
                        break;
                    }
                    mm = nullptr;
                }
            }
            if (!mm || !mm->docView) {
                AppendError(req, "TestMarkdownAdapter no-markdown-view");
                break;
            }
            Point p = mm->docView->GetScrollPos();
            logf("[md] adapter ok scroll=%d,%d\n", p.x, p.y);
            Str res = fmt("OK markdown-adapter scroll=%d,%d\n", p.x, p.y);
            AppendTestResult(req, 0, res);
            break;
        }

        case ControlCmd::TestProfileImportPrompt: {
            // Dev-only: force-launch the profile import prompt, bypassing the
            // -for-testing skip in SyncWndProc. Runs asynchronously on the UI
            // thread so the control pipe returns immediately; the dialog stays
            // open until dismissed (or until the app quits).
            if (len(gWindows) == 0) {
                AppendError(req, "TestProfileImportPrompt no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            uitask::Post(MkFunc0(TestLaunchImportPrompt, win), "TestProfileImportPrompt");
            AppendTestResult(req, 0, StrL("OK posted\n"));
            break;
        }

        case ControlCmd::TestProfileCloudRevert: {
            // Dev-only: force-launch the cloud-revert prompt with fabricated
            // profile data under a temp scratch dir (appdata override).
            // Creates primary + one backup slot with backupTs > primaryTs so
            // the dialog renders a real revert scenario.
            if (len(gWindows) == 0) {
                AppendError(req, "TestProfileCloudRevert no-window");
                break;
            }
            MainWindow* win = gWindows[0];

            // Build a temp appdata dir: <tmp>/s40v-cloudrevert/
            TempStr tmpRoot = GetTempFilePathTemp(StrL("s40v-cloudrevert"));
            dir::CreateAll(Str(tmpRoot));
            TempStr primaryPath = path::JoinTemp(Str(tmpRoot), StrL("tumatraPDF-data_1.json"));
            TempStr backupDir = path::JoinTemp(Str(tmpRoot), StrL("backup"));
            TempStr slotDir = path::JoinTemp(Str(backupDir), StrL("00"));
            dir::CreateAll(Str(slotDir));
            TempStr slotPath = path::JoinTemp(Str(slotDir), StrL("tumatraPDF-data_1.json"));

            // primary: older timestamp
            file::WriteFile(Str(primaryPath), StrL("{\"lastWriteTs\": 1000}"));
            // backup slot 0: newer timestamp -> triggers cloud revert
            file::WriteFile(Str(slotPath), StrL("{\"lastWriteTs\": 2000}"));

            // Override the profile data dir so reads go to our scratch dir.
            str::FreePtr(&gGlobalPrefs->profileDataDir);
            gGlobalPrefs->profileDataDir = str::Dup(tmpRoot);

            // Launch asynchronously on the UI thread.
            uitask::Post(MkFunc0(TestLaunchCloudRevert, win), "TestProfileCloudRevert");
            AppendTestResult(req, 0, StrL("OK posted\n"));
            break;
        }

        case ControlCmd::TestProfileSetFolder: {
            i32 exitCode = 0;
            Str path = StringArg(req, 0);
            if (!path) {
                AppendError(req, "TestProfileSetFolder expects string folder path");
                break;
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestProfileSetFolder no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            if (!win) {
                AppendError(req, "TestProfileSetFolder no-window");
                break;
            }
            bool ok = ProfileSyncSetFolderToPath(win, CStrTemp(path));
            exitCode = ok ? 1 : 0;
            if (!ok) {
                AppendError(req, StrL("ProfileSyncSetFolderToPath failed"));
            }
            AppendTestResult(req, exitCode, ok ? StrL("OK\n") : StrL("FAILED\n"));
            break;
        }

        case ControlCmd::TestProfileExport: {
            i32 exitCode = 0;
            i32 overwrite = 0;
            Str destDir = StringArg(req, 0);
            if (!destDir) {
                AppendError(req, "TestProfileExport expects string destDir and int overwrite");
                break;
            }
            if (!IntArg(req, 1, overwrite)) {
                AppendError(req, "TestProfileExport expects int overwrite (0/1)");
                break;
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestProfileExport no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            if (!win) {
                AppendError(req, "TestProfileExport no-window");
                break;
            }
            bool ok = ProfileSyncExportToPath(win, CStrTemp(destDir), overwrite != 0);
            exitCode = ok ? 1 : 0;
            if (!ok) {
                AppendError(req, StrL("ProfileSyncExportToPath failed"));
            }
            AppendTestResult(req, exitCode, ok ? StrL("OK\n") : StrL("FAILED\n"));
            break;
        }

        case ControlCmd::TestProfileImport: {
            i32 exitCode = 0;
            i32 replace = 0;
            Str srcPath = StringArg(req, 0);
            if (!srcPath) {
                AppendError(req, "TestProfileImport expects string srcPath and int replace");
                break;
            }
            if (!IntArg(req, 1, replace)) {
                AppendError(req, "TestProfileImport expects int replace (0/1)");
                break;
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestProfileImport no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            if (!win) {
                AppendError(req, "TestProfileImport no-window");
                break;
            }
            bool ok = ProfileSyncImportFromPath(win, CStrTemp(srcPath), replace != 0);
            exitCode = ok ? 1 : 0;
            if (!ok) {
                AppendError(req, StrL("ProfileSyncImportFromPath failed"));
            }
            AppendTestResult(req, exitCode, ok ? StrL("OK\n") : StrL("FAILED\n"));
            break;
        }

        case ControlCmd::TestProfileStatus: {
            i32 exitCode = 0;
            i32 version = 0;
            i32 books = 0;
            i32 readingDays = 0;
            i32 studyDays = 0;
            i64 lastWriteTs = 0;

            if (len(gWindows) == 0) {
                AppendError(req, "TestProfileStatus no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            if (!win) {
                AppendError(req, "TestProfileStatus no-window");
                break;
            }

            // Get profile data root path
            TempStr rootPathTemp = ProfileDataGetDataRootTemp();
            Str rootPath = rootPathTemp ? rootPathTemp : StrL("");

            // Get migration status
            TempStr migrationStatusTemp = ProfileDataLastMigrationStatusTemp();
            Str migrationStatus = migrationStatusTemp ? migrationStatusTemp : StrL("");

            // Get stats
            if (!ProfileDataStats(&version, &books, &readingDays, &studyDays, &lastWriteTs)) {
                version = 0;
                books = 0;
                readingDays = 0;
                studyDays = 0;
                lastWriteTs = 0;
            }

            // Build response list: [rootPath, migrationStatus, version, books, readingDays, studyDays, lastWriteTs]
            // Encode lastWriteTs as utf8 string since protocol has no i64 type
            str::Builder listBuilder;
            // List type + count (7 elements)
            AppendU16(listBuilder, (u16)ControlArgType::List);
            AppendU16(listBuilder, (u16)7);

            // Element 0: rootPath (utf8 string)
            AppendU16(listBuilder, (u16)ControlArgType::String);
            AppendU32(listBuilder, (u32)rootPath.len);
            listBuilder.Append(rootPath);
            listBuilder.AppendChar(0);

            // Element 1: migrationStatus (utf8 string)
            AppendU16(listBuilder, (u16)ControlArgType::String);
            AppendU32(listBuilder, (u32)migrationStatus.len);
            listBuilder.Append(migrationStatus);
            listBuilder.AppendChar(0);

            // Element 2: version (i32)
            AppendU16(listBuilder, (u16)ControlArgType::Int32);
            AppendU32(listBuilder, (u32)version);

            // Element 3: books (i32)
            AppendU16(listBuilder, (u16)ControlArgType::Int32);
            AppendU32(listBuilder, (u32)books);

            // Element 4: readingDays (i32)
            AppendU16(listBuilder, (u16)ControlArgType::Int32);
            AppendU32(listBuilder, (u32)readingDays);

            // Element 5: studyDays (i32)
            AppendU16(listBuilder, (u16)ControlArgType::Int32);
            AppendU32(listBuilder, (u32)studyDays);

            // Element 6: lastWriteTs (i64 encoded as utf8 string)
            char tsBuf[64];
            int tsLen = snprintf(tsBuf, sizeof(tsBuf), "%lld", (long long)lastWriteTs);
            AppendU16(listBuilder, (u16)ControlArgType::String);
            AppendU32(listBuilder, (u32)tsLen);
            for (int i = 0; i < tsLen; i++) {
                listBuilder.AppendChar(tsBuf[i]);
            }
            listBuilder.AppendChar(0);

            // List end marker
            AppendU16(listBuilder, (u16)ControlArgType::End);

            AppendTestResult(req, exitCode, ToStrTemp(listBuilder));
            break;
        }

        case ControlCmd::TestCommandEnabled: {
            i32 cmdId = 0;
            if (!IntArg(req, 0, cmdId)) {
                AppendError(req, "TestCommandEnabled expects int cmdId");
                break;
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestCommandEnabled no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            bool enabled = IsCmdEnabled(win, cmdId);
            AppendTestResult(req, 0, fmt("OK cmdId=%d enabled=%d\n", cmdId, enabled ? 1 : 0));
            break;
        }

        case ControlCmd::TestGetEngineErrors: {
            // Returns [exitCode = error count, error text] for the current
            // tab's engine, so -dbg-control tests can assert damage was
            // detected and that a repair cleared it.
            int count = -1;
            Str text;
            if (len(gWindows) > 0) {
                MainWindow* win = gWindows[0];
                if (auto* tab = win->CurrentTab()) {
                    if (auto* engine = tab->GetEngine()) {
                        count = engine->ErrorCount();
                        text = engine->GetErrorsTextTemp();
                    }
                }
            }
            AppendTestResult(req, count, text);
            break;
        }

        case ControlCmd::TestBookmarkAdd: {
            i32 pageNo = 0;
            Str title = StringArg(req, 1);
            if (!IntArg(req, 0, pageNo) || !title) {
                AppendError(req, "TestBookmarkAdd expects int pageNo, string title");
                break;
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestBookmarkAdd no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            auto* engine = win->CurrentTab() ? win->CurrentTab()->GetEngine() : nullptr;
            if (!engine || !engine->OutlineEdit(1, 0, StrL(""), title, pageNo)) {
                AppendError(req, "TestBookmarkAdd failed");
                break;
            }
            AppendTestResult(req, 0, StrL("OK\n"));
            break;
        }

        case ControlCmd::TestBookmarkDirty: {
            int dirty = 0;
            if (len(gWindows) > 0) {
                MainWindow* win = gWindows[0];
                if (auto* tab = win->CurrentTab()) {
                    if (auto* engine = tab->GetEngine()) {
                        dirty = engine->HasUnsavedOutline() ? 1 : 0;
                    }
                }
            }
            AppendTestResult(req, dirty, dirty ? StrL("dirty\n") : StrL("clean\n"));
            break;
        }

        case ControlCmd::TestBookmarkToc: {
            str::Builder out;
            int n = 0;
            if (len(gWindows) > 0) {
                MainWindow* win = gWindows[0];
                if (auto* tab = win->CurrentTab()) {
                    if (auto* engine = tab->GetEngine()) {
                        if (auto* toc = engine->GetToc()) {
                            auto* r = (TocItem*)toc->Root();
                            n = (r ? 1 : 0);
                            AppendBookmarkToc(out, r ? r->child : nullptr);
                        } else {
                            out.Append("toc-null\n");
                        }
                    } else {
                        out.Append("engine-null\n");
                    }
                } else {
                    out.Append("tab-null\n");
                }
            } else {
                out.Append("nowindow\n");
            }
            out.Append(fmt("count=%d\n", n));
            AppendTestResult(req, 0, ToStrTemp(out));
            break;
        }

        case ControlCmd::TestBookmarkSave: {
            if (len(gWindows) == 0) {
                AppendError(req, "TestBookmarkSave no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            auto* engine = win->CurrentTab() ? win->CurrentTab()->GetEngine() : nullptr;
            if (!engine) {
                AppendError(req, "TestBookmarkSave no-engine");
                break;
            }
            bool ok = EngineMupdfSaveUpdated(engine, nullptr, Func1<Str>{}, nullptr, false);
            AppendTestResult(req, ok ? 0 : 1, ok ? StrL("saved\n") : StrL("save-failed\n"));
            break;
        }

        case ControlCmd::TestBookmarkDelete: {
            i32 pageNo = 0;
            ControlArg* titleArg = ArgAt(req, 1, ControlArgType::String);
            if (!IntArg(req, 0, pageNo) || !titleArg) {
                AppendError(req, "TestBookmarkDelete expects int pageNo, string title (empty allowed)");
                break;
            }
            Str title = titleArg->str; // may be empty: the ghost entry (pre-s60 corruption class)
            if (len(gWindows) == 0) {
                AppendError(req, "TestBookmarkDelete no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            auto* engine = win->CurrentTab() ? win->CurrentTab()->GetEngine() : nullptr;
            if (!engine || !engine->OutlineEdit(2, pageNo, title, StrL(""), 0)) {
                AppendError(req, "TestBookmarkDelete failed");
                break;
            }
            AppendTestResult(req, 0, StrL("OK\n"));
            break;
        }

        case ControlCmd::TestRunCommand: {
            // Execute a command by ID on the focused window's frame (same as toolbar/menu)
            // Request: int32 commandId
            // Response: int32 ok/result (1 = dispatched, 0 = error)
            i32 cmdId = 0;
            if (!IntArg(req, 0, cmdId)) {
                AppendError(req, "TestRunCommand expects int commandId");
                break;
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestRunCommand no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            if (!win || !win->hwndFrame) {
                AppendError(req, "TestRunCommand no-frame");
                break;
            }
            // Post to UI thread via SendMessage (same as toolbar buttons do)
            SendMessageW(win->hwndFrame, WM_COMMAND, (WPARAM)cmdId, 0);
            AppendTestResult(req, 1, StrL("OK\n"));
            break;
        }

        case ControlCmd::TestPageHold: {
            // Toggle Page Hold and return state
            // Returns list: [active, virtualPage, physAnchor, x, y, zoomTimes100, tc2Flag]
            if (len(gWindows) == 0) {
                AppendError(req, "TestPageHold no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            if (!win || !win->ctrl) {
                AppendError(req, "TestPageHold no-doc");
                break;
            }
            DisplayModel* dm = win->AsFixed();
            if (!dm) {
                AppendError(req, "TestPageHold not-fixed-layout");
                break;
            }
            // Toggle the hold (same as CmdPageHoldToggle handler)
            if (dm->pageHold.active) {
                dm->PageHoldRestore();
                dm->pageHold.active = false;
            } else {
                dm->PageHoldCapture(win->hwndCanvas);
                dm->pageHold.active = dm->pageHold.valid;
            }
            SetToolbarButtonCheckedState(win, CmdPageHoldToggle, dm->pageHold.active);
            HwndRepaintNow(win->hwndCanvas);

            // Build response list: [active, virtualPage, physAnchor, x, y, zoomTimes100, tc2Flag]
            ScrollState ss = dm->GetScrollState();
            int active = dm->pageHold.active ? 1 : 0;
            int virtPage = ss.page;
            int physAnchor = dm->pageHold.physAnchor;
            int x = ss.x;
            int y = ss.y;
            int zoomTimes100 = (int)(dm->zoomVirtual * 100.0f + 0.5f);
            int tc2Flag = dm->IsViewportCropV2Active() ? 1 : 0;

            // Manually encode list: type=List(4), count=7, then 7 Int32 elements
            AppendU16(req->results, (u16)ControlArgType::List);
            AppendU16(req->results, 7);
            AppendArgInt(req->results, active);
            AppendArgInt(req->results, virtPage);
            AppendArgInt(req->results, physAnchor);
            AppendArgInt(req->results, x);
            AppendArgInt(req->results, y);
            AppendArgInt(req->results, zoomTimes100);
            AppendArgInt(req->results, tc2Flag);
            AppendArgEnd(req->results);
            break;
        }

        case ControlCmd::TestGetZoom: {
            // Returns current zoom * 100 (no toggle, no state change)
            if (len(gWindows) == 0) {
                AppendError(req, "TestGetZoom no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            if (!win || !win->ctrl) {
                AppendError(req, "TestGetZoom no-doc");
                break;
            }
            DisplayModel* dm = win->AsFixed();
            if (!dm) {
                AppendError(req, "TestGetZoom not-fixed-layout");
                break;
            }
            int zoomTimes100 = (int)(dm->zoomVirtual * 100.0f + 0.5f);
            AppendArgInt(req->results, zoomTimes100);
            AppendArgEnd(req->results);
            break;
        }

        case ControlCmd::TestNewWindow: {
            // Dispatch CmdNewWindow and return the number of open windows
            if (len(gWindows) == 0) {
                AppendError(req, "TestNewWindow no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            SendMessageW(win->hwndFrame, WM_COMMAND, CmdNewWindow, 0);
            // Wait a bit for the window to be created
            Sleep(200);
            int numWindows = len(gWindows);
            AppendArgInt(req->results, numWindows);
            AppendArgEnd(req->results);
            break;
        }

        case ControlCmd::TestWindowStats: {
            // Returns stats for gWindows[index]
            // Request: int32 index
            // Response: list [filePath, readingTimeSec, flashcardStudyTimeSec, readingStatsAccumSec,
            // flashcardStatsAccumSec]
            i32 index = 0;
            if (!IntArg(req, 0, index)) {
                AppendError(req, "TestWindowStats expects int index");
                break;
            }
            if (index < 0 || index >= len(gWindows)) {
                // Out of range: return zeros/empty
                AppendU16(req->results, (u16)ControlArgType::List);
                AppendU16(req->results, 5);
                AppendArgString(req->results, StrL(""));
                AppendArgInt(req->results, 0);
                AppendArgInt(req->results, 0);
                AppendArgInt(req->results, 0);
                AppendArgInt(req->results, 0);
                AppendArgEnd(req->results);
                break;
            }
            MainWindow* win = gWindows[index];
            Str filePath = StrL("");
            int readingTimeSec = 0;
            int flashcardStudyTimeSec = 0;
            int readingStatsAccumSec = 0;
            int flashcardStatsAccumSec = 0;

            if (win && win->ctrl) {
                WindowTab* tab = win->CurrentTab();
                if (tab && tab->filePath) {
                    filePath = tab->filePath;
                    FileState* fs = gFileHistory.FindByPath(tab->filePath);
                    if (fs) {
                        readingTimeSec = fs->readingTimeSec;
                        flashcardStudyTimeSec = fs->flashcardStudyTimeSec;
                    }
                }
                readingStatsAccumSec = (int)(win->readingStatsAccumSec + 0.5f);
                flashcardStatsAccumSec = (int)(win->flashcardStatsAccumSec + 0.5f);
            }

            // Manually encode list: type=List(4), count=5, then 5 elements (string + 4 ints)
            AppendU16(req->results, (u16)ControlArgType::List);
            AppendU16(req->results, 5);
            AppendArgString(req->results, filePath);
            AppendArgInt(req->results, readingTimeSec);
            AppendArgInt(req->results, flashcardStudyTimeSec);
            AppendArgInt(req->results, readingStatsAccumSec);
            AppendArgInt(req->results, flashcardStatsAccumSec);
            AppendArgEnd(req->results);
            break;
        }

        case ControlCmd::TestOpenInWindow: {
            // Open a file in a specific window
            // Request: int32 index, string filePath
            i32 index = 0;
            Str filePath = StringArg(req, 1);
            if (!IntArg(req, 0, index) || !filePath) {
                AppendError(req, "TestOpenInWindow expects int index, string filePath");
                break;
            }
            if (index < 0 || index >= len(gWindows)) {
                AppendError(req, "TestOpenInWindow index out of range");
                break;
            }
            MainWindow* win = gWindows[index];
            if (!win) {
                AppendError(req, "TestOpenInWindow no-window");
                break;
            }
            // Create LoadArgs for the target window
            LoadArgs* args = new LoadArgs(filePath, win);
            args->forceReuse = false;
            args->noPlaceWindow = false;
            // Use StartLoadDocument to load asynchronously on the UI thread
            StartLoadDocument(args);
            AppendArgInt(req->results, 1);
            AppendArgEnd(req->results);
            break;
        }

        case ControlCmd::TestFocusWindow: {
            // Focus/activate window by index
            // Request: int32 index
            // Response: int32 ok (1 = focused, 0 = error)
            i32 index = 0;
            if (!IntArg(req, 0, index)) {
                AppendError(req, "TestFocusWindow expects int index");
                break;
            }
            if (index < 0 || index >= len(gWindows)) {
                AppendError(req, "TestFocusWindow index out of range");
                break;
            }
            MainWindow* win = gWindows[index];
            if (!win || !win->hwndFrame) {
                AppendError(req, "TestFocusWindow no-frame");
                break;
            }
            SetForegroundWindow(win->hwndFrame);
            AppendArgInt(req->results, 1);
            AppendArgEnd(req->results);
            break;
        }

        case ControlCmd::TestRunCommandInWindow: {
            // Run a command by ID on a specific window's frame (unlike
            // TestRunCommand, which uses the focused window gWindows[0])
            // Request: int32 index, int32 commandId
            // Response: int32 ok/result (1 = dispatched, 0 = error)
            i32 index = 0;
            i32 cmdId = 0;
            if (!IntArg(req, 0, index) || !IntArg(req, 1, cmdId)) {
                AppendError(req, "TestRunCommandInWindow expects int index, int commandId");
                break;
            }
            if (index < 0 || index >= len(gWindows)) {
                AppendError(req, "TestRunCommandInWindow index out of range");
                break;
            }
            MainWindow* win = gWindows[index];
            if (!win || !win->hwndFrame) {
                AppendError(req, "TestRunCommandInWindow no-frame");
                break;
            }
            SendMessageW(win->hwndFrame, WM_COMMAND, (WPARAM)cmdId, 0);
            AppendArgInt(req->results, 1);
            AppendArgEnd(req->results);
            break;
        }

        case ControlCmd::TestBookmarkMove: {
            i32 sourcePageNo = 0;
            i32 newParentPageNo = 0;
            i32 moveBeforePageNo = 0;
            Str sourceTitle = StringArg(req, 1);
            Str newParentTitle = StringArg(req, 3);
            Str moveBeforeTitle = StringArg(req, 5);
            if (!IntArg(req, 0, sourcePageNo) || !sourceTitle || !IntArg(req, 2, newParentPageNo) ||
                !IntArg(req, 4, moveBeforePageNo)) {
                AppendError(
                    req,
                    "TestBookmarkMove expects int sourcePageNo, string sourceTitle, int newParentPageNo(-1=top), "
                    "string newParentTitle, int moveBeforePageNo, string moveBeforeTitle");
                break;
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestBookmarkMove no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            auto* engine = win->CurrentTab() ? win->CurrentTab()->GetEngine() : nullptr;
            if (!engine ||
                !engine->OutlineEdit(5, sourcePageNo, sourceTitle, newParentPageNo < 0 ? StrL("") : newParentTitle,
                                     newParentPageNo < 0 ? 0 : newParentPageNo, moveBeforePageNo, moveBeforeTitle)) {
                AppendError(req, "TestBookmarkMove failed");
                break;
            }
            AppendTestResult(req, 0, StrL("OK\n"));
            break;
        }

        case ControlCmd::TestGetPageNo: {
            int cur = 0;
            if (len(gWindows) > 0) {
                MainWindow* win = gWindows[0];
                if (win->ctrl) {
                    cur = win->ctrl->CurrentPageNo();
                }
            }
            AppendTestResult(req, cur, StrL(""));
            break;
        }

        case ControlCmd::TestTocToolbarButton: {
            // posts WM_COMMAND 9001 (kTocToolbarCmdReorder) to the TOC box so
            // WndProcTocBox -> TocReorderToggle toggles drag-reorder mode ON/OFF.
            // Runs async (PostMessage); the app replies immediately.
            if (len(gWindows) == 0) {
                AppendError(req, "TestTocToolbarButton no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            if (!win || !win->hwndTocBox) {
                AppendError(req, "TestTocToolbarButton no-tocbox");
                break;
            }
            PostMessageW(win->hwndTocBox, WM_COMMAND, MAKEWPARAM(9001, 0), 0);
            AppendTestResult(req, 0, StrL("OK posted\n"));
            break;
        }

        case ControlCmd::TestAlignQueue: {
            i32 pageNo = 0;
            float angleDeg = 0;
            float cropX = 0, cropY = 0, cropDx = 0, cropDy = 0;
            if (!IntArg(req, 0, pageNo) || !FloatArg(req, 1, angleDeg) || !FloatArg(req, 2, cropX) ||
                !FloatArg(req, 3, cropY) || !FloatArg(req, 4, cropDx) || !FloatArg(req, 5, cropDy)) {
                AppendError(req,
                            "TestAlignQueue expects int pageNo, float angleDeg, float cropX, float cropY, float "
                            "cropDx, float cropDy");
                break;
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestAlignQueue no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            auto* engine = win->CurrentTab() ? win->CurrentTab()->GetEngine() : nullptr;
            if (!engine) {
                AppendError(req, "TestAlignQueue no-engine");
                break;
            }
            RectF cropRect{cropX, cropY, cropDx, cropDy};
            if (!engine->QueuePageGeometry(pageNo, angleDeg, cropRect)) {
                AppendError(req, "TestAlignQueue failed (page may have annotations)");
                break;
            }
            // Also mark dirty for UI
            EngineMupdf* em = AsEngineMupdf(engine);
            if (em) {
                em->modifiedPageGeometry = true;
            }
            ToolbarUpdateStateForWindow(win, false);
            AppendTestResult(req, 0, StrL("OK\n"));
            break;
        }

        case ControlCmd::TestAlignApply: {
            // Real gated preview path: EngineMupdf::ApplyPreviewGeometry
            // (s64-R2). Stores previewGeomEdit for later promote/discard — does
            // NOT promote to pendingGeomEdits (that is TestAlignCommit's job).
            // Optional 7th arg: int rotatedCrop (0/1) for R9 rotated page crop.
            i32 pageNo = 0;
            float angleDeg = 0;
            float cropX = 0, cropY = 0, cropDx = 0, cropDy = 0;
            i32 rotatedCrop = 0;
            if (!IntArg(req, 0, pageNo) || !FloatArg(req, 1, angleDeg) || !FloatArg(req, 2, cropX) ||
                !FloatArg(req, 3, cropY) || !FloatArg(req, 4, cropDx) || !FloatArg(req, 5, cropDy)) {
                AppendError(req,
                            "TestAlignApply expects int pageNo, float angleDeg, float cropX, float cropY, float "
                            "cropDx, float cropDy [, int rotatedCrop]");
                break;
            }
            if (len(req->args) > 6 && !IntArg(req, 6, rotatedCrop)) {
                AppendError(req, "TestAlignApply rotatedCrop must be 0/1");
                break;
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestAlignApply no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            auto* engine = win->CurrentTab() ? win->CurrentTab()->GetEngine() : nullptr;
            if (!engine) {
                AppendError(req, "TestAlignApply no-engine");
                break;
            }
            EngineMupdf* em = AsEngineMupdf(engine);
            if (!em) {
                AppendError(req, "TestAlignApply not-pdf-engine");
                break;
            }
            DisplayModel* dm = win->AsFixed();
            RectF cropRect{cropX, cropY, cropDx, cropDy};
            bool applied = em->ApplyPreviewGeometry(pageNo, angleDeg, cropRect, dm, rotatedCrop != 0);
            RectF mb = engine->PageMediabox(pageNo);
            str::Builder sb;
            sb.Append(fmt("applied=%d\n", applied ? 1 : 0));
            sb.Append(fmt("mediabox=%.2f,%.2f,%.2f,%.2f\n", mb.x, mb.y, mb.dx, mb.dy));
            AppendTestResult(req, 0, sb.TakeStr());
            break;
        }

        case ControlCmd::TestAlignCommit: {
            // Promote preview to pending (commit for save). Optional 2nd arg:
            // outPath — if supplied, also save the live engine to that path.
            // With no outPath this is promote-only (requires a live preview).
            i32 pageNo = 0;
            if (!IntArg(req, 0, pageNo)) {
                AppendError(req, "TestAlignCommit expects int pageNo [, string outPath]");
                break;
            }
            Str outPath;
            if (len(req->args) > 1) {
                outPath = StringArg(req, 1);
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestAlignCommit no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            DisplayModel* dm = win->AsFixed();
            if (!dm) {
                AppendError(req, "TestAlignCommit no-displaymodel");
                break;
            }
            EngineMupdf* em = AsEngineMupdf(dm->GetEngine());
            if (!em) {
                AppendError(req, "TestAlignCommit not-pdf-engine");
                break;
            }
            bool promoted = false;
            if (em->hasPreviewGeomEdit && em->previewGeomEdit.pageNo == pageNo) {
                promoted = em->PromotePreviewToPending(pageNo);
                if (promoted) {
                    em->hasPreviewGeomEdit = false;
                    em->modifiedPageGeometry = true;
                    em->previewGeomEdit = EngineMupdf::PageGeomEdit{};
                }
            }
            if (outPath) {
                // Save the live engine (promoted edits, or pristine if none).
                // apply-without-commit leaves no pending edits -> saved file pristine.
                bool ok = EngineMupdfSaveUpdated(dm->GetEngine(), outPath, Func1<Str>{}, nullptr, false);
                str::Builder sb;
                sb.Append(fmt("saved=%d\n", ok ? 1 : 0));
                sb.Append(fmt("promoted=%d\n", promoted ? 1 : 0));
                AppendTestResult(req, ok ? 0 : 1, sb.TakeStr());
            } else {
                if (promoted) {
                    AppendTestResult(req, 0, StrL("committed\n"));
                } else {
                    AppendError(req, "TestAlignCommit no-preview-on-page");
                }
            }
            break;
        }

        case ControlCmd::TestAlignDiscard: {
            // Discard preview and restore pristine geometry
            i32 pageNo = 0;
            if (!IntArg(req, 0, pageNo)) {
                AppendError(req, "TestAlignDiscard expects int pageNo");
                break;
            }
            if (len(gWindows) == 0) {
                AppendError(req, "TestAlignDiscard no-window");
                break;
            }
            MainWindow* win = gWindows[0];
            DisplayModel* dm = win->AsFixed();
            if (!dm) {
                AppendError(req, "TestAlignDiscard no-displaymodel");
                break;
            }
            EngineMupdf* em = AsEngineMupdf(dm->GetEngine());
            if (!em || !em->hasPreviewGeomEdit || em->previewGeomEdit.pageNo != pageNo) {
                AppendError(req, "TestAlignDiscard no-preview-on-page");
                break;
            }
            if (em->DiscardPreview(pageNo, dm)) {
                AppendTestResult(req, 0, StrL("discarded\n"));
            } else {
                AppendError(req, "TestAlignDiscard discard-failed");
            }
            break;
        }

        case ControlCmd::TestGeomSave: {
            // Headless: open a PDF, queue a page-geometry edit (rotation + crop),
            // save to outPath (exercises EngineMupdf::ApplyPendingGeometryEdits
            // before pdf_save_document). No GUI window/engine needed.
            Str inPath = StringArg(req, 0);
            Str outPath = StringArg(req, 1);
            i32 pageNo = 0;
            float angleDeg = 0;
            float cropX = 0, cropY = 0, cropDx = 0, cropDy = 0;
            if (!inPath || !outPath || !IntArg(req, 2, pageNo) || !FloatArg(req, 3, angleDeg) ||
                !FloatArg(req, 4, cropX) || !FloatArg(req, 5, cropY) || !FloatArg(req, 6, cropDx) ||
                !FloatArg(req, 7, cropDy)) {
                AppendError(req,
                            "TestGeomSave expects string inPath, string outPath, int pageNo, float angleDeg, float "
                            "cropX, float cropY, float cropDx, float cropDy");
                break;
            }
            EngineBase* engine = CreateEngineFromFile(inPath, nullptr, false);
            if (!engine) {
                AppendError(req, "TestGeomSave engine-create-failed");
                break;
            }
            RectF cropRect{cropX, cropY, cropDx, cropDy};
            if (!engine->QueuePageGeometry(pageNo, angleDeg, cropRect)) {
                SafeEngineRelease(&engine);
                AppendError(req, "TestGeomSave queue-failed (page may have annotations)");
                break;
            }
            bool ok = EngineMupdfSaveUpdated(engine, outPath, Func1<Str>{}, nullptr, false);
            SafeEngineRelease(&engine);
            if (!ok) {
                AppendError(req, "TestGeomSave save-failed");
                break;
            }
            AppendTestResult(req, 0, StrL("OK\n"));
            break;
        }

        default:
            AppendError(req, "unknown control command");
            break;
    }
    SetEvent(req->done);
}

static bool ReadExact(HANDLE h, void* data, DWORD n) {
    u8* d = (u8*)data;
    DWORD total = 0;
    while (total < n) {
        DWORD nRead = 0;
        if (!ReadFile(h, d + total, n - total, &nRead, nullptr) || nRead == 0) {
            return false;
        }
        total += nRead;
    }
    return true;
}

static bool WriteExact(HANDLE h, Str data) {
    const u8* d = (const u8*)data.s;
    int total = 0;
    while (total < data.len) {
        DWORD nWritten = 0;
        if (!WriteFile(h, d + total, (DWORD)(data.len - total), &nWritten, nullptr) || nWritten == 0) {
            return false;
        }
        total += (int)nWritten;
    }
    return true;
}

static ControlRequest* ReadControlRequest(HANDLE h) {
    u32 size = 0;
    if (!ReadExact(h, &size, sizeof(size))) {
        return nullptr;
    }
    if (size < 4 || size > 16 * 1024 * 1024) {
        return nullptr;
    }
    u8* data = AllocArray<u8>((int)size);
    if (!ReadExact(h, data, size)) {
        free(data);
        return nullptr;
    }

    PacketReader r{data, size};
    ControlRequest* req = new ControlRequest();
    if (!r.ReadU16(req->cmd) || !r.ReadU16(req->reqId) || !ParseArgList(r, &req->args, false)) {
        DeleteControlRequest(req);
        free(data);
        return nullptr;
    }
    req->done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    free(data);
    return req;
}

static bool WriteControlResponse(HANDLE h, ControlRequest* req) {
    str::Builder payload;
    AppendU16(payload, req->reqId);
    payload.Append(ToStr(req->results));

    str::Builder packet;
    AppendU32(packet, (u32)len(payload));
    packet.Append(ToStr(payload));
    return WriteExact(h, ToStr(packet));
}

static void ProcessControlConnection(HANDLE h) {
    for (;;) {
        ControlRequest* req = ReadControlRequest(h);
        if (!req) {
            return;
        }
        uitask::Post(MkFunc0<ControlRequest>(ExecuteControlRequest, req), "SumatraControl");
        WaitForSingleObject(req->done, INFINITE);
        bool ok = WriteControlResponse(h, req);
        DeleteControlRequest(req);
        if (!ok) {
            return;
        }
    }
}

static WStr FullPipeNameOwned(Str pipeName) {
    if (str::StartsWith(pipeName, StrL(R"(\\.\pipe\)"))) {
        return ToWStr(pipeName);
    }
    TempStr fullName = str::JoinTemp(StrL(R"(\\.\pipe\)"), pipeName);
    return ToWStr(fullName);
}

struct ControlThreadArg {
    Str pipeName;
};

static void SumatraControlThread(ControlThreadArg* arg) {
    WStr pipeNameW = FullPipeNameOwned(arg->pipeName);
    str::FreePtr(&arg->pipeName);
    delete arg;

    for (;;) {
        HANDLE pipe = CreateNamedPipeW(pipeNameW.s, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       1, 64 * 1024, 64 * 1024, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            logf("CreateNamedPipeW failed for control pipe, err=%u\n", (unsigned)GetLastError());
            return;
        }
        BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            ProcessControlConnection(pipe);
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

void StartSumatraControl(Str pipeName) {
    if (len(pipeName) == 0) {
        return;
    }
    auto* arg = new ControlThreadArg{str::Dup(pipeName)};
    RunAsync(MkFunc0(SumatraControlThread, arg), "SumatraControl");
}
