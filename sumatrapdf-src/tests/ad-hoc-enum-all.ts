// Enumerate ALL window classes immediately after waitForFrame to find rebar2
import { launchSumatra, waitForFrame } from "./win-automation";
import {
  getWindowRect,
  enumChildWindows,
  getClassName,
} from "./winapi";
import { runStandalone } from "./util";

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);

  // Enumerate ALL children (not just ToolbarWindow32)
  console.log("\n=== ALL child windows ===");
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
