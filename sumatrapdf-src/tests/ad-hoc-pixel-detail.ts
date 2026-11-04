import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import { captureWindowDCToPng, getWindowRect, sleep, enumChildWindows, getClassName, sendMessage } from "./winapi";
import { runStandalone, cmdId } from "./util";
import { readFileSync } from "node:fs";

const TMP = "tests/tmp/";

const WM_USER = 0x0400;
const TB_GETITEMRECT_MSG = WM_USER + 0x2D;
const TB_BUTTONCOUNT_MSG = WM_USER + 0x1A;

function tbButtonCount(hwnd: number): number {
  const res = sendMessage(hwnd, TB_BUTTONCOUNT_MSG, 0n, 0n);
  return Number(res);
}

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame: ${frame}`);
  await sleep(3000);

  // Try to bring window to foreground
  try {
    const { setForegroundWindow } = await import("./winapi");
    setForegroundWindow(frame);
    await sleep(500);
  } catch (e) {
    console.log(`setForegroundWindow: ${e}`);
  }

  // List ALL ToolbarWindow32 windows
  const toolbars: number[] = [];
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    if (cls === "ToolbarWindow32") {
      toolbars.push(hwnd);
      const r = getWindowRect(hwnd);
      const count = tbButtonCount(hwnd);
      console.log(`ToolbarWindow32 hwnd=0x${hwnd.toString(16)} rect=(${r.left},${r.top},${r.right},${r.bottom}) size=${r.right-r.left}x${r.bottom-r.top} btnCount=${count}`);
    }
    return true;
  });
  console.log(`Total toolbars found: ${toolbars.length}`);

  // === Arch Tools ===
  console.log("\n=== ARCH TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  const archToolbars: number[] = [];
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    if (cls === "ToolbarWindow32") {
      archToolbars.push(hwnd);
      const r = getWindowRect(hwnd);
      const count = tbButtonCount(hwnd);
      console.log(`[after toggle] ToolbarWindow32 hwnd=0x${hwnd.toString(16)} rect=(${r.left},${r.top},${r.right},${r.bottom}) size=${r.right-r.left}x${r.bottom-r.top} btnCount=${count}`);
    }
    return true;
  });

  // Find Arch toolbar (2nd ToolbarWindow32, typically at y~105)
  let archTb = 0;
  if (archToolbars.length >= 2) {
    archTb = archToolbars[1]; // 2nd toolbar is typically the secondary one
  } else if (archToolbars.length === 1) {
    archTb = archToolbars[0];
  }

  if (archTb) {
    const count = tbButtonCount(archTb);
    console.log(`Arch toolbar button count: ${count}`);
    const rect = getWindowRect(archTb);
    console.log(`Arch toolbar rect: (${rect.left},${rect.top},${rect.right},${rect.bottom}) size=${rect.right-rect.left}x${rect.bottom-rect.top}`);

    const archOk = captureWindowDCToPng(archTb, TMP + "arch-full.png");
    console.log(`Arch full capture: ${archOk}`);

    try {
      const buf = readFileSync(TMP + "arch-full.png");
      console.log(`Arch PNG size: ${buf.length} bytes`);
    } catch (e) {
      console.log(`Arch PNG size: unknown`);
    }
  } else {
    console.log("Arch toolbar NOT found");
  }

  // === Edit Tools ===
  console.log("\n=== EDIT TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(1000);
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);

  const editToolbars: number[] = [];
  enumChildWindows(frame, (hwnd: number) => {
    const cls = getClassName(hwnd);
    if (cls === "ToolbarWindow32") {
      editToolbars.push(hwnd);
      const r = getWindowRect(hwnd);
      const count = tbButtonCount(hwnd);
      console.log(`[after edit toggle] ToolbarWindow32 hwnd=0x${hwnd.toString(16)} rect=(${r.left},${r.top},${r.right},${r.bottom}) size=${r.right-r.left}x${r.bottom-r.top} btnCount=${count}`);
    }
    return true;
  });

  let editTb = 0;
  if (editToolbars.length >= 2) {
    editTb = editToolbars[1];
  } else if (editToolbars.length === 1) {
    editTb = editToolbars[0];
  }

  if (editTb) {
    const count = tbButtonCount(editTb);
    console.log(`Edit toolbar button count: ${count}`);
    const rect = getWindowRect(editTb);
    console.log(`Edit toolbar rect: (${rect.left},${rect.top},${rect.right},${rect.bottom}) size=${rect.right-rect.left}x${rect.bottom-rect.top}`);

    const editOk = captureWindowDCToPng(editTb, TMP + "edit-full.png");
    console.log(`Edit full capture: ${editOk}`);

    try {
      const buf = readFileSync(TMP + "edit-full.png");
      console.log(`Edit PNG size: ${buf.length} bytes`);
    } catch (e) {
      console.log(`Edit PNG size: unknown`);
    }
  } else {
    console.log("Edit toolbar NOT found");
  }

  proc.kill();
  console.log("\nDone. Check arch-full.png and edit-full.png in tests/tmp/");
}

if (import.meta.main) {
  await runStandalone(testit);
}
