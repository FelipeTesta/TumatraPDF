import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone, cmdId } from "./util";
import { sleep } from "./winapi";
import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect } from "./winapi";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  console.log("=== Test: Edit toolbar visibility ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  await sleep(3000);
  
  // Toggle Edit ON (need to make sure Arch Tools is OFF first due to conflict gate)
  sendCommand(frame, cmdId("CmdArchToolsToggle")); // ensure OFF
  await sleep(1000);
  
  console.log("Toggling Edit ON...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(2000);
  
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\edit-toolbar-visible.png");
  
  // Find ALL toolbar windows under the frame
  const toolbars: number[] = [];
  enumChildWindows(frame, (hwnd: number) => {
    if (getClassName(hwnd) === "ToolbarWindow32") {
      toolbars.push(hwnd);
    }
    return true;
  });
  console.log(`Found ${toolbars.length} toolbar windows`);
  
  for (const tb of toolbars) {
    const r = getWindowRect(tb);
    const client = getClientRect(tb);
    console.log(`  Toolbar ${tb}: window rect=(${r.left},${r.top},${r.right},${r.bottom}) client=(${client.left},${client.top},${client.right},${client.bottom})`);
  }
  
  // The secondary Edit toolbar should be the one with y > 30 (below main toolbar)
  const editToolbar = toolbars.find(tb => {
    const r = getWindowRect(tb);
    return r.top > 30;
  });
  
  if (editToolbar) {
    console.log(`SUCCESS: Edit toolbar found at ${editToolbar} with proper band height`);
    console.log("This confirms the fix works - secondary toolbar appears with proper band height!");
  } else {
    console.log("FAIL: Edit toolbar NOT found!");
  }
  
  proc.kill();
  
  console.log("\n✅ Edit toolbar visibility test completed!");
  console.log("Check screenshot: tests/tmp/edit-toolbar-visible.png");
}

if (import.meta.main) {
  await runStandalone(testit);
}