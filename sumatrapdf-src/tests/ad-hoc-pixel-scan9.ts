// Broad pixel scan: sample grid of pixels in the expected toolbar area
// using screen DC (works after sleep). Compares before/after toggle.
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

// Scan a horizontal line at y, from x1 to x2, step pxStep. Returns count of
// x positions where the column differs from refColor.
function scanLine(hwnd: number, x1: number, x2: number, y: number, pxStep: number, refColor: number): { diffX: number[]; totalDiff: number } {
  const diffX: number[] = [];
  let totalDiff = 0;
  for (let x = x1; x < x2; x += pxStep) {
    const col = readWindowDCColumn(hwnd, x, y - 2, 5);
    const diff = countNonMatching(col, refColor);
    if (diff > 0) {
      diffX.push(x);
      totalDiff += diff;
    }
  }
  return { diffX, totalDiff };
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
    await sleep(200);
  }
  await sleep(2000);

  // Get reference color from desktop area (well outside the app window)
  const refCol = readWindowDCColumn(0, 2000, 200, 1);
  const desktopColor = refCol[0] === CLR_INVALID ? 0 : refCol[0];
  console.log(`Desktop color at (2000,200): 0x${desktopColor.toString(16)}`);

  // Get reference color from frame client area (below toolbar area)
  const clientRefCol = readWindowDCColumn(0, frameRect.left + 100, frameRect.top + 200, 1);
  const clientColor = clientRefCol[0] === CLR_INVALID ? 0 : clientRefCol[0];
  console.log(`Client area color at (${frameRect.left + 100},${frameRect.top + 200}): 0x${clientColor.toString(16)}`);

  // The toolbar area is below the main toolbar (y~57) and above the canvas
  // Main toolbar is at frame y~30..57. Secondary toolbar at frame y~105..129.
  // Screen coords = frame rect + offset
  const toolbarY = frameRect.top + 105;
  const toolbarBottom = frameRect.top + 129;
  const toolbarMidY = Math.floor((toolbarY + toolbarBottom) / 2);
  const toolbarX1 = frameRect.left + 10;
  const toolbarX2 = frameRect.right - 10;

  console.log(`\nToolbar scan area: x=${toolbarX1}..${toolbarX2}, y=${toolbarMidY}`);

  // === BASELINE: before toggling Arch Tools ===
  console.log("\n=== BASELINE (Arch Tools OFF) ===");
  const baseResult = scanLine(0, toolbarX1, toolbarX2, toolbarMidY, 20, clientColor);
  console.log(`Baseline: ${baseResult.diffX.length} positions differ from client color, total diff=${baseResult.totalDiff}`);

  // === ARCH TOOLS ===
  console.log("\n=== ARCH TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  const archResult = scanLine(0, toolbarX1, toolbarX2, toolbarMidY, 20, clientColor);
  console.log(`Arch: ${archResult.diffX.length} positions differ from client color, total diff=${archResult.totalDiff}`);
  if (archResult.diffX.length > 0) {
    console.log(`  First few diff positions: ${archResult.diffX.slice(0,5)}`);
  }

  // Also scan at multiple Y positions within the toolbar height
  console.log("\n  Scanning multiple Y positions within toolbar:");
  for (let y = toolbarY + 2; y < toolbarBottom - 2; y += 8) {
    const r = scanLine(0, toolbarX1, toolbarX2, y, 30, clientColor);
    if (r.diffX.length > 0) {
      console.log(`    y=${y}: ${r.diffX.length} positions differ`);
    }
  }

  // === EDIT TOOLS ===
  console.log("\n=== EDIT TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(1000);
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);

  const editResult = scanLine(0, toolbarX1, toolbarX2, toolbarMidY, 20, clientColor);
  console.log(`Edit: ${editResult.diffX.length} positions differ from client color, total diff=${editResult.totalDiff}`);
  if (editResult.diffX.length > 0) {
    console.log(`  First few diff positions: ${editResult.diffX.slice(0,5)}`);
  }

  console.log("\n  Scanning multiple Y positions within toolbar:");
  for (let y = toolbarY + 2; y < toolbarBottom - 2; y += 8) {
    const r = scanLine(0, toolbarX1, toolbarX2, y, 30, clientColor);
    if (r.diffX.length > 0) {
      console.log(`    y=${y}: ${r.diffX.length} positions differ`);
    }
  }

  proc.kill();

  console.log("\n=== RESULT ===");
  const archOk = archResult.diffX.length > 2;
  const editOk = editResult.diffX.length > 2;
  console.log(`Arch toolbar visible: ${archOk} (${archResult.diffX.length} positions differ)`);
  console.log(`Edit toolbar visible: ${editOk} (${editResult.diffX.length} positions differ)`);

  if (!archOk || !editOk) {
    console.log("⚠️  Toolbars may be blank");
  } else {
    console.log("✅ Both toolbars have visible content");
  }

  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
