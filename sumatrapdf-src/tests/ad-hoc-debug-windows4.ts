// Debug: immediate child walk without sleep
import { launchSumatra, waitForFrame } from "./win-automation";
import { getWindowRect, getClassName, getWindow } from "./winapi";
import { runStandalone } from "./util";

const GW_CHILD = 5;
const GW_HWNDNEXT = 2;

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  // Immediate child walk
  console.log("\n=== Immediate children ===");
  let child = getWindow(frame, GW_CHILD);
  let idx = 0;
  while (child && idx < 20) {
    const cls = getClassName(child);
    const r = getWindowRect(child);
    console.log(`  Child ${idx}: hwnd=0x${child.toString(16)} class="${cls}" rect=(${r.left},${r.top},${r.right},${r.bottom})`);
    child = getWindow(child, GW_HWNDNEXT);
    idx++;
  }

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
