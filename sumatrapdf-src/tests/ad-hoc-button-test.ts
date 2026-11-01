import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng, clickAt, findChildByClass } from "./win-automation";
import { enumChildWindows, getClassName, getWindowRect, getClientRect, isWindowVisible, sleep } from "./winapi";
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
    const vis = isWindowVisible(hwnd);
    console.log(
      `  [${n}] hwnd=${hwnd} class="${cls}" vis=${vis} win=(${r.left},${r.top},${r.right},${r.bottom}) client=${c.right - c.left}x${c.bottom - c.top}`,
    );
    return true;
  });
  console.log(`  toolbars/rebars=${n}`);
}

async function testButtonClick(frame: number, cmdName: string, label: string) {
  console.log(`\n--- Testing ${label} (${cmdName}) ---`);
  sendCommand(frame, cmdId(cmdName));
  await sleep(1000);
  captureWindowToPng(frame, TMP + `btn-${cmdName.toLowerCase()}.png`);
  console.log(`  Screenshot saved: btn-${cmdName.toLowerCase()}.png`);
}

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame: ${frame}`);
  
  await sleep(3000);
  dump(frame, "BASELINE");
  captureWindowToPng(frame, TMP + "baseline.png");

  // Test Arch Tools ON
  console.log("\n>>> Toggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(2000);
  dump(frame, "ARCH ON");
  captureWindowToPng(frame, TMP + "arch-on.png");

  // Test each Arch Tools button
  const archButtons = [
    ["CmdArchScale", "Scale"],
    ["CmdArchMeasure", "Measure"],
    ["CmdArchMeasureRadius", "Radius"],
    ["CmdArchRefLine", "Ref Line"],
    ["CmdArchClear", "Clean Lines"],
    ["CmdArchResetScale", "Reset Scale"],
    ["CmdArchCleanAll", "Clean All"],
  ];
  
  for (const [cmd, label] of archButtons) {
    await testButtonClick(frame, cmd, `Arch ${label}`);
    dump(frame, `after Arch ${label}`);
  }

  // Test Arch Tools OFF
  console.log("\n>>> Toggling Arch Tools OFF...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(1500);
  dump(frame, "ARCH OFF");
  captureWindowToPng(frame, TMP + "arch-off.png");

  // Test Edit ON
  console.log("\n>>> Toggling Edit ON...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(2000);
  dump(frame, "EDIT ON");
  captureWindowToPng(frame, TMP + "edit-on.png");

  // Test each Edit button
  const editButtons = [
    ["CmdEditAddText", "+Text"],
    ["CmdEditAddImage", "+Img"],
    ["CmdEditImageCrop", "Crop"],
    ["CmdEditImageApply", "Apply"],
  ];
  
  for (const [cmd, label] of editButtons) {
    await testButtonClick(frame, cmd, `Edit ${label}`);
    dump(frame, `after Edit ${label}`);
  }

  // Test Edit OFF
  console.log("\n>>> Toggling Edit OFF...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(1500);
  dump(frame, "EDIT OFF");
  captureWindowToPng(frame, TMP + "edit-off.png");

  proc.kill();
  console.log("\n\u2705 Button test complete. Check screenshots in tests/tmp/");
}

if (import.meta.main) {
  await runStandalone(testit);
}