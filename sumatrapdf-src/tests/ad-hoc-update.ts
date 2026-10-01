// ad-hoc E2E: in-app update flow — user-initiated check, download, self-replace, relaunch.
// Tests the RELEASE exe in Compiled\ (version must be < update.txt Latest on main).
// Debug builds hardcode myVer=50000 and never update, which is why EXE from util is unused.
import { join } from "node:path";
import { cmdId, runStandalone } from "./util";
import { sendCommand, pressKey } from "./win-automation";

const ROOT = process.cwd(); // sumatrapdf-src
const EXE_REL = join(ROOT, "..", "Compiled", "TumatraPDF.exe");

async function sleep(ms: number) {
  await new Promise((r) => setTimeout(r, ms));
}

async function findWindowMatching(re: RegExp, timeoutMs: number): Promise<number> {
  const { enumWindows, getWindowText } = await import("./winapi");
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    let hit = 0;
    enumWindows((hwnd) => {
      const t = getWindowText(hwnd);
      if (re.test(t)) {
        hit = hwnd;
        return false; // stop enumerating
      }
      return true;
    });
    if (hit) return hit;
    await sleep(600);
  }
  return 0;
}

async function waitFrameForPid(pid: number, timeoutMs = 15000): Promise<number> {
  const { enumWindows, getClassName, getWindowThreadProcessId } = await import("./winapi");
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    let hit = 0;
    enumWindows((hwnd) => {
      if (getWindowThreadProcessId(hwnd) === pid && getClassName(hwnd) === "SUMATRA_PDF_FRAME") {
        hit = hwnd;
        return false;
      }
      return true;
    });
    if (hit) return hit;
    await sleep(400);
  }
  return 0;
}

export async function testit(): Promise<void> {
  console.log("[upd] exe under test:", EXE_REL);
  let before = Bun.file(EXE_REL).lastModified;
  console.log("[upd] exe mtime before:", new Date(before).toISOString());

  const proc = Bun.spawn([EXE_REL, "-for-testing", "-console"], { stdout: "pipe", stderr: "pipe" });
  const frame = await waitFrameForPid(proc.pid);
  if (!frame) throw new Error("frame not found");
  console.log("[upd] frame found, pid", proc.pid);

  const cid = cmdId("CmdCheckUpdate");
  console.log("[upd] sending CmdCheckUpdate", cid);
  sendCommand(frame, cid);

  // check (async) -> 11MB download (async) -> TaskDialog "SumatraPDF Update" ("Install and relaunch" default)
  // debug: every 5s dump any window w/ a taskdialog-ish title or class
  const { enumWindows, getWindowText, getClassName } = await import("./winapi");
  let dlg = 0;
  for (let i = 0; i < 24 && !dlg; i++) {
    await sleep(5000);
    const seen: string[] = [];
    enumWindows((hwnd) => {
      const t = getWindowText(hwnd);
      const c = getClassName(hwnd);
      if (t && c === "#32770") {
        seen.push(`[${c}] ${t}`);
        if (/update|atual/i.test(t)) dlg = hwnd;
      }
      return !dlg;
    });
    console.log(`[upd] t+${(i + 1) * 5}s dialogs:`, seen.length ? seen.join(" | ") : "(none)");
  }
  if (!dlg) throw new Error("update dialog never appeared (120s timeout)");
  console.log("[upd] dialog hwnd:", dlg);
  const shot1 = process.env.TEMP + "\\upd-dialog-before.png";
  const { captureWindowToPng } = await import("./winapi");
  captureWindowToPng(dlg, shot1);
  console.log("[upd] shot before:", shot1);

  await sleep(700);
  pressKey(dlg, 0x0d); // VK_RETURN -> default button
  await sleep(3000);
  const shot2 = process.env.TEMP + "\\upd-dialog-after.png";
  captureWindowToPng(dlg, shot2);
  console.log("[upd] shot after:", shot2);

  // app exits; helper batch waits pid exit, moves downloaded exe over self, relaunches
  let replaced = false;
  for (let i = 0; i < 60; i++) {
    await sleep(1000);
    const m = Bun.file(EXE_REL).lastModified;
    if (m !== before) {
      replaced = true;
      console.log("[upd] exe replaced at", new Date(m).toISOString());
      break;
    }
  }
  if (!replaced) {
    const out = await new Response(proc.stdout).text();
    console.log("=== console tail ===");
    for (const l of out.split("\n").slice(-25)) console.log(l.trim());
    try {
      proc.kill();
    } catch {}
    throw new Error("exe was not replaced within 60s");
  }

  // relaunched frame (new process)
  const frames2 = await findWindowMatching(/TumatraPDF|No document|Documents/, 30_000);
  if (!frames2) throw new Error("app did not relaunch after self-update");
  console.log("[upd] relaunched frame hwnd:", frames2);

  try {
    sendCommand(frames2, cmdId("CmdExit"));
  } catch {}
  console.log("[upd] E2E PASS");
}

if (import.meta.main) {
  await runStandalone(testit);
}
