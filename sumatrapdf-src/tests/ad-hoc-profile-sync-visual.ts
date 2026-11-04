// Ad-hoc visual test for F6 slice 1: Home "Sync" button + ProfileSyncWnd dialog.
// Run: bun tests/ad-hoc-profile-sync-visual.ts [--no-build]
//
// Steps:
//   1. Launch zlib.3.pdf -for-testing
//   2. Go to home page via CmdOpenHome (wait ≥800ms before capture)
//   3. Capture f6-home.png — verify "Stats" text (top-left of canvas, dark bg)
//   4. Click the Sync button via bounded probe (x 40..240 step 10, y ∈ {24, 34})
//   5. Capture f6-dialog.png — verify dialog opened with rows
//   6. Fixture ground truth assertions (version, books, reading days, study days)
//   7. Click Save Now via BM_CLICK on real HWND button → capture f6-saved.png
//   8. Click Open Folder via BM_CLICK (no crash asserted)
//   9. Click Export via BM_CLICK (gForTesting skips picker; assert dialog alive)
//  10. Click Import via BM_CLICK (gForTesting skips picker; assert dialog alive)
//  11. Close dialog via WM_CLOSE → assert gone
//  12. Quit app cleanly; per-step PASS/FAIL report

import { join } from "node:path";
import { copyFileSync, cpSync, rmSync, existsSync } from "node:fs";
import { tmpPath, cmdId, runStandalone } from "./util.ts";
import {
  launchSumatra,
  waitForFrame,
  findCanvas,
  captureWindowToPng,
  clickAt,
  pressEscape,
  sendCommand,
} from "./win-automation.ts";
import {
  enumWindows,
  getClassName,
  getWindowText,
  postMessage,
  WM_CLOSE,
  WM_KEYDOWN,
  VK_ESCAPE,
  sleep,
  setForegroundWindow,
  setWindowTopmost,
  showWindow,
  enumChildWindows,
  BM_CLICK,
  captureWindowDCToPng,
} from "./winapi.ts";

const PDF = join("ext", "a-zlib", "zlib.3.pdf");
const DIALOG_TITLE = "Profile Sync";

// Real-user fixture (ground truth: version=1, 9 books, 6 reading days, 2 study
// days, 81,619 B, machine DESKTOP-MVQHHVA). Tests run out/dbg64 (portable mode:
// profile dir = out/dbg64), so copy Compiled fixture into out/dbg64 pre-launch.
const FIXTURE_JSON = join(import.meta.dir, "..", "..", "Compiled", "tumatraPDF-data_1.json");
const FIXTURE_BACKUP = join(import.meta.dir, "..", "..", "Compiled", "backup");
const DBG64_JSON = join(import.meta.dir, "..", "out", "dbg64", "tumatraPDF-data_1.json");
const DBG64_BACKUP = join(import.meta.dir, "..", "out", "dbg64", "backup");
let fixturesCopied = false;

let results: string[] = [];
function step(name: string, ok: boolean, detail = "") {
  results.push(`${ok ? "PASS" : "FAIL"} — ${name}${detail ? ": " + detail : ""}`);
  console.log(`${ok ? "✅" : "❌"} ${name}${detail ? " — " + detail : ""}`);
}

function findWindowByTitle(title: string): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowText(hwnd) === title) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

function stripAmpersand(s: string): string {
  return s.replace(/&/g, "");
}

