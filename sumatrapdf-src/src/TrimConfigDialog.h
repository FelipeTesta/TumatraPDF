/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void ShowTrimConfigDialog(MainWindow* win);
void TrimConfigDialogSyncEdits();

// SafeDeleteTrimConfigDialog: posts a deferred deletion task for the trim config
// dialog window. Called from the dialog's OnClose/OnDestroy handlers and from
// OnSave/OnCancel to clean up after the dialog closes.