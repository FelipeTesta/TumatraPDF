// Repro: user report — a flashcard "highlight zone" that spans TWO pages only
// covers ONE page. Drives the DEBUG build, creates a cross-page selection with
// the keyboard-select path (visual mode + PageDown extends past the page break),
// adds a cloze with 'S', then asserts the loader re-grouped the per-page
// highlights into ONE card ("found 1 flashcards") while the selection covered 2
// pages ("2 page(s) covered"). Also dumps screenshots (reading + study) so the
// mask over both pages can be checked by eye.
import { launchSumatra, waitForFrame, findCanvas, sendCommand } from "./win-automation";
import { captureWindowToPng, sleep, postMessage, WM_KEYDOWN, getWindowTextFull, getClientRect } from "./winapi";
import { cmdId, tmpPath, EXE } from "./util";
import * as fs from "node:fs";
import * as path from "node:path";

export async function testit(): Promise<void> {
  const pdf = process.argv[2] || path.resolve("ext", "a-zlib", "zlib.3.pdf");
  const exeDir = path.dirname(EXE);
  const proc = launchSumatra(["-for-testing", "-page", "1", pdf]);
  const frame = await waitForFrame(proc.pid);
  await sleep(4000);
  const canvas = findCanvas(frame);
  console.log("frame title:", getWindowTextFull(frame));
  console.log("canvas rect:", JSON.stringify(getClientRect(canvas)));

  // fit page: the keyboard-selection path below works reliably at this zoom
  sendCommand(frame, cmdId("CmdZoomFitPage"));
  await sleep(1500);

  // keyboard selection: caret starts at page 1; 'v' enters visual mode so
  // PageDown EXTENDS the selection instead of just moving the caret.
  sendCommand(frame, cmdId("CmdSelectTextViaKeyboard"));
  await sleep(700);
  postMessage(frame, WM_KEYDOWN, 0x56, 0); // 'v'
  await sleep(400);
  for (let i = 0; i < 8; i++) {
    postMessage(frame, WM_KEYDOWN, 0x22, 0); // VK_NEXT = PageDown extends
    await sleep(300);
  }
  await sleep(600);
  captureWindowToPng(canvas, tmpPath("fcx-sel.png"));

  // create the cloze with 'S'
  postMessage(frame, WM_KEYDOWN, 0x53, 0);
  await sleep(1500);
  captureWindowToPng(canvas, tmpPath("fcx-added.png"));

  // flashcard ON to see the reading-mode markers
  sendCommand(frame, cmdId("CmdFlashcardToggle"));
  await sleep(1500);
  captureWindowToPng(canvas, tmpPath("fcx-reading.png"));

  // study mode: the opaque mask must cover EVERY page of the card. Fit width
  // + continuous so the page-1 mask is fully framed; capture it, then go to
  // page 2 and capture again (the mask must be there too).
  sendCommand(frame, cmdId("CmdZoomFitWidthAndContinuous"));
  await sleep(1200);
  sendCommand(frame, cmdId("CmdFlashcardStudy"));
  await sleep(1500);
  captureWindowToPng(canvas, tmpPath("fcx-study-p1.png"));
  sendCommand(frame, cmdId("CmdGoToNextPage"));
  await sleep(1500);
  captureWindowToPng(canvas, tmpPath("fcx-study-p2.png"));

  proc.kill();
  await sleep(1000);

  const candidates = [path.join(exeDir, "sumlog.txt"), path.join(exeDir, "sumatra-log.txt")];
  const localAppData = process.env.LOCALAPPDATA || "";
  if (localAppData) {
    const base = path.join(localAppData, "SumatraPDF");
    if (fs.existsSync(base)) {
      for (const d of fs.readdirSync(base)) {
        candidates.push(path.join(base, d, "sumatra-log.txt"));
      }
    }
  }
  let fc: string[] = [];
  let logSource = "";
  for (const c of candidates) {
    if (!fs.existsSync(c)) continue;
    const lines = fs.readFileSync(c, "utf8").split(/\r?\n/);
    const found = lines.filter((l) => l.includes("[fc"));
    if (found.length > 0) {
      fc = found;
      logSource = c;
      break;
    }
  }
  console.log(`=== ${logSource || "(no log)"} ===`);
  for (const l of fc) console.log(l);

  const pagesCovered = fc.find((l) => l.includes("page(s) covered"));
  const covered = pagesCovered ? parseInt(pagesCovered.match(/(\d+) page\(s\) covered/)?.[1] || "0") : 0;
  const tokenLine = fc.find((l) => l.includes("cross-page cloze: group token"));
  const foundLine = [...fc].reverse().find((l) => l.includes("FlashcardLoadFromDocument - found"));
  const found = foundLine ? parseInt(foundLine.match(/found (\d+) flashcards/)?.[1] || "-1") : -1;

  console.log(`\nselection covered ${covered} page(s); loader found ${found} card(s)`);
  if (0 === covered) {
    console.log("SKIP: selection did not cross the page break (harness limitation) - cannot verify grouping");
    return;
  }
  if (covered < 2) {
    console.log("SKIP: selection stayed on one page - cannot verify grouping");
    return;
  }
  if (!tokenLine) {
    throw new Error("FAIL: cross-page selection but no group token was written (author not tagged)");
  }
  if (1 !== found) {
    throw new Error(`FAIL: cross-page cloze should be ONE card, loader found ${found}`);
  }
  console.log("PASS: cross-page cloze grouped into 1 card");
}

if (import.meta.main) {
  await testit();
}
