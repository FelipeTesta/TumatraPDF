// Diagnose .md autoscroll ETA: launch a .md doc, toggle autoscroll, then
// sample the ETA overlay (class TumatraPDFEtaOverlay): text, visibility and
// rect — plus frame screenshots — to see whether the ETA updates at all.
// ad-hoc: run directly with `bun tests/ad-hoc-md-eta.ts`, not in all.ts.
import { join } from "path";
import {
    captureWindowToPng,
    enumChildWindows,
    getClassName,
    getWindowRect,
    getWindowText,
    isWindowVisible,
    readWindowDCColumn,
    sleep,
} from "./winapi";
import { countDifferingPixels, sendCommand, waitForFrame } from "./win-automation";
import { cmdId, tmpPath } from "./util";

// tests/util.ts EXE still points at the old SumatraPDF.exe name (stale
// binary in out/dbg64); use the current TumatraPDF.exe directly
const EXE_MD = join(import.meta.dir, "..", "out", "dbg64", "TumatraPDF.exe");
const ETA_CLASS = "TumatraPDFEtaOverlay";

function findEtaOverlay(frame: number): number {
    let found = 0;
    enumChildWindows(frame, (hwnd) => {
        if (getClassName(hwnd) === ETA_CLASS) {
            found = hwnd;
        }
        return true;
    });
    return found;
}

export async function testit(): Promise<void> {
    const md = join(import.meta.dir, "..", "docs", "md", "Accessibility.md");
    const proc = Bun.spawn([EXE_MD, "-for-testing", md], { stdout: "ignore", stderr: "ignore" });
    const frame = await waitForFrame(proc.pid);
    if (!frame) {
        throw new Error("ad-hoc-md-eta: no frame window");
    }
    await sleep(4000); // let the webview render the document

    // enumerate all child classes once (find the WebView2 host hwnd)
    const childClasses: { hwnd: number; cls: string }[] = [];
    enumChildWindows(frame, (hwnd) => {
        childClasses.push({ hwnd, cls: getClassName(hwnd) });
        return true;
    });
    console.log("children:", JSON.stringify(childClasses));
    const wv = childClasses.find((c) => c.cls.includes("Chrome_WidgetWin"));
    if (!wv) {
        console.log("NO webview child found");
    }

    // speed up x10 so scrolling is visible in the samples, then toggle on
    for (let i = 0; i < 10; i++) {
        sendCommand(frame, cmdId("CmdAutoScrollSpeedUp"));
        await sleep(60);
    }
    await sleep(300);
    sendCommand(frame, cmdId("CmdAutoScrollToggle"));

    let refCol: number[] = [];
    for (let i = 0; i < 8; i++) {
        await sleep(5000);
        const eta = findEtaOverlay(frame);
        // sample a pixel column from the middle of the webview (or frame) to
        // detect whether the page actually scrolls between samples
        const target = wv ? wv.hwnd : frame;
        const r = getWindowRect(target);
        const col = readWindowDCColumn(target, Math.floor((r.left + r.right) / 2), r.top + 1, r.bottom - r.top - 2);
        const diff = refCol.length === col.length ? countDifferingPixels(refCol, col) : -1;
        refCol = col;
        console.log(
            JSON.stringify({
                t: (i + 1) * 5,
                pixelDiff: diff,
                etaText: eta ? getWindowText(eta) : "(none)",
                etaVisible: eta ? isWindowVisible(eta) : false,
                etaRect: eta ? getWindowRect(eta) : null,
            }),
        );
        captureWindowToPng(frame, tmpPath(`md-eta-${i}.png`));
    }
    proc.kill();
}

if (import.meta.main) {
    await testit();
}
