// Measures EngineMupdfSaveUpdated (CmdSaveAnnotations) wall time on a COPY of
// the user's scanned book (copy => the real file is never modified).
// Used to quantify the save-freeze fix (no re-compression on same-file save):
//   bun tests/ad-hoc-save-timing.ts <pdf>
import { launchSumatra, waitForFrame, findCanvas, sendCommand } from "./win-automation";
import { sleep, postMessage, WM_KEYDOWN } from "./winapi";
import { cmdId, tmpPath, EXE } from "./util";
import * as fs from "node:fs";
import * as path from "node:path";

export async function testit(): Promise<void> {
    const src = process.argv[2];
    if (!src || !fs.existsSync(src)) {
        throw new Error("usage: bun tests/ad-hoc-save-timing.ts <pdf>");
    }
    const copyPath = tmpPath("save-timing.pdf");
    fs.copyFileSync(src, copyPath);
    const sizeBefore = fs.statSync(copyPath).size;
    console.log("copy:", copyPath, sizeBefore, "bytes");

    const proc = launchSumatra(["-for-testing", "-page", "3", copyPath]);
    const frame = await waitForFrame(proc.pid);
    await sleep(4000);
    const canvas = findCanvas(frame);

    // add one unsaved card so the annotation save has work to do
    sendCommand(frame, cmdId("CmdSelectTextViaKeyboard"));
    await sleep(700);
    for (let i = 0; i < 3; i++) {
        sendCommand(frame, cmdId("CmdExtendSelectionWordRight"));
        await sleep(250);
    }
    postMessage(frame, WM_KEYDOWN, 0x53, 0);
    await sleep(1500);

    // same-file save: blocks the UI thread while pdf_save_document runs;
    // poll the (fflush-per-line) log for the duration line
    const logPath = path.join(path.dirname(EXE), "sumlog.txt");
    fs.rmSync(logPath, { force: true });
    sendCommand(frame, cmdId("CmdSaveAnnotations"));
    const t0 = Date.now();
    let line = "";
    for (let i = 0; i < 360; i++) {
        await sleep(500); // up to 3 minutes
        if (fs.existsSync(logPath)) {
            const txt = fs.readFileSync(logPath, "utf8");
            const m = txt.match(/Saved annotations to .* ms.*/);
            if (m) {
                line = m[0];
                break;
            }
        }
    }
    const wallSec = ((Date.now() - t0) / 1000).toFixed(1);
    console.log("save log line:", line || "(NOT FOUND)");
    console.log("wall clock:", wallSec, "s");
    console.log("file size after:", fs.statSync(copyPath).size, "bytes");
    proc.kill();
}

if (import.meta.main) {
    await testit();
}
