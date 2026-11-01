import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone, cmdId } from "./util";
import { sleep } from "./winapi";
import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng, clickAt } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect } from "./winapi";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  // Test Arch Tools buttons
  console.log("=== Test: Arch Tools buttons (click Scale, Measure) ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  await sleep(3000);
  
  // Toggle Arch Tools ON
  console.log("Toggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(2000);
  
  // Capture screenshot before clicking
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\arch-tools-before2.png");
  
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
    console.log(`Arch Tools toolbar: ${archToolbar}`);
    const tbRect = getClientRect(archToolbar);
    console.log(`Toolbar client rect: ${tbRect.left},${tbRect.top},${tbRect.right},${tbRect.bottom}`);
    
    // Click Scale button (first button on second row)
    const clickX = tbRect.left + 40;
    const clickY = tbRect.top + 10;
    console.log(`Clicking Scale at client (${clickX}, ${clickY})`);
    await clickAt(archToolbar, clickX, clickY);
    await sleep(500);
    
    // Click Measure button (second button on second row)
    const clickX2 = tbRect.left + 120;
    const clickY2 = tbRect.top + 10;
    console.log(`Clicking Measure at client (${clickX2}, ${clickY2})`);
    await clickAt(archToolbar, clickX2, clickY2);
    await sleep(500);
    
    captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\arch-tools-after-click2.png");
    console.log("Arch Tools clicks done!");
  } else {
    console.log("Arch Tools toolbar NOT found!");
  }
  
  proc.kill();
  
  console.log("\n✅ Arch Tools button click test completed!");
  console.log("Check screenshots in tests/tmp/");
}

if (import.meta.main) {
  await runStandalone(testit);
}