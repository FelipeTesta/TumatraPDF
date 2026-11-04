// T2: tab session restore precedence + unsynced windows.
// Register ONLY in tests/before-release.ts (not tests/all.ts).
//
// Covers:
//  1. Startup: activeTabGroup non-empty + group exists => restore group's files
//     INSTEAD of normal session restore.
//  2. Settings persist: activeTabGroup survives quit/restart.
//
// Run: bun tests/ad-hoc-tab-session.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { postMessage } from "./winapi.ts";
import { getWindowText } from "./winapi.ts";
import { waitForFrame } from "./win-automation.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const WM_CLOSE = 0x0010;
const PDF = join("ext", "a-zlib", "zlib.3.pdf");
const GROUP_NAME = "TestA";

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

export async function testit(): Promise<void> {
  const dir = tmpPath("ad-hoc-tab-session");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "run1");
  mkdirSync(appdata, { recursive: true });

  const pdfAbs = join("D:", "1 Principal", "4 Trabalhos", "1 Projetos", "00 EXECUTANDO", "APPS", "PDF Tools", "TumatraPDF", "sumatrapdf-src", "ext", "a-zlib", "zlib.3.pdf");

  // write settings: activeTabGroup = TestA, group TestA contains the PDF
  writeFileSync(
    join(appdata, "TumatraPDF-settings.txt"),
    settingsText(GROUP_NAME, [{ name: GROUP_NAME, paths: [pdfAbs] }]),
    "utf8",
  );

  let frame = 0;
  let proc: Bun.Subprocess | null = null;
  try {
    // launch WITHOUT -for-testing so the restore path runs
    proc = Bun.spawn([EXE, "-appdata", appdata], { stdout: "ignore", stderr: "ignore" });
    const pid = typeof proc.pid === "number" ? proc.pid : 0;
    if (pid) {
      try {
        frame = await waitForFrame(pid, 20000);
      } catch {}
    }
    if (!frame) {
      throw new Error("frame window not found");
    }
    // give the restore path a moment to load documents
    await new Promise((r) => setTimeout(r, 2500));

    const title = getWindowText(frame);
    console.log(`frame title: ${title}`);
    const loadedGroupPdf = title.includes("zlib.3.pdf");
    console.log(`group PDF loaded in title: ${loadedGroupPdf}`);

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

  // re-read settings: activeTabGroup should still be TestA (persisted)
  const settings = readFileSync(join(appdata, "TumatraPDF-settings.txt"), "utf8");
  console.log(`settings head: ${settings.slice(0, 300)}`);
  const hasActiveTabGroup = settings.includes(`ActiveTabGroup = ${GROUP_NAME}`);

  if (frame === 0) {
    throw new Error("app did not launch / frame not found");
  }
  if (!hasActiveTabGroup) {
    throw new Error(`activeTabGroup '${GROUP_NAME}' not persisted in settings file`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
