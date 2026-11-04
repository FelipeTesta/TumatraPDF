// Dense Y scan in secondary toolbar area (85-145) before and after toggle
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

  // Scan at multiple X positions to find toolbar
  const scanXs = [frameRect.left + 50, frameRect.left + 200, frameRect.left + 400, frameRect.left + 600];

  function scanArea(yOffset: number, label: string) {
    const screenY = frameRect.top + yOffset;
    let totalDiff = 0;
    let positionsWithDiff = 0;
    for (const sx of scanXs) {
      const col = readWindowDCColumn(0, sx, screenY, 3);
      const diff = countDiff(col, clientColor);
      if (diff > 0) {
        positionsWithDiff++;
        totalDiff += diff;
      }
    }
    console.log(`  ${label} y=${screenY} (frame+y=${yOffset}): ${positionsWithDiff}/${scanXs.length} X-positions differ, total=${totalDiff}`);
  }

  // === BEFORE TOGGLE ===
  console.log("\n=== BEFORE Arch Tools toggle ===");
  for (let y = 85; y <= 145; y += 5) {
    scanArea(y, "");
  }

  // Toggle Arch Tools ON
  console.log("\nToggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  // === AFTER TOGGLE ===
  console.log("\n=== AFTER Arch Tools toggle ===");
  for (let y = 85; y <= 145; y += 5) {
    scanArea(y, "");
  }

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
