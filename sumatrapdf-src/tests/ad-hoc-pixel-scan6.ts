// Pixel verification using readWindowDCColumn (numbers-only FFI, works after sleep).
// Caches frame rect immediately, toggles toolbar, then samples pixels at toolbar position.
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import {
  setForegroundWindow,
  getWindowRect,
  readWindowDCColumn,
  CLR_INVALID,
  sleep,
} from "./winapi";
import { runStandalone, cmdId } from "./util";

const TMP = "tests/tmp/";

function countNonBg(pixels: number[]): number {
  return pixels.filter(p => p !== CLR_INVALID && p !== 0 && p !== 0xff000000).length;
}

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  // Get frame rect IMMEDIATELY (before any sleep - getWindowRect breaks after sleep)
  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);

  // Bring to foreground
  for (let i = 0; i < 5; i++) {
    if (setForegroundWindow(frame)) break;
    await sleep(200);
  }
  await sleep(2000);

  // Toolbar position within frame window coords
  // Screen rect: (452,105,1085,129), Frame rect: (450,0,...)
  // Window coords: x=452-450=2, y=105-0=105
  const tbX = 452 - frameRect.left;
  const tbY = 105 - frameRect.top;
  const tbBottom = 129 - frameRect.top;
  const tbMidY = Math.floor((tbY + tbBottom) / 2);
  const tbWidth = (1085 - frameRect.left) - tbX;

  console.log(`Toolbar area in frame coords: x=${tbX}..${tbX + tbWidth}, y=${tbY}..${tbBottom}, midY=${tbMidY}`);

  // === ARCH TOOLS ===
  console.log("\n=== ARCH TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  let archColumnsWithContent = 0;
  let archTotalNonBg = 0;
  for (let x = tbX + 10; x < tbX + tbWidth - 10; x += 15) {
    const col = readWindowDCColumn(frame, x, tbMidY - 3, 6);
    const nonBg = countNonBg(col);
    if (nonBg > 0) {
      archColumnsWithContent++;
      archTotalNonBg += nonBg;
      if (archColumnsWithContent <= 3) {
        console.log(`  x=${x}: ${nonBg}/6 non-bg pixels`);
      }
    }
  }
  console.log(`Arch: ${archColumnsWithContent} columns with content, ${archTotalNonBg} total non-bg`);

  // === EDIT TOOLS ===
  console.log("\n=== EDIT TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(1000);
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);

  let editColumnsWithContent = 0;
  let editTotalNonBg = 0;
  for (let x = tbX + 10; x < tbX + tbWidth - 10; x += 15) {
    const col = readWindowDCColumn(frame, x, tbMidY - 3, 6);
    const nonBg = countNonBg(col);
    if (nonBg > 0) {
      editColumnsWithContent++;
      editTotalNonBg += nonBg;
      if (editColumnsWithContent <= 3) {
        console.log(`  x=${x}: ${nonBg}/6 non-bg pixels`);
      }
    }
  }
  console.log(`Edit: ${editColumnsWithContent} columns with content, ${editTotalNonBg} total non-bg`);

  proc.kill();

  console.log("\n=== RESULT ===");
  console.log(`Arch toolbar columns with content: ${archColumnsWithContent}`);
  console.log(`Edit toolbar columns with content: ${editColumnsWithContent}`);

  if (archColumnsWithContent > 0 && editColumnsWithContent > 0) {
    console.log("✅ Both toolbars have visible pixel content (not blank)");
  } else {
    console.log("⚠️  One or both toolbars may be blank");
  }

  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
