// Pixel verification using screen DC (GetDC(0) = hwnd=0 in readWindowDCColumn).
// Reads actual screen pixels at toolbar position after toggling.
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

const TMP = "tests/tmp/";

function countNonBg(pixels: number[], bgColor: number): number {
  return pixels.filter(p => p !== CLR_INVALID && p !== bgColor).length;
}

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  // Get frame rect IMMEDIATELY (screen coords)
  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);

  // Bring to foreground
  for (let i = 0; i < 5; i++) {
    if (setForegroundWindow(frame)) break;
    await sleep(200);
  }
  await sleep(2000);

  // Toolbar screen position (from previous successful test)
  // Arch/Edit toolbar at screen (452,105,1085,129)
  const tbScreenX = 452;
  const tbScreenY = 105;
  const tbScreenRight = 1085;
  const tbScreenBottom = 129;
  const tbScreenMidY = Math.floor((tbScreenY + tbScreenBottom) / 2);
  const tbScreenWidth = tbScreenRight - tbScreenX;

  console.log(`Toolbar screen area: x=${tbScreenX}..${tbScreenRight}, y=${tbScreenY}..${tbScreenBottom}, midY=${tbScreenMidY}`);

  // Sample screen background color at a point away from the toolbar
  const bgSample = readWindowDCColumn(0, 900, 400, 1);
  const bgColor = bgSample[0] === CLR_INVALID ? 0 : bgSample[0];
  console.log(`Screen background color at (900,400): 0x${bgColor.toString(16)}`);

  // === ARCH TOOLS ===
  console.log("\n=== ARCH TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  let archColumnsWithContent = 0;
  let archTotalNonBg = 0;
  for (let x = tbScreenX + 10; x < tbScreenRight - 10; x += 15) {
    const col = readWindowDCColumn(0, x, tbScreenMidY - 3, 6);
    const nonBg = countNonBg(col, bgColor);
    if (nonBg > 0) {
      archColumnsWithContent++;
      archTotalNonBg += nonBg;
      if (archColumnsWithContent <= 3) {
        console.log(`  x=${x}: ${nonBg}/6 non-bg pixels (samples: ${col.slice(0,6).map(p=>p.toString(16)).join(",")})`);
      }
    }
  }
  console.log(`Arch: ${archColumnsWithContent} columns with content, ${archTotalNonBg} total non-bg`);

  // === EDIT TOOLS ===
  console.log("\n=== EDIT TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(1000);
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);

  let editColumnsWithContent = 0;
  let editTotalNonBg = 0;
  for (let x = tbScreenX + 10; x < tbScreenRight - 10; x += 15) {
    const col = readWindowDCColumn(0, x, tbScreenMidY - 3, 6);
    const nonBg = countNonBg(col, bgColor);
    if (nonBg > 0) {
      editColumnsWithContent++;
      editTotalNonBg += nonBg;
      if (editColumnsWithContent <= 3) {
        console.log(`  x=${x}: ${nonBg}/6 non-bg pixels (samples: ${col.slice(0,6).map(p=>p.toString(16)).join(",")})`);
      }
    }
  }
  console.log(`Edit: ${editColumnsWithContent} columns with content, ${editTotalNonBg} total non-bg`);

  proc.kill();

  console.log("\n=== RESULT ===");
  console.log(`Arch toolbar columns with content: ${archColumnsWithContent}`);
  console.log(`Edit toolbar columns with content: ${editColumnsWithContent}`);

  if (archColumnsWithContent > 0 && editColumnsWithContent > 0) {
    console.log("✅ Both toolbars have visible pixel content (not blank)");
  } else {
    console.log("⚠️  One or both toolbars may be blank");
  }

  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
