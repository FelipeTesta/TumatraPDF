// Repro: user report 2026-09-23 — create cloze with S (no save), Flashcard ON, Study ON:
// "nothing happens; gray highlight visible but mask doesn't cover the text".
// Drives the DEBUG build so [fc] / [fc-paint] logs land in the log file, then
// dumps every [fc*] line + screenshots for geometry diagnosis.
import { launchSumatra, waitForFrame, findCanvas, sendCommand } from "./win-automation";
import {
    captureWindowToPng,
    sleep,
    postMessage,
    WM_LBUTTONDOWN,
    WM_LBUTTONUP,
    WM_MOUSEMOVE,
    WM_KEYDOWN,
    packCoords,
} from "./winapi";
import { cmdId, tmpPath, EXE } from "./util";
import * as fs from "node:fs";
import * as path from "node:path";
import * as os from "node:os";

export async function testit(): Promise<void> {
    const pdf = process.argv[2] || path.resolve("ext", "a-zlib", "zlib.3.pdf");
    const proc = launchSumatra(["-for-testing", "-page", "3", pdf]);
    const frame = await waitForFrame(proc.pid);
    await sleep(4000);
    const canvas = findCanvas(frame);

    // diagnostics: did the document load?
    const winapi = await import("./winapi");
    console.log("frame title:", winapi.getWindowTextFull(frame));
    const wr = winapi.getWindowRect(frame);
    console.log("frame rect:", JSON.stringify(wr));
    winapi.enumChildWindows(frame, (h) => {
        const cls = winapi.getClassName(h);
        if (cls === "Edit" || cls === "SUMATRA_PDF_PAGEEDIT") {
            console.log("  edit child:", cls, "text:", JSON.stringify(winapi.getControlText(h, 64)));
        }
    });

    // select text via keyboard selection commands (mouse drag can't be
    // synthesized reliably — see issue-1699.ts)
    sendCommand(frame, cmdId("CmdZoomFitPage"));
    await sleep(1500);
    sendCommand(frame, cmdId("CmdSelectTextViaKeyboard"));
    await sleep(700);
    for (let i = 0; i < 3; i++) {
        sendCommand(frame, cmdId("CmdExtendSelectionWordRight"));
        await sleep(250);
    }
    await sleep(500);
    captureWindowToPng(canvas, tmpPath("fc2-sel.png"));

    // create the cloze with 'S' (no document save afterwards — matches user flow)
    postMessage(frame, WM_KEYDOWN, 0x53, 0);
    await sleep(1200);
    captureWindowToPng(canvas, tmpPath("fc2-added.png"));

    // second card so the advance/centering path can be exercised
    sendCommand(frame, cmdId("CmdSelectTextViaKeyboard"));
    await sleep(700);
    for (let i = 0; i < 5; i++) {
        sendCommand(frame, cmdId("CmdExtendSelectionWordRight"));
        await sleep(250);
    }
    await sleep(500);
    postMessage(frame, WM_KEYDOWN, 0x53, 0);
    await sleep(1200);

    // Flashcard ON then Study ON — first card centered, masked
    sendCommand(frame, cmdId("CmdFlashcardToggle"));
    await sleep(1500);
    sendCommand(frame, cmdId("CmdFlashcardStudy"));
    await sleep(1500);
    captureWindowToPng(canvas, tmpPath("fc2-study.png"));

    // Space -> Reveal
    postMessage(frame, WM_KEYDOWN, 0x20, 0); // VK_SPACE
    await sleep(1000);
    captureWindowToPng(canvas, tmpPath("fc2-revealed.png"));

    // rate '3' (Good) -> advance to next card, centered + masked
    postMessage(frame, WM_KEYDOWN, 0x33, 0); // '3'
    await sleep(1200);
    captureWindowToPng(canvas, tmpPath("fc2-advanced.png"));

    // Back -> previous card arrives REVEALED, scroll position kept
    sendCommand(frame, cmdId("CmdFlashcardBack"));
    await sleep(1200);
    captureWindowToPng(canvas, tmpPath("fc2-back.png"));

    // Next -> skip without revealing or rating (arrives masked, centered)
    sendCommand(frame, cmdId("CmdFlashcardNext"));
    await sleep(1200);
    captureWindowToPng(canvas, tmpPath("fc2-next.png"));

    // Order toggle ON (random): rebuild + restart from first card; toggle ON
    // again — second NavigateToCard for the same card must show dy ~= 0,
    // proving the vertical centering landed
    sendCommand(frame, cmdId("CmdFlashcardOrderToggle"));
    await sleep(1200);
    sendCommand(frame, cmdId("CmdFlashcardOrderToggle"));
    await sleep(1200);
    captureWindowToPng(canvas, tmpPath("fc2-random.png"));

    // ---- Clean History: hold-to-confirm dialog ----
    function findCleanDialog(): number {
        let found = 0;
        winapi.enumWindows((h: number) => {
            if (winapi.getWindowText(h) === "Clean History") {
                found = h;
                return false;
            }
            return true;
        });
        return found;
    }
    sendCommand(frame, cmdId("CmdFlashcardCleanHistory"));
    await sleep(1000);
    const dlg = findCleanDialog();
    console.log("clean dialog open:", !!dlg);
    if (dlg) {
        const cr = winapi.getClientRect(dlg);
        // layout is DPI-proportional: Yes center at ~0.648/0.762 of client
        const yesX = Math.round(cr.right * 0.648);
        const yesY = Math.round(cr.bottom * 0.762);
        captureWindowToPng(dlg, tmpPath("fc2-clean1.png"));
        // early release (600ms hold): must NOT confirm, dialog stays open
        postMessage(dlg, WM_LBUTTONDOWN, 0, packCoords(yesX, yesY));
        await sleep(600);
        postMessage(dlg, WM_LBUTTONUP, 0, packCoords(yesX, yesY));
        await sleep(300);
        captureWindowToPng(dlg, tmpPath("fc2-clean2.png"));
        console.log("after 600ms hold + release, still open:", !!findCleanDialog());
        // full hold (2.3s): confirms, dialog closes, history cleared
        postMessage(dlg, WM_LBUTTONDOWN, 0, packCoords(yesX, yesY));
        await sleep(2300);
        await sleep(400);
        console.log("after 2.3s hold, gone:", !findCleanDialog());
    }

    proc.kill();
    await sleep(1000);

    // dump every [fc] / [fc-paint] line from the debug log
    const exeDir = path.dirname(EXE);
    const candidates = [
        path.join(exeDir, "sumlog.txt"),
        path.join(exeDir, "sumatra-log.txt"),
    ];
    const localAppData = process.env.LOCALAPPDATA || "";
    if (localAppData) {
        const base = path.join(localAppData, "SumatraPDF");
        if (fs.existsSync(base)) {
            for (const d of fs.readdirSync(base)) {
                candidates.push(path.join(base, d, "sumatra-log.txt"));
            }
        }
    }
    for (const c of candidates) {
        if (!fs.existsSync(c)) continue;
        const lines = fs.readFileSync(c, "utf8").split(/\r?\n/);
        const fc = lines.filter((l) => l.includes("[fc"));
        if (fc.length > 0) {
            console.log(`=== ${c} ===`);
            for (const l of fc) console.log(l);
        }
    }
}

if (import.meta.main) {
    await testit();
}
