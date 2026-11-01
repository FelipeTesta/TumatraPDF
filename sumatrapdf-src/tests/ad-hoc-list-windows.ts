import { launchSumatra, waitForFrame } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, sleep, sendMessage } from "./winapi";
import { runStandalone } from "./util";

const WM_USER = 0x0400;
const TB_BUTTONCOUNT_MSG = WM_USER + 0x1A;

function tbButtonCount(hwnd: number): number {
  const res = sendMessage(hwnd, TB_BUTTONCOUNT_MSG, 0n, 0n);
  return Number(res);
}

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame: ${frame}`);
  await sleep(5000); // wait longer for full window creation

  // List ALL child windows recursively with their classes
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    const count = tbButtonCount(hwnd);
    console.log(`Child hwnd=0x${hwnd.toString(16)} class="${cls}" rect=(${r.left},${r.top},${r.right},${r.bottom}) btnCount=${count}`);
    return true;
  });

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
