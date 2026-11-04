// Test captureWindowDCRegionToPng with explicit coordinates
import { launchSumatra, waitForFrame } from "./win-automation";
import {
  setProcessDpiAware,
  setForegroundWindow,
  getWindowRect,
  sleep,
  captureWindowDCRegionToPng,
} from "./winapi";
import { runStandalone } from "./util";
import { readFileSync } from "node:fs";

const TMP = "tests/tmp/";

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);
  const fw = frameRect.right - frameRect.left;
  const fh = frameRect.bottom - frameRect.top;

  for (let i = 0; i < 5; i++) {
    if (setForegroundWindow(frame)) break;
  }

  // Try capture immediately (no sleep yet)
  console.log("\n=== Capture immediately (before sleep) ===");
  const ok1 = captureWindowDCRegionToPng(frame, 0, 0, fw, fh, TMP + "cap1.png");
  console.log(`Capture 1: ${ok1}`);
  try {
    const buf1 = readFileSync(TMP + "cap1.png");
    console.log(`Cap1 size: ${buf1.length} bytes`);
  } catch { console.log("Cap1: file not found"); }

  await sleep(2000);

  // Try capture after sleep
  console.log("\n=== Capture after sleep ===");
  const ok2 = captureWindowDCRegionToPng(frame, 0, 0, fw, fh, TMP + "cap2.png");
  console.log(`Capture 2: ${ok2}`);
  try {
    const buf2 = readFileSync(TMP + "cap2.png");
    console.log(`Cap2 size: ${buf2.length} bytes`);
  } catch { console.log("Cap2: file not found"); }

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
