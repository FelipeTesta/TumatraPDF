// s50-ZT ad-hoc: repair/replace — verify a broken PDF reports engine errors and that
// the CmdRepairDocumentFile command exists.
// NOTE: repair itself is a GUI context-menu command (CmdRepairDocumentFile in
// Commands_Edit.cpp / CommandAvailability.cpp) with NO pipe-drivable TestRepair
// control command, so the actual repair+replace cannot be driven here and is NOT
// asserted — honest gap, reported below. (LOG.md s42: repair re-serializes to a
// .repair-tmp via EngineMupdfSaveUpdated(garbage=2, clean=1, non-incremental),
// MoveFileExW swap over original, then ReloadDocument.)
// Not registered in tests/all.ts; imported by tests/before-release.ts. Run:
//   bun tests/ad-hoc-repair-replace.ts --no-build
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath, ROOT } from "./util.ts";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

// Minimal valid n-page outline-less PDF.
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

// Broken PDF: valid bytes with the trailer/startxref/%%EOF tail removed. mupdf's
// load-time repair kicks in ("cannot find startxref ... trying to repair broken xref")
// and records >=1 engine errors while still producing a loadable engine.
function makeBrokenPdf(nPages: number): Buffer {
  const valid = makePdf(nPages);
  const text = valid.toString("latin1");
  const i = text.indexOf("trailer");
  if (i < 0) throw new Error("makeBrokenPdf: no trailer found");
  return Buffer.from(text.slice(0, i), "latin1");
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

export async function testit(): Promise<void> {
  const dir = tmpPath("szt-repair-replace");
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const broken = join(dir, "broken.pdf");
  writeFileSync(broken, makeBrokenPdf(2));
  writeFileSync(
    join(appdata, "TumatraPDF-settings.txt"),
    "ReuseInstance = false\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
    "utf8",
  );

  let errorCount = -1;
  let errorText = "";
  await withControlledSumatra(
    EXE,
    async (client) => {
      await waitReady(client);
      const e = await ctrl(client, ControlCommand.TestGetEngineErrors);
      errorCount = e.code;
      errorText = e.text;
    },
    [broken],
    { env: { LOCALAPPDATA: appdata }, connectTimeoutMs: 60000 },
  );

  if (errorCount <= 0) {
    throw new Error(
      `broken PDF did not report engine errors: count=${errorCount} text=${errorText.trim()}`,
    );
  }

  // Assert the repair command id exists (repair itself is GUI-only, no pipe command).
  const commandsH = readFileSync(join(ROOT, "src", "Commands.h"), "utf8");
  if (!commandsH.includes("CmdRepairDocumentFile")) {
    throw new Error("CmdRepairDocumentFile not found in src/Commands.h");
  }

  console.log(
    `ad-hoc-repair-replace PASS (verifiable part): broken-PDF engine errors=${errorCount} text="${errorText
      .trim()
      .replace(/\s+/g, " ")}"; CmdRepairDocumentFile exists.`,
  );
  console.log("GAP: no pipe-drivable TestRepair command exists; the actual repair+replace (re-serialize to")
  console.log("       .repair-tmp, MoveFileExW swap over original, ReloadDocument) is GUI context-menu only");
  console.log("       and cannot be driven via -dbg-control here. See LOG.md s42; a GUI-driven repair test is a gap.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
