/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void ShowTrimConfigDialog(MainWindow* win);
void TrimConfigDialogSyncEdits();

// SafeDeleteTrimConfigDialog: posts a deferred deletion task for the trim config
// dialog window. Called from the dialog's OnClose/OnDestroy handlers and from
// OnSave/OnCancel to clean up after the dialog closes.

struct TrimConfigWnd : Wnd {
    MainWindow* win = nullptr;
    HFONT font = nullptr;
    Edit* editTop = nullptr;
    Edit* editBottom = nullptr;
    Edit* editColGap = nullptr;
    Button* btnSave = nullptr;
    Button* btnReset = nullptr;
    Button* btnCancel = nullptr;
    int savedTop = 0, savedBottom = 0; // values at dialog open (for cancel)
    int savedColGap = 0;               // two-column v2 column overlap (for cancel)
    bool suppressEditUpdate = false;   // prevents feedback loop when syncing edits
    bool isDialog = false;
    bool hadTrimEnabled = false; // trim was on at dialog open (temporarily disabled, s29)

    bool Create(MainWindow* mainWin);
    void OnEditTopChanged();
    void OnEditBottomChanged();
    void OnEditColGapChanged();
    void OnReset();
    void OnSave();
    void OnCancel();
    void SyncEditsFromLine();
    void ScheduleDelete();
    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
};