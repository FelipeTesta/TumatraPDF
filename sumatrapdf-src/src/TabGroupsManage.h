/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void ShowSaveTabGroupDialog(MainWindow* win);
void ShowOpenTabGroupDialog(MainWindow* win);

// Tab system T1: live auto-sync of active tab group
TabGroup* FindTabGroup(const char* name);
void SyncGroupFromWindow(const char* name, MainWindow* win);
void SetActiveTabGroup(MainWindow* win, const char* name);
void UpdateActiveGroupAutoSave(MainWindow* win);
void SuppressGroupAutoSave(bool suppress);
// Tab system T3: activate a group in a specific window (close current tabs, load group files)
void ActivateTabGroupInWindow(MainWindow* win, const char* name);
