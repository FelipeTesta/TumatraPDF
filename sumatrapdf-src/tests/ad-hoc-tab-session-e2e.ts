// T3: tab-group session data-loss E2E via -dbg-control pipe.
// Register ONLY in tests/before-release.ts (not tests/all.ts).
//
// Covers the REAL UI route (TrackPopupMenu -> WM_COMMAND -> HandleTabGroupMenuCmd):
//  1. ActivateTabGroupInWindow does NOT clobber destination group's tabFiles
//     with currently-open tabs (async LoadDocument hazard).
//  2. NewSession creates an empty group (does NOT sync current tabs).
//  3. OpenNewWindow opens group files in a new frame (no SetActiveTabGroup clobber).
//
// The dialog-open pipe command is ASYNC: SumatraControl.cpp stashes the params,
// posts WM_COMMAND(kTabGroupDialogTestCmd) to the frame and replies "OK posted"
// at once. The main UI thread then runs the real modal dialog; the test drives
// it via WM_SETTEXT + WM_COMMAND IDOK. This avoids the pipe-msg deadlock that
// a synchronous modal GetMessage loop on the pipe thread would cause.
//
// Note: SaveSettings() is a no-op under -for-testing, so all verification is
// done via control-pipe responses (TestTabGroupList returns group list +
// activeTabGroup). Delete is skipped under -for-testing because MessageBox
// blocks the UI thread and cannot be auto-answered.
//
// Run: bun tests/ad-hoc-tab-session-e2e.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import {
  findWindowW,
  getDlgItem,
  postMessage,
  sendMessage,
  WM_SETTEXT,
  WM_COMMAND,
  IDOK,
  ptr,
  wideZ,
} from "./winapi.ts";

const GROUP_A = "GroupA";
const GROUP_B = "GroupB";
const GROUP_E = "E";

function settingsText(activeGroup: string, groups: { name: string; paths: string[] }[]): string {
  let s = `ActiveTabGroup = ${activeGroup}\r\n`;
  s += "TabGroups [\r\n";
  for (const g of groups) {
    s += "  [\r\n";
    s += `    Name = ${g.name}\r\n`;
    s += "    TabFiles [\r\n";
    for (const p of g.paths) {
      s += "      [\r\n";
      s += `        Path = ${p}\r\n`;
      s += "      ]\r\n";
    }
    s += "    ]\r\n";
    s += "  ]\r\n";
  }
  s += "]\r\n";
  return s;
}

// Parse TestTabGroupList response: "OK groups=2 GroupA=1 GroupB=2\nactive=GroupB\n"
function parseGroupList(resp: string): { groups: Record<string, number>; active: string | null } {
  const groups: Record<string, number> = {};
  let active: string | null = null;
  for (const line of resp.split("\n")) {
    const m = line.match(/^active=(.*)$/);
    if (m) {
      active = m[1] || null;
      continue;
    }
    const parts = line.split(" ");
    for (const p of parts) {
      const kv = p.match(/^(\w+)=(\d+)$/);
      if (kv) {
        groups[kv[1]] = parseInt(kv[2], 10);
      }
    }
  }
  return { groups, active };
}

