/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "base/SquareTreeParser.h"
#include "base/Http.h"
#include "base/Win.h"
#include "base/File.h"
#include "base/Crypto.h"

#include "wingui/Layout.h"
#include "wingui/UIModels.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "Flags.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "HomePage.h"
#include "Installer.h"
#include "UpdateCheck.h"
#include "Theme.h"
#include "base/Dpi.h"
#include "base/GuessFileType.h"
#include "WindowTab.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "EngineAll.h"

static Kind kNotifUpdateCheckInProgress = StrL("notifUpdateCheckInProgress").s;

// TumatraPDF: update metadata lives in this repo (update.txt at repo root, served via
// raw.githubusercontent.com) and downloads come from GitHub Releases

// clang-format off
// tried in order; later entries are backups if earlier HTTP gets fail
static const Str updateInfoURLs[] = {
    StrL("https://raw.githubusercontent.com/FelipeTesta/TumatraPDF/main/update.txt"),
};
// clang-format on

#ifndef kWebisteDownloadPageURL
#define kWebisteDownloadPageURL "https://github.com/FelipeTesta/TumatraPDF/releases/latest"
#endif

// prevent multiple update tasks from happening simultaneously
// (this might e.g. happen if a user checks manually very quickly after startup)
static bool gUpdateCheckInProgress = false;

// when true, NotifyUserOfUpdate skips the install-confirmation dialog and just
// installs (set when the user clicks "Download and update" in the pre-release
// update notification)
static bool gUpdateAutoInstall = false;

// the bottom-left "update available" notification (with the download link)
static Kind kNotifUpdateAvailable = StrL("notifUpdateAvailable").s;

struct UpdateInfo {
    HWND hwndParent = nullptr;
    Str latestVer;

    Str installer64;
    Str portable64;

    Str installer32;
    Str portable32;

    Str installerArm64;
    Str portableArm64;

    Str dlURL;
    Str installerPath;

    UpdateInfo() = default;
    ~UpdateInfo() {
        str::Free(latestVer);
        str::Free(installer64);
        str::Free(portable64);
        str::Free(installer32);
        str::Free(portable32);
        str::Free(installerArm64);
        str::Free(portableArm64);
        str::Free(dlURL);
        str::Free(installerPath);
    }
};

// an available update surfaced by the pre-release startup notification; the
// "Download and update" link downloads & installs it (owned here until then)
static UpdateInfo* gPendingUpdate = nullptr;

/*
The format of update information downloaded from the server (repo-root update.txt):

[TumatraPDF]
Latest: 1.0
PortableExe64: https://github.com/FelipeTesta/TumatraPDF/releases/latest/download/TumatraPDF.exe

(upstream keys Installer64/Installer32/PortableExeArm64/... still parsed; section name
may also be [SumatraPDF] for compatibility)

[Promo]
[
    Name = MarkLexis
    URL = https://marklexis.arslexis.io
    Info = Bookmarking web application
]
*/
static UpdateInfo* ParseUpdateInfo(Str d) {
    // if a user configures os-wide proxy that is not a regular ie proxy
    // (which we pick up) we might get garbage http response
    // check if response looks valid
    if (!d) {
        return nullptr;
    }
    bool bracketed = d.s[0] == '[';
    Str prefix = bracketed ? StrL("[TumatraPDF]") : StrL("TumatraPDF");
    Str prefixUpstream = bracketed ? StrL("[SumatraPDF]") : StrL("SumatraPDF");
    bool isTumatra = str::StartsWith(d, prefix);
    if (!isTumatra && !str::StartsWith(d, prefixUpstream)) {
        return nullptr;
    }

    SquareTreeNode* root = ParseSquareTree(d);
    if (!root) {
        return nullptr;
    }
    AutoDelete delRoot(root);

    SetPromoString(SerializeSquareTreeNodeTemp(root->GetChild(StrL("Promo"))));

    SquareTreeNode* node = root->GetChild(StrL("TumatraPDF"));
    if (!node) {
        node = root->GetChild(StrL("SumatraPDF")); // accept upstream layout too
    }
    if (!node) {
        return nullptr;
    }

    Str latestVer = node->GetValue(StrL("Latest"));
    if (!IsValidProgramVersion(latestVer)) {
        return nullptr;
    }
    auto* res = new UpdateInfo();
    res->latestVer = str::Dup(latestVer);

    // those are optional. if missing, we'll just tell the user to go to website to download
    res->installer64 = str::Dup(node->GetValue(StrL("Installer64")));
    res->installerArm64 = str::Dup(node->GetValue(StrL("InstallerArm64")));
    res->installer32 = str::Dup(node->GetValue(StrL("Installer32")));

    res->portable64 = str::Dup(node->GetValue(StrL("PortableExe64")));
    res->portableArm64 = str::Dup(node->GetValue(StrL("PortableExeArm64")));
    res->portable32 = str::Dup(node->GetValue(StrL("PortableExe32")));

    // figure out which executable to download
    Str dlURL;
    bool isDll = IsDllBuild();
    if (IsArmBuild()) {
        dlURL = isDll ? res->installerArm64 : res->portableArm64;
    } else if (IsProcess64()) {
        dlURL = isDll ? res->installer64 : res->portable64;
    } else {
        dlURL = isDll ? res->installer32 : res->portable32;
    }
    res->dlURL = str::Dup(dlURL);
    return res;
}

