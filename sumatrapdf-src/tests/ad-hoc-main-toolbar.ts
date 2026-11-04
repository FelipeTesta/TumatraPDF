import { launchSumatra, waitForFrame, captureWindowToPng } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect } from "./winapi";
import { sleep } from "./winapi";
import { EXE, runStandalone } from "./util";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  console.log("=== Test: Main toolbar visibility (no toggles) ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  const frameClass = getClassName(frame);
  console.log(`Frame class: ${frameClass}`);
  
  await sleep(5000); // wait for full init
  
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\main-toolbar.png");
  
  console.log("\n=== Frame children (no toggles) ===");
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
  console.log("Check screenshot: tests/tmp/main-toolbar.png");
}

if (import.meta.main) {
  await runStandalone(testit);
}