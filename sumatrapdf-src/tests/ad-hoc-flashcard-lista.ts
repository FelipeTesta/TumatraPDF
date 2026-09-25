// Repro: Lista panel redo (2026-09-25) — docked right-side panel (AI-chat
// pattern) with a 4-column owner-drawn listbox:
//   Flashcard (masked-text label) | Pág | Due (countdown) | Status (colored)
// Verifies: panel opens docked (listbox at the right edge, visible), rows
// populate with the current doc's cards (new/due/learn/ok counts in the log),
// clicking a row navigates (log line), toggle off hides the panel, toggle on
// re-shows + repopulates. Card creation reuses the keyboard-selection path
// with retry via the per-line-flushed [fc] log.
import { launchSumatra, waitForFrame, findCanvas, sendCommand } from "./win-automation";
import {
  captureWindowToPng,
  sleep,
  postMessage,
  getWindowRect,
  isWindowVisible,
  enumChildWindows,
  getClassName,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_KEYDOWN,
  packCoords,
} from "./winapi";
import { cmdId, tmpPath, EXE } from "./util";
import * as fs from "node:fs";
import * as path from "node:path";

function findLogPath(): string {
  const exeDir = path.dirname(EXE);
  const cands = [path.join(exeDir, "sumlog.txt"), path.join(exeDir, "sumatra-log.txt")];
  const laa = process.env.LOCALAPPDATA || "";
  if (laa && fs.existsSync(path.join(laa, "SumatraPDF"))) {
    for (const d of fs.readdirSync(path.join(laa, "SumatraPDF"))) {
      cands.push(path.join(laa, "SumatraPDF", d, "sumatra-log.txt"));
    }
  }
  for (const c of cands) {
    if (fs.existsSync(c)) {
      return c;
    }
  }
  return "";
}

// read only what the app wrote since the test started (rotation-safe)
let logStartOffset = 0;
function readLogSlice(): string {
  const p = findLogPath();
  if (!p || !fs.existsSync(p)) {
    return "";
  }
  const size = fs.statSync(p).size;
  if (size < logStartOffset) {
    logStartOffset = 0; // rotated
  }
  const fd = fs.openSync(p, "r");
  const buf = Buffer.alloc(size - logStartOffset);
  fs.readSync(fd, buf, 0, buf.length, logStartOffset);
  fs.closeSync(fd);
  return buf.toString("utf8");
}

function countInLog(needle: string): number {
  return readLogSlice()
    .split(/\r?\n/)
    .filter((l) => l.includes(needle)).length;
}

async function waitForLogCount(needle: string, minCount: number, timeoutMs = 12000): Promise<boolean> {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    if (countInLog(needle) >= minCount) {
      return true;
    }
    await sleep(400);
  }
  return countInLog(needle) >= minCount;
}

function findListBox(frame: number): number {
  let found = 0;
  enumChildWindows(frame, (h) => {
    if (getClassName(h) === "ListBox") {
      found = h;
      return false;
    }
    return true;
  });
  return found;
}

async function createTwoCards(frame: number): Promise<number> {
  // 2 cloze cards via keyboard selection + 'S'; each attempt is verified via
  // the [fc] log ("Cloze card added") and retried — the second card selects
  // 5 words so its bounds (and key) differ from the first
  const base = countInLog("Cloze card added");
  let created = 0;
  for (let attempt = 0; attempt < 8 && created < 2; attempt++) {
    const words = created === 0 ? 3 : 5;
    sendCommand(frame, cmdId("CmdSelectTextViaKeyboard"));
    await sleep(800);
    for (let i = 0; i < words; i++) {
      sendCommand(frame, cmdId("CmdExtendSelectionWordRight"));
      await sleep(300);
    }
    await sleep(500);
    postMessage(frame, WM_KEYDOWN, 0x53, 0); // 'S'
    if (await waitForLogCount("Cloze card added", base + created + 1, 6000)) {
      created++;
    }
  }
  return created;
}

