// Verify pixel scanning works and find secondary toolbar position after toggle
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

function countDiff(pixels: number[], refColor: number): number {
  let n = 0;
  for (const p of pixels) {
    if (p !== CLR_INVALID && p !== refColor) n++;
  }
  return n;
}

function scanY(hwnd: number, x: number, y1: number, y2: number, refColor: number): { diffY: number[]; totalDiff: number } {
  const diffY: number[] = [];
  let totalDiff = 0;
  for (let y = y1; y < y2; y += 5) {
    const col = readWindowDCColumn(hwnd, x, y, 3);
    const diff = countDiff(col, refColor);
    if (diff > 0) {
      diffY.push(y);
      totalDiff += diff;
    }
  }
  return { diffY, totalDiff };
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

  const clientCol = readWindowDCColumn(0, frameRect.left + 100, frameRect.top + 300, 1);
  const clientColor = clientCol[0] === CLR_INVALID ? 0 : clientCol[0];
  console.log(`Client color: 0x${clientColor.toString(16)}`);

  const scanX = frameRect.left + 100;

  // Scan ALL Y positions from 20 to 200 to find toolbars
  console.log("\n=== Scanning all Y positions (before any toggle) ===");
  for (let y = 20; y < 200; y += 10) {
    const screenY = frameRect.top + y;
    const col = readWindowDCColumn(0, scanX, screenY, 3);
    const diff = countDiff(col, clientColor);
    if (diff > 0) {
      console.log(`  y=${screenY} (frame+y=${y}): ${diff}/3 diff pixels`);
    }
  }

  // Toggle Arch Tools ON
  console.log("\nToggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  console.log("\n=== Scanning all Y positions (after Arch Tools ON) ===");
  for (let y = 20; y < 200; y += 10) {
    const screenY = frameRect.top + y;
    const col = readWindowDCColumn(0, scanX, screenY, 3);
    const diff = countDiff(col, clientColor);
    if (diff > 0) {
      console.log(`  y=${screenY} (frame+y=${y}): ${diff}/3 diff pixels`);
    }
  }

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