static bool ShouldCheckForUpdate(UpdateCheck updateCheckType) {
    if (gUpdateCheckInProgress) {
        logf("CheckForUpdate: skipping because gUpdateCheckInProgress\n");
        return false;
    }

    // when forcing, we download pre-release, which shows greater version than our build
    // so we don't want to download during automatic check, only when user initiated

#if defined(FORCE_AUTO_UPDATE)
    if (updateCheckType == UpdateCheck::UserInitiated) {
        logf("CheckForUpdate: checking, user initiated (FORCE_AUTO_UPDATE)\n");
        return true;
    } else {
        logf("CheckForUpdate: skipping auto check, FORCE_AUTO_UPDATE downloads pre-release\n");
        return false;
    }
#endif

    if (!HasPermission(Perm::InternetAccess)) {
        logf("CheckForUpdate: skipping because no internet access\n");
        return false;
    }

    if (updateCheckType == UpdateCheck::UserInitiated) {
        logf("CheckForUpdate: checking, user initiated\n");
        return true;
    }

    // don't check if the timestamp or version to skip can't be updated
    // (mainly in plugin mode, stress testing and restricted settings)
    if (!HasPermission(Perm::SavePreferences)) {
        logf("CheckForUpdate: skipping auto check because no prefs access\n");
        return false;
    }

    // only applies to automatic update check
    if (!gGlobalPrefs->checkForUpdates) {
        logf("CheckForUpdate: skipping auto check because CheckForUpdates is false\n");
        return false;
    }

    // don't check for updates at the first start, so that privacy
    // sensitive users can disable the update check in time
    FILETIME never{};
    if (FileTimeEq(gGlobalPrefs->timeOfLastUpdateCheck, never)) {
        logf("CheckForUpdate: skipping auto check, first start (TimeOfLastUpdateCheck not set)\n");
        return false;
    }

    // pre-release builds check on every startup (testers want the newest build);
    // skip the daily/weekly throttle below
    if (gIsPreReleaseBuild) {
        logf("CheckForUpdate: checking, pre-release build checks on every startup\n");
        return true;
    }

    // only check if at least a day passed since last check
    FILETIME currentTimeFt;
    GetSystemTimeAsFileTime(&currentTimeFt);
    int secsSinceLastUpdate = FileTimeDiffInSecs(currentTimeFt, gGlobalPrefs->timeOfLastUpdateCheck);

    constexpr int kSecondsInDay = 60 * 60 * 24;
    constexpr int kSecondsInWeek = 7 * 60 * 60 * 24;

    int secsBetweenChecks = gIsPreReleaseBuild ? kSecondsInWeek : kSecondsInDay;
    bool checkUpdate = secsSinceLastUpdate > secsBetweenChecks;
    logf("CheckForUpdate: %s auto check, %d secs since the last one, %d secs between checks\n",
         checkUpdate ? StrL("doing") : StrL("skipping"), secsSinceLastUpdate, secsBetweenChecks);
    return checkUpdate;
}

// Returns true if the updater process was launched. Callers that then exit the
// app must check the return value: a failed launch means the update never
// happens, and the user must not be left with a silent no-op.
bool StartInstallerAutoUpgrade(Str installerPath, bool silent) {
    // Read-only check: log whether the downloaded file carries a download mark.
    // We no longer delete the Zone.Identifier (Mark-of-the-Web bypass is
    // ineffective for SmartScreen on unsigned binaries and triggers AV heuristics).
    int zoneId = file::GetZoneIdentifier(installerPath);
    logf("[upd] installer zone id: %d path=%s\n", zoneId, installerPath);

    bool signed_ = IsPEFileSigned(installerPath);
    if (!signed_) {
        if (IsOurExeInstalled()) {
            // TumatraPDF: unsigned fork build. The downloaded file IS our installer
            // asset (Installer64 in update.txt); the -install flag flips it into
            // installer mode regardless of its temp file name, it reuses the
            // install dir remembered in the registry and replaces files in place.
            // For silent auto-update, also pass -fast-install so the installer
            // auto-starts the install, relaunches the app, and exits without
            // showing any window.
            logf("StartInstallerAutoUpgrade: unsigned update for installed exe, running installer (silent=%d)\n",
                 (int)silent);
            str::Builder cmd;
            cmd.Append(" -install");
            if (silent) {
                cmd.Append(" -silent");
                cmd.Append(" -fast-install");
            }
            return CreateProcessHelper(installerPath, ToStr(cmd));
        }
        // TumatraPDF: unsigned portable exe — self-replace by launching the
        // downloaded exe (same bytes) with -update-self-to, same as signed path.
        // No cmd.exe, no .bat, no tasklist — the downloaded exe in portable mode
        // waits for parent to exit, overwrites in place, relaunches, and exits.
        logf("StartInstallerAutoUpgrade: unsigned portable update, self-replacing via direct exe\n");
        str::Builder cmd;
        cmd.Append(fmt(R"( -sleep-ms 5000 -exit-when-done -update-self-to "%s")", GetSelfExePathTemp()));
        if (silent) cmd.Append(" -silent");
        logf("StartInstallerAutoUpgrade: installer cmd: '%s'\n", ToStr(cmd));
        return CreateProcessHelper(installerPath, ToStr(cmd));
    }
    str::Builder cmd;
    if (IsOurExeInstalled()) {
        // no need for sleep because it shows the installer dialog anyway
        if (gIsPreReleaseBuild) {
            cmd.Append(" -fast-install");
        } else {
            cmd.Append(" -install");
        }
        if (silent) cmd.Append(" -silent");
    } else {
        // we're asking to over-write over ourselves, so also wait 5 secs to allow
        // our process to exit
        cmd.Append(fmt(R"( -sleep-ms 5000 -exit-when-done -update-self-to "%s")", GetSelfExePathTemp()));
        if (silent) cmd.Append(" -silent");
    }
    logf("StartInstallerAutoUpgrade: installer cmd: '%s'\n", ToStr(cmd));
    return CreateProcessHelper(installerPath, ToStr(cmd));
}

static void ExitAfterStartingUpdater() {
    // Exit immediately so the updater can overwrite our exe. PostQuitMessage(0)
    // is unreliable when the dialog was shown from a uitask during startup.
    if (gPluginMode) {
        PostQuitMessage(0);
        return;
    }
    ::ExitProcess(0);
}

// Reads ProductVersion string from a file's VERSIONINFO resource.
// Returns empty TempStr on failure (missing resource, parse error, etc.).
static TempStr GetFileProductVersionTemp(Str path) {
    TempStr result;
    DWORD dummy;
    DWORD size = GetFileVersionInfoSizeW(CWStrTemp(path), &dummy);
    if (size == 0) {
        return result;
    }
    ScopedMem<void> versionInfo(malloc(size));
    if (!versionInfo || !GetFileVersionInfoW(CWStrTemp(path), 0, size, versionInfo)) {
        return result;
    }

    struct Translation {
        WORD lang;
        WORD codepage;
    };
    Translation* trans = nullptr;
    UINT len = 0;
    if (VerQueryValueW(versionInfo, L"\\VarFileInfo\\Translation", (LPVOID*)&trans, &len)) {
        int count = len / sizeof(Translation);
        for (int i = 0; i < count; i++) {
            WCHAR subBlock[64];
            wsprintfW(subBlock, L"\\StringFileInfo\\%04x%04x\\ProductVersion", trans[i].lang, trans[i].codepage);
            void* ptr = nullptr;
            UINT vlen = 0;
            if (VerQueryValueW(versionInfo, subBlock, &ptr, &vlen)) {
                if (ptr && vlen > 0) {
                    result = ToUtf8Temp(WStr((WCHAR*)ptr));
                    return result;
                }
            }
        }
    }
    return result;
}

