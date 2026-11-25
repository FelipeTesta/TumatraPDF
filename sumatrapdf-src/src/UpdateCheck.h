/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class UpdateCheck {
    UserInitiated, // user used menu "Check update"
    Automatic,     // an automatic, periodic check done at startup
};

void StartAsyncUpdateCheck(MainWindow* win, UpdateCheck updateCheckType);
void DownloadAndInstallPendingUpdate(MainWindow* win);
// returns true if the updater process was launched
bool StartInstallerAutoUpgrade(Str installerPath, bool silent);
void UpdateSelfTo(Str dstPath);

// Testing hook: opens the update dialog with synthetic data.
// installerExists controls whether "Install and relaunch" or "Download update" is shown.
void ShowUpdateDialogForTesting(MainWindow* win, bool installerExists);

// Testing hook that returns the dialog button result (100=don't install, 101=install).
int ShowUpdateDialogForTestingRet(MainWindow* win, bool installerExists);
