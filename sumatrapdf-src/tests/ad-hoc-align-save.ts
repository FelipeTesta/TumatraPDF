// Ad-hoc E2E test for feature s50-R2 "align apply-at-save".
//
// Headless via -dbg-control pipe. TestGeomSave (=65) opens a PDF headlessly
// (CreateEngineFromFile, no GUI window/tab), queues a page-geometry edit
// (rotation + crop), and saves to outPath via EngineMupdfSaveUpdated. Then
// reopens the saved file and verifies it loads + extract-text works.
//
// /Rotate trap: PDF /Rotate only accepts multiples of 90°, so small angles
// (e.g. 5°) round to 0 and rotation is silently lost. Use 90° here to
// exercise the rotation path; crop still applies for any angle.
//
// Run:   bun tests/ad-hoc-align-save.ts   (builds first)
//        bun tests/ad-hoc-align-save.ts --no-build   (if already built)
//
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, extractPageText, runStandalone, tmpPath } from "./util.ts";
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";

// Minimal 2-page PDF with NO outline.
function makePdf(nPages: number): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  const kids: string[] = [];
  for (let i = 0; i < nPages; i++) {
    kids.push(`${4 + i * 2} 0 R`);
  }
  body[2] = enc(`<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${nPages} >>`);
  body[3] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  for (let i = 0; i < nPages; i++) {
    const po = 4 + i * 2;
    const co = po + 1;
    const stream = `BT /F1 24 Tf 72 720 Td (Page ${i + 1}) Tj ET`;
    body[po] = enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 3 0 R >> >> /Contents ${co} 0 R >>`,
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
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

async function geomSave(
  client: ControlClient,
  inPath: string,
  outPath: string,
  pageNo: number,
  angleDeg: number,
  cropX: number,
  cropY: number,
  cropDx: number,
  cropDy: number,
): Promise<void> {
  // FloatArg on C++ side expects String type (strtod-parsed), so pass floats
  // as strings. pageNo is Int32.
  const res = await client.request(ControlCommand.TestGeomSave, [
    inPath,
    outPath,
    pageNo,
    angleDeg.toString(),
    cropX.toString(),
    cropY.toString(),
    cropDx.toString(),
    cropDy.toString(),
  ]);
  const exitCode = res[0] as number;
  const raw = String(res[1] ?? "");
  if (exitCode !== 0) {
    throw new Error(`TestGeomSave failed exitCode=${exitCode} ${raw.trim()}`);
  }
}

async function getToc(client: ControlClient, pdfPath: string): Promise<{ exitCode: number; text: string }> {
  const res = await client.request(ControlCommand.TestGetToc, [pdfPath]);
  const exitCode = res[0] as number;
  return { exitCode, text: String(res[1] ?? "") };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("s50-r2-align-save");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const pdf = join(dir, "s50-r2-align.pdf");
  const outPath = join(dir, "s50-r2-align-saved.pdf");
  writeFileSync(pdf, makePdf(2));
  writeFileSync(
    join(appdata, "TumatraPDF-settings.txt"),
    "ReuseInstance = false\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

  // Headless session: queue geometry edit + save to outPath (no window/tab needed)
  await withControlledSumatra(
    EXE,
    async (client) => {
      await geomSave(client, pdf, outPath, 1, 90, 10, 10, 592, 772);
    },
    [],
    { env: { LOCALAPPDATA: appdata }, connectTimeoutMs: 60000 },
  );

  // Reopen saved file via control: verify it loads (no crash). exitCode != 0
  // is OK for outline-less PDF; key is the app survived TestGetToc on outPath.
  await withControlledSumatra(
    EXE,
    async (client) => {
      await getToc(client, outPath);
    },
    [],
    { env: { LOCALAPPDATA: appdata }, connectTimeoutMs: 60000 },
  );

  // Text extraction on the saved file (verifies it's a valid, readable PDF)
  const text = extractPageText(outPath, -1);
  if (!text.includes("Page 1") || !text.includes("Page 2")) {
    throw new Error(`s50-r2: text extraction missing pages on saved file`);
  }
  console.log("Text extraction OK on saved file");
}

if (import.meta.main) {
  await runStandalone(testit);
}