// Forward declaration
static int ShowUpdateAvailableDialog(UpdateInfo* updateInfo);

// Check if any open document has unsaved annotations/outline/page geometry changes
static bool AnyOpenDocumentHasUnsavedChanges() {
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (!tab->filePath) continue; // skip home/about tabs
            DisplayModel* dm = tab->AsFixed();
            if (!dm) continue;
            EngineBase* engine = dm->GetEngine();
            if (engine && EngineHasUnsavedAnnotations(engine)) {
                logf("AnyOpenDocumentHasUnsavedChanges: tab '%s' has unsaved changes\n", tab->filePath);
                return true;
            }
        }
    }
    return false;
}

static void NotifyUserOfUpdate(UpdateInfo* updateInfo, UpdateCheck updateCheckType) {
    auto installerPathAuto = updateInfo->installerPath;

    // auto-install path: the user already opted in via the "Download and update"
    // link (pre-release one-click update)
    if (gUpdateAutoInstall) {
        gUpdateAutoInstall = false;
        SaveSettings(); // persist timeOfLastUpdateCheck
        if (installerPathAuto && file::Exists(installerPathAuto)) {
            if (StartInstallerAutoUpgrade(installerPathAuto, false)) {
                ExitAfterStartingUpdater();
            } else {
                logf("NotifyUserOfUpdate: auto-install launch failed for '%s'\n", installerPathAuto);
            }
        } else {
            logf("NotifyUserOfUpdate: auto-install requested but installer not downloaded\n");
        }
        return;
    }

    // TumatraPDF auto-update: if AutoUpdate is ON and this was an AUTOMATIC check,
    // install silently WITHOUT showing the dialog, UNLESS there are unsaved changes
    // or the silent install cannot proceed (installer not downloaded, or the
    // updater process failed to launch). In those cases we fall through to the
    // confirm dialog so the user is NOT left with a silent no-op and a vanished
    // "accept update" button.
    if (gGlobalPrefs->autoUpdate && updateCheckType == UpdateCheck::Automatic) {
        logf("NotifyUserOfUpdate: AutoUpdate=ON, automatic check -> silent install\n");
        SaveSettings(); // persist timeOfLastUpdateCheck
        if (AnyOpenDocumentHasUnsavedChanges()) {
            logf("NotifyUserOfUpdate: unsaved changes detected -> falling back to dialog\n");
            // fall through to show dialog
        } else if (installerPathAuto && file::Exists(installerPathAuto)) {
            if (StartInstallerAutoUpgrade(installerPathAuto, true)) {
                ExitAfterStartingUpdater();
                return;
            }
            // silent launch failed: fall through to dialog so the user sees the
            // confirm button and learns the install did not start silently.
            logf("NotifyUserOfUpdate: silent install launch failed -> falling back to dialog\n");
        } else {
            // installer not downloaded yet: fall through to dialog so the user
            // sees the confirm button instead of a silent no-op.
            logf("NotifyUserOfUpdate: auto-update requested but installer not downloaded -> falling back to dialog\n");
        }
    }

    int buttonId = ShowUpdateAvailableDialog(updateInfo);

    // persist timeOfLastUpdateCheck
    SaveSettings();
    constexpr int kBtnIdDontInstall = 100;
    constexpr int kBtnIdInstall = 101;
    bool doInstall = (buttonId == kBtnIdInstall);
    auto installerPath = updateInfo->installerPath;
    bool didDownloadInstaller = file::Exists(installerPath);
    if (!doInstall) {
        file::Delete(installerPath);
        return;
    }

    // if installer not downloaded tell user to download from website
    if (!didDownloadInstaller) {
        SumatraLaunchBrowser(kWebisteDownloadPageURL);
        return;
    }

    if (!StartInstallerAutoUpgrade(installerPath, false)) {
        logf("NotifyUserOfUpdate: failed to launch installer '%s'\n", installerPath);
        return; // do not exit: the user must not be stranded without the app
    }
    ExitAfterStartingUpdater();
}

// --- Themed update dialog (replaces TaskDialogIndirect) --------------------

struct UpdateDialogData {
    HWND hwnd = nullptr;
    HWND hwndParent = nullptr;
    Str title;
    Str mainInstr;
    Str content;
    Str btnDontInstallText;
    Str btnInstallText;
    bool didDownloadInstaller = false;
    bool isRtl = false;
    int result = 0; // 0 = none/cancel, 100 = don't install, 101 = install
    Button* btnDontInstall = nullptr;
    Button* btnInstall = nullptr;
    // Cached theme brush
    HBRUSH bgBrush = nullptr;
    COLORREF bgColor = 0;
};

static LRESULT CALLBACK UpdateDialogWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static void UpdateDialogRegisterClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = UpdateDialogWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // painted in WM_ERASEBKGND
    wc.lpszClassName = L"TUMATRA_UPDATE_DIALOG";
    RegisterClassExW(&wc);
    registered = true;
}

// Static callbacks for buttons
static void UpdateDialogOnDontInstall(UpdateDialogData* d) {
    d->result = 100;
    DestroyWindow(d->hwnd);
}
static void UpdateDialogOnInstall(UpdateDialogData* d) {
    d->result = 101;
    DestroyWindow(d->hwnd);
}

// Cached theme brush for static/button controls
static HBRUSH UpdateDialogCtlColorBrush() {
    static HBRUSH brush = nullptr;
    static COLORREF brushCol = (COLORREF)-1;
    COLORREF col = ThemeWindowBackgroundColor();
    if (!brush || brushCol != col) {
        if (brush) {
            DeleteObject(brush);
        }
        brush = CreateSolidBrush(col);
        brushCol = col;
    }
    return brush;
}