export async function testit(): Promise<void> {
  const logPath = findLogPath();
  if (logPath) {
    logStartOffset = fs.statSync(logPath).size;
  }

  const pdf = process.argv[2] || path.resolve("ext", "a-zlib", "zlib.3.pdf");
  const proc = launchSumatra(["-for-testing", "-page", "3", pdf]);
  const frame = await waitForFrame(proc.pid);
  await sleep(3500);
  const canvas = findCanvas(frame);

  const failures: string[] = [];
  const check = (ok: boolean, what: string) => {
    console.log(ok ? `PASS: ${what}` : `FAIL: ${what}`);
    if (!ok) {
      failures.push(what);
    }
  };

  sendCommand(frame, cmdId("CmdZoomFitPage"));
  await sleep(2000);

  // two cards with distinct masked text (3 vs 5 words), log-verified + retried
  const nCards = await createTwoCards(frame);
  check(nCards === 2, `2 cards created (got ${nCards})`);

  // flashcard mode ON (the toolbar that owns the Lista button)
  sendCommand(frame, cmdId("CmdFlashcardToggle"));
  await sleep(1500);

  // open the Lista panel
  sendCommand(frame, cmdId("CmdFlashcardLista"));
  const opened = await waitForLogCount("FlashcardSidebarPopulate - adding 2 cards to list", 1, 10000);
  check(opened, "Lista populated with 2 cards (log)");
  check(
    await waitForLogCount("FlashcardSidebarPopulate - 2 cards (new=2 due=0 learn=0 ok=0)", 1, 8000),
    "both cards counted as new (log)",
  );

  const listbox = findListBox(frame);
  check(listbox !== 0, "ListBox child found in frame");
  if (!listbox) {
    throw new Error("no listbox found - panel did not create");
  }
  await sleep(800); // let the relayout settle
  check(isWindowVisible(listbox), "list visible after open");

  // docked right: the listbox's right edge must be near the frame's right edge
  const lbRect = getWindowRect(listbox);
  const frameRect = getWindowRect(frame);
  const dockGap = frameRect.right - lbRect.right;
  console.log("listbox rect:", JSON.stringify(lbRect), "frame right:", frameRect.right, "gap:", dockGap);
  check(dockGap >= 0 && dockGap < 40, `panel docked right (gap ${dockGap}px < 40)`);

  // canvas must have lost the panel width (canvas right edge << frame right)
  const canvasRect = getWindowRect(canvas);
  console.log("canvas rect:", JSON.stringify(canvasRect));
  check(canvasRect.right < lbRect.left, "canvas shrunk left of the panel");
  captureWindowToPng(frame, tmpPath("lista-open.png"));

  // click row 0 -> navigates to that card
  postMessage(listbox, WM_LBUTTONDOWN, 1, packCoords(30, 10));
  postMessage(listbox, WM_LBUTTONUP, 0, packCoords(30, 10));
  check(await waitForLogCount("Lista - navigating to card 0", 1, 8000), "row click navigates to card 0 (log)");

  // toggle off -> hidden
  sendCommand(frame, cmdId("CmdFlashcardLista"));
  await sleep(1500); // relayout applies visibility
  check(!isWindowVisible(listbox), "list hidden after toggle off");

  // toggle on -> visible again + repopulated
  const popCountBefore = countInLog("FlashcardSidebarPopulate - adding 2 cards to list");
  sendCommand(frame, cmdId("CmdFlashcardLista"));
  await sleep(1500);
  check(isWindowVisible(listbox), "list visible after toggle on");
  check(
    await waitForLogCount("FlashcardSidebarPopulate - adding 2 cards to list", popCountBefore + 1, 8000),
    "list repopulated on reopen (log)",
  );
  captureWindowToPng(frame, tmpPath("lista-reopen.png"));

  proc.kill();
  await sleep(800);

  if (failures.length > 0) {
    throw new Error(`${failures.length} check(s) failed:\n- ${failures.join("\n- ")}`);
  }
  console.log("ALL CHECKS PASSED");
}

if (import.meta.main) {
  await testit();
}
