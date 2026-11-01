// Debug: list all child windows under frame after toggling Arch Tools
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import {
  setProcessDpiAware,
  setForegroundWindow,
  getWindowRect,
  enumChildWindows,
  getClassName,
  sleep,
} from "./winapi";
import { runStandalone, cmdId } from "./util";

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  // Bring to foreground
  let fgOk = false;
  for (let i = 0; i < 5; i++) {
    fgOk = setForegroundWindow(frame);
    if (fgOk) break;
    await sleep(200);
  }
  console.log(`setForegroundWindow: ${fgOk}`);
  await sleep(3000);

  // List ALL children before toggle
  console.log("\n=== Before toggle ===");
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    console.log(`  hwnd=0x${hwnd.toString(16)} class="${cls}" rect=(${r.left},${r.top},${r.right},${r.bottom})`);
    return true;
  });

  // Toggle Arch Tools ON
  console.log("\n=== Toggling Arch Tools ON ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  // List ALL children after toggle
  console.log("\n=== After toggle ===");
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    console.log(`  hwnd=0x${hwnd.toString(16)} class="${cls}" rect=(${r.left},${r.top},${r.right},${r.bottom})`);
    return true;
  });

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
