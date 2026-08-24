/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void ShowTrimConfigDialog(MainWindow* win);
void TrimConfigDialogSyncEdits(); // sync edit boxes from current trim values (called from Canvas during drag)