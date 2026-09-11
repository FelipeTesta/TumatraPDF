/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// @gen-start cmd-file-decls
// clang-format off

// Forward declarations - types defined in base/Base.h and MainWindow.h
struct MainWindow;
struct Str;

void HandleCmdViewWithExternalViewer(MainWindow* win, Str cmdLine, Str filter);
void HandleCmdSetTheme(MainWindow* win, Str theme);
void HandleCmdFixDefaultApp(MainWindow* win, Str ext);
void HandleCmdSelectionHandler(MainWindow* win, Str exe, Str url, Str method, Str body, Str contentType, Str headers);
void HandleCmdExec(MainWindow* win, Str filter, Str cmdLine);
void HandleCmdNewWindow(MainWindow* win);
void HandleCmdTabGroupSave(MainWindow* win);
void HandleCmdTabGroupRestore(MainWindow* win);
void HandleCmdDuplicateInNewWindow(MainWindow* win);
void HandleCmdDuplicateInNewTab(MainWindow* win);
void HandleCmdOpenFile(MainWindow* win);
void HandleCmdOpenFileWithOSFilePicker(MainWindow* win);
void HandleCmdToggleFilePicker(MainWindow* win);
void HandleCmdToggleBoolSetting(MainWindow* win, Str settingName);
void HandleCmdShowInFolder(MainWindow* win);
void HandleCmdShowGeneratedHTML(MainWindow* win);
void HandleCmdNavigateFilesInFolder(MainWindow* win);
void HandleCmdRenameFile(MainWindow* win);
void HandleCmdDeleteFile(MainWindow* win);
void HandleCmdDeleteFileAndOpenNext(MainWindow* win);
void HandleCmdSaveAs(MainWindow* win);
void HandleCmdPrint(MainWindow* win);
void HandleCmdCopyFilePath(MainWindow* win);
void HandleCmdCommandPalette(MainWindow* win, Str mode);
void HandleCmdAIChat(MainWindow* win, int backend);
void HandleCmdClearHistory(MainWindow* win);
void HandleCmdRemoveDeletedFilesFromHistory(MainWindow* win);
void HandleCmdDeleteCachedFiles(MainWindow* win);
void HandleCmdReopenLastClosedFile(MainWindow* win);
void HandleCmdShowLog(MainWindow* win);
void HandleCmdScreenshot(MainWindow* win);
void HandleCmdSetScreenshotHotkey(MainWindow* win);
void HandleCmdCropImage(MainWindow* win);
void HandleCmdResizeImage(MainWindow* win);
void HandleCmdConvertImageToPdf(MainWindow* win);
void HandleCmdListPrinters(MainWindow* win);
void HandleCmdNextPrevTab(MainWindow* win, bool reverse);
void HandleCmdNextPrevTabSmart(MainWindow* win, int cmdId);
void HandleCmdMoveTab(MainWindow* win, int dir);
void HandleCmdCloseAllTabs(MainWindow* win);
void HandleCmdCloseTabs(MainWindow* win, int cmdId);
void HandleCmdExit(MainWindow* win);
void HandleCmdReloadDocument(MainWindow* win);
void HandleCmdCreateShortcutToFile(MainWindow* win);
void HandleCmdZoomFitWidthAndContinuous(MainWindow* win);
void HandleCmdZoomFitPageAndSinglePage(MainWindow* win);
void HandleCmdPasteClipboardImage(MainWindow* win);
// @gen-end cmd-file-decls