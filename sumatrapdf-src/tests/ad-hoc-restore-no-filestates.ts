// P0: boot crash when TabState.tocState is NULL because FileStates was wiped
// (user machine: FileStates [], SessionData with 1 tab, TabIndex=1 out of range).
// Repro: SessionData tabState deserialized w/o tocState (AllocStruct zero-inits
// pointer settings fields) -> SetTabState (SumatraStartup.cpp:399) deref
// state->tocState NULL -> Vec::operator= reads offset 0 -> C0000005.
//
// Fix: null-guard every tocState deref (SumatraStartup.cpp:399, GlobalPrefs.cpp:181,
// AppSettings.cpp:632/759, SumatraPDF.cpp:824/2429/5747).
//
// Register ONLY in tests/before-release.ts (not tests/all.ts).
//
// Run: bun tests/ad-hoc-restore-no-filestates.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { postMessage } from "./winapi.ts";
import { waitForFrame } from "./win-automation.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const WM_CLOSE = 0x0010;
const PDF_REL = "ext/a-zlib/zlib.3.pdf";

function settingsText(appdataRoot: string): string {
  const pdfAbs = join(appdataRoot, "..", PDF_REL); // placeholder, replaced below
  return "";
}

// Build an absolute path to the fixture PDF (same machine layout as ad-hoc-tab-session.ts).
const ROOT = join(import.meta.dir, "..");
const pdfAbs = join(ROOT, PDF_REL);

function settingsText(pdf: string): string {
  let s = `RememberStatePerDocument = true\r\n`;
  s += `RememberOpenedFiles = true\r\n`;
  s += `RestoreSession = true\r\n`;
  s += `ActiveTabGroup = Teste2\r\n`;
  s += `FileStates []\r\n`;
  s += `SessionData [\r\n`;
  s += `  tabIndex = 1\r\n`;
  s += `  tabStates [\r\n`;
  s += `    [\r\n`;
  s += `      FilePath = ${pdf}\r\n`;
  s += `      PageNo = 1\r\n`;
  s += `      ShowToc = false\r\n`;
  s += `    ]\r\n`;
  s += `  ]\r\n`;
  s += `]\r\n`;
  s += `TabGroups [\r\n`;
  s += `  [\r\n`;
  s += `    Name = Teste2\r\n`;
  s += `    TabFiles []\r\n`;
  s += `  ]\r\n`;
  s += `]\r\n`;
  return s;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ad-hoc-restore-no-filestates");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "run1");
  mkdirSync(appdata, { recursive: true });

  writeFileSync(join(appdata, "TumatraPDF-settings.txt"), settingsText(pdfAbs), "utf8");

  let frame = 0;
  let proc: Bun.Subprocess | null = null;
  try {
    // launch WITHOUT -for-testing so the boot restore path runs
    proc = Bun.spawn([EXE, "-appdata", appdata], { stdout: "ignore", stderr: "ignore" });
    const pid = typeof proc.pid === "number" ? proc.pid : 0;
    if (pid) {
      try {
        frame = await waitForFrame(pid, 8000);
      } catch {}
    }
    // give the restore path a moment to load documents + run SetTabState
    await new Promise((r) => setTimeout(r, 1500));

    if (!frame) {
      // process crashed during boot restore -> frame never appeared
      throw new Error("frame window not found (app crashed during boot restore)");
    }
    console.log(`frame found: app survived boot restore (no crash)`);

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

  // verify settings file still parseable / untouched
  const settings = readFileSync(join(appdata, "TumatraPDF-settings.txt"), "utf8");
  if (!settings.includes("SessionData")) {
    throw new Error("SessionData section missing from settings file");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
