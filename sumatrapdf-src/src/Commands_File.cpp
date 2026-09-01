/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "Commands_File.h"
#include "MainWindow.h"
#include "Tabs.h"

// ---- View external viewer ----

void HandleCmdViewWithExternalViewer(MainWindow* win, Str cmdLine, Str filter) {
    (void)win; (void)cmdLine; (void)filter;
}

// ---- Theme ----

void HandleCmdSetTheme(MainWindow* win, Str theme) {
    (void)win; (void)theme;
}

// ---- Fix default app ----

void HandleCmdFixDefaultApp(MainWindow* win, Str ext) {
    (void)win; (void)ext;
}

// ---- Selection handler ----

void HandleCmdSelectionHandler(MainWindow* win, Str exe, Str url, Str method, Str body, Str contentType, Str headers) {
    (void)win; (void)exe; (void)url; (void)method; (void)body; (void)contentType; (void)headers;
}

// ---- Exec ----

void HandleCmdExec(MainWindow* win, Str filter, Str cmdLine) {
    (void)win; (void)filter; (void)cmdLine;
}

// ---- New window ----

void HandleCmdNewWindow(MainWindow* win) {
}

// ---- Tab group save ----

void HandleCmdTabGroupSave(MainWindow* win) {
    (void)win;
}

// ---- Tab group restore ----

void HandleCmdTabGroupRestore(MainWindow* win) {
    (void)win;
}

// ---- Duplicate in new window ----

void HandleCmdDuplicateInNewWindow(MainWindow* win) {
}

// ---- Duplicate in new tab ----

void HandleCmdDuplicateInNewTab(MainWindow* win) {
}

// ---- Open file ----

void HandleCmdOpenFile(MainWindow* win) {
}

// ---- Open file with OS file picker ----

void HandleCmdOpenFileWithOSFilePicker(MainWindow* win) {
}

// ---- Toggle file picker ----

void HandleCmdToggleFilePicker(MainWindow* win) {
}

// ---- Toggle bool setting ----

void HandleCmdToggleBoolSetting(MainWindow* win, Str settingName) {
    (void)win; (void)settingName;
}

// ---- Show in folder ----

void HandleCmdShowInFolder(MainWindow* win) {
    (void)win;
}

// ---- Show generated HTML ----

void HandleCmdShowGeneratedHTML(MainWindow* win) {
    (void)win;
}

// ---- Navigate files in folder ----

void HandleCmdNavigateFilesInFolder(MainWindow* win) {
    (void)win;
}

// ---- Rename file ----

void HandleCmdRenameFile(MainWindow* win) {
    (void)win;
}

// ---- Delete file ----

void HandleCmdDeleteFile(MainWindow* win) {
    (void)win;
}

// ---- Delete file and open next ----

void HandleCmdDeleteFileAndOpenNext(MainWindow* win) {
    (void)win;
}

// ---- Save as ----

void HandleCmdSaveAs(MainWindow* win) {
    (void)win;
}

// ---- Print ----

void HandleCmdPrint(MainWindow* win) {
    (void)win;
}

// ---- Copy file path ----

void HandleCmdCopyFilePath(MainWindow* win) {
    (void)win;
}

// ---- Command palette ----

void HandleCmdCommandPalette(MainWindow* win, Str mode) {
    (void)win; (void)mode;
}

// ---- AI chat ----

void HandleCmdAIChat(MainWindow* win, int backend) {
    (void)win; (void)backend;
}

// ---- Clear history ----

void HandleCmdClearHistory(MainWindow* win) {
    (void)win;
}

// ---- Remove deleted files from history ----

void HandleCmdRemoveDeletedFilesFromHistory(MainWindow* win) {
    (void)win;
}

// ---- Delete cached files ----

void HandleCmdDeleteCachedFiles(MainWindow* win) {
    (void)win;
}

// ---- Reopen last closed file ----

void HandleCmdReopenLastClosedFile(MainWindow* win) {
    (void)win;
}

// ---- Show log ----

void HandleCmdShowLog(MainWindow* win) {
    (void)win;
}

// ---- Screenshot ----

void HandleCmdScreenshot(MainWindow* win) {
    (void)win;
}

// ---- Set screenshot hotkey ----

void HandleCmdSetScreenshotHotkey(MainWindow* win) {
    (void)win;
}

// ---- Crop image ----

void HandleCmdCropImage(MainWindow* win) {
    (void)win;
}

// ---- Resize image ----

void HandleCmdResizeImage(MainWindow* win) {
    (void)win;
}

// ---- Convert image to PDF ----

void HandleCmdConvertImageToPdf(MainWindow* win) {
    (void)win;
}

// ---- List printers ----

void HandleCmdListPrinters(MainWindow* win) {
    (void)win;
}

// ---- Next/prev tab ----

void HandleCmdNextPrevTab(MainWindow* win, bool reverse) {
    (void)win; (void)reverse;
}

// ---- Next/prev tab smart ----

void HandleCmdNextPrevTabSmart(MainWindow* win, int cmdId) {
    (void)win; (void)cmdId;
}

// ---- Move tab ----

void HandleCmdMoveTab(MainWindow* win, int dir) {
    (void)win; (void)dir;
}

// ---- Close all tabs ----

void HandleCmdCloseAllTabs(MainWindow* win) {
    (void)win;
}

// ---- Close tabs ----

void HandleCmdCloseTabs(MainWindow* win, int cmdId) {
    (void)win; (void)cmdId;
}

// ---- Exit ----

void HandleCmdExit(MainWindow* win) {
    (void)win;
}

// ---- Reload document ----

void HandleCmdReloadDocument(MainWindow* win) {
    (void)win;
}

// ---- Create shortcut to file ----

void HandleCmdCreateShortcutToFile(MainWindow* win) {
    (void)win;
}

// ---- Zoom fit width and continuous ----

// ---- Zoom fit page and single page ----

// ---- Zoom out ----