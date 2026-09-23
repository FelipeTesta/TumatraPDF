// Diagnose: ETA overlay behavior when toggling fullscreen while a .md
// document auto-scrolls (user: "em tela cheia o campo do eta está sumindo").
// ad-hoc: run directly, not in all.ts.
import { join } from "path";
import {
    enumChildWindows,
    getClassName,
    getWindow,
    getWindowRect,
    getWindowText,
    GW_HWNDNEXT,
    GW_HWNDPREV,
    isWindowVisible,
    readWindowDCColumn,
    sleep,
} from "./winapi";
import { sendCommand, waitForFrame } from "./win-automation";
import { cmdId } from "./util";

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

function sample(frame: number, label: string): void {
    const eta = findEtaOverlay(frame);
    const r = eta ? getWindowRect(eta) : null;
    // probe the overlay's center column on the FRAME's DC for the magenta
    // colorkey: present = the overlay actually renders on top of the webview;
    // absent = buried under it even though IsWindowVisible() is true
    let magenta = -1;
    if (r && r.bottom > r.top) {
        const col = readWindowDCColumn(frame, Math.floor((r.left + r.right) / 2), r.top, r.bottom - r.top);
        magenta = col.filter((c) => c === 0xff00ff).length;
    }
    // z-order of the overlay's SIBLINGS, walked from the overlay itself
    // (GW_HWNDPREV = above, GW_HWNDNEXT = below); shows whether the canvas or
    // webview host sits ABOVE the overlay (= buried, invisible in practice)
    const zorder: string[] = [];
    if (eta) {
        let h = getWindow(eta, GW_HWNDPREV);
        const above: string[] = [];
        for (let i = 0; h !== 0 && i < 10; i++) {
            above.unshift(getClassName(h));
            h = getWindow(h, GW_HWNDPREV);
        }
        zorder.push(...above, "(ETA)", getClassName(eta));
        h = getWindow(eta, GW_HWNDNEXT);
        for (let i = 0; h !== 0 && i < 10; i++) {
            zorder.push(getClassName(h));
            h = getWindow(h, GW_HWNDNEXT);
        }
    }
    console.log(
        label,
        JSON.stringify({
            frameRect: getWindowRect(frame),
            etaVisible: eta ? isWindowVisible(eta) : false,
            etaText: eta ? getWindowText(eta) : "(none)",
            etaRect: r,
            magentaPixels: magenta,
            frameChildZOrder: zorder,
        }),
    );
}

export async function testit(): Promise<void> {
    const md = join(import.meta.dir, "..", "docs", "md", "Accessibility.md");
    const proc = Bun.spawn([EXE_MD, "-for-testing", md], { stdout: "ignore", stderr: "ignore" });
    const frame = await waitForFrame(proc.pid);
    if (!frame) {
        throw new Error("ad-hoc-md-fullscreen: no frame window");
    }
    await sleep(4000);
    for (let i = 0; i < 10; i++) {
        sendCommand(frame, cmdId("CmdAutoScrollSpeedUp"));
        await sleep(60);
    }
    sendCommand(frame, cmdId("CmdAutoScrollToggle"));
    await sleep(3000);
    sample(frame, "windowed:");

    sendCommand(frame, cmdId("CmdToggleFullscreen"));
    await sleep(2000);
    sample(frame, "fullscreen+2s:");
    await sleep(3000);
    sample(frame, "fullscreen+5s:");

    sendCommand(frame, cmdId("CmdToggleFullscreen"));
    await sleep(2000);
    sample(frame, "back-to-windowed:");

    // presentation mode (Ctrl+L / F5): the other "tela cheia"
    sendCommand(frame, cmdId("CmdTogglePresentationMode"));
    await sleep(2500);
    sample(frame, "presentation+2.5s:");
    await sleep(3000);
    sample(frame, "presentation+5.5s:");

    sendCommand(frame, cmdId("CmdTogglePresentationMode"));
    await sleep(2000);
    sample(frame, "back-from-presentation:");
    proc.kill();
}

if (import.meta.main) {
    await testit();
}
