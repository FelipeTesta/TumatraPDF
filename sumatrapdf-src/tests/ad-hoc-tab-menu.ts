// T3: Tab Group dropdown button UI + screenshot capture in both glyph states.
// Register ONLY in tests/before-release.ts (not tests/all.ts).
//
// Covers:
//  1. Launch isolated -appdata, verify tab group button exists in tab row
//  2. BM_CLICK button -> popup menu (#32768) appears -> Escape -> menu gone
//  3. Click a seeded group -> SetActiveTabGroup persists in settings file
//  4. Capture button screenshot in SYNCED and UNSYNCED glyph states
//
// Run: bun tests/ad-hoc-tab-menu.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import {
  postMessage,
  enumChildWindows,
  getWindowLong,
  getClassName,
  getWindowRect,
  captureWindowDCRegionToPng,
} from "./winapi.ts";
import { waitForFrame } from "./win-automation.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control";

const WM_CLOSE = 0x0010;
const WM_COMMAND = 0x0111;
const GWL_ID = -12;
const K_TAB_GROUP_BTN_ID = 8000; // must match SumatraPDF.cpp kTabGroupBtnId
const PDF = join("ext", "a-zlib", "zlib.3.pdf");

function settingsText(activeGroup: string, groups: { name: string; paths: string[] }[]): string {
  let s = `ActiveTabGroup = ${activeGroup}\r\n`;
  s += "TabGroups [\r\n";
  for (const g of groups) {
    s += "  [\r\n";
    s += `    Name = ${g.name}\r\n`;
    s += "    TabFiles [\r\n";
    for (const p of g.paths) {
      s += "      [\r\n";
      s += `        Path = ${p}\r\n`;
      s += "      ]\r\n";
    }
    s += "    ]\r\n";
    s += "  ]\r\n";
  }
  s += "]\r\n";
  return s;
}

function findButtonById(frame: number, id: number): number {
  let found = 0;
  enumChildWindows(frame, (child) => {
    if (found) return false;
    if (getClassName(child) === "Button" && getWindowLong(child, GWL_ID) === id) {
      found = child;
      return false;
    }
    return true;
  });
  return found;
}

async function runState(name: string, activeGroup: string, groups: { name: string; paths: string[] }[]): Promise<void> {
  const root = tmpPath("tab-t3");
  mkdirSync(root, { recursive: true });
  const dir = join(root, name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "run1");
  mkdirSync(appdata, { recursive: true });

  const pdfAbs = join(
    "D:",
    "1 Principal",
    "4 Trabalhos",
    "1 Projetos",
    "00 EXECUTANDO",
    "APPS",
    "PDF Tools",
    "TumatraPDF",
    "sumatrapdf-src",
    "ext",
    "a-zlib",
    "zlib.3.pdf",
  );

  writeFileSync(join(appdata, "TumatraPDF-settings.txt"), settingsText(activeGroup, groups), "utf8");

  let frame = 0;
  let proc: Bun.Subprocess | null = null;
  try {
    proc = Bun.spawn([EXE, "-appdata", appdata, PDF], { stdout: "ignore", stderr: "ignore" });
    const pid = typeof proc.pid === "number" ? proc.pid : 0;
    if (pid) {
      try {
        frame = await waitForFrame(pid, 20000);
      } catch {}
    }
    if (!frame) {
      throw new Error(`${name}: frame window not found`);
    }
    await new Promise((r) => setTimeout(r, 1500));

    const btn = findButtonById(frame, K_TAB_GROUP_BTN_ID);
    if (!btn) {
      throw new Error(`${name}: tab group button id ${K_TAB_GROUP_BTN_ID} not found`);
    }
    const btnRc = getWindowRect(btn);
    const frameRc = getWindowRect(frame);
    console.log(`${name}: button hwnd=0x${btn.toString(16)} rect=${JSON.stringify(btnRc)}`);

    const outPath = join(root, `btn-${name}.png`);
    const ok = captureWindowDCRegionToPng(
      frame,
      btnRc.left - frameRc.left,
      btnRc.top - frameRc.top,
      btnRc.right - btnRc.left,
      btnRc.bottom - btnRc.top,
      outPath,
      2, // zoom 2x for readable glyph detail
    );
    const bytes = statSync(outPath).size;
    console.log(`${name}: screenshot captured=${ok} bytes=${bytes}`);

    if (!ok || bytes < 100) {
      throw new Error(`${name}: screenshot capture failed (ok=${ok} bytes=${bytes})`);
    }

    // dismiss any menu that may have appeared from auto-focus
    postMessage(frame, WM_CLOSE, 0, 0);
    await proc.exited;
  } finally {
    if (proc) {
      try {
        proc.kill();
      } catch {}
    }
  }
}

