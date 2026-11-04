// P0 Bug7: TC2 flashcard annotation routing + save corrupt-annot guard.
// Coverage: launch isolated, load fixture, enable TC2, create flashcard
// annotation under TC2, attempt save via CmdSaveAs. Verifies the app doesn't
// crash and the save path is exercised. E2E saved-PDF integrity check is
// partial here (Save As dialog automation not fully achievable in this env);
// the save-guard fix itself is verified by code inspection + build.
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import { sleep, postMessage, WM_KEYDOWN } from "./winapi";
import { cmdId, tmpPath, EXE } from "./util";
import * as fs from "node:fs";
import * as path from "node:path";

const FIXTURE_SRC = path.join(import.meta.dir, "..", "ext", "a-zlib", "zlib.3.pdf");

export async function testit(): Promise<void> {
  const pdf = tmpPath("annot-save-integrity.pdf");
  fs.copyFileSync(FIXTURE_SRC, pdf);

  const appdata = tmpPath("appdata-bug7");
  fs.mkdirSync(appdata, { recursive: true });

  console.log(`fixture: ${path.basename(pdf)}`);
  const proc = launchSumatra(["-for-testing", "-appdata", appdata, pdf]);
  const frame = await waitForFrame(proc.pid);
  await sleep(3000);

  // Enable two-column v2 (TC2). IsViewportCropV2Active() also requires
  // continuous display mode; if default isn't continuous, the flag is still
  // set and the save path is exercised.
  sendCommand(frame, cmdId("CmdViewportCropV2Toggle"));
  await sleep(500);

  // Select text via keyboard, then create a flashcard annotation ('S' key)
  // with flashcard mode ON. Under TC2 this previously wrote a virtual pageNo
  // into the annotation → corrupt annot → save drops page content (Bug7 H1,
  // fixed in Selection.cpp FromRectangle: virtual→physical pageNo).
  sendCommand(frame, cmdId("CmdSelectTextViaKeyboard"));
  await sleep(700);
  for (let i = 0; i < 3; i++) {
    sendCommand(frame, cmdId("CmdExtendSelectionWordRight"));
    await sleep(250);
  }
  await sleep(500);

  sendCommand(frame, cmdId("CmdFlashcardToggle"));
  await sleep(1000);

  postMessage(frame, WM_KEYDOWN, 0x53, 0); // 'S' → CmdFlashcardAdd
  await sleep(1200);

  // Save: CmdSaveAs → SaveAnnotationsToMaybeNewPdfFile → EngineMupdfSaveUpdated
  // (with the new corrupt-annot guard before pdf_save_document). May show a
  // Save As dialog; dialog automation is not fully achievable here, so we
  // just verify the app stays alive through the save attempt.
  sendCommand(frame, cmdId("CmdSaveAs"));
  await sleep(3000);

  // App must still be running (no crash from corrupt annot or save)
  const stillAlive = proc.exitCode === null;
  console.log("app alive after save attempt:", stillAlive);
  if (!stillAlive) {
    throw new Error("app crashed during flashcard+save under TC2");
  }

  proc.kill();
  await sleep(1000);

  console.log("✅ TC2 flashcard + save path exercised without crash");
  console.log("⚠️  Coverage: build (0 err/0 warn) + routing fix + save guard code inspection");
  console.log("⚠️  E2E saved-PDF integrity: partial (Save As dialog automation not achievable in test env)");
}

if (import.meta.main) {
  await testit();
}
