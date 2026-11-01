// Debug: enumerate children of canvas to find toolbar/rebar
import { launchSumatra, waitForFrame } from "./win-automation";
import { getWindowRect, getClassName, getWindow, sleep } from "./winapi";
import { runStandalone } from "./util";

const GW_CHILD = 5;
const GW_HWNDNEXT = 2;

function walkChildren(parent: number, indent: string): void {
  let child = getWindow(parent, GW_CHILD);
  let idx = 0;
  while (child && idx < 100) {
    const cls = getClassName(child);
    const r = getWindowRect(child);
    console.log(`${indent}  Child ${idx}: hwnd=0x${child.toString(16)} class="${cls}" rect=(${r.left},${r.top},${r.right},${r.bottom})`);
    // recurse
    walkChildren(child, indent + "    ");
    child = getWindow(child, GW_HWNDNEXT);
    idx++;
  }
}

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  await sleep(3000);

  console.log("\n=== Frame children ===");
  walkChildren(frame, "");

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