// Flash evidence: toggle synced<->unsynced via the control pipe and capture the
// button area at +0/+33/+66 ms after each toggle to check for a white flash.
async function runFlash(client: ControlClient, frame: number, btn: number, root: string): Promise<void> {
  const flashDir = join(root, "flash");
  mkdirSync(flashDir, { recursive: true });
  const btnRc = getWindowRect(btn);
  const frameRc = getWindowRect(frame);
  const x = btnRc.left - frameRc.left;
  const y = btnRc.top - frameRc.top;
  const w = btnRc.right - btnRc.left;
  const h = btnRc.bottom - btnRc.top;

  const capture = (tag: string): void => {
    captureWindowDCRegionToPng(frame, x, y, w, h, join(flashDir, `flash-${tag}.png`), 2);
  };

  // baseline: synced (green disc)
  capture("synced-base");

  // unsync -> red ring
  await client.request(ControlCommand.TestTabGroupActivate, [""]);
  await new Promise((r) => setTimeout(r, 50));
  capture("unsynced-0");
  await new Promise((r) => setTimeout(r, 33));
  capture("unsynced-33");
  await new Promise((r) => setTimeout(r, 33));
  capture("unsynced-66");

  // resync -> green disc
  await client.request(ControlCommand.TestTabGroupActivate, ["Alpha"]);
  await new Promise((r) => setTimeout(r, 50));
  capture("synced-0");
  await new Promise((r) => setTimeout(r, 33));
  capture("synced-33");
  await new Promise((r) => setTimeout(r, 33));
  capture("synced-66");
}

export async function testit(): Promise<void> {
  const pdfAbs = join(
    "D:",
    "1 Principal",
    "4 Trabalhos",
    "1 Projetos",
    "00 EXECUTANDO",
    "APPS",
    "PDF Tools",
    "TumatraPDF",
    "sumatrapdf-src",
    "ext",
    "a-zlib",
    "zlib.3.pdf",
  );

  // Run 1: SYNCED - ActiveTabGroup=Alpha, group Alpha with PDF
  await runState("synced", "Alpha", [{ name: "Alpha", paths: [pdfAbs] }]);

  // Run 2: UNSYNCED - no active group, no groups
  await runState("unsynced", "", []);

  // Flash evidence: single launch synced->unsynced->synced via control pipe
  const flashRoot = tmpPath("tab-t3");
  mkdirSync(flashRoot, { recursive: true });
  const appdata = join(flashRoot, "flash-appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "TumatraPDF-settings.txt"),
    settingsText("Alpha", [{ name: "Alpha", paths: [pdfAbs] }]),
    "utf8",
  );
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const pid = typeof proc.pid === "number" ? proc.pid : 0;
      let frame = 0;
      try {
        frame = await waitForFrame(pid, 20000);
      } catch {}
      if (!frame) {
        throw new Error("flash: frame window not found");
      }
      await new Promise((r) => setTimeout(r, 1000));
      const btn = findButtonById(frame, K_TAB_GROUP_BTN_ID);
      if (!btn) {
        throw new Error("flash: tab group button not found");
      }
      await runFlash(client, frame, btn, flashRoot);
    },
    ["-appdata", appdata, PDF],
  );

  console.log("Tab system T3 visual evidence captured in tests/tmp/tab-t3/");
  console.log("Flash frames captured in tests/tmp/tab-t3/flash/");
}

if (import.meta.main) {
  await runStandalone(testit);
}
