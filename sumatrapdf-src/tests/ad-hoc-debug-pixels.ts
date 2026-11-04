// Read raw pixels with sleep after foreground to let window paint
import { launchSumatra, waitForFrame } from "./win-automation";
import {
  setProcessDpiAware,
  setForegroundWindow,
  getWindowRect,
  readWindowDCColumn,
  CLR_INVALID,
  sleep,
} from "./winapi";
import { runStandalone } from "./util";

function pixelsToHex(pixels: number[]): string {
  return pixels.map(p => p === CLR_INVALID ? "INV" : p.toString(16)).join(",");
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
  await sleep(3000); // Let window paint fully

  console.log("\n=== Raw pixel values after sleep ===");
  
  const positions = [
    { x: frameRect.left + 50, y: frameRect.top + 5, label: "Title bar" },
    { x: frameRect.left + 100, y: frameRect.top + 43, label: "Main toolbar" },
    { x: frameRect.left + 100, y: frameRect.top + 100, label: "Canvas" },
    { x: frameRect.left + 100, y: frameRect.top + 117, label: "Secondary toolbar area" },
    { x: frameRect.right - 50, y: frameRect.top + 100, label: "Far right" },
  ];

  for (const pos of positions) {
    const px = readWindowDCColumn(0, pos.x, pos.y, 1);
    console.log(`${pos.label} (${pos.x},${pos.y}): ${pixelsToHex(px)}`);
  }

  // Also scan a row at main toolbar y position
  console.log("\n=== Scan row at main toolbar y=43 ===");
  for (let x = frameRect.left + 10; x < frameRect.right - 10; x += 20) {
    const px = readWindowDCColumn(0, x, frameRect.top + 43 - 3, 6);
    const nonBg = px.filter(p => p !== CLR_INVALID && p !== 0x48494c).length;
    if (nonBg > 0) {
      console.log(`  x=${x}: ${pixelsToHex(px)} [${nonBg} non-bg]`);
    }
  }

  // Scan row at secondary toolbar y=117
  console.log("\n=== Scan row at secondary toolbar y=117 ===");
  for (let x = frameRect.left + 10; x < frameRect.right - 10; x += 20) {
    const px = readWindowDCColumn(0, x, frameRect.top + 117 - 3, 6);
    const nonBg = px.filter(p => p !== CLR_INVALID && p !== 0x48494c).length;
    if (nonBg > 0) {
      console.log(`  x=${x}: ${pixelsToHex(px)} [${nonBg} non-bg]`);
    }
  }

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
