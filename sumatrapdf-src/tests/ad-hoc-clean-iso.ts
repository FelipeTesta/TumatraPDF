// Minimal isolation test: launch → Clean command → dialog open? → logs.
// Rules out the long cloze-repro flow when diagnosing dialog issues.
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import { sleep } from "./winapi";
import { cmdId, EXE } from "./util";
import * as fs from "node:fs";
import * as path from "node:path";

export async function testit(): Promise<void> {
    const pdf = process.argv[2] || path.resolve("ext", "a-zlib", "zlib.3.pdf");
    const proc = launchSumatra(["-for-testing", "-page", "3", pdf]);
    const frame = await waitForFrame(proc.pid);
    await sleep(3000);

    const winapi = await import("./winapi");
    console.log("frame title:", winapi.getWindowTextFull(frame));

    // flashcard ON first (toolbar required for the command path)
    sendCommand(frame, cmdId("CmdFlashcardToggle"));
    await sleep(1200);
    // then Clean
    sendCommand(frame, cmdId("CmdFlashcardCleanHistory"));
    await sleep(1500);

    let cleanDlg = 0;
    winapi.enumWindows((h: number) => {
        if (winapi.getWindowText(h) === "Clean History") {
            cleanDlg = h;
            return false;
        }
        return true;
    });
    console.log("clean dialog open:", !!cleanDlg);
    if (cleanDlg) {
        const cr = winapi.getClientRect(cleanDlg);
        console.log("client size:", JSON.stringify(cr));
        winapi.captureWindowToPng(cleanDlg, path.join("tests", "tmp", "clean-iso.png"));
        winapi.postMessage(cleanDlg, 0x0010, 0, 0); // WM_CLOSE
        await sleep(400);
        console.log("closed:", !winapi.isWindowVisible(cleanDlg));
    }

    proc.kill();
    await sleep(800);
    const exeDir = path.dirname(EXE);
    const sumlog = path.join(exeDir, "sumlog.txt");
    if (fs.existsSync(sumlog)) {
        for (const l of fs.readFileSync(sumlog, "utf8").split(/\r?\n/)) {
            if (l.includes("[fc")) console.log(l);
        }
    }
}

if (import.meta.main) {
    await testit();
}
