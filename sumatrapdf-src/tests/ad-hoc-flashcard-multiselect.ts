// Repro: multiple selections -> ONE grouped flashcard (user spec 2026-09-24):
// Alt+drag (mouse) adds the new text selection to the existing one (disjoint
// regions render together); S creates a single flashcard grouping all regions.
// Uses page 4 (no existing cards there; page 3 hosts the user's card 0 whose
// annot swallows clicks - isMoveableAnnot branch, never text selection).
// Phase A probes text bands (plain drags; OnSelectionStart log = hit);
// phase B: plain drag region 1, Alt+drag disjoint region 2, S -> one card.
import { launchSumatra, waitForFrame, findCanvas, sendCommand } from "./win-automation";
import { captureWindowToPng, sleep, postMessage, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE, WM_KEYDOWN } from "./winapi";
import { cmdId, tmpPath, EXE } from "./util";
import * as fs from "node:fs";
import * as path from "node:path";

const WM_KEYUP = 0x101;
const VK_MENU = 0x12;
const SUMLOG = path.join(path.dirname(EXE), "sumlog.txt");

function logLines(): string[] {
    if (!fs.existsSync(SUMLOG)) return [];
    return fs.readFileSync(SUMLOG, "utf8").split(/\r?\n/);
}

function countStarts(): number {
    return logLines().filter((l) => l.includes("OnSelectionStart:")).length;
}

async function dragSelect(
    frame: number,
    canvas: number,
    x1: number,
    y1: number,
    x2: number,
    y2: number,
    withAlt: boolean,
) {
    // MK_ALT in the mouse wParam (Canvas/OnSelectionStart accept it for
    // synthetic input; posted WM_KEYDOWN does NOT update GetKeyState)
    const down = withAlt ? 0x20 : 0x0;
    const move = withAlt ? 0x21 : 0x1; // MK_ALT|MK_LBUTTON : MK_LBUTTON
    postMessage(canvas, WM_LBUTTONDOWN, down, (y1 << 16) | (x1 & 0xffff));
    await sleep(250);
    for (let i = 1; i <= 5; i++) {
        const t = i / 5;
        const x = Math.round(x1 + (x2 - x1) * t);
        const y = Math.round(y1 + (y2 - y1) * t);
        postMessage(canvas, WM_MOUSEMOVE, move, (y << 16) | (x & 0xffff));
        await sleep(80);
    }
    postMessage(canvas, WM_LBUTTONUP, 0, (y2 << 16) | (x2 & 0xffff));
    await sleep(450);
}

async function probeClick(canvas: number, x: number, y: number) {
    // a plain click on text triggers OnSelectionStart (logged) and stops
    // without selecting — critically it does NOT pan the page, unlike a
    // missed drag (which would corrupt all later coordinates)
    postMessage(canvas, WM_LBUTTONDOWN, 0, (y << 16) | (x & 0xffff));
    await sleep(120);
    postMessage(canvas, WM_LBUTTONUP, 0, (y << 16) | (x & 0xffff));
    await sleep(250);
}

export async function testit(): Promise<void> {
    const pdf = process.argv[2] || path.resolve("ext", "a-zlib", "zlib.3.pdf");
    const proc = launchSumatra(["-for-testing", "-page", "4", pdf]);
    const frame = await waitForFrame(proc.pid);
    await sleep(4000);
    const canvas = findCanvas(frame);
    const winapi = await import("./winapi");
    console.log("frame title:", winapi.getWindowTextFull(frame));

    sendCommand(frame, cmdId("CmdZoomFitPage"));
    await sleep(2500); // let the scanned page finish rendering (Slow rendering logs ~400ms)

    // Phase A: probe text bands with plain CLICKS (no pan side-effect); a band
    // "hits" when OnSelectionStart runs for it. Need >= 2 DISTINCT lines so
    // region B's Alt-click starts outside region A's selection.
    const bands = [60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170];
    const hitSet = new Set<number>();
    for (const y of bands) {
        const before = countStarts();
        await probeClick(canvas, 100, y);
        const hit = countStarts() > before;
        if (hit) {
            hitSet.add(y);
            console.log(`probe y=${y}: HIT`);
        }
        await sleep(50);
    }
    const hits = [...hitSet];
    console.log("hit bands:", JSON.stringify(hits));

    // Phase B: need two disjoint regions; use two hit bands (or two x-ranges
    // of one band)
    if (hits.length < 2) {
        console.log("SKIP: fewer than 2 text bands found; cannot test disjoint regions");
    } else {
        const yA = hits[0];
        const yB = hits[1];
        // NOTE: posted coords are DPI-scaled ~1.25x by the app (app-space =
        // posted x 1.25); the proven-text x-range is app 125..163 — stay in
        // it and use DIFFERENT LINES (yA/yB) for disjoint regions. Clicking
        // outside the text column pan-starts instead of selecting.
        await dragSelect(frame, canvas, 100, yA, 130, yA + 3, false);
        captureWindowToPng(canvas, tmpPath("msel-1-plain.png"));
        // region 2: Alt+drag on the other hit line — must ACCUMULATE
        await dragSelect(frame, canvas, 100, yB, 130, yB + 3, true);
        captureWindowToPng(canvas, tmpPath("msel-2-alt.png"));
        // S -> ONE flashcard grouping both regions
        postMessage(frame, WM_KEYDOWN, 0x53 /* 'S' */, 0);
        await sleep(1500);
        captureWindowToPng(canvas, tmpPath("msel-3-added.png"));
    }

    proc.kill();
    await sleep(1000);

    // dump the [fc] lines + selection diagnostic logs
    const lines = logLines();
    const sel = lines.filter((l) => l.includes("[fc") || l.includes("alt merge") || l.includes("OnSelectionStart:"));
    for (const l of sel) console.log(l);
}

if (import.meta.main) {
    await testit();
}
