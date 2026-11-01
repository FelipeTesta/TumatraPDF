import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone, cmdId } from "./util";
import { sleep } from "./winapi";
import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng, FRAME_CLASS } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect } from "./winapi";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  console.log("=== Test: Edit toolbar visibility (verify frame class) ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  // Verify frame class
  const frameClass = getClassName(frame);
  console.log(`Frame class: ${frameClass} (expected: ${FRAME_CLASS})`);
  
  await sleep(5000);
  
  // Toggle Arch Tools OFF first
  console.log("Ensuring Arch Tools OFF...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(2000);
  
  // Toggle Edit ON
  console.log("Toggling Edit ON...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);
  
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\edit-toolbar-verify.png");
  
  // Try enumChildWindows
  console.log("\n=== Frame children ===");
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
  
  console.log("\n✅ Test completed!");
}

if (import.meta.main) {
  await runStandalone(testit);
}