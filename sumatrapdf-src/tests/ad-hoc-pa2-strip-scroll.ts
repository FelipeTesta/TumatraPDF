// PA2 strip-scroll visual verification: TC2 (two columns v2) on + autoscroll
// on + flashcards off — the dominant scenario per the user invariant
// ("autoscroll=on implies flashcard=off"). Captures canvas screenshots during
// the scroll for manual inspection: content must scroll cleanly with no
// smearing/ghosting in the preserved region and correct TC2 column advance.
// Run: bun tests/ad-hoc-pa2-strip-scroll.ts
import { launchSumatra, waitForFrame, findCanvas, sendCommand } from "./win-automation";
import { captureWindowToPng, sleep, postMessage } from "./winapi";
import { cmdId, tmpPath } from "./util";
import path from "node:path";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DELTA_UP = 120 << 16; // high word of wParam

export async function testit(): Promise<void> {
  const pdf = path.resolve(
    import.meta.dir,
    "..",
    "ext",
    "lcms2",
    "plugins",
    "fast_float",
    "doc",
    "LittleCMS floating point extensions 1.4.pdf",
  );
  const proc = launchSumatra(["-log", pdf]);
  const frame = await waitForFrame(proc.pid);
  await sleep(2500);
  sendCommand(frame, cmdId("CmdViewportCropV2Toggle")); // TC2 on
  await sleep(1200);
  sendCommand(frame, cmdId("CmdAutoScrollToggle")); // autoscroll on
  for (let i = 0; i < 6; i++) {
    // default speed is a crawl (0.03 multiplier); bump it so the scroll
    // actually covers ground within the capture window
    sendCommand(frame, cmdId("CmdAutoScrollSpeedUp"));
    await sleep(120);
  }
  for (let i = 0; i < 6; i++) {
    await sleep(700);
    await captureWindowToPng(findCanvas(frame), tmpPath(`pa2-strip-${i}.png`));
  }
  sendCommand(frame, cmdId("CmdAutoScrollToggle")); // off
  await sleep(300);
  // manual wheel-up: strip path must expose (and cleanly repaint) the TOP strip
  const canvas = findCanvas(frame);
  for (let i = 0; i < 5; i++) {
    postMessage(canvas, WM_MOUSEWHEEL, WHEEL_DELTA_UP, 0);
    await sleep(250);
  }
  await captureWindowToPng(canvas, tmpPath("pa2-strip-wheelup.png"));
  // close gracefully so the buffered log file is flushed before we read it
  postMessage(frame, 0x0010 /* WM_CLOSE */, 0, 0);
  await Promise.race([proc.exited, sleep(4000)]);
  proc.kill();
}

if (import.meta.main) {
  await testit();
}
