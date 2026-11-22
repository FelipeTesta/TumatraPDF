#ifndef ProfileSyncWnd_h
#define ProfileSyncWnd_h

struct MainWindow;

void ShowProfileSyncDialog(MainWindow* win);

// Dev-only: force-launch the import prompt (bypasses the -for-testing skip).
// Used by the TestProfileImportPrompt control-pipe command.
void TestLaunchImportPrompt(MainWindow* owner);

// Reusable profile-sync API (no modal dialogs, no gForTesting gate).
// Doc comments on the definitions in ProfileSyncWnd.cpp.
bool ProfileSyncSetFolderToPath(MainWindow* win, const char* path);
bool ProfileSyncExportToPath(MainWindow* win, const char* destDir, bool overwrite);
bool ProfileSyncImportFromPath(MainWindow* win, const char* srcPath, bool replace);

#endif
