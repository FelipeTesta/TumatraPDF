import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng, findChildByClass } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect, isWindowVisible, sleep } from "./winapi";
import { runStandalone, cmdId } from "./util";

const TMP = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\";

function dumpAll(frame: number, label: string) {
  console.log(`\n=== ${label} (frame=${frame}) ===`);
  let n = 0;
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    n++;
    const r = getWindowRect(hwnd);
    const c = getClientRect(hwnd);
    const vis = isWindowVisible(hwnd);
    console.log(
      `  [${n}] hwnd=${hwnd} class="${cls}" vis=${vis} win=(${r.left},${r.top},${r.right},${r.bottom}) client=${c.right - c.left}x${c.bottom - c.top}`,
    );
    return true;
  });
  console.log(`  total children=${n}`);
}

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame: ${frame}`);
  
  await sleep(3000);
  dumpAll(frame, "BASELINE - all children");
  captureWindowToPng(frame, TMP + "vis-baseline.png");

  // Test Arch Tools ON
  console.log("\n>>> Toggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(2000);
  dumpAll(frame, "ARCH ON - all children");
  captureWindowToPng(frame, TMP + "vis-arch-on.png");

  // Test Edit ON
  console.log("\n>>> Toggling Edit ON...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(2000);
  dumpAll(frame, "EDIT ON - all children");
  captureWindowToPng(frame, TMP + "vis-edit-on.png");

  proc.kill();
  console.log("\n\u2705 Visibility test complete.");
}

if (import.meta.main) {
  await runStandalone(testit);
}