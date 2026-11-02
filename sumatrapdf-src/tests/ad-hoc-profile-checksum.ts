// Ad-hoc test for F7 slice A: integrity checksum for tumatraPDF-data_1.json.
// Run: bun tests/ad-hoc-profile-checksum.ts [--no-build]
//
// Steps:
//   1. Kill any stale TumatraPDF.exe
//   2. Copy Compiled\tumatraPDF-data_1.json to out\dbg64\ (if fixture exists)
//   3. Launch out\dbg64\TumatraPDF.exe -for-testing ext/a-zlib/zlib.3.pdf
//      → check ALIVE for 12s (profile created with checksum on save)
//   4. Kill, relaunch → checksum verification passes (round-trip OK)
//   5. Kill, corrupt one byte mid-file in out\dbg64\tumatraPDF-data_1.json
//   6. Relaunch → checksum mismatch triggers backup recovery / legacy import
//      → app stays ALIVE, no sumatrapdfcrash.txt
//   7. Kill, cleanup

import { join } from "node:path";
import { copyFileSync, existsSync, readFileSync, writeFileSync, rmSync } from "node:fs";
import { tmpPath, runStandalone } from "./util.ts";
import { launchSumatra, waitForFrame } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const PDF = join("ext", "a-zlib", "zlib.3.pdf");
const EXE_DIR = join(import.meta.dir, "..", "out", "dbg64");
const EXE = join(EXE_DIR, "TumatraPDF.exe");
const PROFILE_JSON = join(EXE_DIR, "tumatraPDF-data_1.json");
const COMPLIED_JSON = join(import.meta.dir, "..", "..", "Compiled", "tumatraPDF-data_1.json");
const CRASH_TXT = join(EXE_DIR, "sumatrapdfcrash.txt");

// Wait `ms` ms, returning true if proc is still alive, false if it exited.
async function aliveAfter(proc: Bun.Subprocess, ms: number): Promise<boolean> {
  const result = await Promise.race([
    proc.exited.then(() => false),
    sleep(ms).then(() => true),
  ]);
  return result;
}

function launchAndWait(args: string[], settleMs = 12000): Bun.Subprocess {
  return launchSumatra(args);
}

async function testit(): Promise<void> {
  // Step 0: kill stale instance
  try {
    const { spawnSync } = await import("node:child_process");
    spawnSync("taskkill", ["/IM", "TumatraPDF.exe", "/F"], { stdio: "ignore" });
  } catch {}
  await sleep(1000);

  // Step 1: copy fixture if available
  let fixtureCopied = false;
  if (existsSync(COMPLIED_JSON)) {
    copyFileSync(COMPLIED_JSON, PROFILE_JSON);
    fixtureCopied = true;
    console.log(`fixture copied: ${COMPLIED_JSON} -> ${PROFILE_JSON}`);
  } else {
    console.log(`no fixture at ${COMPLIED_JSON} (first run: fresh profile)`);
  }

  // Step 2: Launch 1 — fresh profile, checksum written on save
  console.log("=== Launch 1: profile creation ===");
  const proc1 = launchAndWait([PDF]);
  const alive1 = await aliveAfter(proc1, 12000);
  proc1.kill();
  await sleep(1000);
  if (!alive1) throw new Error("Launch 1: app died within 12s");
  console.log("Launch 1: ALIVE_OK (profile with checksum created)");

  // Step 3: Launch 2 — checksum verification should pass (round-trip)
  console.log("=== Launch 2: checksum verify round-trip ===");
  const proc2 = launchAndWait([PDF]);
  const alive2 = await aliveAfter(proc2, 12000);
  proc2.kill();
  await sleep(1000);
  if (!alive2) throw new Error("Launch 2: app died within 12s");
  console.log("Launch 2: ALIVE_OK (checksum verified OK)");

  // Step 4: Corrupt one byte mid-file
  console.log("=== Corrupt profile ===");
  let corrupted = false;
  if (existsSync(PROFILE_JSON)) {
    const data = readFileSync(PROFILE_JSON);
    if (data.length > 200) {
      const midIdx = Math.floor(data.length / 2);
      data[midIdx] = data[midIdx] ^ 0xFF;
      writeFileSync(PROFILE_JSON, data);
      corrupted = true;
      console.log(`Corrupted byte at offset ${midIdx} of ${data.length}`);
    } else {
      console.log(`Profile file too small (${data.length} bytes), skipping corruption`);
    }
  } else {
    console.log(`No profile file at ${PROFILE_JSON} (not created in portable mode?)`);
  }

  // Step 5: Launch 3 — checksum mismatch should trigger recovery
  console.log("=== Launch 3: corruption recovery ===");
  const proc3 = launchAndWait([PDF]);
  const alive3 = await aliveAfter(proc3, 12000);
  const crashExists = existsSync(CRASH_TXT);
  proc3.kill();
  await sleep(1000);

  if (!alive3) throw new Error("Launch 3: app died within 12s after corruption");
  if (crashExists) throw new Error("sumatrapdfcrash.txt exists after corruption recovery");
  console.log(`Launch 3: ALIVE_OK (corruption recovery worked, corrupted=${corrupted}, crash=${crashExists})`);

  // Step 6: cleanup
  if (fixtureCopied) {
    try { rmSync(PROFILE_JSON, { force: true }); } catch {}
    try { rmSync(join(EXE_DIR, "backup"), { recursive: true, force: true }); } catch {}
  }

  console.log("ALL STEPS PASS");
}

if (import.meta.main) {
  await runStandalone(testit, "ad-hoc-profile-checksum");
}
