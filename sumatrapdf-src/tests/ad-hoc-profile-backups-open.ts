import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import { enumWindows, getWindowText, isWindowVisible, WM_CLOSE, postMessage } from "./winapi";

const CMD_PROFILE_BACKUPS = 527;

function findWindowByTitle(sub: string): number {
  let found = 0;
  enumWindows((hwnd) => {
    const title = getWindowText(hwnd) || "";
    if (title.includes(sub) && isWindowVisible(hwnd)) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function testit() {
  const proc = launchSumatra([]);
  const pid = proc.pid;
  console.log("Launched pid", pid);
  const frame = await waitForFrame(pid, 15000);
  console.log("Frame hwnd", frame);
  sendCommand(frame, CMD_PROFILE_BACKUPS);
  // Poll for dialog title "Profile Backups"
  let dlg = 0;
  for (let i = 0; i < 50; i++) {
    dlg = findWindowByTitle("Profile Backups");
    if (dlg) break;
    await new Promise(r => setTimeout(r, 100));
  }
  if (!dlg) throw new Error("Backup dialog not opened");
  console.log("Backup dialog hwnd", dlg);
  // Close dialog
  postMessage(dlg, WM_CLOSE, 0, 0);
  await new Promise(r => setTimeout(r, 500));
  proc.kill();
  console.log("PASS");
}

if (import.meta.main) {
  await testit();
}