static LRESULT CALLBACK UpdateDialogWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UpdateDialogData* d = (UpdateDialogData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_CREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        d = (UpdateDialogData*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)d);
        d->hwnd = hwnd;

        // Dark title bar / system buttons when a dark theme is active
        ApplyDarkModeToPopupWindow(hwnd);

        // Build UI
        HFONT font = GetAppFont(hwnd);

        // Measure text for sizing
        HDC hdc = GetDC(hwnd);
        HFONT oldFont = (HFONT)SelectObject(hdc, font);
        RECT rcMeasure = {0, 0, DpiScale(hwnd, 400), 0};

        // Main instruction
        DrawTextW(hdc, CWStrTemp(d->mainInstr), -1, &rcMeasure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        int mainInstrH = rcMeasure.bottom - rcMeasure.top;

        // Content text
        rcMeasure.bottom = 0;
        DrawTextW(hdc, CWStrTemp(d->content), -1, &rcMeasure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        int contentH = rcMeasure.bottom - rcMeasure.top;

        SelectObject(hdc, oldFont);
        ReleaseDC(hwnd, hdc);

        int pad = DpiScale(hwnd, 20);
        int vgap = DpiScale(hwnd, 12);

        // Create buttons first to get their ideal sizes
        d->btnDontInstall = CreateButton(hwnd, d->btnDontInstallText, MkFunc0(&UpdateDialogOnDontInstall, d), d->isRtl);
        d->btnInstall = CreateButton(hwnd, d->btnInstallText, MkFunc0(&UpdateDialogOnInstall, d), d->isRtl);
        d->btnInstall->isDefault = true;

        Size szDont = d->btnDontInstall->GetIdealSize();
        Size szInst = d->btnInstall->GetIdealSize();
        int btnH = std::max(szDont.dy, szInst.dy);
        int btnW = std::max(szDont.dx, szInst.dx);
        int gap = DpiScale(hwnd, 10);

        int totalClientW = std::max(DpiScale(hwnd, 400), 2 * pad + btnW * 2 + gap);
        int totalClientH = pad + mainInstrH + vgap + contentH + vgap + btnH + pad;

        // Compute window rect from client size
        RECT wr = {0, 0, totalClientW, totalClientH};
        AdjustWindowRectEx(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT);
        int winW = wr.right - wr.left;
        int winH = wr.bottom - wr.top;

        // Center on parent
        RECT rw;
        GetWindowRect(d->hwndParent, &rw);
        int x = rw.left + ((rw.right - rw.left) - winW) / 2;
        int y = rw.top + ((rw.bottom - rw.top) - winH) / 2;
        SetWindowPos(hwnd, nullptr, x, y, winW, winH, SWP_NOZORDER | SWP_NOACTIVATE);

        // Now get the real client rect and position children
        RECT rc;
        GetClientRect(hwnd, &rc);
        int clientW = rc.right - rc.left;

        // Main instruction static
        HWND hwndMainInstr =
            CreateWindowExW(0, WC_STATICW, CWStrTemp(d->mainInstr), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, pad,
                            pad, clientW - 2 * pad, mainInstrH, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        SetWindowTheme(hwndMainInstr, L"", L"");
        SendMessageW(hwndMainInstr, WM_SETFONT, (WPARAM)font, TRUE);

        // Content static
        HWND hwndContent = CreateWindowExW(
            0, WC_STATICW, CWStrTemp(d->content), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, pad,
            pad + mainInstrH + vgap, clientW - 2 * pad, contentH, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        SetWindowTheme(hwndContent, L"", L"");
        SendMessageW(hwndContent, WM_SETFONT, (WPARAM)font, TRUE);

        // Position buttons (right-aligned, RTL-aware)
        int btnY = pad + mainInstrH + vgap + contentH + vgap;
        int btnDontInstallX, btnInstallX;
        if (d->isRtl) {
            btnInstallX = pad;
            btnDontInstallX = btnInstallX + btnW + gap;
        } else {
            btnDontInstallX = clientW - pad - btnW;
            btnInstallX = btnDontInstallX - btnW - gap;
        }
        Rect rDont{btnDontInstallX, btnY, btnW, btnH};
        Rect rInst{btnInstallX, btnY, btnW, btnH};
        d->btnDontInstall->SetPos(&rDont);
        d->btnInstall->SetPos(&rInst);

        // Create background brush
        d->bgColor = ThemeWindowBackgroundColor();
        d->bgBrush = CreateSolidBrush(d->bgColor);

        return 0;
    }
    if (!d || d->hwnd != hwnd) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_DESTROY: {
            if (d->bgBrush) {
                DeleteObject(d->bgBrush);
                d->bgBrush = nullptr;
            }
            // Buttons are owned by the dialog window, destroyed automatically
            break;
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wp;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, d->bgBrush);
            return 1;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wp;
            SetBkColor(hdc, d->bgColor);
            SetTextColor(hdc, ThemeWindowTextColor());
            return (LRESULT)UpdateDialogCtlColorBrush();
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            int code = HIWORD(wp);
            if (id == IDCANCEL) {
                d->result = 100; // Don't install (Esc or X)
                DestroyWindow(hwnd);
            } else if ((id == 100 || id == 101) && code == BN_CLICKED) {
                // Buttons use callbacks, but keep this as fallback
                d->result = id;
                DestroyWindow(hwnd);
            }
            break;
        }
        case WM_CLOSE: {
            d->result = 100; // Don't install on X/close
            DestroyWindow(hwnd);
            break;
        }
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int ShowUpdateAvailableDialog(UpdateInfo* updateInfo) {
    UpdateDialogData dlgData;
    dlgData.hwndParent = updateInfo->hwndParent;
    dlgData.isRtl = trans::IsCurrLangRtl();

    dlgData.title = fmt(_TRA("%s Update").s, StrL(kAppName));
    dlgData.mainInstr = _TRA("New version available");
    Str ver = updateInfo->latestVer;
    Str fmtStr = _TRA("You have version '%s' and version '%s' is available.\nDo you want to install the new version?");
    dlgData.content = str::Dup(fmt(fmtStr.s, StrL(CURR_VERSION_STRA), ver));

    dlgData.btnDontInstallText = _TRA("Don't install");
    if (file::Exists(updateInfo->installerPath)) {
        dlgData.btnInstallText = _TRA("Install and relaunch");
        dlgData.didDownloadInstaller = true;
    } else {
        dlgData.btnInstallText = _TRA("Download update");
        dlgData.didDownloadInstaller = false;
    }

    UpdateDialogRegisterClass();

    // Create with a minimal size; WM_CREATE will compute and resize
    int initW = DpiScale(dlgData.hwndParent, 440);
    int initH = DpiScale(dlgData.hwndParent, 220);

    RECT rw;
    GetWindowRect(dlgData.hwndParent, &rw);
    int x = rw.left + ((rw.right - rw.left) - initW) / 2;
    int y = rw.top + ((rw.bottom - rw.top) - initH) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"TUMATRA_UPDATE_DIALOG",
                                CWStrTemp(dlgData.title), WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, initW, initH,
                                dlgData.hwndParent, nullptr, GetModuleHandle(nullptr), &dlgData);

    if (!hwnd) {
        logf("ShowUpdateAvailableDialog: CreateWindowExW failed\n");
        str::Free(dlgData.content);
        return 100; // Don't install on failure
    }

    // Modal pump with IsDialogMessage for Tab/Esc/Enter handling
    EnableWindow(dlgData.hwndParent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(dlgData.btnInstall->hwnd);

    MSG msg;
    while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_QUIT) {
            break;
        }
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(dlgData.hwndParent, TRUE);
    SetActiveWindow(dlgData.hwndParent);

    int result = dlgData.result;
    str::Free(dlgData.content);
    return result;
}

struct UpdateProgressData {
    HWND hwndForNotif = nullptr;
    i64 nDownloaded = 0;
};

struct DownloadUpdateAsyncData {
    HWND hwndForNotif = nullptr;
    UpdateInfo* updateInfo = nullptr;
    HttpProgress httpProgress = {};
    UpdateCheck updateCheckType = UpdateCheck::Automatic;

    DownloadUpdateAsyncData() = default;
    ~DownloadUpdateAsyncData() { delete updateInfo; }
};

static void DownloadUpdateFinish(DownloadUpdateAsyncData* data) {
    auto* hwndForNotif = data->hwndForNotif;
    auto* updateInfo = data->updateInfo;
    auto updateCheckType = data->updateCheckType;
    data->updateInfo = nullptr;
    RemoveNotificationsForGroup(hwndForNotif, kNotifUpdateCheckInProgress);
    NotifyUserOfUpdate(updateInfo, updateCheckType);
    delete updateInfo;
    gUpdateCheckInProgress = false;
    delete data;
}

static void UpdateDownloadProgressNotif(UpdateProgressData* data) {
    TempStr size = FormatFileSizeTransTemp(data->nDownloaded);
    logf("UpdateDownloadProgressNotif: %s\n", size);
    auto* wnd = GetNotificationForGroup(data->hwndForNotif, kNotifUpdateCheckInProgress);
    if (wnd) {
        TempStr msg = fmt("Downloading update: %s\n", size);
        NotificationUpdateMessage(wnd, msg, 0, true);
    } else {
        logf("UpdateDownloadProgressNotif: no wnd\n");
    }
    delete data;
}

static void UpdateProgressCb(UpdateProgressData* data, HttpProgress* progress) {
    logf("UpdateProgressCb: n: %d\n", (int)progress->nDownloaded);
    auto* fnData = new UpdateProgressData;
    fnData->hwndForNotif = data->hwndForNotif;
    fnData->nDownloaded = progress->nDownloaded;
    auto fn = MkFunc0<UpdateProgressData>(UpdateDownloadProgressNotif, fnData);
    uitask::Post(fn, nullptr);
}

static void DownloadUpdateAsync(DownloadUpdateAsyncData* data) {
    auto* hwndForNotif = data->hwndForNotif;
    auto* updateInfo = data->updateInfo;

    TempStr installerPath = GetTempFilePathTemp("sumatra-installer");
    // the installer must be named .exe or it won't be able to self-elevate
    // with "runas"
    installerPath = str::JoinTemp(installerPath, StrL(".exe"));
    UpdateProgressData pd;
    pd.hwndForNotif = hwndForNotif;
    auto cb = MkFunc1<UpdateProgressData, HttpProgress*>(UpdateProgressCb, &pd);
    constexpr i64 kMaxUpdateDownloadSize = 256LL * 1024 * 1024;
    bool ok = HttpGetToFile(updateInfo->dlURL, installerPath, cb, kMaxUpdateDownloadSize);
    logf("ShowAutoUpdateDialog: HttpGetToFile(): ok=%d, downloaded to '%s'\n", (int)ok, installerPath);
    // TumatraPDF releases are unsigned portable exes: instead of a signature check,
    // require a plausible PE file (MZ header, >= 1 MiB)
    if (ok) {
        constexpr i64 kMinExeSize = 1024LL * 1024;
        Str hdr = file::ReadFile(installerPath);
        i64 size = len(hdr);
        ok = size >= kMinExeSize && hdr.s[0] == 'M' && hdr.s[1] == 'Z';
        str::Free(hdr);
        logf("ShowAutoUpdateDialog: size=%lld, PE header ok=%d\n", size, (int)ok);
    }
    // Verify downloaded file version matches expected version (FIX 2)
    if (ok) {
        TempStr fileVer = GetFileProductVersionTemp(installerPath);
        Str expectedVer = updateInfo->latestVer;
        if (fileVer) {
            // Normalize both strings (trim whitespace)
            Str fv = str::Dup(fileVer);
            str::TrimWSInPlace(fv, str::TrimOpt::Both);
            Str ev = str::Dup(expectedVer);
            str::TrimWSInPlace(ev, str::TrimOpt::Both);
            // Pre-release channel uses plain build number (no '.') - log only, don't block
            if (!str::ContainsChar(ev, '.')) {
                logf("DownloadUpdateAsync: pre-release channel (build %s), file version '%s' - logging only\n", ev, fv);
            } else if (!str::Eq(fv, ev)) {
                logf("DownloadUpdateAsync: version mismatch for '%s': file version '%s' expected '%s'\n", installerPath,
                     fv, ev);
                file::Delete(installerPath);
                ok = false;
            } else {
                logf("DownloadUpdateAsync: version OK for '%s': '%s'\n", installerPath, fv);
            }
            str::Free(fv);
            str::Free(ev);
        } else {
            // No version info - log and accept (don't block)
            logf("DownloadUpdateAsync: no VERSIONINFO in '%s', expected '%s' - accepting\n", installerPath,
                 expectedVer);
        }
    }
    if (ok) {
        updateInfo->installerPath = str::Dup(installerPath);
    } else {
        file::Delete(installerPath);
    }

    // process the rest on ui thread to avoid threading issues
    auto fn = MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateFinish, data);
    uitask::Post(fn, "TaskShowAutoUpdateDialog");
}

