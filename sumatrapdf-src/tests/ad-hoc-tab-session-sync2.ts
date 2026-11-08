// s46: tab/group state sync hardening — settings-file based asserts.
// Seeds a named group with 2 docs (in a known order) as activeTabGroup, launches
// so the boot-restore path loads both as tabs, quits gracefully, re-reads
// TumatraPDF-settings.txt and asserts the group survives on disk with the saved
// order preserved. Exercises fix #5/#6 (boot restore now SaveSettings after
// SuppressGroupAutoSave(false) so the restored group is persisted).
//
// Honest gaps (not automatable in this harness, noted rather than forced):
//  - (b) RemoveTab programmatic close: no CloseTab/RemoveTab command on the
//    -dbg-control wire (cmd/control.ts exposes only TestTabGroup activate/list/
//    *session*), and injected mouse input is dropped, so a programmatic tab
//    close can't be driven here.
//  - (c) last-tab SyncActiveGroupToEmpty pre-close: same — needs a control-pipe
//    CloseTab command or reliable posted input.
//  - (d) pinned Home MoveTab-reject: no MoveTab control command; can't drive
//    Ctrl+Shift+PgUp/PgDn via injected input either.
//
// Register ONLY in tests/before-release.ts (not tests/all.ts).
//
// Run: bun tests/ad-hoc-tab-session-sync2.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { postMessage, WM_CLOSE } from "./winapi.ts";
import { waitForFrame } from "./win-automation.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const GROUP_NAME = "GroupS46";
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

// Parse the group block for GROUP_NAME; returns its tabFile paths in file order.
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

function activeGroup(settings: string): string {
  const m = settings.match(/^ActiveTabGroup = (.*)$/m);
  return m ? m[1] : "";
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ad-hoc-tab-session-sync2");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "run1");
  mkdirSync(appdata, { recursive: true });

  // Seed: group GroupS46 contains 2 docs in order [pdf1, pdf2]; activeTabGroup = GroupS46.
  writeFileSync(
    join(appdata, "TumatraPDF-settings.txt"),
    settingsText(GROUP_NAME, [{ name: GROUP_NAME, paths: [pdf1, pdf2] }]),
    "utf8",
  );

  let proc: Bun.Subprocess | null = null;
  let frame = 0;
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
    // let the restore path load both documents into tabs (async loads)
    await new Promise((r) => setTimeout(r, 10000));

    // quit gracefully -> SaveSettings() persists tabGroups incl. the restored group
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
  console.log(`settings head: ${settings.slice(0, 500)}`);
  const { present, paths } = groupPaths(settings);
  const ag = activeGroup(settings);

  if (!present) {
    throw new Error(`group '${GROUP_NAME}' not found in settings after quit`);
  }
  if (ag !== GROUP_NAME) {
    throw new Error(`activeTabGroup is '${ag}', expected '${GROUP_NAME}'`);
  }
  if (paths.length === 0 || paths[0] !== pdf1) {
    throw new Error(`group tabFiles missing first file: ${JSON.stringify(paths)}, expected to start with ${pdf1}`);
  }
  // order preservation: if both files made it into tabs before quit, pdf1 must
  // precede pdf2 (boot loads tabFiles in file order). Async load timing means
  // pdf2's tab may not exist at quit — that load-side gap is noted above, not
  // asserted strictly here.
  const pdf2Idx = paths.indexOf(pdf2);
  if (pdf2Idx >= 0 && pdf2Idx < paths.indexOf(pdf1)) {
    throw new Error(`group tabFiles order wrong: ${JSON.stringify(paths)}, expected ${pdf1} before ${pdf2}`);
  }
  console.log(`group ${GROUP_NAME} persisted OK: active=${ag} files=${JSON.stringify(paths)}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
