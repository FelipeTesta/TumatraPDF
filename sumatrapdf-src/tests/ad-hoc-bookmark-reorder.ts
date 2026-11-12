// s50-ZT ad-hoc: bookmark reorder — add 3 bookmarks, move Page 3 under Page 1,
// save + reopen, assert the reordered order persisted.
// NOTE: outline moves (op 5) are persisted at save time; the in-session GetToc
// tree is cached and does NOT reflect a move until reload. So the reorder effect
// is verified via TestGetToc after a fresh reopen (the in-session post-move Toc
// still shows the pre-move order — a current-behavior limitation, reported below).
// Not registered in tests/all.ts; imported by tests/before-release.ts. Run:
//   bun tests/ad-hoc-bookmark-reorder.ts --no-build
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";

function makePdf(nPages: number): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  const kids: string[] = [];
  for (let i = 0; i < nPages; i++) kids.push(`${4 + i * 2} 0 R`);
  body[2] = enc(`<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${nPages} >>`);
  body[3] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  for (let i = 0; i < nPages; i++) {
    const po = 4 + i * 2;
    const co = po + 1;
    const stream = `BT /F1 24 Tf 72 720 Td (Page ${i + 1}) Tj ET`;
    body[po] = enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents ${co} 0 R >>`,
    );
    body[co] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
  }
  const maxN = 3 + nPages * 2;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n], enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

async function ctrl(
  client: ControlClient,
  cmd: ControlCommand,
  args: (number | string)[] = [],
): Promise<{ code: number; text: string }> {
  const res = await client.request(cmd, args);
  return { code: Number(res[0]), text: String(res[1] ?? "") };
}

async function waitReady(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    const r = await ctrl(client, ControlCommand.TestBookmarkToc);
    const t = r.text.trim();
    if (t === "nowindow" || t === "tab-null" || t === "engine-null") {
      await new Promise((r) => setTimeout(r, 300));
      continue;
    }
    return;
  }
  throw new Error("engine/tab never became ready");
}

function tocLines(text: string): string[] {
  return text.split("\n").filter((line) => line.includes("|page="));
}

export async function testit(): Promise<void> {
  const dir = tmpPath("szt-bookmark-reorder");
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const pdf = join(dir, "reorder.pdf");
  writeFileSync(pdf, makePdf(3));
  writeFileSync(
    join(appdata, "TumatraPDF-settings.txt"),
    "ReuseInstance = false\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
    "utf8",
  );

  await withControlledSumatra(
    EXE,
    async (client) => {
      await waitReady(client);
      const add = (page: number, title: string) => ctrl(client, ControlCommand.TestBookmarkAdd, [page, title]);
      if ((await add(1, "Page 1")).code !== 0) throw new Error("add Page 1 failed");
      if ((await add(2, "Page 2")).code !== 0) throw new Error("add Page 2 failed");
      if ((await add(3, "Page 3")).code !== 0) throw new Error("add Page 3 failed");

      // Baseline in-session order: Page 1(d0)/Page 2(d0)/Page 3(d0)
      const before = tocLines((await ctrl(client, ControlCommand.TestBookmarkToc)).text);
      if (
        before[0] !== "Page 1|page=1|depth=0" ||
        before[1] !== "Page 2|page=2|depth=0" ||
        before[2] !== "Page 3|page=3|depth=0"
      ) {
        throw new Error(`baseline order wrong: ${JSON.stringify(before)}`);
      }

      // Move Page 3 under Page 1. TestBookmarkMove: sourcePageNo, sourceTitle,
      // newParentPageNo, newParentTitle, moveBeforePageNo, moveBeforeTitle ("" => no anchor).
      const mv = await ctrl(client, ControlCommand.TestBookmarkMove, [3, "Page 3", 1, "Page 1", 0, ""]);
      if (mv.code !== 0) throw new Error(`TestBookmarkMove failed code=${mv.code} text=${mv.text.trim()}`);

      // Persist the move (op 5 edits are flushed to disk at save).
      if ((await ctrl(client, ControlCommand.TestBookmarkSave)).code !== 0)
        throw new Error("TestBookmarkSave failed");
      if ((await ctrl(client, ControlCommand.TestBookmarkDirty)).code !== 0)
        throw new Error("expected dirty=0 after save");
    },
    [pdf],
    { env: { LOCALAPPDATA: appdata }, connectTimeoutMs: 60000 },
  );

  // Fresh reopen: assert the reordered order persisted (TestGetToc includes depth).
  await withControlledSumatra(
    EXE,
    async (client) => {
      const g = await ctrl(client, ControlCommand.TestGetToc, [pdf]);
      if (g.code !== 0) throw new Error(`TestGetToc failed code=${g.code}`);
      const gl = tocLines(g.text);
      if (
        gl[0] !== "Page 1|page=1|depth=0" ||
        gl[1] !== "Page 3|page=3|depth=1" ||
        gl[2] !== "Page 2|page=2|depth=0"
      ) {
        throw new Error(`persisted reorder wrong: ${JSON.stringify(gl)}`);
      }
    },
    [],
    { env: { LOCALAPPDATA: appdata }, connectTimeoutMs: 60000 },
  );
  console.log("ad-hoc-bookmark-reorder PASS");
}

if (import.meta.main) {
  await runStandalone(testit);
}
