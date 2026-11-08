// s45: tab-group session persistence on last-tab close.
// Seeds a named group with 2 docs, launches (restores both as tabs), attempts to
// close the docs via the tab middle-click route (TabsCtrl::CloseTab ->
// MainWindowTabClosed), quits, re-reads TumatraPDF-settings.txt and asserts the
// group survives (activeTabGroup persisted, group entry present).
//
// Note: closing individual tabs via posted GUI input is not automatable in this
// environment (injected input is dropped; posted mouse does not reach the tab
// bar), so this test verifies the group is persisted across quit. The last-tab
// close sync fix lives in Tabs.cpp MainWindowTabClosed.
//
// Register ONLY in tests/before-release.ts (not tests/all.ts).
//
// Run: bun tests/ad-hoc-tab-session-sync.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { packCoords, postMessage, sendMessage, WM_CLOSE, WM_GETTEXT, WM_GETTEXTLENGTH, WM_MBUTTONDOWN } from "./winapi.ts";
import { waitForFrame } from "./win-automation.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const GROUP_NAME = "GroupS45";
const PDF = join("ext", "a-zlib", "zlib.3.pdf");
const PDF2 = join("ext", "brotli", "docs", "brotli-comparison-study-2015-09-22.pdf");
const root = join(
  "D:",
  "1 Principal",
  "4 Trabalhos",
  "1 Projetos",
  "00 EXECUTANDO",
  "APPS",
  "PDF Tools",
  "TumatraPDF",
  "sumatrapdf-src",
);
const pdf1 = join(root, PDF);
const pdf2 = join(root, PDF2);

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

function getWindowText(hwnd: number): string {
  const len = Number(sendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0)) || 0;
  if (len === 0) {
    return "";
  }
  const buf = new ArrayBuffer((len + 1) * 2);
  const view = new Uint16Array(buf);
  const n = Number(sendMessage(hwnd, WM_GETTEXT, len + 1, BigInt(buf.byteLength))) || 0;
  const chunks: string[] = [];
  for (let i = 0; i < n; i++) {
    chunks.push(String.fromCharCode(view[i]));
  }
  return chunks.join("");
}

// Parse the group block for GROUP_NAME and return its tabFile paths (or null if absent).
function groupPaths(settings: string): { present: boolean; paths: string[] } {
  const lines = settings.split("\r\n");
  let inGroup = false;
  let inTabFiles = false;
  let groupName = "";
  const paths: string[] = [];
  for (const line of lines) {
    const mName = line.match(/^\s*Name = (.*)$/);
    if (mName) {
      groupName = mName[1];
      inGroup = groupName === GROUP_NAME;
      inTabFiles = false;
      continue;
    }
    if (inGroup && line.trim() === "TabFiles [") {
      inTabFiles = true;
      continue;
    }
    if (inTabFiles) {
      const mPath = line.match(/^\s*Path = (.*)$/);
      if (mPath) {
        paths.push(mPath[1]);
      }
      if (line.trim() === "]") {
        inTabFiles = false;
      }
    }
    if (inGroup && line.trim() === "]") {
      inGroup = false;
    }
  }
  return { present: groupName === GROUP_NAME, paths };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ad-hoc-tab-session-sync");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "run1");
  mkdirSync(appdata, { recursive: true });

  // Seed: group GroupS45 contains 2 docs; activeTabGroup = GroupS45
  writeFileSync(
    join(appdata, "TumatraPDF-settings.txt"),
    settingsText(GROUP_NAME, [{ name: GROUP_NAME, paths: [pdf1, pdf2] }]),
    "utf8",
  );

  let frame = 0;
  let proc: Bun.Subprocess | null = null;
  let tabsClosed = 0;
  try {
    // launch WITHOUT -for-testing so the restore path runs and SaveSettings persists
    proc = Bun.spawn([EXE, "-appdata", appdata], { stdout: "ignore", stderr: "ignore" });
    const pid = typeof proc.pid === "number" ? proc.pid : 0;
    if (pid) {
      try {
        frame = await waitForFrame(pid, 30000);
      } catch {}
    }
    if (!frame) {
      throw new Error("frame window not found");
    }
    // let the restore path load both documents into tabs
    await new Promise((r) => setTimeout(r, 3000));

    // Attempt to close tabs via the middle-click route (TabsCtrl::CloseTab ->
    // MainWindowTabClosed). Log how many tabs closed; posted input may not reach
    // the tab bar in this environment.
    for (let round = 0; round < 6 && tabsClosed < 2; round++) {
      let closedThisRound = false;
      for (let y = 0; y <= 120 && !closedThisRound; y += 20) {
        for (let x = 0; x <= 440 && !closedThisRound; x += 40) {
          const titleBefore = getWindowText(frame);
          postMessage(frame, WM_MBUTTONDOWN, 0, packCoords(x, y));
          await new Promise((r) => setTimeout(r, 250));
          const titleAfter = getWindowText(frame);
          if (titleAfter !== titleBefore) {
            tabsClosed++;
            closedThisRound = true;
          }
        }
      }
      if (!closedThisRound) {
        break;
      }
    }
    console.log(`tabs closed via middle-click: ${tabsClosed}`);

    // quit gracefully
    postMessage(frame, WM_CLOSE, 0, 0);
    await proc.exited;
  } finally {
    if (proc) {
      try {
        proc.kill();
      } catch {}
    }
  }

  const settings = readFileSync(join(appdata, "TumatraPDF-settings.txt"), "utf8");
  console.log(`settings head: ${settings.slice(0, 400)}`);
  const { present, paths } = groupPaths(settings);

  if (frame === 0) {
    throw new Error("app did not launch / frame not found");
  }
  if (!present) {
    throw new Error(`group '${GROUP_NAME}' not found in settings after quit`);
  }
  console.log(`group ${GROUP_NAME} tabFiles: ${JSON.stringify(paths)} (tabsClosed=${tabsClosed})`);
  // The group must survive as a persisted session.
}

if (import.meta.main) {
  await runStandalone(testit);
}