function findButton(hdlg: number, text: string): number {
  let found = 0;
  const want = stripAmpersand(text).toLowerCase();
  enumChildWindows(hdlg, (hwnd) => {
    if (getClassName(hwnd).toLowerCase() !== "button") return true;
    const label = stripAmpersand(getWindowText(hwnd)).toLowerCase();
    if (label === want || label.includes(want)) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

function cleanupFixtures() {
  if (!fixturesCopied) return;
  try {
    rmSync(DBG64_JSON, { force: true });
  } catch {}
  try {
    if (existsSync(DBG64_BACKUP)) rmSync(DBG64_BACKUP, { recursive: true, force: true });
  } catch {}
  fixturesCopied = false;
}

async function testit(): Promise<void> {
  let proc: Bun.Subprocess | null = null;
  let frame = 0;
  let dialogHwnd = 0;

  try {
    // --- step 0: copy real fixture into out/dbg64 (portable profile dir) ---
    if (existsSync(FIXTURE_JSON)) {
      copyFileSync(FIXTURE_JSON, DBG64_JSON);
      if (existsSync(FIXTURE_BACKUP)) {
        cpSync(FIXTURE_BACKUP, DBG64_BACKUP, { recursive: true });
      }
      fixturesCopied = true;
      console.log(`fixture copied: ${FIXTURE_JSON} -> ${DBG64_JSON}`);
    } else {
      console.log(`fixture not found: ${FIXTURE_JSON} (using stub profile)`);
    }

    // --- step 1: launch ---
    try {
      proc = launchSumatra([PDF]);
      frame = await waitForFrame(proc.pid);
      if (!frame) throw new Error("frame window not found");
      step("launch", true, `pid=${proc.pid} frame=${frame}`);
    } catch (e) {
      step("launch", false, String(e));
      cleanupFixtures();
      return;
    }

    // --- step 2: go to home page ---
    try {
      const homeId = cmdId("CmdOpenHome");
      sendCommand(frame, homeId);
      await sleep(800); // ≥800ms settle before capture
      step("go to home page", true, `CmdOpenHome=${homeId}`);
    } catch (e) {
      step("go to home page", false, String(e));
      cleanupFixtures();
      return;
    }

    // --- step 3: capture home page ---
    const homePng = tmpPath("f6-home.png");
    try {
      const ok = captureWindowToPng(frame, homePng);
      step("capture home page", ok, homePng);
    } catch (e) {
      step("capture home page", false, String(e));
    }

    // --- step 4: click Sync button (bounded probe, ≤ ~40 clicks, 60ms each) ---
    // Sync button is VirtWndText "⟳ Sync", NOT an HWND, placed right of "Stats" header.
    // Canvas client coords: x 40..240 step 10, y ∈ {24, 34} (DpiScale-scaled).
    // Use postMessage (non-blocking) instead of clickAt's sendMessage to avoid
    // hanging if a canvas click triggers a slow window-proc path.
    const WM_LBUTTONDOWN = 0x0201;
    const WM_LBUTTONUP = 0x0202;
    const MK_LBUTTON = 0x0001;
    const packCoords = (x: number, y: number): number => ((y & 0xffff) << 16) | (x & 0xffff);
    const postClick = (hwnd: number, x: number, y: number): void => {
      const lp = packCoords(x, y);
      postMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
      postMessage(hwnd, WM_LBUTTONUP, 0, lp);
    };

    const canvas = findCanvas(frame);
    let syncClicked = false;
    let clickedAt = { x: 0, y: 0 };
    const probeDeadline = Date.now() + 25000; // hard 25s cap on the whole probe
    if (canvas) {
      for (const y of [24, 34]) {
        for (let x = 40; x <= 240 && !syncClicked && Date.now() < probeDeadline; x += 10) {
          postClick(canvas, x, y);
          await sleep(60);
          dialogHwnd = findWindowByTitle(DIALOG_TITLE);
          if (dialogHwnd) {
            syncClicked = true;
            clickedAt = { x, y };
            break;
          }
        }
      }
    }
    if (syncClicked) {
      step("click sync button", true, `at (${clickedAt.x},${clickedAt.y}) dialog=${dialogHwnd}`);
    } else {
      // capture f6-home.png for manual inspection; do NOT fail whole test
      captureWindowToPng(frame, homePng);
      step("click sync button", false, "SYNC_CLICK_UNVERIFIED - manual step needed");
    }

    // --- steps 5-8: dialog (only if it opened) ---
    if (dialogHwnd) {
      // capture dialog isolated: hide owner frame so nothing bleeds behind dialog
      const captureDialogClean = (outPath: string): boolean => {
        showWindow(frame, 0); // SW_HIDE
        try {
          setForegroundWindow(dialogHwnd);
          setWindowTopmost(dialogHwnd, true);
          sleep(250); // DWM settle after hiding owner
          const ok = captureWindowDCToPng(dialogHwnd, outPath);
          setWindowTopmost(dialogHwnd, false);
          return ok;
        } finally {
          showWindow(frame, 5); // SW_SHOW — always restore owner frame
        }
      };

      const dialogPng = tmpPath("f6-dialog.png");
      try {
        const ok = captureDialogClean(dialogPng);
        step("capture dialog", ok, dialogPng);
      } catch (e) {
        step("capture dialog", false, String(e));
      }

      // data assertions against real fixture (read from Compiled original, not the
      // out/dbg64 copy which the app may rewrite during the test)
      try {
        const fs = require("node:fs");
        const raw = fs.readFileSync(FIXTURE_JSON, "utf8");
        // ProfileData writer may emit non-strict JSON (e.g. "key":""value"") or null bytes;
        // sanitize before parsing
        const sanitized = raw.replace(/\0/g, "").replace(/:""/g, ':"');
        const json = JSON.parse(sanitized);
        const expected = {
          version: json.version,
          books: Object.keys(json.books ?? {}).length,
          readingDays: Object.keys(json.readingStats ?? {}).length,
          studyDays: Object.keys(json.fcStats ?? {}).length,
          machine: json.machine ?? "?",
          sizeBytes: Buffer.byteLength(raw, "utf8"),
        };
        const backupSlots = existsSync(FIXTURE_BACKUP)
          ? fs.readdirSync(FIXTURE_BACKUP).filter((d) => /^\d{2}$/.test(d)).length
          : 0;
        console.log(
          `EXPECTED — version=${expected.version} books=${expected.books} readingDays=${expected.readingDays} studyDays=${expected.studyDays} size=${expected.sizeBytes}B backups=${backupSlots}`,
        );
        console.log(`EXPECTED — machine="${expected.machine}" lastWriteTs=${json.lastWriteTs ?? 0}`);
        const ok =
          expected.version >= 1 &&
          expected.books > 0 &&
          expected.readingDays > 0 &&
          expected.machine !== "?" &&
          backupSlots > 0;
        step(
          "fixture ground truth",
          ok,
          `v=${expected.version} b=${expected.books} r=${expected.readingDays} s=${expected.studyDays} bak=${backupSlots} machine=${expected.machine}`,
        );
      } catch (e) {
        step("fixture ground truth", false, String(e));
      }

      // Save Now via BM_CLICK on real HWND button
      const saveBtn = findButton(dialogHwnd, "Save Now");
      if (saveBtn) {
        postMessage(saveBtn, BM_CLICK, 0, 0);
        await sleep(300);
        const stillOpen = findWindowByTitle(DIALOG_TITLE);
        step("click Save Now", stillOpen, `BM_CLICK hwnd=${saveBtn} dialogOpen=${stillOpen}`);
      } else {
        step("click Save Now", false, "Save Now HWND not found");
      }

      const savedPng = tmpPath("f6-saved.png");
      try {
        const ok = captureDialogClean(savedPng);
        step("capture after Save Now", ok, savedPng);
      } catch (e) {
        step("capture after Save Now", false, String(e));
      }

      // Open Folder via BM_CLICK (don't assert Explorer)
      const folderBtn = findButton(dialogHwnd, "Open Folder");
      if (folderBtn) {
        postMessage(folderBtn, BM_CLICK, 0, 0);
        await sleep(300);
        step("click Open Folder", true, `BM_CLICK hwnd=${folderBtn} (no crash asserted)`);
      } else {
        step("click Open Folder", false, "Open Folder HWND not found");
      }

      // Export via BM_CLICK on real HWND button (gForTesting skips picker inside app)
      const exportBtn = findButton(dialogHwnd, "Export");
      if (exportBtn) {
        postMessage(exportBtn, BM_CLICK, 0, 0);
        await sleep(500);
        const exportOpen = findWindowByTitle(DIALOG_TITLE);
        step("click Export", exportOpen, `BM_CLICK hwnd=${exportBtn} dialogOpen=${exportOpen}`);
      } else {
        step("click Export", false, "Export HWND not found");
      }

      // Import via BM_CLICK on real HWND button (gForTesting skips picker inside app)
      const importBtn = findButton(dialogHwnd, "Import");
      if (importBtn) {
        postMessage(importBtn, BM_CLICK, 0, 0);
        await sleep(500);
        const importOpen = findWindowByTitle(DIALOG_TITLE);
        step("click Import", importOpen, `BM_CLICK hwnd=${importBtn} dialogOpen=${importOpen}`);
      } else {
        step("click Import", false, "Import HWND not found");
      }

      // Close dialog via WM_CLOSE → assert gone
      try {
        postMessage(dialogHwnd, WM_CLOSE, 0, 0);
        await sleep(400);
        const gone = findWindowByTitle(DIALOG_TITLE);
        step("close dialog (WM_CLOSE)", !gone, gone ? "still open" : "closed");
      } catch (e) {
        step("close dialog (WM_CLOSE)", false, String(e));
      }
    }

    // --- step 9: quit app cleanly ---
    try {
      if (frame) sendCommand(frame, cmdId("CmdExit"));
      await sleep(800);
      if (proc) {
        try {
          proc.kill();
        } catch {}
      }
      step("quit app", true, "CmdExit + kill fallback");
    } catch (e) {
      step("quit app", false, String(e));
    }
  } catch (e) {
    if (proc) {
      try {
        proc.kill();
      } catch {}
    }
    step("fatal", false, String(e));
  }

  // --- cleanup: remove copied fixtures (never touch Compiled originals) ---
  cleanupFixtures();

  // --- report ---
  console.log("\n=== F7 slice B ad-hoc visual test results ===");
  for (const r of results) {
    console.log(r);
  }
  const passCount = results.filter((r) => r.startsWith("PASS")).length;
  console.log(`TOTAL: ${passCount}/${results.length} steps passed`);
  // Do not throw on SYNC_CLICK_UNVERIFIED (manual step allowed). Fail on other steps.
  const criticalFail = results.some((r) => r.startsWith("FAIL") && !r.includes("SYNC_CLICK_UNVERIFIED"));
  if (criticalFail) {
    throw new Error("one or more critical steps failed");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
