// Verify pixel scanning works by scanning at main toolbar position (known to have content)
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

function countNonMatching(pixels: number[], refColor: number): number {
  return pixels.filter(p => p !== CLR_INVALID && p !== refColor).count || 0;
}

// Fix: countNonMatching should be:
function countDiff(pixels: number[], refColor: number): number {
  let n = 0;
  for (const p of pixels) {
    if (p !== CLR_INVALID && p !== refColor) n++;
  }
  return n;
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

  // Get client area color (canvas background)
  const clientCol = readWindowDCColumn(0, frameRect.left + 100, frameRect.top + 200, 1);
  const clientColor = clientCol[0] === CLR_INVALID ? 0 : clientCol[0];
  console.log(`Client color: 0x${clientColor.toString(16)}`);

  // Scan at main toolbar position (y=30+frame.top, known to have content)
  const mainToolbarY = frameRect.top + 43; // middle of main toolbar
  console.log(`\nScanning main toolbar at y=${mainToolbarY}:`);
  let mainDiffCount = 0;
  for (let x = frameRect.left + 10; x < frameRect.right - 10; x += 20) {
    const col = readWindowDCColumn(0, x, mainToolbarY - 3, 6);
    const diff = countDiff(col, clientColor);
    if (diff > 0) {
      mainDiffCount++;
      if (mainDiffCount <= 5) {
        console.log(`  x=${x}: ${diff}/6 diff pixels (${col.map(p=>p.toString(16)).join(",")})`);
      }
    }
  }
  console.log(`Main toolbar: ${mainDiffCount} positions differ from client color`);

  // Scan at secondary toolbar position (y=105+frame.top)
  const secToolbarY = frameRect.top + 117;
  console.log(`\nScanning secondary toolbar area at y=${secToolbarY}:`);
  let secDiffCount = 0;
  for (let x = frameRect.left + 10; x < frameRect.right - 10; x += 20) {
    const col = readWindowDCColumn(0, x, secToolbarY - 3, 6);
    const diff = countDiff(col, clientColor);
    if (diff > 0) {
      secDiffCount++;
      if (secDiffCount <= 5) {
        console.log(`  x=${x}: ${diff}/6 diff pixels (${col.map(p=>p.toString(16)).join(",")})`);
      }
    }
  }
  console.log(`Secondary toolbar area: ${secDiffCount} positions differ from client color`);

  // Toggle Arch Tools ON
  console.log("\nToggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  console.log(`\nScanning secondary toolbar area at y=${secToolbarY} (after toggle):`);
  let secDiffCount2 = 0;
  for (let x = frameRect.left + 10; x < frameRect.right - 10; x += 20) {
    const col = readWindowDCColumn(0, x, secToolbarY - 3, 6);
    const diff = countDiff(col, clientColor);
    if (diff > 0) {
      secDiffCount2++;
      if (secDiffCount2 <= 5) {
        console.log(`  x=${x}: ${diff}/6 diff pixels (${col.map(p=>p.toString(16)).join(",")})`);
      }
    }
  }
  console.log(`Secondary toolbar area after toggle: ${secDiffCount2} positions differ from client color`);

  proc.kill();

  console.log("\n=== RESULT ===");
  console.log(`Main toolbar detectable: ${mainDiffCount > 0}`);
  console.log(`Secondary toolbar before toggle: ${secDiffCount > 0}`);
  console.log(`Secondary toolbar after toggle: ${secDiffCount2 > 0}`);

  if (mainDiffCount === 0) {
    console.log("⚠️  Main toolbar NOT detected - screen DC approach may not work");
  }

  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
