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
bool IsGroupAutoSaveSuppressed();
// Tab system T3: activate a group in a specific window (close current tabs, load group files)
void ActivateTabGroupInWindow(MainWindow* win, const char* name);
// Per-window active tab group helpers (T3: active group is per-window; gGlobalPrefs->activeTabGroup is a persistence
// mirror only)
// Returned Str is a BORROWED view of win->activeTabGroup — callers MUST NOT free it and MUST NOT keep it across
// a SetWindowActiveTabGroup() call on that window.
Str GetWindowActiveTabGroup(MainWindow* win);
void SetWindowActiveTabGroup(MainWindow* win, const char* name);