// pre-release builds surface an available update with a bottom-left notification
// whose "Download and update" link triggers a one-click download + install
static void ShowUpdateAvailableNotification(MainWindow* win, UpdateInfo* updateInfo) {
    if (!win || !updateInfo) {
        return;
    }
    TempStr link = fmt("[%s](CmdInstallPrereleaseUpdate)", _TRA("Download and install latest version"));
    // pre-release "Latest" is a build number (e.g. 17616); show as 3.7.17616
    TempStr displayVer = updateInfo->latestVer;
    if (!str::ContainsChar(displayVer, '.')) {
        displayVer = fmt("%s.%s", StrL(CURR_VERSION_MAJOR_STRA), displayVer);
    }
    TempStr msg = fmt(_TRA("Version %s available. %s").s, displayVer, link);
    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.msg = msg;
    args.warning = true; // yellowish background so it stands out
    args.groupId = kNotifUpdateAvailable;
    args.timeoutMs = 0; // persist until the user clicks the link or closes it
    args.corner = NotifCorner::BottomLeft;
    args.xMargin = 2;
    args.yMargin = 2;
    ShowNotification(args);
}

// called when the user clicks "Download and update" in the pre-release update
// notification: download the pending update and (via gUpdateAutoInstall) install
// it without the confirmation dialog
// download + install the update surfaced by the pre-release update notification
void DownloadAndInstallPendingUpdate(MainWindow* win) {
    if (!win || !gPendingUpdate) {
        return;
    }
    UpdateInfo* updateInfo = gPendingUpdate;
    gPendingUpdate = nullptr;
    gUpdateAutoInstall = true;

    HWND hwndForNotif = win->hwndCanvas;
    updateInfo->hwndParent = win->hwndFrame;
    RemoveNotificationsForGroup(hwndForNotif, kNotifUpdateAvailable);

    // progress notification updated by UpdateDownloadProgressNotif (same group)
    NotificationCreateArgs nargs;
    nargs.hwndParent = hwndForNotif;
    nargs.msg = _TRA("Downloading update...");
    nargs.warning = true;
    nargs.groupId = kNotifUpdateCheckInProgress;
    nargs.timeoutMs = 0;
    nargs.corner = NotifCorner::BottomLeft;
    nargs.xMargin = 2;
    nargs.yMargin = 2;
    ShowNotification(nargs);

    gUpdateCheckInProgress = true;
    auto* fnData = new DownloadUpdateAsyncData;
    fnData->hwndForNotif = hwndForNotif;
    fnData->updateInfo = updateInfo;
    fnData->updateCheckType = UpdateCheck::UserInitiated;
    auto fn = MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateAsync, fnData);
    RunAsync(fn, "DownloadUpdateAsync");
}

