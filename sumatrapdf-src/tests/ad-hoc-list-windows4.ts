import { launchSumatra } from "./win-automation";
import { waitForFrame } from "./win-automation";
import { getClassName, getWindowRect, sleep, getWindow, sendMessage, enumWindows } from "./winapi";
import { runStandalone } from "./util";

const GW_OWNER = 4;
const WM_USER = 0x0400;
const TB_BUTTONCOUNT_MSG = WM_USER + 0x1A;

function tbButtonCount(hwnd: number): number {
  const res = sendMessage(hwnd, TB_BUTTONCOUNT_MSG, 0n, 0n);
  return Number(res);
}

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const pid = proc.pid;
  
  const fhwnd = await waitForFrame(pid);
  console.log(`Frame HWND: ${fhwnd}`);
  
  // Enumerate ALL top-level windows and find ones owned by our frame
  console.log("\n=== All top-level windows ===");
  enumWindows((hwnd) => {
    const wpid = Number(sendMessage(hwnd, 0, 0, 0)); // can't get PID this way
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    const owner = getWindow(hwnd, 4); // GW_OWNER
    const count = tbButtonCount(hwnd);
    if (owner === fhwnd || cls.includes("Toolbar") || cls.includes("ReBar") || cls.includes("SUMATRA")) {
      console.log(`  hwnd=0x${hwnd.toString(16)} class="${cls}" rect=(${r.left},${r.top},${r.right},${r.bottom}) owner=0x${owner.toString(16)} btnCount=${count}`);
    }
    return true;
  });

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
