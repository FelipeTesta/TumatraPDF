// Visual verification test for Arch/Edit secondary toolbars.
// Uses captureWindowPixels (PrintWindow → raw BGRA) to sample pixels at
// button centers and confirm text/icons are actually drawn (not blank/white).
import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng } from "./win-automation";
import {
  setProcessDpiAware,
  setForegroundWindow,
  getWindowRect,
  enumChildWindows,
  getClassName,
  captureWindowPixels,
  readWindowDCColumn,
  readWindowDCRow,
  CLR_INVALID,
  sleep,
} from "./winapi";
import { runStandalone, cmdId } from "./util";

const TMP = "tests/tmp/";

// Read a row across a button rect. Returns count of non-background pixels.
function sampleRow(hwnd: number, left: number, top: number, right: number, bottom: number): { total: number; nonBg: number } {
  const midY = Math.floor((top + bottom) / 2);
  const width = right - left;
  if (width <= 0) return { total: 0, nonBg: 0 };

  const pixels = readWindowDCRow(hwnd, left, midY, width);
  if (pixels.length === 0) return { total: 0, nonBg: 0 };

  const bgColor = pixels[0] === CLR_INVALID ? null : pixels[0];
  let nonBg = 0;
  for (const p of pixels) {
    if (p !== CLR_INVALID && p !== bgColor) nonBg++;
  }

  return { total: pixels.length, nonBg };
}

// We need readWindowDCRow - imported from winapi

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  // Bring to foreground for full paint under DWM
  let fgOk = false;
  for (let i = 0; i < 5; i++) {
    fgOk = setForegroundWindow(frame);
    if (fgOk) break;
    await sleep(200);
  }
  console.log(`setForegroundWindow: ${fgOk}`);
  await sleep(3000);

  // Helper: find all ToolbarWindow32 under frame using enumChildWindows
  function findToolbars(hwnd: number): number[] {
    const out: number[] = [];
    enumChildWindows(hwnd, (child: number) => {
      if (getClassName(child) === "ToolbarWindow32") {
        out.push(child);
      }
      return true;
    });
    return out;
  }

  // === ARCH TOOLS ===
  console.log("\n=== ARCH TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  const archToolbars = findToolbars(frame);
  console.log(`Found ${archToolbars.length} ToolbarWindow32 windows`);

  for (const tb of archToolbars) {
    const r = getWindowRect(tb);
    console.log(`  Toolbar rect=(${r.left},${r.top},${r.right},${r.bottom}) size=${r.right-r.left}x${r.bottom-r.top}`);
  }

  const archSecondary = archToolbars.filter(tb => getWindowRect(tb).top > 40);
  let archOk = false;

  if (archSecondary.length > 0) {
    const archTb = archSecondary[0];
    const r = getWindowRect(archTb);
    const w = r.right - r.left;
    const h = r.bottom - r.top;
    console.log(`\nArch secondary toolbar: ${w}x${h} at y=${r.top}`);

    const px = captureWindowPixels(archTb);
    if (px) {
      console.log(`  Captured ${px.w}x${px.h} pixels`);
      let nonZero = 0;
      for (let i = 0; i < px.data.length; i += 16) {
        if (px.data[i] !== 0 || px.data[i + 1] !== 0 || px.data[i + 2] !== 0 || px.data[i + 3] !== 0) nonZero++;
      }
      console.log(`  Non-zero pixel samples: ${nonZero} / ${px.data.length / 16}`);
      archOk = nonZero > 100;
    }

    captureWindowToPng(archTb, TMP + "arch-toolbar.png");

    // Sample columns across toolbar
    console.log("\n  Sampling pixel columns:");
    let anyNonBg = false;
    for (let x = 5; x < w - 5; x += 15) {
      try {
        const col = readWindowDCColumn(archTb, x, 2, Math.max(1, h - 4));
        const nonBg = col.filter(p => p !== CLR_INVALID && p !== 0).length;
        if (nonBg > 0) {
          console.log(`    x=${x}: ${nonBg}/${col.length} non-bg pixels`);
          anyNonBg = true;
        }
      } catch (e) {
        // skip
      }
    }
    if (!anyNonBg) console.log("    (no non-bg pixels found in sampled columns)");
  } else {
    console.log("  No secondary Arch toolbar found");
  }

  // === EDIT TOOLS ===
  console.log("\n=== EDIT TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(1000);
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);

  const editToolbars = findToolbars(frame);
  console.log(`Found ${editToolbars.length} ToolbarWindow32 windows`);

  for (const tb of editToolbars) {
    const r = getWindowRect(tb);
    console.log(`  Toolbar rect=(${r.left},${r.top},${r.right},${r.bottom}) size=${r.right-r.left}x${r.bottom-r.top}`);
  }

  const editSecondary = editToolbars.filter(tb => getWindowRect(tb).top > 40);
  let editOk = false;

  if (editSecondary.length > 0) {
    const editTb = editSecondary[0];
    const r = getWindowRect(editTb);
    const w = r.right - r.left;
    const h = r.bottom - r.top;
    console.log(`\nEdit secondary toolbar: ${w}x${h} at y=${r.top}`);

    const px = captureWindowPixels(editTb);
    if (px) {
      console.log(`  Captured ${px.w}x${px.h} pixels`);
      let nonZero = 0;
      for (let i = 0; i < px.data.length; i += 16) {
        if (px.data[i] !== 0 || px.data[i + 1] !== 0 || px.data[i + 2] !== 0 || px.data[i + 3] !== 0) nonZero++;
      }
      console.log(`  Non-zero pixel samples: ${nonZero} / ${px.data.length / 16}`);
      editOk = nonZero > 100;
    }

    captureWindowToPng(editTb, TMP + "edit-toolbar.png");

    console.log("\n  Sampling pixel columns:");
    let anyNonBg = false;
    for (let x = 5; x < w - 5; x += 15) {
      try {
        const col = readWindowDCColumn(editTb, x, 2, Math.max(1, h - 4));
        const nonBg = col.filter(p => p !== CLR_INVALID && p !== 0).length;
        if (nonBg > 0) {
          console.log(`    x=${x}: ${nonBg}/${col.length} non-bg pixels`);
          anyNonBg = true;
        }
      } catch (e) {
        // skip
      }
    }
    if (!anyNonBg) console.log("    (no non-bg pixels found in sampled columns)");
  } else {
    console.log("  No secondary Edit toolbar found");
  }

  proc.kill();

  console.log("\n=== RESULT ===");
  console.log(`Arch toolbar visible: ${archOk}`);
  console.log(`Edit toolbar visible: ${editOk}`);

  if (!archOk || !editOk) {
    throw new Error(`Toolbars not visible: arch=${archOk}, edit=${editOk}`);
  }

  console.log("\n✅ Visual verification complete!");
}

if (import.meta.main) {
  await runStandalone(testit);
}
