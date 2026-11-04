// Final visual verification: synchronous SendMessage toggle + immediate pixel read.
// No sleep between toggle and pixel read, so FFI calls remain valid.
import { launchSumatra, waitForFrame } from "./win-automation";
import {
  setProcessDpiAware,
  setForegroundWindow,
  getWindowRect,
  enumChildWindows,
  getClassName,
  readWindowDCColumn,
  CLR_INVALID,
  sendMessage,
} from "./winapi";
import { runStandalone } from "./util";

const WM_COMMAND = 0x0111;
const TMP = "tests/tmp/";

function countDiff(pixels: number[], refColor: number): number {
  let n = 0;
  for (const p of pixels) {
    if (p !== CLR_INVALID && p !== refColor) n++;
  }
  return n;
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

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  // Get frame rect immediately
  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);

  // Bring to foreground
  for (let i = 0; i < 5; i++) {
    if (setForegroundWindow(frame)) break;
    // no sleep - just retry
  }

  // Find toolbars immediately (FFI works before any yield)
  const toolbarsBefore = findToolbars(frame);
  console.log(`\nToolbars found immediately: ${toolbarsBefore.length}`);
  for (const tb of toolbarsBefore) {
    const r = getWindowRect(tb);
    console.log(`  hwnd=0x${tb.toString(16)} rect=(${r.left},${r.top},${r.right},${r.bottom}) size=${r.right-r.left}x${r.bottom-r.top}`);
  }

  // Get client color reference
  const clientCol = readWindowDCColumn(0, frameRect.left + 100, frameRect.top + 300, 1);
  const clientColor = clientCol[0] === CLR_INVALID ? 0 : clientCol[0];
  console.log(`\nClient color: 0x${clientColor.toString(16)}`);

  // We need CmdArchToolsToggle id. Let's find it via gen-commands or just use the number.
  // From gen-commands.ts: CmdArchToolsToggle is defined there. We need its numeric value.
  // Since we can't easily get it, let's try to find it from Commands.h
  // For now, let's just scan pixels at known positions
  const { cmdId } = await import("./util");
  const archToggleId = cmdId("CmdArchToolsToggle");
  console.log(`\nCmdArchToolsToggle id: ${archToggleId}`);

  // Send synchronous toggle command
  console.log("\nSending synchronous WM_COMMAND for Arch Tools Toggle...");
  const result = sendMessage(frame, WM_COMMAND, archToggleId, 0);
  console.log(`SendMessage result: ${result}`);

  // Immediately read pixels at secondary toolbar position (no sleep!)
  // From previous tests: toolbar at screen (452,105,1085,129) when frame at (450,0,...)
  // Current frame: (frameRect.left, 0, ...)
  const tbScreenX = frameRect.left + 2; // toolbar window x offset = 2
  const tbScreenY = 105; // frame top = 0
  const tbScreenRight = frameRect.left + 635;
  const tbScreenBottom = 129;
  const tbScreenMidY = Math.floor((tbScreenY + tbScreenBottom) / 2);

  console.log(`\nScanning toolbar area at screen coords: x=${tbScreenX}..${tbScreenRight}, y=${tbScreenMidY}`);
  let columnsWithContent = 0;
  for (let x = tbScreenX + 10; x < tbScreenRight - 10; x += 15) {
    const col = readWindowDCColumn(0, x, tbScreenMidY - 3, 6);
    const diff = countDiff(col, clientColor);
    if (diff > 0) {
      columnsWithContent++;
      if (columnsWithContent <= 5) {
        console.log(`  x=${x}: ${diff}/6 diff pixels (${col.map(p=>p.toString(16)).join(",")})`);
      }
    }
  }
  console.log(`Toolbar columns with content: ${columnsWithContent}`);

  // Also scan at multiple Y positions
  console.log("\nScanning multiple Y positions:");
  for (let y = 85; y <= 145; y += 10) {
    const screenY = frameRect.top + y;
    let yDiff = 0;
    for (let x = tbScreenX + 10; x < tbScreenRight - 10; x += 30) {
      const col = readWindowDCColumn(0, x, screenY, 3);
      yDiff += countDiff(col, clientColor);
    }
    if (yDiff > 0) {
      console.log(`  y=${screenY} (frame+y=${y}): total diff=${yDiff}`);
    }
  }

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
