/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct Edit;
struct DropDown;
struct Button;
struct Static;

struct ArchScaleDialogWnd : Wnd {
    MainWindow* win = nullptr;
    HFONT font = nullptr;
    Edit* editLen = nullptr;
    DropDown* comboUnit = nullptr;
    Button* btnDraw = nullptr;
    Button* btnOk = nullptr;
    Static* hint = nullptr;
    bool isDialog = false;

    bool Create(MainWindow* mainWin);
    void OnDrawLine();
    void OnOk();
    void OnClose();
    void SyncUnitFromWin();
    void ScheduleDelete();
    void SetLength(float realLen);
    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
};

void ShowArchScaleDialog(MainWindow* win);