// Final visual verification: before/after toggle comparison + Edit Tools test.
// Uses synchronous SendMessage + immediate pixel read (no sleep between).
import { launchSumatra, waitForFrame } from "./win-automation";
import {
  setProcessDpiAware,
  setForegroundWindow,
  getWindowRect,
  readWindowDCColumn,
  CLR_INVALID,
  sendMessage,
} from "./winapi";
import { runStandalone, cmdId } from "./util";

const WM_COMMAND = 0x0111;
const TMP = "tests/tmp/";

function countDiff(pixels: number[], refColor: number): number {
  let n = 0;
  for (const p of pixels) {
    if (p !== CLR_INVALID && p !== refColor) n++;
  }
  return n;
}

function scanPosition(x: number, screenY: number, refColor: number): number {
  const col = readWindowDCColumn(0, x, screenY - 3, 6);
  return countDiff(col, refColor);
}

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);

  for (let i = 0; i < 5; i++) {
    if (setForegroundWindow(frame)) break;
  }

  const clientCol = readWindowDCColumn(0, frameRect.left + 100, frameRect.top + 300, 1);
  const clientColor = clientCol[0] === CLR_INVALID ? 0 : clientCol[0];
  console.log(`Client color: 0x${clientColor.toString(16)}`);

  const { cmdId } = await import("./util");
  const archToggleId = cmdId("CmdArchToolsToggle");
  const editToggleId = cmdId("CmdEditToolsToggle");
  console.log(`CmdArchToolsToggle id: ${archToggleId}, CmdEditToolsToggle id: ${editToggleId}`);

  // Secondary toolbar screen position
  const tbScreenX = frameRect.left + 2;
  const tbScreenRight = frameRect.left + 635;
  const tbScreenY = 105;
  const tbScreenBottom = 129;
  const tbScreenMidY = Math.floor((tbScreenY + tbScreenBottom) / 2);

  console.log(`\nToolbar screen area: x=${tbScreenX}..${tbScreenRight}, midY=${tbScreenMidY}`);

  // Scan function: count columns with content across the toolbar width
  function scanToolbar(screenY: number): { columns: number; totalDiff: number } {
    let columns = 0;
    let totalDiff = 0;
    for (let x = tbScreenX + 10; x < tbScreenRight - 10; x += 15) {
      const diff = scanPosition(x, screenY, clientColor);
      if (diff > 0) {
        columns++;
        totalDiff += diff;
      }
    }
    return { columns, totalDiff };
  }

  // === BASELINE: Arch Tools OFF ===
  console.log("\n=== BASELINE: Arch Tools OFF ===");
  const baseResult = scanToolbar(tbScreenMidY);
  console.log(`Secondary toolbar area: ${baseResult.columns} columns with content, total diff=${baseResult.totalDiff}`);

  // === ARCH TOOLS ON ===
  console.log("\n=== ARCH TOOLS ON ===");
  sendMessage(frame, WM_COMMAND, archToggleId, 0);
  const archResult = scanToolbar(tbScreenMidY);
  console.log(`Secondary toolbar area: ${archResult.columns} columns with content, total diff=${archResult.totalDiff}`);

  // Show some pixel samples
  console.log("Pixel samples at toolbar position:");
  for (let x = tbScreenX + 10; x < tbScreenX + 100; x += 20) {
    const col = readWindowDCColumn(0, x, tbScreenMidY - 3, 6);
    const nonBg = col.filter(p => p !== CLR_INVALID && p !== clientColor);
    if (nonBg.length > 0) {
      console.log(`  x=${x}: ${nonBg.map(p=>p.toString(16)).join(",")}`);
    }
  }

  const archNewContent = archResult.columns - baseResult.columns;
  console.log(`\nArch: NEW columns with content after toggle: ${archNewContent}`);

  // === EDIT TOOLS ON (Arch OFF first) ===
  console.log("\n=== EDIT TOOLS ON ===");
  sendMessage(frame, WM_COMMAND, archToggleId, 0); // Arch OFF
  sendMessage(frame, WM_COMMAND, editToggleId, 0); // Edit ON
  const editResult = scanToolbar(tbScreenMidY);
  console.log(`Secondary toolbar area: ${editResult.columns} columns with content, total diff=${editResult.totalDiff}`);

  console.log("Pixel samples at toolbar position:");
  for (let x = tbScreenX + 10; x < tbScreenX + 100; x += 20) {
    const col = readWindowDCColumn(0, x, tbScreenMidY - 3, 6);
    const nonBg = col.filter(p => p !== CLR_INVALID && p !== clientColor);
    if (nonBg.length > 0) {
      console.log(`  x=${x}: ${nonBg.map(p=>p.toString(16)).join(",")}`);
    }
  }

  const editNewContent = editResult.columns - baseResult.columns;
  console.log(`\nEdit: NEW columns with content after toggle: ${editNewContent}`);

  proc.kill();

  console.log("\n=== FINAL RESULT ===");
  console.log(`Baseline columns: ${baseResult.columns}`);
  console.log(`Arch columns: ${archResult.columns} (new: ${archNewContent})`);
  console.log(`Edit columns: ${editResult.columns} (new: ${editNewContent})`);

  const archOk = archNewContent > 0;
  const editOk = editNewContent > 0;
  console.log(`Arch toolbar has NEW visible content: ${archOk}`);
  console.log(`Edit toolbar has NEW visible content: ${editOk}`);

  if (!archOk) {
    console.log("⚠️  Arch toolbar: no new content detected after toggle");
  }
  if (!editOk) {
    console.log("⚠️  Edit toolbar: no new content detected after toggle");
  }
  if (archOk && editOk) {
    console.log("✅ Both toolbars show visible content after toggle!");
  }

  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
