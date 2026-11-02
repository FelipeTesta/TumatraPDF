// P1-migration: verify on-disk FileState names-blob migration for gen1/gen2/gen3.
// Run: bun tests/ad-hoc-settings-migration.ts [--no-build]
//
// Synthesis writes real SquareTree on-disk settings files. The app is launched
// WITHOUT -for-testing (-for-testing blocks SaveSettings), so a graceful exit
// re-saves the settings file reflecting the in-memory migrated state.

import { mkdirSync, rmSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { postMessage } from "./winapi.ts";
import { waitForFrame } from "./win-automation.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const WM_CLOSE = 0x0010;

const BASE = `ReuseInstance = false\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n`;

function gen1File(): string {
  // neither flashcard key; migration applies the same arch shift
  return (
    BASE +
    `FileStates [\n` +
    `  [\n` +
    `    filePath = C:\\Users\\Testa\\doc.pdf\n` +
    `    pageNo = 3\n` +
    `    ArchScaleFactor = 1.5\n` +
    `    ArchMeasurements = M1\n` +
    `  ]\n` +
    `]\n`
  );
}

function gen2File(): string {
  // old names: InvertColors holds flashcardStudyTimeSec(1234),
  // FlashcardCardsDone holds archScaleStates("ARCH-DATA"), no FlashcardStudyTimeSec key
  return (
    BASE +
    `FileStates [\n` +
    `  [\n` +
    `    filePath = C:\\Users\\Testa\\doc.pdf\n` +
    `    pageNo = 3\n` +
    `    InvertColors = 1234\n` +
    `    ContrastEnabled = false\n` +
    `    ContrastOpacity = 50\n` +
    `    FlashcardCardsDone = ARCH-DATA\n` +
    `  ]\n` +
    `]\n`
  );
}

function gen3File(): string {
  // current format: FlashcardStudyTimeSec present -> migration skipped
  return (
    BASE +
    `FileStates [\n` +
    `  [\n` +
    `    filePath = C:\\Users\\Testa\\doc.pdf\n` +
    `    pageNo = 3\n` +
    `    FlashcardCardsDone = 2\n` +
    `    FlashcardStudyTimeSec = 99\n` +
    `  ]\n` +
    `]\n`
  );
}

// launch (no -for-testing), wait for frame, graceful close, return re-saved settings text
async function runCycle(appdata: string, settingsText: string): Promise<string> {
  writeFileSync(join(appdata, "TumatraPDF-settings.txt"), settingsText);
  const proc = Bun.spawn([EXE, "-appdata", appdata], { stdout: "ignore", stderr: "ignore" });
  const pid = typeof proc.pid === "number" ? proc.pid : 0;
  let frame = 0;
  if (pid) {
    try {
      frame = await waitForFrame(pid, 15000);
    } catch {}
  }
  if (frame) {
    postMessage(frame, WM_CLOSE, 0, 0);
  }
  await Promise.race([
    proc.exited,
    new Promise((r) => setTimeout(r, 8000)).then(() => {
      proc.kill();
    }),
  ]);
  await new Promise((r) => setTimeout(r, 400));
  return readFileSync(join(appdata, "TumatraPDF-settings.txt"), "utf8");
}

function contains(text: string, key: string, val: string): boolean {
  return text.includes(`${key} = ${val}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ad-hoc-settings-migration");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  const runGen = async (name: string, makeFile: () => string): Promise<string> => {
    const appdata = join(dir, name);
    mkdirSync(appdata, { recursive: true });
    return await runCycle(appdata, makeFile());
  };

  // ---- GEN2: migration moves studyTime out of InvertColors ----
  const g2 = await runGen("gen2", gen2File);
  if (!contains(g2, "FlashcardStudyTimeSec", "1234")) {
    throw new Error(`gen2: FlashcardStudyTimeSec not 1234 after migration:\n${g2}`);
  }
  if (contains(g2, "InvertColors", "1234")) {
    throw new Error(`gen2: InvertColors polluted with studyTime value:\n${g2}`);
  }
  if (!contains(g2, "ArchScaleStates", "ARCH-DATA")) {
    throw new Error(`gen2: ArchScaleStates not remapped from FlashcardCardsDone:\n${g2}`);
  }
  console.log("PASS gen2: FlashcardStudyTimeSec=1234, InvertColors not polluted, ArchScaleStates remapped");

  // ---- GEN3: identity roundtrip, migration skipped ----
  const g3 = await runGen("gen3", gen3File);
  if (!contains(g3, "FlashcardCardsDone", "2")) {
    throw new Error(`gen3: FlashcardCardsDone not preserved:\n${g3}`);
  }
  if (!contains(g3, "FlashcardStudyTimeSec", "99")) {
    throw new Error(`gen3: FlashcardStudyTimeSec not preserved:\n${g3}`);
  }
  console.log("PASS gen3: identity roundtrip, migration skipped");

  // ---- GEN1: migration runs, no crash, file still re-saved ----
  const g1 = await runGen("gen1", gen1File);
  if (!g1.includes("PageNo")) {
    throw new Error(`gen1: settings file not re-saved / corrupt:\n${g1}`);
  }
  if (!contains(g1, "FlashcardCardsDone", "0")) {
    throw new Error(`gen1: FlashcardCardsDone not 0:\n${g1}`);
  }
  console.log("PASS gen1: migration ran, no crash, file re-saved, FlashcardCardsDone=0");

  console.log("PASS: settings FileState names-blob migration (P1-migration)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