static bool ShouldDownloadUpdate(UpdateInfo* updateInfo) {
    if (gIsStoreBuild) {
        // I assume store will take care of updates
        return false;
    }
    Str latestVer = updateInfo->latestVer;
    Str myVer = StrL(UPDATE_CHECK_VERA);
    if (gIsDebugBuild) {
        // in debug build we compare against pre-rel version, like "17616"
        // but our version is like "3.6" so it triggers update
        myVer = StrL("50000");
    }
    bool hasUpdate = CompareProgramVersion(latestVer, myVer) > 0;
    return hasUpdate;
}

static HRESULT CALLBACK TaskDialogHyperlinkCallback(HWND /*hwnd*/, UINT msg, WPARAM /*wParam*/, LPARAM lParam,
                                                    LONG_PTR /*lpRefData*/) {
    if (msg == TDN_HYPERLINK_CLICKED) {
        WCHAR* url = (WCHAR*)lParam;
        SumatraLaunchBrowser(ToUtf8Temp(url));
    }
    return S_OK;
}

static const Str kExpectedDlHost = StrL("https://github.com/FelipeTesta/TumatraPDF/releases/");

static void NotifySuspiciousUpdate(HWND hwndParent, Str dlURL) {
    logf("NotifySuspiciousUpdate: suspicious download url '%s'\n", dlURL);
    ReportIfFast(true);
    auto title = fmt(_TRA("%s Update").s, StrL(kAppName));
    auto content = fmt(R"(Suspicious update.

Download link should come from <a href="%s">%s</a> but is %s.

Visit <a href="%s">%s</a> to download the latest version.)",
                       kExpectedDlHost, kExpectedDlHost, dlURL, kExpectedDlHost, kExpectedDlHost);

    TASKDIALOGCONFIG dialogConfig{};
    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }

    constexpr int kBtnIdVisitWebsite = 100;
    TASKDIALOG_BUTTON buttons[1];
    buttons[0].nButtonID = kBtnIdVisitWebsite;
    buttons[0].pszButtonText = CWStrTemp(_TRA("Visit &Website"));

    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = CWStrTemp(title);
    dialogConfig.pszContent = CWStrTemp(content);
    dialogConfig.dwFlags = (TASKDIALOG_FLAGS)flags;
    dialogConfig.dwCommonButtons = TDCBF_CLOSE_BUTTON;
    dialogConfig.cButtons = dimof(buttons);
    dialogConfig.pButtons = buttons;
    dialogConfig.nDefaultButton = kBtnIdVisitWebsite;
    dialogConfig.pszMainIcon = TD_WARNING_ICON;
    dialogConfig.hwndParent = hwndParent;
    dialogConfig.pfCallback = TaskDialogHyperlinkCallback;
    int buttonPressedId = 0;
    TaskDialogIndirect(&dialogConfig, &buttonPressedId, nullptr, nullptr);
    if (buttonPressedId == kBtnIdVisitWebsite) {
        SumatraLaunchBrowser(kExpectedDlHost);
    }
}

