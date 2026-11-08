#ifndef ProfileSyncWnd_h
#define ProfileSyncWnd_h

struct MainWindow;

void ShowProfileSyncDialog(MainWindow* win);

// Dev-only: force-launch the import prompt (bypasses the -for-testing skip).
// Used by the TestProfileImportPrompt control-pipe command.
void TestLaunchImportPrompt(MainWindow* owner);

#endif
