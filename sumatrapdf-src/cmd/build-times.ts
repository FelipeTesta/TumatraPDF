// Rank the slowest translation units by cl front-end (c1/c1xx) + back-end (c2) time.
// Reads the per-TU command lines MSBuild logged in out/<config>/obj/<proj>/<proj>.tlog/
// CL.command.1.tlog, re-runs each compile with the /Bt+ timing flag (6 at a time) and
// parses `time(...c1xx.dll)=Xs` / `time(...c2.dll)=Ys` from cl's stderr.
//
// The re-run needs the MSVC environment (INCLUDE for the CRT/WinSDK headers), captured
// once from vcvars64.bat.
//
// Usage:
//   bun cmd/build-times.ts                       # default tlog (Debug x64)
//   bun cmd/build-times.ts <path-to-CL.command.1.tlog>

import { mkdir, readFile, writeFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join } from "node:path";

interface TuTime {
  file: string;
  fe: number;
  be: number;
}

const defaultTlog = join("out", "dbg64", "obj", "TumatraPDF", "TumatraPDF.tlog", "CL.command.1.tlog");
const clPath =
  "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.44.35207\\bin\\Hostx64\\x64\\cl.exe";
const vcvars = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat";

interface Entry {
  file: string;
  flags: string; // full cl switch line WITH the (quoted) source path
}

async function parseTlog(tlogPath: string): Promise<Entry[]> {
  const content = await readFile(tlogPath, "utf16le");
  const entries: Entry[] = [];
  const lines = content.split("\n");
  for (let i = 0; i < lines.length - 1; i++) {
    const line = lines[i].replace(/\r$/, "");
    if (!line.startsWith("^")) continue;
    const file = line.slice(1).trim();
    const flagsLine = lines[i + 1]?.replace(/\r$/, "").trim();
    if (!file || !flagsLine) continue;
    // the flags line ends with an unquoted copy of the source path (spaces intact),
    // so strip it via a case-insensitive endsWith and re-append it quoted
    let switches = flagsLine;
    if (flagsLine.toUpperCase().endsWith(file.toUpperCase())) {
      switches = flagsLine.slice(0, flagsLine.length - file.length).trimEnd();
    }
    entries.push({ file, flags: `${switches} "${file}"` });
  }
  return entries;
}

// cl resolves the CRT/WinSDK headers through INCLUDE, which MSBuild sets from
// vcvars64.bat; capture it once (via a temp batch - Bun's arg quoting mangles a
// direct `cmd /c "vcvars && set"` invocation) and reuse for every cl spawn
async function getMsvcEnv(): Promise<Record<string, string>> {
  const bat = join(process.env.TEMP ?? ".", "bt-env.bat");
  await Bun.write(bat, `@echo off\r\ncall "${vcvars}" >nul 2>&1\r\nset INCLUDE\r\n`);
  const out = Bun.spawnSync(["cmd.exe", "/c", bat]);
  const text = `${out.stdout}`;
  const m = /INCLUDE=(.*)/ms.exec(text);
  if (!m) {
    console.log("could not capture INCLUDE from vcvars64.bat; running without it (compiles will fail)");
    return {};
  }
  return { INCLUDE: m[1].trim() };
}

let failedShown = 0;

async function timeCompile(entry: Entry, env: Record<string, string>, worker: number): Promise<TuTime> {
  const rspPath = join(process.env.TEMP ?? ".", `bt-tu-${worker}.rsp`);
  await Bun.write(rspPath, entry.flags.replace(/\s+$/, "") + " /nologo /Bt+\r\n");
  const proc = Bun.spawnSync([clPath, `@${rspPath}`], {
    cwd: process.cwd(),
    env: { ...process.env, ...env },
    stdout: "pipe",
    stderr: "pipe",
  });
  const err = `${proc.stdout}\n${proc.stderr}`;
  const feMatch = err.match(/time\(.*?c1(?:xx)?\.dll\)=([0-9.]+)s/i);
  const beMatch = err.match(/time\(.*?c2\.dll\)=([0-9.]+)s/i);
  if (!feMatch) {
    // a failed compile contributes no timing; surface the first errors
    if (failedShown < 3) {
      failedShown++;
      console.log(`  compile failed (${entry.file.split(/[\\/]/).pop()}): ${err.split("\n")[0]?.slice(0, 120)}`);
    }
    return { file: entry.file, fe: -1, be: -1 };
  }
  return { file: entry.file, fe: Number.parseFloat(feMatch[1]), be: Number.parseFloat(beMatch?.[1] ?? "0") };
}

async function main() {
  const tlogPath = process.argv[2] ?? defaultTlog;
  if (!existsSync(tlogPath)) {
    console.log(`tlog not found: ${tlogPath}`);
    process.exit(1);
  }
  const entries = await parseTlog(tlogPath);
  if (entries.length === 0) {
    console.log("no command entries parsed from tlog");
    process.exit(1);
  }
  const env = await getMsvcEnv();
  console.log(`timing ${entries.length} TUs (6 in parallel)...`);

  const results: TuTime[] = [];
  let next = 0;
  const workers = Array.from({ length: 6 }, async (_, worker) => {
    while (next < entries.length) {
      const idx = next++;
      const t = await timeCompile(entries[idx], env, worker);
      results.push(t);
      if (results.length % 50 === 0) console.log(`  [${results.length}/${entries.length}]`);
    }
  });
  await Promise.all(workers);

  results.sort((a, b) => b.fe + b.be - (a.fe + a.be));
  const ok = results.filter((t) => t.fe >= 0);
  const failed = results.length - ok.length;
  const totalFe = ok.reduce((s, t) => s + t.fe, 0);
  const totalBe = ok.reduce((s, t) => s + t.be, 0);

  const lines: string[] = [];
  lines.push(
    `TUs timed: ${ok.length}${failed ? ` (${failed} failed, skipped)` : ""}, front-end total ${totalFe.toFixed(1)}s, back-end total ${totalBe.toFixed(1)}s`,
  );
  lines.push("");
  lines.push("top 30 slowest TUs (fe+be seconds):");
  for (const t of ok.slice(0, 30)) {
    lines.push(`${(t.fe + t.be).toFixed(2).padStart(7)}  (fe ${t.fe.toFixed(2)} + be ${t.be.toFixed(2)})  ${t.file}`);
  }
  const text = lines.join("\n");
  console.log(text);
  await mkdir(join("out"), { recursive: true });
  await writeFile(join("out", "build-times.txt"), text + "\n");
  console.log("\nwritten to out/build-times.txt");
}

await main();
