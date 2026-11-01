// Pixel scan with cached frame rect - avoids getWindowRect-after-sleep failure
import { launchSumatra, waitForFrame, sendCommand, captureWindowToPng } from "./win-automation";
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
  setForegroundWindow; // reference to ensure loaded

  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  // Get frame rect IMMEDIATELY (before any sleep)
  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);

  // Bring to foreground
  let fgOk = false;
  for (let i = 0; i < 5; i++) {
    fgOk = setForegroundWindow(frame);
    if (fgOk) break;
    await sleep(200);
  }
  console.log(`setForegroundWindow: ${fgOk}`);
  await sleep(2000);

  // Capture frame PNG
  captureWindowToPng(frame, TMP + "frame.png");

  // === ARCH TOOLS ===
  console.log("\n=== ARCH TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  // Use cached frame rect + known offsets
  // Toolbar at screen (452,105,1085,129), frame at (450,0,...)
  // Within frame window coords: x=2..635, y=105..129
  const archLeft = 452 - frameRect.left;
  const archTop = 105 - frameRect.top;
  const archRight = 1085 - frameRect.left;
  const archBottom = 129 - frameRect.top;
  const archMidY = Math.floor((archTop + archBottom) / 2);

  console.log(`Arch toolbar at frame coords: x=${archLeft}..${archRight}, y=${archTop}..${archBottom}, midY=${archMidY}`);

  let archColumnsWithContent = 0;
  let archTotalNonBg = 0;
  for (let x = archLeft + 5; x < archRight - 5; x += 12) {
    const col = readWindowDCColumn(frame, x, archMidY - 4, 8);
    const nonBg = countNonBg(col);
    if (nonBg > 0) {
      archColumnsWithContent++;
      archTotalNonBg += nonBg;
      if (archColumnsWithContent <= 3) {
        console.log(`  x=${x}: ${nonBg}/8 non-bg`);
      }
    }
  }
  console.log(`Arch: ${archColumnsWithContent} columns with content, ${archTotalNonBg} total non-bg pixels`);

  // === EDIT TOOLS ===
  console.log("\n=== EDIT TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(1000);
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);

  const editLeft = 452 - frameRect.left;
  const editTop = 105 - frameRect.top;
  const editRight = 1085 - frameRect.left;
  const editBottom = 129 - frameRect.top;
  const editMidY = Math.floor((editTop + editBottom) / 2);

  console.log(`Edit toolbar at frame coords: x=${editLeft}..${editRight}, y=${editTop}..${editBottom}, midY=${editMidY}`);

  let editColumnsWithContent = 0;
  let editTotalNonBg = 0;
  for (let x = editLeft + 5; x < editRight - 5; x += 12) {
    const col = readWindowDCColumn(frame, x, editMidY - 4, 8);
    const nonBg = countNonBg(col);
    if (nonBg > 0) {
      editColumnsWithContent++;
      editTotalNonBg += nonBg;
      if (editColumnsWithContent <= 3) {
        console.log(`  x=${x}: ${nonBg}/8 non-bg`);
      }
    }
  }
  console.log(`Edit: ${editColumnsWithContent} columns with content, ${editTotalNonBg} total non-bg pixels`);

  proc.kill();

  console.log("\n=== RESULT ===");
  console.log(`Arch columns with content: ${archColumnsWithContent}`);
  console.log(`Edit columns with content: ${editColumnsWithContent}`);

  if (archColumnsWithContent > 0 && editColumnsWithContent > 0) {
    console.log("✅ Both toolbars have visible content!");
  } else {
    console.log("⚠️  One or both toolbars may be blank");
  }

  console.log("\nDone. Check screenshot: tests/tmp/frame.png");
}

if (import.meta.main) {
  await runStandalone(testit);
}
