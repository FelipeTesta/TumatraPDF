// Test getWindowRect without setForegroundWindow
import { launchSumatra, waitForFrame } from "./win-automation";
import { getWindowRect, sleep } from "./winapi";
import { runStandalone } from "./util";

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame} type=${typeof frame}`);

  const r1 = getWindowRect(frame);
  console.log(`getWindowRect immediately: (${r1.left},${r1.top},${r1.right},${r1.bottom})`);

  await sleep(3000);

  const r2 = getWindowRect(frame);
  console.log(`getWindowRect after 3s: (${r2.left},${r2.top},${r2.right},${r2.bottom})`);

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