// Shown only for a user-initiated update check that couldn't download/parse the
// update info. Tells the user and points them at the download page so they can
// update manually (e.g. if TLS validation or the network failed).
static void NotifyUpdateCheckFailed(HWND hwndParent, DWORD err) {
    logf("NotifyUpdateCheckFailed: err=%#x\n", (unsigned)err);
    auto title = _TRA("SumatraPDF Update");
    auto mainInstr = _TRA("Couldn't check for updates");
    TempStr msg = fmt(_TRA("Couldn't download update information (error %#x).").s, err);
    TempStr content = fmt(R"(%s

Visit <a href="%s">%s</a> to download the latest version.)",
                          msg, StrL(kWebisteDownloadPageURL), StrL(kWebisteDownloadPageURL));

    TASKDIALOGCONFIG dialogConfig{};
    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }

    constexpr int kBtnIdVisitWebsite = 100;
    TASKDIALOG_BUTTON buttons[1];
    buttons[0].nButtonID = kBtnIdVisitWebsite;
    buttons[0].pszButtonText = CWStrTemp(_TRA("Visit &Website"));

    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = CWStrTemp(title);
    dialogConfig.pszMainInstruction = CWStrTemp(mainInstr);
    dialogConfig.pszContent = CWStrTemp(content);
    dialogConfig.dwFlags = (TASKDIALOG_FLAGS)flags;
    dialogConfig.dwCommonButtons = TDCBF_CLOSE_BUTTON;
    dialogConfig.cButtons = dimof(buttons);
    dialogConfig.pButtons = buttons;
    dialogConfig.nDefaultButton = kBtnIdVisitWebsite;
    dialogConfig.pszMainIcon = TD_WARNING_ICON;
    dialogConfig.hwndParent = hwndParent;
    dialogConfig.pfCallback = TaskDialogHyperlinkCallback;
    int buttonPressedId = 0;
    TaskDialogIndirect(&dialogConfig, &buttonPressedId, nullptr, nullptr);
    if (buttonPressedId == kBtnIdVisitWebsite) {
        SumatraLaunchBrowser(kWebisteDownloadPageURL);
    }
}

static DWORD MaybeStartUpdateDownload(HWND hwndParent, HttpRsp* rsp, UpdateCheck updateCheckType) {
    // for store builds we do update check but ignore the result
#if 0
    if (gIsStoreBuild) {
        return 0;
    }
#endif

    Str url = rsp->url;

    if (rsp->error != 0) {
        logf("ShowAutoUpdateDialog: http get of '%s' failed with %d\n", url, (int)rsp->error);
        return rsp->error;
    }
    if (rsp->httpStatusCode != 200) {
        logf("ShowAutoUpdateDialog: http get of '%s' failed with code %d\n", url, (int)rsp->httpStatusCode);
        return ERROR_INTERNET_INVALID_URL;
    }

    bool isValidURL = false;
    for (auto updateInfoURL : updateInfoURLs) {
        if (str::StartsWith(url, updateInfoURL)) {
            isValidURL = true;
            break;
        }
    }
    if (!isValidURL) {
        logf("ShowAutoUpdateDialog: '%s' is not a valid url\n", url);
        return ERROR_INTERNET_INVALID_URL;
    }
    str::Builder* data = &rsp->data;
    if (0 == len(*data)) {
        logf("ShowAutoUpdateDialog: empty response from url '%s'\n", url);
        return ERROR_INTERNET_CONNECTION_ABORTED;
    }

    UpdateInfo* updateInfo = ParseUpdateInfo(ToStr(*data));
    if (!updateInfo) {
        logf("ShowAutoUpdateDialog: ParseUpdateInfo() failed. URL: '%s'\nAuto update data:\n%s\n", url, ToStr(*data));
        return ERROR_INTERNET_INCORRECT_FORMAT;
    }
    updateInfo->hwndParent = hwndParent;

    MainWindow* win = FindMainWindowByHwnd(hwndParent);
    if (!win) {
        // could be destroyed since we issued update check
        delete updateInfo;
        return 0;
    }
    HWND hwndForNotif = win->hwndCanvas;
    if (!ShouldDownloadUpdate(updateInfo)) {
        Str myVer = StrL(UPDATE_CHECK_VERA);
        logf("ShowAutoUpdateDialog: myVer >= latestVer ('%s' >= '%s')\n", myVer, updateInfo->latestVer);
        /* if automated => don't notify that there is no new version */
        if (updateCheckType == UpdateCheck::UserInitiated) {
            auto* wnd = GetNotificationForGroup(hwndForNotif, kNotifUpdateCheckInProgress);
            if (wnd) {
                NotificationUpdateMessage(wnd, _TRA("You have the latest version."), 5 * 1000, true);
            }
        }
        delete updateInfo;
        return 0;
    }

    if (!updateInfo->dlURL) {
        // currently for release builds we don't set this and redirecto to a website instead
        logf("ShowAutoUpdateDialog: didn't find download url. Auto update data:\n%s\n", ToStr(*data));
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifUpdateCheckInProgress);
        NotifyUserOfUpdate(updateInfo, updateCheckType);
        delete updateInfo;
        return 0;
    }

    if (!str::StartsWith(updateInfo->dlURL, kExpectedDlHost)) {
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifUpdateCheckInProgress);
        NotifySuspiciousUpdate(hwndParent, updateInfo->dlURL);
        delete updateInfo;
        return 0;
    }

    // pre-release automatic check: don't download yet. Show a bottom-left
    // notification whose "Download and update" link does the download + install.
    if (updateCheckType == UpdateCheck::Automatic && gIsPreReleaseBuild) {
        RemoveNotificationsForGroup(hwndForNotif, kNotifUpdateCheckInProgress);
        delete gPendingUpdate;       // drop any update from a previous check
        gPendingUpdate = updateInfo; // take ownership (freed when installed/replaced)
        ShowUpdateAvailableNotification(win, updateInfo);
        return 0;
    }

    // download the installer to make update feel instant to the user
    logf("ShowAutoUpdateDialog: starting to download '%s'\n", updateInfo->dlURL);
    gUpdateCheckInProgress = true;

    auto* fnData = new DownloadUpdateAsyncData;
    fnData->hwndForNotif = hwndForNotif;
    fnData->updateInfo = updateInfo;
    fnData->updateCheckType = updateCheckType;
    auto fn = MkFunc0<DownloadUpdateAsyncData>(DownloadUpdateAsync, fnData);
    RunAsync(fn, "DownloadUpdateAsync");
    return 0;
}

