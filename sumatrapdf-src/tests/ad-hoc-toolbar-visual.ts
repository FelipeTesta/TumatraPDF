// Ad-hoc visual verification: Arch/Edit secondary toolbars render dark-theme
// (NOT white) after toggles. Captures PNGs into tests/tmp/ for inspection.
// Run: bun tests/ad-hoc-toolbar-visual.ts --no-build   (from sumatrapdf-src)
// Requires a pre-built out\dbg64\TumatraPDF.exe.

import { readdirSync, statSync } from "node:fs";
import { launchSumatra, waitForFrame } from "./win-automation.ts";
import {
  setProcessDpiAware,
  sleep,
  sendMessage,
  captureWindowDCRegionToPng,
  captureWindowToPng,
  WM_COMMAND,
  isWindowVisible,
  getWindowRect,
} from "./winapi.ts";
import { cmdId, tmpPath } from "./util.ts";

// Frame rect (screen coords) consistently (562,0,1357,1030) -> w=795 h=1030.
// captureWindowDCRegionToPng uses window-relative coords (0,0 = top-left of the
// frame's non-client area). Per the requested design we pass these exact values.
const REGION_X = 562;
const REGION_Y = 0;
const REGION_W = 795;
const REGION_H = 1030;

type CapResult = { name: string; ok: boolean; bytes?: number };

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const proc = launchSumatra(["ext/a-zlib/zlib.3.pdf"]);
  let frame = await waitForFrame(proc.pid);
  if (!frame) {
    throw new Error("frame window not found");
  }
  console.log("frame rect:", JSON.stringify(getWindowRect(frame)), "visible=", isWindowVisible(frame));

  await sleep(3000);

  const results: CapResult[] = [];

  // Capture one frame region to `name`. Region capture first (per spec); if it
  // returns false, fall back to whole-window PrintWindow. Reports ok + bytes.
  async function cap(name: string): Promise<void> {
    const path = tmpPath(name);
    let ok = captureWindowDCRegionToPng(frame, REGION_X, REGION_Y, REGION_W, REGION_H, path);
    if (!ok) {
      ok = captureWindowToPng(frame, path);
    }
    let bytes: number | undefined;
    try {
      bytes = statSync(path).size;
    } catch {
      bytes = undefined;
    }
    results.push({ name, ok, bytes });
    console.log(`[cap] ${name}: ok=${ok} bytes=${bytes ?? "-"}`);
  }

  // 3. BASELINE (no toggle yet)
  await cap("vis-baseline.png");

  // 4. Toggle Arch ON -> capture immediately, then after 1s
  sendMessage(frame, WM_COMMAND, cmdId("CmdArchToolsToggle"), 0);
  // Toggling a secondary toolbar destroys/recreates the frame, so re-acquire
  // the (possibly new) frame HWND before capturing.
  frame = await waitForFrame(proc.pid);
  if (!frame) {
    throw new Error("frame not found after Arch toggle");
  }
  console.log("after Arch toggle rect=", JSON.stringify(getWindowRect(frame)), "visible=", isWindowVisible(frame));
  await cap("vis-arch-nosleep.png");
  await sleep(1000);
  await cap("vis-arch-1s.png");

  // 5. Toggle Arch OFF (sleep 800), then Edit ON -> capture immediately, then 1s
  sendMessage(frame, WM_COMMAND, cmdId("CmdArchToolsToggle"), 0);
  await sleep(800);
  sendMessage(frame, WM_COMMAND, cmdId("CmdEditToolsToggle"), 0);
  frame = await waitForFrame(proc.pid);
  if (!frame) {
    throw new Error("frame not found after Edit toggle");
  }
  console.log("after Edit toggle rect=", JSON.stringify(getWindowRect(frame)), "visible=", isWindowVisible(frame));
  await cap("vis-edit-nosleep.png");
  await sleep(1000);
  await cap("vis-edit-1s.png");

  proc.kill();

  console.log("\n== PNG summary ==");
  for (const r of results) {
    console.log(`${r.name}: ok=${r.ok} bytes=${r.bytes ?? "-"}`);
  }
  try {
    const files = readdirSync(tmpPath("")).filter((f) => f.startsWith("vis-"));
    console.log("\n== files in tests/tmp/ ==");
    for (const f of files.sort()) {
      console.log(f);
    }
  } catch {
    console.log("\n== files: (none / tmp dir missing) ==");
  }
}

if (import.meta.main) {
  await testit();
}
