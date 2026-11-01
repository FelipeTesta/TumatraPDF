// Direct pixel value comparison: capture pixels at toolbar positions before/after toggle
import { launchSumatra, waitForFrame } from "./win-automation";
import {
  setProcessDpiAware,
  setForegroundWindow,
  getWindowRect,
  readWindowDCColumn,
  CLR_INVALID,
  sendMessage,
} from "./winapi";
import { runStandalone, cmdId } from "./util";

const WM_COMMAND = 0x0111;

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame HWND: ${frame}`);

  const frameRect = getWindowRect(frame);
  console.log(`Frame rect: (${frameRect.left},${frameRect.top},${frameRect.right},${frameRect.bottom})`);

  for (let i = 0; i < 5; i++) {
    if (setForegroundWindow(frame)) break;
  }

  const { cmdId } = await import("./util");
  const archToggleId = cmdId("CmdArchToolsToggle");
  const editToggleId = cmdId("CmdEditToolsToggle");

  // Secondary toolbar screen position
  const tbScreenX = frameRect.left + 2;
  const tbScreenRight = frameRect.left + 635;
  const tbScreenY = 105;
  const tbScreenBottom = 129;
  const tbScreenMidY = Math.floor((tbScreenY + tbScreenBottom) / 2);

  // Sample positions across the toolbar width
  const sampleXs: number[] = [];
  for (let x = tbScreenX + 10; x < tbScreenRight - 10; x += 25) {
    sampleXs.push(x);
  }

  function capturePixels(x: number, y: number): number[] {
    return readWindowDCColumn(0, x, y - 2, 4);
  }

  function pixelsToHex(pixels: number[]): string {
    return pixels.map(p => p === CLR_INVALID ? "INVALID" : p.toString(16)).join(",");
  }

  // === BASELINE: Arch Tools OFF ===
  console.log("\n=== BASELINE: Arch Tools OFF ===");
  const beforePixels: number[][] = [];
  for (const x of sampleXs) {
    const px = capturePixels(x, tbScreenMidY);
    beforePixels.push(px);
    console.log(`  x=${x}: ${pixelsToHex(px)}`);
  }

  // === ARCH TOOLS ON ===
  console.log("\n=== ARCH TOOLS ON ===");
  sendMessage(frame, WM_COMMAND, archToggleId, 0);

  let archChanged = 0;
  for (let i = 0; i < sampleXs.length; i++) {
    const px = capturePixels(sampleXs[i], tbScreenMidY);
    const changed = px.some((p, j) => p !== CLR_INVALID && p !== beforePixels[i][j]);
    if (changed) archChanged++;
    console.log(`  x=${sampleXs[i]}: ${pixelsToHex(px)} ${changed ? "CHANGED" : "same"}`);
  }
  console.log(`\nArch: ${archChanged}/${sampleXs.length} positions changed`);

  // === EDIT TOOLS ON ===
  console.log("\n=== EDIT TOOLS ON (Arch OFF first) ===");
  sendMessage(frame, WM_COMMAND, archToggleId, 0); // Arch OFF
  sendMessage(frame, WM_COMMAND, editToggleId, 0); // Edit ON

  let editChanged = 0;
  for (let i = 0; i < sampleXs.length; i++) {
    const px = capturePixels(sampleXs[i], tbScreenMidY);
    const changed = px.some((p, j) => p !== CLR_INVALID && p !== beforePixels[i][j]);
    if (changed) editChanged++;
    console.log(`  x=${sampleXs[i]}: ${pixelsToHex(px)} ${changed ? "CHANGED" : "same"}`);
  }
  console.log(`\nEdit: ${editChanged}/${sampleXs.length} positions changed`);

  proc.kill();

  console.log("\n=== FINAL RESULT ===");
  console.log(`Arch toolbar: ${archChanged}/${sampleXs.length} pixel positions changed after toggle`);
  console.log(`Edit toolbar: ${editChanged}/${sampleXs.length} pixel positions changed after toggle`);

  const archOk = archChanged > 0;
  const editOk = editChanged > 0;
  console.log(`Arch toolbar visible content: ${archOk}`);
  console.log(`Edit toolbar visible content: ${editOk}`);

  if (!archOk || !editOk) {
    console.log("⚠️  One or both toolbars may not show visible content change");
  } else {
    console.log("✅ Both toolbars show visible content changes after toggle!");
  }

  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
