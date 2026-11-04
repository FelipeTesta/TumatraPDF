import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone, cmdId } from "./util";
import { sleep } from "./winapi";
import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect, enumWindows } from "./winapi";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  console.log("=== Test: Edit toolbar visibility (debug) ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  await sleep(3000);
  
  // Toggle Arch Tools OFF first (conflict gate)
  console.log("Ensuring Arch Tools OFF...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(1000);
  
  // Toggle Edit ON
  console.log("Toggling Edit ON...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000); // longer wait
  
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\edit-toolbar-visible2.png");
  
  // Try enumWindows (top-level) to see all windows
  console.log("\n=== All top-level windows ===");
  enumWindows((hwnd: number) => {
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    if (cls.includes("Toolbar") || cls.includes("ReBar") || cls.includes("SUMATRA")) {
      console.log(`  ${hwnd}: class=${cls} rect=(${r.left},${r.top},${r.right},${r.bottom})`);
    }
    return true;
  });
  
  // Try enumChildWindows on frame
  console.log("\n=== Frame children ===");
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    const r = getWindowRect(hwnd);
    console.log(`  ${hwnd}: class=${cls} rect=(${r.left},${r.top},${r.right},${r.bottom})`);
    return true;
  });
  
  // Also check if there's a rebar child
  console.log("\n=== Looking for ReBar children ===");
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    if (cls.includes("ReBar") || cls.includes("Toolbar")) {
      const r = getWindowRect(hwnd);
      console.log(`  ${hwnd}: class=${cls} rect=(${r.left},${r.top},${r.right},${r.bottom})`);
      // Check its children
      enumChildWindows(hwnd, (child: number) => {
        const ccls = getClassName(child);
        const cr = getWindowRect(child);
        console.log(`    child ${child}: class=${ccls} rect=(${cr.left},${cr.top},${cr.right},${cr.bottom})`);
        return true;
      });
    }
    return true;
  });
  
  proc.kill();
  
  console.log("\n✅ Debug test completed!");
  console.log("Check screenshot: tests/tmp/edit-toolbar-visible2.png");
}

if (import.meta.main) {
  await runStandalone(testit);
}