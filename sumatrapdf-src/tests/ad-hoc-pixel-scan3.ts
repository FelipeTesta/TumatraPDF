// Find toolbar HWND immediately, then sample its DC directly
import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng } from "./win-automation";
import {
  setForegroundWindow,
  getWindowRect,
  enumChildWindows,
  getClassName,
  readWindowDCColumn,
  captureWindowPixels,
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

  // Find toolbars IMMEDIATELY (no sleep)
  const toolbarsBefore: number[] = [];
  enumChildWindows(frame, (hwnd: number) => {
    if (getClassName(hwnd) === "ToolbarWindow32") {
      toolbarsBefore.push(hwnd);
    }
    return true;
  });
  console.log(`Toolbars before toggle: ${toolbarsBefore.length}`);
  for (const tb of toolbarsBefore) {
    const r = getWindowRect(tb);
    console.log(`  hwnd=0x${tb.toString(16)} rect=(${r.left},${r.top},${r.right},${r.bottom})`);
  }

  // Bring to foreground
  for (let i = 0; i < 5; i++) {
    if (setForegroundWindow(frame)) break;
    await sleep(200);
  }
  await sleep(2000);

  // Toggle Arch Tools ON
  console.log("\nToggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  // Find toolbars after toggle - try multiple times
  let archToolbars: number[] = [];
  for (let attempt = 0; attempt < 3 && archToolbars.length === 0; attempt++) {
    enumChildWindows(frame, (hwnd: number) => {
      if (getClassName(hwnd) === "ToolbarWindow32") {
        archToolbars.push(hwnd);
      }
      return true;
    });
    if (archToolbars.length === 0) {
      console.log(`  Attempt ${attempt + 1}: 0 toolbars, retrying...`);
      await sleep(1000);
    }
  }
  console.log(`Toolbars after Arch toggle: ${archToolbars.length}`);
  for (const tb of archToolbars) {
    const r = getWindowRect(tb);
    console.log(`  hwnd=0x${tb.toString(16)} rect=(${r.left},${r.top},${r.right},${r.bottom})`);
  }

  // Sample pixels from each toolbar's own DC
  for (const tb of archToolbars) {
    const r = getWindowRect(tb);
    const w = r.right - r.left;
    const h = r.bottom - r.top;
    console.log(`\nSampling toolbar at 0x${tb.toString(16)}, ${w}x${h}`);

    // Try captureWindowPixels on the toolbar itself
    const px = captureWindowPixels(tb);
    if (px) {
      let nonZero = 0;
      for (let i = 0; i < px.data.length; i += 16) {
        if (px.data[i] !== 0 || px.data[i + 1] !== 0 || px.data[i + 2] !== 0 || px.data[i + 3] !== 0) nonZero++;
      }
      console.log(`  captureWindowPixels: ${px.w}x${px.h}, ${nonZero}/${px.data.length/16} non-zero samples`);
    }

    // Sample columns from toolbar's own DC
    let columnsWithContent = 0;
    for (let x = 5; x < w - 5; x += 10) {
      const col = readWindowDCColumn(tb, x, 2, Math.max(1, h - 4));
      const nonBg = countNonBg(col);
      if (nonBg > 0) {
        columnsWithContent++;
        if (columnsWithContent <= 3) {
          console.log(`  x=${x}: ${nonBg}/${col.length} non-bg`);
        }
      }
    }
    console.log(`  ${columnsWithContent} columns with content`);
  }

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
