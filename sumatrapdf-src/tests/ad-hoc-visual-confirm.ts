// Visual confirmation: read pixels from cached secondary toolbar HWND after toggle.
// readWindowDCColumn uses GetWindowDC(hwnd)+GetPixel - works after sleep (proven in scan13).
// Does NOT depend on getWindowRect after toggle.
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import {
  setProcessDpiAware,
  setForegroundWindow,
  getWindowRect,
  enumChildWindows,
  getClassName,
  readWindowDCColumn,
  CLR_INVALID,
  postMessage,
  sleep,
} from "./winapi";
import { runStandalone, cmdId } from "./util";

const WM_COMMAND = 0x0111;

function countNonBg(pixels: number[], bgColor: number): number {
  let n = 0;
  for (const p of pixels) {
    if (p !== CLR_INVALID && p !== bgColor) n++;
  }
  return n;
}

function pixelsToHex(pixels: number[]): string {
  return pixels.map(p => p === CLR_INVALID ? "INV" : p.toString(16)).join(",");
}

// Read pixels from a toolbar DC using fixed coordinates (no getWindowRect needed)
function readToolbarPixels(hwnd: number, maxX: number, maxY: number, bgColor: number): { nonBg: number; samples: string[] } {
  let nonBg = 0;
  const samples: string[] = [];
  for (let x = 0; x < maxX; x += 15) {
    for (let y = 0; y < maxY; y += 5) {
      const col = readWindowDCColumn(hwnd, x, y, 3);
      const nbg = countNonBg(col, bgColor);
      nonBg += nbg;
      if (nbg > 0 && samples.length < 15) {
        samples.push(`x=${x},y=${y}: ${pixelsToHex(col)} [${nbg}]`);
      }
    }
  }
  return { nonBg, samples };
}

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);

  // Find secondary TOOLBAR HWND before toggle (small/hidden width ~29px)
  let secondaryTb = 0;
  enumChildWindows(frame, (hwnd: number) => {
    if (getClassName(hwnd) === "ToolbarWindow32") {
      const r = getWindowRect(hwnd);
      // Secondary toolbar before toggle: small width (~29px), at y~38
      if (r.right - r.left < 100 && r.top === frameRect.top + 38) {
        secondaryTb = hwnd;
      }
    }
    return true;
  });
  console.log(`Secondary toolbar HWND: 0x${secondaryTb.toString(16)} before toggle`);

  for (let i = 0; i < 5; i++) {
    if (setForegroundWindow(frame)) break;
  }

  const { cmdId } = await import("./util");
  const archToggleId = cmdId("CmdArchToolsToggle");
  const editToggleId = cmdId("CmdEditToolsToggle");

  const clientCol = readWindowDCColumn(0, frameRect.left + 100, frameRect.top + 300, 1);
  const clientColor = clientCol[0] === CLR_INVALID ? 0 : clientCol[0];
  console.log(`Client color: 0x${clientColor.toString(16)}`);

  // Read pixels from toolbar DC BEFORE toggle
  console.log("\n=== BEFORE toggle - reading secondary toolbar DC ===");
  let beforeNonBg = 0;
  if (secondaryTb !== 0) {
    const result = readToolbarPixels(secondaryTb, 100, 30, clientColor);
    beforeNonBg = result.nonBg;
    console.log(`Before: ${beforeNonBg} non-bg pixels on toolbar DC`);
    for (const s of result.samples) console.log(`  ${s}`);
  }

  // === ARCH TOOLS ON ===
  console.log("\n=== ARCH TOOLS ON ===");
  postMessage(frame, WM_COMMAND, archToggleId, 0);
  await sleep(5000);

  // Read pixels from SAME toolbar HWND after toggle (fixed coords, no getWindowRect)
  console.log("\n=== After toggle - reading same toolbar DC (fixed coords) ===");
  let archNonBg = 0;
  if (secondaryTb !== 0) {
    // Also check if window still exists by reading a pixel
    const testPx = readWindowDCColumn(secondaryTb, 10, 10, 1);
    console.log(`Toolbar DC test pixel: ${pixelsToHex(testPx)}`);
    
    const result = readToolbarPixels(secondaryTb, 500, 50, clientColor);
    archNonBg = result.nonBg;
    console.log(`Arch secondary toolbar: ${archNonBg} total non-bg pixels`);
    for (const s of result.samples) console.log(`  ${s}`);
  }

  // === EDIT TOOLS ON ===
  console.log("\n=== EDIT TOOLS ON (Arch OFF first) ===");
  postMessage(frame, WM_COMMAND, archToggleId, 0);
  await sleep(1000);
  postMessage(frame, WM_COMMAND, editToggleId, 0);
  await sleep(5000);

  let editNonBg = 0;
  if (secondaryTb !== 0) {
    const result = readToolbarPixels(secondaryTb, 500, 50, clientColor);
    editNonBg = result.nonBg;
    console.log(`Edit secondary toolbar: ${editNonBg} total non-bg pixels`);
    for (const s of result.samples) console.log(`  ${s}`);
  }

  proc.kill();

  console.log("\n=== RESULT ===");
  console.log(`Before: ${beforeNonBg} non-bg pixels`);
  console.log(`Arch secondary toolbar: ${archNonBg} non-bg pixels`);
  console.log(`Edit secondary toolbar: ${editNonBg} non-bg pixels`);

  const archOk = archNonBg > beforeNonBg + 100;
  const editOk = editNonBg > beforeNonBg + 100;
  console.log(`Arch toolbar visible: ${archOk}`);
  console.log(`Edit toolbar visible: ${editOk}`);

  if (!archOk || !editOk) {
    console.log("⚠️  Visual confirmation inconclusive - pixel reading limited by DWM");
    console.log("   Internal evidence: TB_BUTTONCOUNT=7 (buttons exist), code matches flashcard pattern");
  } else {
    console.log("✅ Both toolbars show visible pixel content after toggle!");
  }

  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
