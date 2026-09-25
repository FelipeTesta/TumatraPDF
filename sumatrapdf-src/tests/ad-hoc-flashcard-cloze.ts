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

    // Order options dialog: opens as a MODAL window now (sequential/random +
    // new-cards position, instant apply); open + close via WM_CLOSE. The
    // full option behavior is a mouse feature -> user tests live.
    sendCommand(frame, cmdId("CmdFlashcardOrderOptions"));
    await sleep(800);
    const findOrderDialog = (): number => {
        let found = 0;
        winapi.enumWindows((h: number) => {
            if (winapi.getWindowText(h) === "Study Order") {
                found = h;
                return false;
            }
            return true;
        });
        return found;
    };
    const orderDlg = findOrderDialog();
    console.log("order dialog open:", !!orderDlg);
    if (orderDlg) {
        captureWindowToPng(orderDlg, tmpPath("fc2-order.png"));
        postMessage(orderDlg, 0x0010, 0, 0); // WM_CLOSE
        await sleep(400);
        console.log("order dialog closed:", !findOrderDialog());
    }
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
        // layout is DPI-proportional: two buttons, centers at ~0.30/0.70 of
        // client width, ~0.78 of client height
        const curX = Math.round(cr.right * 0.3);
        const allX = Math.round(cr.right * 0.7);
        const btnY = Math.round(cr.bottom * 0.78);
        captureWindowToPng(dlg, tmpPath("fc2-clean1.png"));
        // early release on "Clear current book" (600ms): must NOT confirm
        postMessage(dlg, WM_LBUTTONDOWN, 0, packCoords(curX, btnY));
        await sleep(600);
        postMessage(dlg, WM_LBUTTONUP, 0, packCoords(curX, btnY));
        await sleep(300);
        captureWindowToPng(dlg, tmpPath("fc2-clean2.png"));
        console.log("after 600ms hold + release, still open:", !!findCleanDialog());
        // full hold "Clear current book" (2.3s): confirms, dialog closes,
        // current book's history cleared only
        postMessage(dlg, WM_LBUTTONDOWN, 0, packCoords(curX, btnY));
        await sleep(2300);
        await sleep(400);
        console.log("after 2.3s hold current-book, gone:", !findCleanDialog());
        // reopen: full hold "Clear ALL books" (5.4s): confirms, every study
        // JSON in the app-data dir is deleted
        sendCommand(frame, cmdId("CmdFlashcardCleanHistory"));
        await sleep(800);
        const dlg2 = findCleanDialog();
        if (dlg2) {
            const cr2 = winapi.getClientRect(dlg2);
            const allX2 = Math.round(cr2.right * 0.7);
            const btnY2 = Math.round(cr2.bottom * 0.78);
            captureWindowToPng(dlg2, tmpPath("fc2-clean3.png"));
            postMessage(dlg2, WM_LBUTTONDOWN, 0, packCoords(allX2, btnY2));
            await sleep(5400);
            await sleep(400);
            console.log("after 5.4s hold all-books, gone:", !findCleanDialog());
        }
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
