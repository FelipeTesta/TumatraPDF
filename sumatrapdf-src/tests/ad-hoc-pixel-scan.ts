// Direct pixel verification: sample frame DC at known toolbar positions.
// Toolbar rect from previous tests: Arch at (452,105,1085,129), frame at (450,0,...)
// Within frame window coords: toolbar at x=2..635, y=105..129
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import {
  setForegroundWindow,
  getWindowRect,
  readWindowDCColumn,
  CLR_INVALID,
  sleep,
} from "./winapi";
import { runStandalone, cmdId } from "./util";
import { captureWindowToPng } from "./win-automation";

const TMP = "tests/tmp/";

// Count non-background pixels in a column
function countNonBg(pixels: number[]): number {
  return pixels.filter(p => p !== CLR_INVALID && p !== 0 && p !== 0xff000000).length;
}

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  let fgOk = false;
  for (let i = 0; i < 5; i++) {
    fgOk = setForegroundWindow(frame);
    if (fgOk) break;
    await sleep(200);
  }
  console.log(`setForegroundWindow: ${fgOk}`);
  await sleep(3000);

  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);

  // Capture frame PNG for reference
  captureWindowToPng(frame, TMP + "frame.png");

  // === ARCH TOOLS ===
  console.log("\n=== ARCH TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(3000);

  // Frame window coords: toolbar at x=2..635 (relative to frame left=450), y=105..129
  const archTbLeft = 452 - frameRect.left; // = 2
  const archTbTop = 105 - frameRect.top;   // = 105
  const archTbRight = 1085 - frameRect.left; // = 635
  const archTbBottom = 129 - frameRect.top;  // = 129
  const archTbMidY = (archTbTop + archTbBottom) / 2; // ~117

  console.log(`Arch toolbar expected at frame coords: x=${archTbLeft}..${archTbRight}, y=${archTbTop}..${archTbBottom}`);

  let archNonBgTotal = 0;
  let archColumnsWithContent = 0;
  for (let x = archTbLeft + 5; x < archTbRight - 5; x += 10) {
    const col = readWindowDCColumn(frame, x, archTbMidY - 5, 10);
    const nonBg = countNonBg(col);
    if (nonBg > 0) {
      archNonBgTotal += nonBg;
      archColumnsWithContent++;
      if (archColumnsWithContent <= 5) {
        console.log(`  x=${x}: ${nonBg}/10 non-bg pixels (sample: ${col.slice(0,5).map(p=>p.toString(16)).join(",")})`);
      }
    }
  }
  console.log(`Arch: ${archColumnsWithContent} columns with content, ${archNonBgTotal} total non-bg pixels`);

  // === EDIT TOOLS ===
  console.log("\n=== EDIT TOOLS ===");
  sendCommand(frame, cmdId("CmdArchToolsToggle")); // Arch OFF first
  await sleep(1000);
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(3000);

  // Edit toolbar at same position
  const editTbLeft = 452 - frameRect.left;
  const editTbTop = 105 - frameRect.top;
  const editTbRight = 1085 - frameRect.left;
  const editTbBottom = 129 - frameRect.top;
  const editTbMidY = (editTbTop + editTbBottom) / 2;

  console.log(`Edit toolbar expected at frame coords: x=${editTbLeft}..${editTbRight}, y=${editTbTop}..${editTbBottom}`);

  let editNonBgTotal = 0;
  let editColumnsWithContent = 0;
  for (let x = editTbLeft + 5; x < editTbRight - 5; x += 10) {
    const col = readWindowDCColumn(frame, x, editTbMidY - 5, 10);
    const nonBg = countNonBg(col);
    if (nonBg > 0) {
      editNonBgTotal += nonBg;
      editColumnsWithContent++;
      if (editColumnsWithContent <= 5) {
        console.log(`  x=${x}: ${nonBg}/10 non-bg pixels (sample: ${col.slice(0,5).map(p=>p.toString(16)).join(",")})`);
      }
    }
  }
  console.log(`Edit: ${editColumnsWithContent} columns with content, ${editNonBgTotal} total non-bg pixels`);

  proc.kill();

  console.log("\n=== RESULT ===");
  console.log(`Arch toolbar has content: ${archColumnsWithContent > 5}`);
  console.log(`Edit toolbar has content: ${editColumnsWithContent > 5}`);

  if (archColumnsWithContent <= 5) {
    console.log("WARNING: Arch toolbar may be blank (few columns with content)");
  }
  if (editColumnsWithContent <= 5) {
    console.log("WARNING: Edit toolbar may be blank (few columns with content)");
  }

  console.log("\nDone. Check screenshot: tests/tmp/frame.png");
}

if (import.meta.main) {
  await runStandalone(testit);
}
