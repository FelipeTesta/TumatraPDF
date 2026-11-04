#ifndef ProfileBackupWnd_h
#define ProfileBackupWnd_h

struct MainWindow;

void ShowProfileBackupsDialog(MainWindow* win);
void ProfileDataPromptCloudRevert(MainWindow* win, HWND hwndParent);

#endif
