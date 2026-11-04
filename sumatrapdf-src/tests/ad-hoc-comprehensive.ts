import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect } from "./winapi";
import { sleep } from "./winapi";
import { EXE, runStandalone, cmdId } from "./util";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  console.log("=== Comprehensive Arch Tools Test ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame: ${frame}`);
  
  await sleep(5000);
  
  // 1. Baseline - main toolbar only
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\1-baseline.png");
  
  // 2. Toggle Arch Tools ON
  console.log("Toggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);
  
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\2-arch-on.png");
  
  // 3. Check all children for toolbar/rebar
  console.log("\n=== All children after Arch ON ===");
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    const client = getClientRect(hwnd);
    if (cls.includes("Toolbar") || cls.includes("ReBar") || cls.includes("Rebar")) {
      console.log(`  ${hwnd}: class=${cls} rect=(${r.left},${r.top},${r.right},${r.bottom}) client=(${client.left},${client.top},${client.right},${client.bottom})`);
    }
    return true;
  });
  
  // 4. Toggle Arch Tools OFF
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(2000);
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\3-arch-off.png");
  
  // 5. Toggle Edit ON
  console.log("\nToggling Edit ON...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);
  
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\4-edit-on.png");
  
  console.log("\n=== All children after Edit ON ===");
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    const client = getClientRect(hwnd);
    if (cls.includes("Toolbar") || cls.includes("ReBar") || cls.includes("Rebar")) {
      console.log(`  ${hwnd}: class=${cls} rect=(${r.left},${r.top},${r.right},${r.bottom}) client=(${client.left},${client.top},${client.right},${client.bottom})`);
    }
    return true;
  });
  
  proc.kill();
  
  console.log("\n✅ Check screenshots in tests/tmp/");
}

if (import.meta.main) {
  await runStandalone(testit);
}