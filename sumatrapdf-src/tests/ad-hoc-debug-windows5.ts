// Debug: walk canvas children to find toolbar/rebar
import { launchSumatra, waitForFrame } from "./win-automation";
import { getWindowRect, getClassName, getWindow, sleep } from "./winapi";
import { runStandalone } from "./util";

const GW_CHILD = 5;
const GW_HWNDNEXT = 2;

function walkChildren(parent: number, indent: string, maxDepth: number): void {
  if (maxDepth <= 0) return;
  let child = getWindow(parent, GW_CHILD);
  let idx = 0;
  while (child && idx < 50) {
    const cls = getClassName(child);
    const r = getWindowRect(child);
    console.log(`${indent}  Child ${idx}: hwnd=0x${child.toString(16)} class="${cls}" rect=(${r.left},${r.top},${r.right},${r.bottom})`);
    walkChildren(child, indent + "    ", maxDepth - 1);
    child = getWindow(child, GW_HWNDNEXT);
    idx++;
  }
}

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  // Immediate child walk
  let child = getWindow(frame, GW_CHILD);
  if (!child) {
    console.log("No frame children!");
    proc.kill();
    return;
  }
  const canvasCls = getClassName(child);
  console.log(`Frame child 0: class="${canvasCls}"`);

  // Toggle Arch Tools ON first to create the rebar/toolbar
  const { sendCommand } = await import("./win-automation");
  const { cmdId } = await import("./util");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  console.log("\n=== Canvas children after Arch Tools ON ===");
  walkChildren(child, "", 3);

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
