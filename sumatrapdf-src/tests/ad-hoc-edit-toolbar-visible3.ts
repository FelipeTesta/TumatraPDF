import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone, cmdId } from "./util";
import { sleep } from "./winapi";
import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect } from "./winapi";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  console.log("=== Test: Edit toolbar visibility (debug v2) ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  await sleep(5000); // longer wait
  
  // Toggle Arch Tools OFF first (conflict gate)
  console.log("Ensuring Arch Tools OFF...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(2000);
  
  // Toggle Edit ON
  console.log("Toggling Edit ON...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000); // longer wait
  
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\edit-toolbar-visible3.png");
  
  // Try enumChildWindows on frame with detailed logging
  console.log("\n=== Frame children (with enumChildWindows) ===");
  let childCount = 0;
  enumChildWindows(frame, (hwnd: number) => {
    childCount++;
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    const client = getClientRect(hwnd);
    console.log(`  Child ${childCount}: hwnd=${hwnd} class=${cls} rect=(${r.left},${r.top},${r.right},${r.bottom}) client=(${client.left},${client.top},${client.right},${client.bottom})`);
    return true;
  });
  console.log(`Total children: ${childCount}`);
  
  proc.kill();
  
  console.log("\n✅ Debug test v2 completed!");
  console.log("Check screenshot: tests/tmp/edit-toolbar-visible3.png");
}

if (import.meta.main) {
  await runStandalone(testit);
}