static void BuildUpdateURL(str::Builder& url, Str baseURL, UpdateCheck updateCheckType) {
    url.Reset(baseURL);
    url.Append("?v=");
    url.Append(UPDATE_CHECK_VERA);
    TempStr osVerTemp = GetWindowsVerTemp();
    url.Append("&os=");
    url.Append(osVerTemp);
    url.Append("&64bit=");
    url.Append(IsProcess64() ? "yes" : "no");
    url.Append("&arm=");
    url.Append(IsArmBuild() ? "yes" : "no");
    Str lang = trans::GetCurrentLangCode();
    url.Append("&lang=");
    url.Append(lang);
    TempStr webView2ver = GetWebView2VersionTemp();
    if (webView2ver) {
        url.Append("&webview=");
        url.Append(webView2ver);
    }
    if (gIsStoreBuild) {
        url.Append("&store");
    }
    url.Append("&simd=");
    url.Append(LatestSupportedSIMD().s);
    url.Append("&withPromo");
    if (UpdateCheck::UserInitiated == updateCheckType) {
        url.Append("&force");
    }
}

struct UpdateCheckAsyncData {
    MainWindow* win = nullptr;
    UpdateCheck updateCheckType = UpdateCheck::Automatic;
    HttpRsp* rsp = nullptr;
    UpdateCheckAsyncData() = default;
    ~UpdateCheckAsyncData() { delete rsp; }
};

static void UpdateCheckFinish(UpdateCheckAsyncData* data) {
    log("UpdateCheckFinish\n");
    gUpdateCheckInProgress = false;

    AutoDelete delData(data);

    auto updateCheckType = data->updateCheckType;
    auto* rsp = data->rsp;
    MainWindow* win = nullptr;
    if (IsMainWindowValid(data->win)) {
        win = data->win;
    } else {
        if (len(gWindows) > 0) {
            win = gWindows[0];
        }
    }
    if (!win) {
        return;
    }
    HWND hwnd = win->hwndFrame;
    DWORD err = MaybeStartUpdateDownload(hwnd, rsp, updateCheckType);
    if ((err != 0) && (updateCheckType == UpdateCheck::UserInitiated)) {
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifUpdateCheckInProgress);
        // a manual check that couldn't fetch update info: tell the user and point
        // them at the website so they can update manually
        NotifyUpdateCheckFailed(hwnd, err);
    }
}

static void UpdateCheckAsync(UpdateCheckAsyncData* data) {
    auto updateCheckType = data->updateCheckType;
    HttpRsp* rsp = nullptr;
    for (auto updateInfoURL : updateInfoURLs) {
        if (rsp) {
            delete rsp;
        }
        str::Builder url;
        BuildUpdateURL(url, updateInfoURL, updateCheckType);
        Str uri = ToStr(url);
        rsp = new HttpRsp;
        str::ReplaceWithCopy(&rsp->url, uri);
        if (HttpGet(uri, rsp)) {
            break;
        }
    }
    data->rsp = rsp;
    auto fn = MkFunc0<UpdateCheckAsyncData>(UpdateCheckFinish, data);
    uitask::Post(fn, "TaskUpdateCheckFinish");
}

// start auto-update check by downloading auto-update information from url
// on a background thread and processing the retrieved data on ui thread
// if autoCheck is true, this is a check *not* triggered by explicit action
// of the user and therefore will show less UI
void StartAsyncUpdateCheck(MainWindow* win, UpdateCheck updateCheckType) {
    if (!ShouldCheckForUpdate(updateCheckType)) {
        return;
    }

    logf("StartAsyncUpdateCheck: updateCheckType=%d\n", (int)updateCheckType);
    if (UpdateCheck::UserInitiated == updateCheckType) {
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.msg = _TRA("Checking for update...");
        args.warning = true;
        args.timeoutMs = 0;
        args.groupId = kNotifUpdateCheckInProgress;
        ShowNotification(args);
    }
    GetSystemTimeAsFileTime(&gGlobalPrefs->timeOfLastUpdateCheck);
    gUpdateCheckInProgress = true;

    // data freed in UpdateCheckFinish()
    auto* data = new UpdateCheckAsyncData();
    data->win = win;
    data->updateCheckType = updateCheckType;
    auto fn = MkFunc0<UpdateCheckAsyncData>(UpdateCheckAsync, data);
    RunAsync(fn, "UpdateCheckAsync");
}

// the assumption is that this is a portable version downloaded to temp directory
// we should copy ourselves over the existing file, launch ourselves and
// tell our new copy to delete ourselves
void UpdateSelfTo(Str dstPath) {
    ReportIf(!dstPath);
    if (!file::Exists(dstPath)) {
        logf("UpdateSelfTo: failed because destination doesn't exist\n");
        return;
    }

    auto sleepMs = gCli->sleepMs;
    logf("UpdateSelfTo: '%s', sleep for %d ms\n", dstPath, sleepMs);
    // sleeping for a bit to make sure that the program that launched us
    // had time to exit so that we can overwrite it
    ::Sleep(gCli->sleepMs);

    // OverwriteAtomicRetry(dst, src): copy this process (new build) onto dstPath
    // wait window: 60 retries x 500 ms = 30 s (plus the 5 s sleep above = ~35 s total)
    TempStr srcPath = GetSelfExePathTemp();
    bool ok = file::OverwriteAtomicRetry(dstPath, srcPath, 60, 500);
    if (!ok) {
        logf("UpdateSelfTo: failed to overwrite '%s' with '%s' after ~35 s wait (sleep 5 s + 60x500 ms retries)\n", dstPath, srcPath);
        return;
    }
    logf("UpdateSelfTo: copied self to '%s'\n", dstPath);

    TempStr args = fmt(R"(-sleep-ms 500 -delete-file "%s")", srcPath);
    CreateProcessHelper(dstPath, args);
}