// Drive the real "New session / Rename" dialog: the control-pipe command opens
// it (forceReal) on the main UI thread, then we find the dialog window by class
// name, type the name into the EDIT control (id 1001) and press OK via WM_COMMAND
// IDOK (async, avoids cross-process BM_CLICK hazards). Poll-based: the dialog hwnd
// may exist before its children, so poll for getDlgItem(hwnd, 1001) too.
async function driveDialog(name: string): Promise<void> {
  let hwnd = 0;
  for (let i = 0; i < 60; i++) {
    hwnd = findWindowW("TUMATRA_TAB_GROUP_NAME", null);
    if (hwnd !== 0) break;
    await new Promise((r) => setTimeout(r, 50));
  }
  if (hwnd === 0) throw new Error("driveDialog: dialog window never appeared");
  let edit = 0;
  for (let i = 0; i < 60; i++) {
    edit = getDlgItem(hwnd, 1001);
    if (edit !== 0) break;
    await new Promise((r) => setTimeout(r, 50));
  }
  if (edit === 0) throw new Error("driveDialog: edit control not found");
  sendMessage(edit, WM_SETTEXT, 0, BigInt(ptr(wideZ(name))));
  postMessage(hwnd, WM_COMMAND, IDOK, 0);
  for (let i = 0; i < 60; i++) {
    if (findWindowW("TUMATRA_TAB_GROUP_NAME", null) === 0) return;
    await new Promise((r) => setTimeout(r, 50));
  }
  throw new Error("driveDialog: dialog never closed");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ad-hoc-tab-session-e2e");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "run1");
  mkdirSync(appdata, { recursive: true });

  const root = join(
    "D:",
    "1 Principal",
    "4 Trabalhos",
    "1 Projetos",
    "00 EXECUTANDO",
    "APPS",
    "PDF Tools",
    "TumatraPDF",
    "sumatrapdf-src",
  );
  const pdfA = join(root, "ext", "a-zlib", "zlib.3.pdf");

  // seed: groups A (1 file) + B (2 files) + E (0 files, empty session), activeTabGroup=""
  writeFileSync(
    join(appdata, "TumatraPDF-settings.txt"),
    settingsText("", [
      { name: GROUP_A, paths: [pdfA] },
      { name: GROUP_B, paths: [pdfA, pdfA] },
      { name: GROUP_E, paths: [] },
    ]),
    "utf8",
  );

  const keepAppData = process.env.KEEP_APPDATA === "1";
  try {
    const res = await withControlledSumatra(
      EXE,
      async (client) => {
        // wait for frame + settings load
        await new Promise((r) => setTimeout(r, 2000));

        // helper: poll TestTabGroupList until condition or timeout
        const pollList = async (
          check: (g: Record<string, number>, a: string | null) => boolean | string,
          timeoutMs = 10000,
        ): Promise<void> => {
          const deadline = Date.now() + timeoutMs;
          while (Date.now() < deadline) {
            const r = await client.request(ControlCommand.TestTabGroupList, []);
            const resp = String(r[1] ?? "");
            const { groups, active } = parseGroupList(resp);
            const result = check(groups, active);
            if (result === true) return;
            if (typeof result === "string") throw new Error(result);
            await new Promise((r) => setTimeout(r, 100));
          }
          const r = await client.request(ControlCommand.TestTabGroupList, []);
          throw new Error(`timeout waiting for group list: ${String(r[1] ?? "")}`);
        };

        // Step 1: openhere:GroupB via real WM_COMMAND route
        const r1 = await client.request(ControlCommand.TestTabGroupMenuCmd, [`openhere:${GROUP_B}`]);
        const r1Str = String(r1[1] ?? "");
        console.log(`openhere B: ${r1Str}`);
        if (!r1Str.startsWith("OK")) {
          throw new Error(`TestTabGroupMenuCmd openhere failed: ${r1Str}`);
        }

        // poll until group B has 2 tabFiles (preserved, not clobbered)
        await pollList((groups, active) => {
          if (groups[GROUP_B] !== 2) return `group B has ${groups[GROUP_B]} tabFiles, expected 2`;
          if (groups[GROUP_A] !== 1) return `group A has ${groups[GROUP_A]} tabFiles, expected 1`;
          if (active !== GROUP_B) return `active is ${active}, expected ${GROUP_B}`;
          return true;
        });
        console.log(`after openhere: A=1 B=2 active=${GROUP_B} (group files preserved)`);

        // Step 2: newsession via real dialog (forceReal). The pipe handler posts
        // WM_COMMAND and replies immediately; the main UI thread opens the modal
        // dialog. Drive it (type name + OK), then confirm via group list.
        await client.request(ControlCommand.TestTabGroupDialogOpen, [""]);
        await driveDialog("T1");
        await pollList((groups, active) => {
          if (groups["T1"] < 1) return `group T1 has ${groups["T1"]} tabFiles, expected >=1 (captures open books)`;
          if (active !== "T1") return `active is ${active}, expected T1`;
          return true;
        });
        console.log("after newsession: T1>=1 active=T1 (captured open books)");

        // Step 3: opennew:GroupA via WM_COMMAND
        const r4 = await client.request(ControlCommand.TestTabGroupMenuCmd, [`opennew:${GROUP_A}`]);
        const r4Str = String(r4[1] ?? "");
        console.log(`opennew A: ${r4Str}`);
        if (!r4Str.startsWith("OK")) {
          throw new Error(`TestTabGroupMenuCmd opennew failed: ${r4Str}`);
        }
        await pollList((groups, active) => {
          if (active !== GROUP_A) return `active is ${active}, expected ${GROUP_A}`;
          if (groups[GROUP_A] !== 1) return `group A has ${groups[GROUP_A]} tabFiles, expected 1`;
          return true;
        });
        console.log(`after opennew: active=${GROUP_A}`);

        // Step 4: rename GroupB → GroupZ via real dialog (GroupB not active, active stays GroupA)
        await client.request(ControlCommand.TestTabGroupDialogOpen, ["GroupB"]);
        await driveDialog("GroupZ");
        await pollList((groups, active) => {
          if (groups["GroupB"] !== undefined) return `group GroupB still exists`;
          // rename preserves the group's tabFiles under the new name (no data loss)
          if (groups["GroupZ"] !== 2) return `group GroupZ has ${groups["GroupZ"]} tabFiles, expected 2`;
          if (active !== GROUP_A) return `active is ${active}, expected ${GROUP_A}`;
          return true;
        });
        console.log("after rename: GroupB→GroupZ, active=GroupA");

        // Step 5: activate empty group E — empty sessions are first-class
        const r5 = await client.request(ControlCommand.TestTabGroupActivate, [GROUP_E]);
        const r5Str = String(r5[1] ?? "");
        console.log(`activate E: ${r5Str}`);
        if (!r5Str.startsWith("OK")) {
          throw new Error(`TestTabGroupActivate ${GROUP_E} failed: ${r5Str}`);
        }
        await pollList((groups, active) => {
          if (active !== GROUP_E) return `active is ${active}, expected ${GROUP_E}`;
          if (groups[GROUP_E] !== 0) return `group ${GROUP_E} has ${groups[GROUP_E]} tabFiles, expected 0`;
          return true;
        });
        console.log(`after activate E: active=${GROUP_E}, ${GROUP_E}=0 files`);

        // Step 6: open GroupA's file while E is active, then reopen E → E stays 0
        // (Unsynced-buf protection: activating empty group doesn't absorb open tabs)
        const r6 = await client.request(ControlCommand.TestTabGroupMenuCmd, [`openhere:${GROUP_A}`]);
        const r6Str = String(r6[1] ?? "");
        console.log(`openhere A while E active: ${r6Str}`);
        if (!r6Str.startsWith("OK")) {
          throw new Error(`TestTabGroupMenuCmd openhere:${GROUP_A} failed: ${r6Str}`);
        }
        await pollList((groups, active) => {
          if (active !== GROUP_A) return `active is ${active}, expected ${GROUP_A}`;
          return true;
        });
        console.log(`after openhere A: active=${GROUP_A}`);

        const r7 = await client.request(ControlCommand.TestTabGroupActivate, [GROUP_E]);
        const r7Str = String(r7[1] ?? "");
        console.log(`reopen E: ${r7Str}`);
        if (!r7Str.startsWith("OK")) {
          throw new Error(`TestTabGroupActivate ${GROUP_E} failed: ${r7Str}`);
        }
        await pollList((groups, active) => {
          if (active !== GROUP_E) return `active is ${active}, expected ${GROUP_E}`;
          if (groups[GROUP_E] !== 0) return `group ${GROUP_E} has ${groups[GROUP_E]} tabFiles, expected 0 (did not absorb GroupA's doc)`;
          if (groups[GROUP_A] !== 1) return `group ${GROUP_A} has ${groups[GROUP_A]} tabFiles, expected 1`;
          return true;
        });
        console.log(`after reopen E: active=${GROUP_E}, ${GROUP_E}=0 files, ${GROUP_A}=1 file (unchanged)`);

        return "all assertions passed";
      },
      ["-appdata", appdata, "-log"],
      { allowedExitCodes: [0] },
    );
    console.log(`E2E result: ${res}`);
  } finally {
    if (!keepAppData) {
      rmSync(dir, { recursive: true, force: true });
    } else {
      console.log(`KEEP_APPDATA=1: appdata preserved at ${dir}`);
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
