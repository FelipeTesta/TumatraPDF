import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect } from "./winapi";
import { sleep } from "./winapi";
import { EXE, runStandalone, cmdId } from "./util";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  console.log("=== Test: Edit toolbar (direct toggle) ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  await sleep(5000);
  
  // Toggle Edit ON directly (code handles turning off Arch/Flashcard)
  console.log("Toggling Edit ON...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);
  
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\edit-direct.png");
  
  console.log("\n=== Frame children after Edit ON ===");
  let childCount = 0;
  enumChildWindows(frame, (hwnd: number) => {
    childCount++;
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    const client = getClientRect(hwnd);
    if (cls.includes("Toolbar") || cls.includes("ReBar")) {
      console.log(`  Child ${childCount}: hwnd=${hwnd} class=${cls} rect=(${r.left},${r.top},${r.right},${r.bottom}) client=(${client.left},${client.top},${client.right},${client.bottom})`);
    }
    return true;
  });
  console.log(`Total children checked`);
  
  proc.kill();
  
  console.log("\n✅ Test completed!");
  console.log("Check screenshot: tests/tmp/edit-direct.png");
}

if (import.meta.main) {
  await runStandalone(testit);
}