// Debug: list all child windows under frame, minimal setup
import { launchSumatra, waitForFrame } from "./win-automation";
import { getWindowRect, enumChildWindows, getClassName, sleep } from "./winapi";
import { runStandalone } from "./util";

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  await sleep(3000);

  // List ALL children
  console.log("\n=== Children ===");
  let count = 0;
  enumChildWindows(frame, (hwnd: number) => {
    count++;
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    console.log(`  [${count}] hwnd=0x${hwnd.toString(16)} class="${cls}" rect=(${r.left},${r.top},${r.right},${r.bottom})`);
    return true;
  });
  console.log(`Total: ${count}`);

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
