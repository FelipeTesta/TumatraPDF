// Pixel verification using screen DC. Calculates toolbar position from actual frame rect.
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import {
  setProcessDpiAware,
  setForegroundWindow,
  getWindowRect,
  readWindowDCColumn,
  CLR_INVALID,
  sleep,
} from "./winapi";
import { runStandalone, cmdId } from "./util";

const TMP = "tests/tmp/";

function countNonMatching(pixels: number[], refColor: number): number {
  return pixels.filter(p => p !== CLR_INVALID && p !== refColor).length;
}

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  // Get frame rect IMMEDIATELY (screen coords)
  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);

  // Bring to foreground
  for (let i = 0; i < 5; i++) {
    if (setForegroundWindow(frame)) break;
    await sleep(200);
  }
  await sleep(2000);

  // Calculate toolbar screen position from frame rect
  // Toolbar window coords (relative to frame): x=2..635, y=105..129
  // Screen coords = frame rect + window coords
  const tbScreenX = frameRect.left + 2;
  const tbScreenY = frameRect.top + 105;
  const tbScreenRight = frameRect.left + 635;
  const tbScreenBottom = frameRect.top + 129;
  const tbScreenMidY = Math.floor((tbScreenY + tbScreenBottom) / 2);
  const tbScreenWidth = tbScreenRight - tbScreenX;

  console.log(`Toolbar screen area: x=${tbScreenX}..${tbScreenRight}, y=${tbScreenY}..${tbScreenBottom}, midY=${tbScreenMidY}`);

  // Get reference color from area just above toolbar (frame title bar area)
  const refCol = readWindowDCColumn(0, tbScreenX + 100, tbScreenY - 10, 1);
  const refColor = refCol[0] === CLR_INVALID ? 0 : refCol[0];
  console.log(`Reference color above toolbar: 0x${refColor.toString(16)}`);

  // Get reference color from area just below toolbar (canvas area)
  const refCol2 = readWindowDCColumn(0, tbScreenX + 100, tbScreenBottom + 10, 1);
  const refColor2 = refCol2[0] === CLR_INVALID ? 0 : refCol2[0];
  console.log(`Reference color below toolbar: 0x${refColor2.toString(16)}`);

  // === ARCH TOOLS ===
  console.log("\n=== ARCH TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  let archDiffColumns = 0;
  let archTotalDiff = 0;
  for (let x = tbScreenX + 10; x < tbScreenRight - 10; x += 15) {
    const col = readWindowDCColumn(0, x, tbScreenMidY - 3, 6);
    const diffFromRef = countNonMatching(col, refColor);
    const diffFromRef2 = countNonMatching(col, refColor2);
    const nonBg = Math.max(diffFromRef, diffFromRef2);
    if (nonBg > 0) {
      archDiffColumns++;
      archTotalDiff += nonBg;
      if (archDiffColumns <= 3) {
        console.log(`  x=${x}: ${nonBg}/6 diff pixels (samples: ${col.slice(0,6).map(p=>p.toString(16)).join(",")})`);
      }
    }
  }
  console.log(`Arch: ${archDiffColumns} columns differ from background, ${archTotalDiff} total`);

  // === EDIT TOOLS ===
  console.log("\n=== EDIT TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(1000);
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);

  let editDiffColumns = 0;
  let editTotalDiff = 0;
  for (let x = tbScreenX + 10; x < tbScreenRight - 10; x += 15) {
    const col = readWindowDCColumn(0, x, tbScreenMidY - 3, 6);
    const diffFromRef = countNonMatching(col, refColor);
    const diffFromRef2 = countNonMatching(col, refColor2);
    const nonBg = Math.max(diffFromRef, diffFromRef2);
    if (nonBg > 0) {
      editDiffColumns++;
      editTotalDiff += nonBg;
      if (editDiffColumns <= 3) {
        console.log(`  x=${x}: ${nonBg}/6 diff pixels (samples: ${col.slice(0,6).map(p=>p.toString(16)).join(",")})`);
      }
    }
  }
  console.log(`Edit: ${editDiffColumns} columns differ from background, ${editTotalDiff} total`);

  proc.kill();

  console.log("\n=== RESULT ===");
  console.log(`Arch toolbar columns with content: ${archDiffColumns}`);
  console.log(`Edit toolbar columns with content: ${editDiffColumns}`);

  if (archDiffColumns > 0 && editDiffColumns > 0) {
    console.log("✅ Both toolbars have visible pixel content (not blank)");
  } else {
    console.log("⚠️  One or both toolbars may be blank");
  }

  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
