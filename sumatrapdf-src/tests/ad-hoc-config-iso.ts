// Minimal isolation test: launch → flashcard ON → Config command → window
// open? listbox populated? → MIGRATION round-trip (set custom study dir via
// the path edit + Enter → files move → clear back to default → files move
// back) → close → logs. Rules out the long cloze-repro flow when diagnosing
// Config-window issues.
import { launchSumatra, waitForFrame, sendCommand, pressKey } from "./win-automation";
import { sleep } from "./winapi";
import { cmdId, EXE, TMP_DIR } from "./util";
import * as fs from "node:fs";
import * as path from "node:path";
import { ptr } from "bun:ffi";

function check(cond: boolean, msg: string): void {
    console.log(`${cond ? "PASS" : "FAIL"}: ${msg}`);
    if (!cond) {
        throw new Error(`config-iso: ${msg}`);
    }
}

// The app TRUNCATES sumlog.txt at startup (a launch's log contains only that
// run), so NO offset slicing here: reading the whole file after the run gives
// exactly this run's lines. (Slicing by the pre-launch size is wrong under
// truncate: this run's file can grow past the old size, and slicing then
// discards everything written so far.)
function readLog(sumlogPath: string): string {
    if (!fs.existsSync(sumlogPath)) {
        return "";
    }
    return fs.readFileSync(sumlogPath).toString("utf8");
}

export async function testit(): Promise<void> {
    const pdf = process.argv[2] || path.resolve("ext", "a-zlib", "zlib.3.pdf");

    // --- seed 2 fake study JSONs in the default per-exe study dir
    const exeDir = path.dirname(EXE);
    const studyDir = path.join(exeDir, "FlashcardStudy");
    const sumlogPath = path.join(exeDir, "sumlog.txt");
    fs.mkdirSync(studyDir, { recursive: true });
    for (const f of fs.readdirSync(studyDir)) {
        if (f.endsWith(".json")) {
            fs.rmSync(path.join(studyDir, f));
        }
    }
    const fakeJson = (n: number) => `{"docName":"migration-test-${n}.pdf","states":[]}`;
    fs.writeFileSync(path.join(studyDir, "aaaa-migration-a.json"), fakeJson(1));
    fs.writeFileSync(path.join(studyDir, "aaaa-migration-b.json"), fakeJson(2));
    const migDir = path.join(TMP_DIR, "fc-migrate");
    fs.rmSync(migDir, { recursive: true, force: true });
    fs.mkdirSync(migDir, { recursive: true });

    const proc = launchSumatra(["-for-testing", "-page", "3", pdf]);
    const frame = await waitForFrame(proc.pid);
    await sleep(3000);

    const winapi = await import("./winapi");
    console.log("frame title:", winapi.getWindowTextFull(frame));

    // flashcard ON first (toolbar required for the command path)
    sendCommand(frame, cmdId("CmdFlashcardToggle"));
    await sleep(1200);
    // then Config
    sendCommand(frame, cmdId("CmdFlashcardConfig"));
    await sleep(1500);

    let configDlg = 0;
    winapi.enumWindows((h: number) => {
        if (winapi.getWindowText(h) === "Configurações") {
            configDlg = h;
            return false;
        }
        return true;
    });
    check(!!configDlg, "config window open");
    if (!configDlg) {
        proc.kill();
        throw new Error("config-iso: no config window");
    }
    const cr = winapi.getClientRect(configDlg);
    console.log("client size:", JSON.stringify(cr));
    // the history listbox (LISTBOX child of the config window; probe
    // children with LB_GETCOUNT — only a listbox answers with count info)
    let listbox = 0;
    winapi.enumChildWindows(configDlg, (h: number) => {
        const cnt = Number(winapi.sendMessage(h, 0x018b /* LB_GETCOUNT */, 0, 0));
        if (cnt > 0) {
            listbox = h;
            return false;
        }
        return true;
    });
    check(!!listbox, "history listbox found");
    const lbCount = () => Number(winapi.sendMessage(listbox, 0x018b, 0, 0));
    check(lbCount() === 2, `list starts with the 2 fake docs (got ${lbCount()})`);

    // the path EDIT child
    const edit = winapi.findChildWindow(configDlg, "Edit");
    check(!!edit, "path edit found");
    const setEditText = (text: string) => {
        const buf = winapi.wideZ(text);
        winapi.sendMessage(edit, winapi.WM_SETTEXT, 0, BigInt(ptr(buf)));
    };
    const getEditText = (): string => {
        const buf = new Uint16Array(1024);
        const n = Number(winapi.sendMessage(edit, winapi.WM_GETTEXT, 1024, BigInt(ptr(buf))));
        return String.fromCharCode(...buf.subarray(0, Math.max(0, n)));
    };

    // --- migrate OUT: apply the custom dir typed into the edit + Enter
    setEditText(migDir);
    await pressKey(edit, winapi.VK_RETURN, 700);
    const jsonsIn = (d: string) =>
        fs.existsSync(d) ? fs.readdirSync(d).filter((f) => f.endsWith(".json")).sort() : [];
    check(jsonsIn(migDir).length === 2, `2 files moved INTO custom dir (got ${jsonsIn(migDir).length})`);
    check(jsonsIn(studyDir).length === 0, `0 files left in default dir (got ${jsonsIn(studyDir).length})`);
    check(lbCount() === 2, `list reloaded from custom dir (got ${lbCount()})`);
    check(getEditText().toLowerCase() === migDir.toLowerCase(), `edit shows the custom dir`);

    // --- migrate BACK: clear the edit + Enter -> default app-data dir
    setEditText("");
    await pressKey(edit, winapi.VK_RETURN, 700);
    check(jsonsIn(studyDir).length === 2, `2 files moved BACK to default dir (got ${jsonsIn(studyDir).length})`);
    check(jsonsIn(migDir).length === 0, `custom dir empty again (got ${jsonsIn(migDir).length})`);
    check(lbCount() === 2, `list reloaded from default dir (got ${lbCount()})`);
    check(getEditText().toLowerCase().includes("flashcardstudy"), `edit shows the default dir`);

    winapi.captureWindowToPng(configDlg, path.join("tests", "tmp", "config-iso.png"));
    winapi.postMessage(configDlg, winapi.WM_CLOSE, 0, 0);
    await sleep(400);
    check(!winapi.isWindowVisible(configDlg), "window closed");

    proc.kill();
    await sleep(800);

    const log = readLog(sumlogPath);
    const logHas = (s: string) => log.includes(s);
    check(logHas("FlashcardStudyMigrateFiles - moved 2, kept 0"), "log: moved 2 out");
    check(logHas("moved 2 file(s) from"), "log: Config apply with 2 moved");
    check(logHas("study dir set to '(default app-data)'"), "log: back to default");
    for (const l of log.split(/\r?\n/)) {
        if (l.includes("[fc")) {
            console.log(l);
        }
    }

    // cleanup: remove the fakes + tmp dir (default dir back to pre-test state)
    for (const f of fs.readdirSync(studyDir)) {
        if (f.startsWith("aaaa-migration-") && f.endsWith(".json")) {
            fs.rmSync(path.join(studyDir, f));
        }
    }
    fs.rmSync(migDir, { recursive: true, force: true });
}

if (import.meta.main) {
    await testit();
}
