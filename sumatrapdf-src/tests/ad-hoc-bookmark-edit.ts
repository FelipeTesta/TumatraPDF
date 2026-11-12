// s50-ZT ad-hoc: bookmark add + dirty on an outline-less PDF (headless via -dbg-control).
// Not registered in tests/all.ts; imported by tests/before-release.ts. Run:
//   bun tests/ad-hoc-bookmark-edit.ts --no-build
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";

// Minimal n-page outline-less PDF (fabricator duplicated inline per s50-ZT rules).
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

// Ready when the current tab has an engine (TestBookmarkToc is not nowindow/tab-null/engine-null).
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
  const dir = tmpPath("szt-bookmark-edit");
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const pdf = join(dir, "edit.pdf");
  writeFileSync(pdf, makePdf(2));
  writeFileSync(
    join(appdata, "TumatraPDF-settings.txt"),
    "ReuseInstance = false\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
    "utf8",
  );

  // Launch with the PDF on the command line so it opens in a tab/window.
  await withControlledSumatra(
    EXE,
    async (client) => {
      await waitReady(client);

      // 1) outline-less PDF: no toc
      const g0 = await ctrl(client, ControlCommand.TestGetToc, [pdf]);
      if (g0.code === 0 || /Page \d/.test(g0.text)) {
        throw new Error(`expected no toc, got code=${g0.code} text=${g0.text.trim()}`);
      }

      // 2) add bookmark "Page 1" -> page 1
      const a = await ctrl(client, ControlCommand.TestBookmarkAdd, [1, "Page 1"]);
      if (a.code !== 0) throw new Error(`TestBookmarkAdd failed code=${a.code} text=${a.text.trim()}`);

      // 3) toc lists Page 1|page=1|depth=0
      const t = await ctrl(client, ControlCommand.TestBookmarkToc);
      if (!t.text.includes("Page 1|page=1|depth=0")) {
        throw new Error(`toc missing Page 1: ${t.text.trim()}`);
      }

      // 4) dirty = 1
      const d = await ctrl(client, ControlCommand.TestBookmarkDirty);
      if (d.code !== 1) throw new Error(`expected dirty=1 got code=${d.code} text=${d.text.trim()}`);

      // save in place so the (dirty) quit is clean; also verifies save path
      const s = await ctrl(client, ControlCommand.TestBookmarkSave);
      if (s.code !== 0) throw new Error(`TestBookmarkSave failed code=${s.code} text=${s.text.trim()}`);
    },
    [pdf],
    { env: { LOCALAPPDATA: appdata }, connectTimeoutMs: 60000 },
  );
  console.log("ad-hoc-bookmark-edit PASS");
}

if (import.meta.main) {
  await runStandalone(testit);
}
