// Busy-wait instead of sleep to keep FFI state valid after waiting.
// Finds toolbar HWNDs and samples pixels from their DCs.
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import {
  getWindowRect,
  enumChildWindows,
  getClassName,
  readWindowDCColumn,
  captureWindowPixels,
  CLR_INVALID,
} from "./winapi";
import { runStandalone, cmdId } from "./util";

const TMP = "tests/tmp/";

function busyWait(ms: number) {
  const start = Date.now();
  while (Date.now() - start < ms) {}
}

function countNonBg(pixels: number[]): number {
  return pixels.filter(p => p !== CLR_INVALID && p !== 0 && p !== 0xff000000).length;
}

function findToolbars(frame: number): number[] {
  const out: number[] = [];
  enumChildWindows(frame, (hwnd: number) => {
    if (getClassName(hwnd) === "ToolbarWindow32") {
      out.push(hwnd);
    }
    return true;
  });
  return out;
}

function sampleToolbar(tb: number, label: string): { hasContent: boolean; columnsWithContent: number } {
  const r = getWindowRect(tb);
  const w = r.right - r.left;
  const h = r.bottom - r.top;
  console.log(`\n${label}: hwnd=0x${tb.toString(16)}, ${w}x${h} at (${r.left},${r.top})`);

  const px = captureWindowPixels(tb);
  let pixelInfo = "";
  if (px) {
    let nonZero = 0;
    for (let i = 0; i < px.data.length; i += 16) {
      if (px.data[i] !== 0 || px.data[i + 1] !== 0 || px.data[i + 2] !== 0 || px.data[i + 3] !== 0) nonZero++;
    }
    pixelInfo = `, ${nonZero}/${Math.floor(px.data.length / 16)} non-zero pixel samples`;
  }

  let columnsWithContent = 0;
  for (let x = 5; x < w - 5; x += 10) {
    const col = readWindowDCColumn(tb, x, 2, Math.max(1, h - 4));
    const nonBg = countNonBg(col);
    if (nonBg > 0) {
      columnsWithContent++;
    }
  }

  console.log(`${label}: ${columnsWithContent} columns with content${pixelInfo}`);
  return { hasContent: columnsWithContent > 0, columnsWithContent };
}

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  // Busy-wait instead of sleep
  busyWait(3000);

  // Find toolbars immediately (no sleep was used, so FFI should work)
  const toolbarsBefore = findToolbars(frame);
  console.log(`\nToolbars before toggle: ${toolbarsBefore.length}`);
  for (const tb of toolbarsBefore) {
    const r = getWindowRect(tb);
    console.log(`  hwnd=0x${tb.toString(16)} rect=(${r.left},${r.top},${r.right},${r.bottom})`);
  }

  // Toggle Arch Tools ON
  console.log("\nToggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  busyWait(3000);

  // Find toolbars after toggle
  const toolbarsAfter = findToolbars(frame);
  console.log(`\nToolbars after Arch toggle: ${toolbarsAfter.length}`);
  for (const tb of toolbarsAfter) {
    const r = getWindowRect(tb);
    console.log(`  hwnd=0x${tb.toString(16)} rect=(${r.left},${r.top},${r.right},${r.bottom})`);
  }

  // Sample pixels from secondary toolbars (y > 50)
  const secondaryBefore = toolbarsBefore.filter(tb => getWindowRect(tb).top > 50);
  const secondaryAfter = toolbarsAfter.filter(tb => getWindowRect(tb).top > 50);

  let archHasContent = false;
  for (const tb of secondaryBefore) {
    const result = sampleToolbar(tb, "Arch toolbar (before toggle)");
    archHasContent = archHasContent || result.hasContent;
  }
  for (const tb of secondaryAfter) {
    const result = sampleToolbar(tb, "Arch toolbar (after toggle)");
    archHasContent = archHasContent || result.hasContent;
  }

  // Toggle Edit ON
  console.log("\nToggling Arch Tools OFF, then Edit ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  busyWait(1000);
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  busyWait(3000);

  const toolbarsEdit = findToolbars(frame);
  console.log(`\nToolbars after Edit toggle: ${toolbarsEdit.length}`);
  for (const tb of toolbarsEdit) {
    const r = getWindowRect(tb);
    console.log(`  hwnd=0x${tb.toString(16)} rect=(${r.left},${r.top},${r.right},${r.bottom})`);
  }

  const secondaryEdit = toolbarsEdit.filter(tb => getWindowRect(tb).top > 50);
  let editHasContent = false;
  for (const tb of secondaryEdit) {
    const result = sampleToolbar(tb, "Edit toolbar");
    editHasContent = editHasContent || result.hasContent;
  }

  proc.kill();

  console.log("\n=== RESULT ===");
  console.log(`Arch toolbar has content: ${archHasContent}`);
  console.log(`Edit toolbar has content: ${editHasContent}`);

  if (!archHasContent || !editHasContent) {
    throw new Error(`Toolbars may be blank: arch=${archHasContent}, edit=${editHasContent}`);
  }

  console.log("\n✅ Visual verification passed!");
}

if (import.meta.main) {
  await runStandalone(testit);
}
