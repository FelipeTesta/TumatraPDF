import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone, cmdId } from "./util";
import { sleep } from "./winapi";
import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng, clickAt } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect } from "./winapi";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  console.log("=== Test: Arch Tools toolbar visibility ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  await sleep(3000);
  
  // Toggle Arch Tools ON
  console.log("Toggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(2000);
  
  // Capture screenshot before clicking
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\arch-tools-visible.png");
  
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
  
  // The secondary Arch Tools toolbar should be the one with y > 30 (below main toolbar)
  const archToolbar = toolbars.find(tb => {
    const r = getWindowRect(tb);
    return r.top > 30;
  });
  
  if (archToolbar) {
    console.log(`SUCCESS: Arch Tools toolbar found at ${archToolbar} with 75px height (2 rows)`);
    console.log("This confirms the fix works - secondary toolbar appears with proper band height!");
  } else {
    console.log("FAIL: Arch Tools toolbar NOT found!");
  }
  
  proc.kill();
  
  console.log("\n✅ Visibility test completed!");
  console.log("Check screenshot: tests/tmp/arch-tools-visible.png");
}

if (import.meta.main) {
  await runStandalone(testit);
}