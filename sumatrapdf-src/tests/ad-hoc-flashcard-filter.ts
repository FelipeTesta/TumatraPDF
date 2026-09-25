// Repro: Study Filter window + cross-document session (2026-09-25).
// Phase A (filter dialog on tab 0):
//   - expression input with set syntax ("-1-99999;" removes every page)
//   - Filters ON/OFF toggle keeps the expression
//   - bookmark mirror injects/removes a chapter range (only if the doc has
//     a TOC — printed, non-fatal otherwise)
//   - Clear is a 2s hold
// Phase B (global session):
//   - cross-doc checkbox switches the study scope to all PDF tabs of the
//     window; the queue rebuilds across tabs; per-book filters are
//     respected (tab 0 filtered to 0 -> queue holds only the copy's cards)
//   - advancing switches tabs automatically; rating saves to the card's
//     OWN book JSON (per-book, keyed by MD5 of the path)
// Uses two copies of zlib.3.pdf (small, local, identity ctm) so no Google
// Drive stall can interfere; cards are created at runtime via keyboard
// selection (mouse drag can't be synthesized — see issue-1699.ts).
// Tab navigation goes BY FRAME TITLE: this fork starts with an extra About
// tab at index 0, so "next tab" counts are not stable across runs.
// Card creation retries with log polling: the [fc] log is flushed per line,
// so we wait for "Cloze card added" to appear before trying again.
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import {
  captureWindowToPng,
  sleep,
  postMessage,
  sendText,
  getControlText,
  getClientRect,
  getWindowText,
  getWindowTextFull,
  enumWindows,
  enumChildWindows,
  getClassName,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_KEYDOWN,
  WM_CLOSE,
  VK_RETURN,
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

// read only what the app wrote since the test started (rotation-safe: if
// the file shrank it was rotated, read it whole)
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

function findWindowByTitle(title: string): number {
  let found = 0;
  enumWindows((h: number) => {
    if (getWindowText(h) === title) {
      found = h;
      return false;
    }
    return true;
  });
  return found;
}

function findChildByClass(parent: number, className: string): number {
  let found = 0;
  enumChildWindows(parent, (h: number) => {
    if (getClassName(h) === className) {
      found = h;
      return false;
    }
    return true;
  });
  return found;
}

async function gotoTabByTitle(frame: number, titleNeedle: string, maxSteps = 5) {
  for (let i = 0; i < maxSteps; i++) {
    if (getWindowTextFull(frame).includes(titleNeedle)) {
      return true;
    }
    sendCommand(frame, cmdId("CmdNextTab"));
    await sleep(1800);
  }
  return getWindowTextFull(frame).includes(titleNeedle);
}

async function createTwoCards(frame: number): Promise<number> {
  // 2 cloze cards via keyboard selection + 'S'; each press is verified via
  // the [fc] log ("Cloze card added") and retried — the second card
  // selects 5 words so its bounds (and key) differ from the first
  sendCommand(frame, cmdId("CmdZoomFitPage"));
  await sleep(2000);
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
  const src = path.resolve("ext", "a-zlib", "zlib.3.pdf");
  const copy = tmpPath("fc-crossdoc-copy.pdf");
  fs.copyFileSync(src, copy);
  const logP = findLogPath();
  if (logP && fs.existsSync(logP)) {
    logStartOffset = fs.statSync(logP).size; // assert only on THIS run
  }

  const proc = launchSumatra(["-for-testing", "-page", "3", src, copy]);
  const frame = await waitForFrame(proc.pid);
  await sleep(5000);
  console.log("frame title:", getWindowTextFull(frame));

  // ---- create 2 cards in EACH document (runtime annotations, no save)
  if (!(await gotoTabByTitle(frame, "zlib.3.pdf"))) {
    proc.kill();
    throw new Error("could not reach the zlib tab");
  }
  const n0 = await createTwoCards(frame);
  console.log("cards created in zlib tab:", n0);
  if (!(await gotoTabByTitle(frame, "fc-crossdoc-copy"))) {
    proc.kill();
    throw new Error("could not reach the copy tab");
  }
  const n1 = await createTwoCards(frame);
  console.log("cards created in copy tab:", n1);
  await gotoTabByTitle(frame, "zlib.3.pdf");

  // flashcard mode ON + study ON on the zlib tab -> queue = its 2 new cards
  sendCommand(frame, cmdId("CmdFlashcardToggle"));
  await sleep(1200);
  sendCommand(frame, cmdId("CmdFlashcardStudy"));
  await sleep(1200);

  // ---- Phase A: filter dialog on the zlib tab ---------------------------
  sendCommand(frame, cmdId("CmdFlashcardFilter"));
  await sleep(800);
  const dlg = findWindowByTitle("Filtro de Estudo");
  console.log("filter dialog open:", !!dlg);
  if (!dlg) {
    proc.kill();
    throw new Error("filter dialog did not open");
  }
  const cr = getClientRect(dlg);
  captureWindowToPng(dlg, tmpPath("fcf-open.png"));
  const edit = findChildByClass(dlg, "Edit");
  const list = findChildByClass(dlg, "ListBox");
  console.log("edit child:", !!edit, "list child:", !!list);

  // fraction helpers (layout is DPI-proportional inside the client rect)
  const pt = (fx: number, fy: number) => packCoords(Math.round(cr.right * fx), Math.round(cr.bottom * fy));
  const crossDocPt = () => pt(0.4, 0.06);
  const togglePt = () => pt(0.36, 0.92);
  const clearPt = () => pt(0.65, 0.92);

  // 1) expression input: remove EVERY page -> queue must drop to 0
  sendText(edit, "-1-99999;");
  await sleep(300);
  postMessage(edit, WM_KEYDOWN, VK_RETURN, 0);
  await sleep(800);
  captureWindowToPng(dlg, tmpPath("fcf-removeall.png"));

  // 2) Filters ON/OFF: disable without discarding the expression
  postMessage(dlg, WM_LBUTTONDOWN, 0, togglePt());
  await sleep(350);
  postMessage(dlg, WM_LBUTTONUP, 0, togglePt());
  await sleep(500);
  captureWindowToPng(dlg, tmpPath("fcf-off.png"));
  // re-enable for the next steps
  postMessage(dlg, WM_LBUTTONDOWN, 0, togglePt());
  await sleep(350);
  postMessage(dlg, WM_LBUTTONUP, 0, togglePt());
  await sleep(500);

  // 3) bookmark mirror: click row 0 of the listbox (only if the doc has a
  // TOC — zlib has none, so this is a no-op print for diagnosis)
  if (list) {
    const lcr = getClientRect(list);
    const rowPt = packCoords(Math.round(lcr.right * 0.5), Math.round(lcr.bottom * 0.05));
    postMessage(list, WM_LBUTTONDOWN, 0, rowPt);
    await sleep(350);
    postMessage(list, WM_LBUTTONUP, 0, rowPt);
    await sleep(600);
    console.log("edit text after bookmark click:", JSON.stringify(getControlText(edit, 256)));
    captureWindowToPng(dlg, tmpPath("fcf-bookmark.png"));
    // uncheck -> the token is removed again
    postMessage(list, WM_LBUTTONDOWN, 0, rowPt);
    await sleep(350);
    postMessage(list, WM_LBUTTONUP, 0, rowPt);
    await sleep(600);
    console.log("edit text after uncheck:", JSON.stringify(getControlText(edit, 256)));
  }

  // 4) Clear: hold 2s (draining line), then the expression is emptied
  postMessage(dlg, WM_LBUTTONDOWN, 0, clearPt());
  await sleep(600);
  postMessage(dlg, WM_LBUTTONUP, 0, clearPt());
  await sleep(400);
  console.log("after 600ms hold, still open:", !!findWindowByTitle("Filtro de Estudo"));
  postMessage(dlg, WM_LBUTTONDOWN, 0, clearPt());
  await sleep(2300);
  postMessage(dlg, WM_LBUTTONUP, 0, clearPt());
  await sleep(500);
  console.log("after 2.3s hold, still open:", !!findWindowByTitle("Filtro de Estudo"));
  console.log("edit after clear:", JSON.stringify(getControlText(edit, 256)));
  captureWindowToPng(dlg, tmpPath("fcf-clear.png"));

  // 5) re-apply remove-all on the ZLIB tab only, then switch the scope to
  // the global session: zlib contributes 0 cards (its filter removes every
  // page), the copy contributes its 2 new cards -> queue == 2 and every
  // advance must switch tabs
  sendText(edit, "-1-99999;");
  await sleep(300);
  postMessage(edit, WM_KEYDOWN, VK_RETURN, 0);
  await sleep(600);
  postMessage(dlg, WM_LBUTTONDOWN, 0, crossDocPt());
  await sleep(350);
  postMessage(dlg, WM_LBUTTONUP, 0, crossDocPt());
  await sleep(1200);
  captureWindowToPng(dlg, tmpPath("fcf-crossdoc.png"));
  postMessage(dlg, WM_CLOSE, 0, 0);
  await sleep(600);
  console.log("filter dialog closed:", !findWindowByTitle("Filtro de Estudo"));

  // ---- Phase B: global session navigation -------------------------------
  // the first card already belongs to the copy tab -> the app must have
  // switched tabs when the queue rebuilt; advance a few times to be safe
  let switchedToCopy = false;
  for (let i = 0; i < 4; i++) {
    if (getWindowTextFull(frame).includes("fc-crossdoc-copy")) {
      switchedToCopy = true;
      break;
    }
    sendCommand(frame, cmdId("CmdFlashcardNext"));
    await sleep(1000);
  }
  console.log("current frame title:", getWindowTextFull(frame));
  console.log("switched to copy tab:", switchedToCopy);

  // rate the current card (Good): must save to the card's OWN book JSON
  postMessage(frame, WM_KEYDOWN, 0x33, 0); // '3'
  await sleep(1500);

  proc.kill();
  await sleep(1000);

  // ---- assert on THIS run's [fc] log lines -------------------------------
  const fcLines = readLogSlice()
    .split(/\r?\n/)
    .filter((l) => l.includes("[fc"));
  console.log(`=== ${logP || "(no log file)"} ===`);
  for (const l of fcLines) console.log(l);
  const logText = fcLines.join("\n");

  const mustHave = [
    "[fc] BuildFilteredStudyOrder - 2 cards (scope=current-doc, tabsUsed=1,",
    "[fc] Filter - dialog opened",
    "[fc] Filter - applied expr '-1-99999;' (enabled=1)",
    "[fc] CollectTabCards - 'zlib.3.pdf': 0 cards pass (of 2)",
    "[fc] BuildFilteredStudyOrder - 0 cards (scope=current-doc, tabsUsed=1,",
    "[fc] Filter - toggle -> OFF (expr kept: '-1-99999;')",
    "[fc] Filter - CLEARED (hold 2s confirmed), re-enabled",
    "[fc] Filter - study scope -> GLOBAL SESSION (all PDF tabs of this window)",
    "[fc] CollectTabCards - 'fc-crossdoc-copy.pdf': 2 cards pass (of 2)",
    "[fc] BuildFilteredStudyOrder - 2 cards (scope=global-session, tabsUsed=2,",
    "[fc] NavigateToEntry - global session: switching to tab",
    "[fc] HandleFlashcardRate - rated 3 card",
  ];
  const missing = mustHave.filter((m) => !logText.includes(m));
  // the rated card must belong to the copy tab and the save must target
  // the copy's own JSON (per-book isolation)
  const rateOf = logText.match(/rated 3 card \d+ of '([^']+)'/);
  if (!rateOf) {
    missing.push("rate log with book name");
  } else {
    console.log("rated card belongs to:", rateOf[1]);
    if (!rateOf[1].includes("fc-crossdoc-copy")) {
      missing.push("rated card belongs to copy tab (got: " + rateOf[1] + ")");
    }
    const saveLine = fcLines.find((l) => l.includes("FlashcardStudySave - saving to"));
    if (saveLine && !saveLine.includes("fc-crossdoc-copy")) {
      missing.push("save targets the copy's JSON (got: " + saveLine + ")");
    }
  }
  if (!switchedToCopy) {
    missing.push("frame title switched to the copy tab");
  }
  if (n0 < 2 || n1 < 2) {
    missing.push(`card creation incomplete (zlib=${n0}, copy=${n1})`);
  }
  if (missing.length > 0) {
    throw new Error("missing expected [fc] log lines:\n- " + missing.join("\n- "));
  }
  console.log("ALL FILTER + CROSS-DOC ASSERTIONS PASSED");
}

if (import.meta.main) {
  await testit();
}
