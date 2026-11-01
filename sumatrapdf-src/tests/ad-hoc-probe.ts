import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect, sleep, isWindowVisible } from "./winapi";
import { runStandalone, cmdId } from "./util";

const TMP = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\";

function dump(frame: number, label: string) {
  console.log(`\n=== ${label} (frame=${frame}) ===`);
  let n = 0;
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    if (!cls.includes("Toolbar") && !cls.includes("ReBar") && !cls.includes("Rebar")) return true;
    n++;
    const r = getWindowRect(hwnd);
    const c = getClientRect(hwnd);
    console.log(
      `  [${n}] hwnd=${hwnd} class="${cls}" vis=${isWindowVisible(hwnd)} win=(${r.left},${r.top},${r.right},${r.bottom}) client=${c.right - c.left}x${c.bottom - c.top}`,
    );
    return true;
  });
  console.log(`  toolbars/rebars=${n}`);
}

export async function testit(): Promise<void> {
  const pdf =
    "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";

  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame: ${frame}`);
  await sleep(5000);
  dump(frame, "baseline");

  console.log("\n>>> Toggling Edit ON (fresh, no arch)");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);
  console.log(`  exitCode=${proc.exitCode}`);
  dump(frame, "edit ON");
  captureWindowToPng(frame, TMP + "e1-edit-on.png");

  console.log("\n>>> Toggling Edit OFF");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(2000);
  console.log(`  exitCode=${proc.exitCode}`);
  dump(frame, "edit OFF");

  console.log("\n>>> Toggling Arch ON");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);
  console.log(`  exitCode=${proc.exitCode}`);
  dump(frame, "arch ON");
  captureWindowToPng(frame, TMP + "e2-arch-on.png");

  proc.kill();
}

if (import.meta.main) {
  await runStandalone(testit);
}
