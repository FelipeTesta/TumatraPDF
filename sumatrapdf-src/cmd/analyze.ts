// analyze.ts - MSVC native code analysis (/analyze) over the fork's own sources.
//
// Scope: the TumatraPDF project (all of src/**), NOT the vendored trees. Dependency
// projects (mupdf, ext/*) must already be built; the analysis pass rebuilds only the
// app project (BuildProjectReferences=false), so their warnings cannot drown the
// report. Use -with-deps for the full cascade (slow, noisy).
//
// Advisory by design: findings never fail the run, only a real MSBuild failure does.
// Everything is logged to out/analyze/msvc-analyze.log and aggregated into
// out/analyze/warnings-summary.txt (counts per rule id + sample locations).
//
// Full sweep (every fork TU; dependencies must be prebuilt — bun cmd/build-all.ts):
//   bun cmd/analyze.ts
// Recompile-then-analyze run (the touched files are recompiled, but the RunCodeAnalysis
// target still analyzes the whole app project, so the summary stays project-wide):
//   bun cmd/analyze.ts -files src\Toolbar.cpp src\HomePage.cpp
//
// MSBuild properties used are the documented C++ ones: RunCodeAnalysis (/analyze),
// SDLCheck (/sdl, also turns on the Core Guidelines subset), BuildProjectReferences
// (skip dependency builds). SelectedFiles is NOT used: it is a global property that
// filters every other project's items too, and a filtered run overwrote base.lib with a
// single object (BasePch.obj) — which surfaced later as 781 LNK2001/LNK1120 errors.
import { mkdir, writeFile, appendFile, readFile } from "node:fs/promises";
import { existsSync, utimesSync } from "node:fs";
import { basename, join } from "node:path";
import { detectVisualStudio } from "./util";

const outDir = join("out", "analyze");
const logFile = join(outDir, "msvc-analyze.log");
const summaryFile = join(outDir, "warnings-summary.txt");

const slnPath = String.raw`vs2022\TumatraPDF.sln`;
const projectPath = String.raw`vs2022\TumatraPDF.vcxproj`;
const config = "Release";
const platform = "x64";
const outputExe = join("out", "rel64", "TumatraPDF.exe");

async function runAndLog(exePath: string, args: string[]): Promise<number> {
  const proc = Bun.spawn([exePath, ...args], {
    stdout: "pipe",
    stderr: "pipe",
    // English diagnostics: the machine default is localized and the log is parsed.
    env: { ...process.env, VSLANG: "1033" },
  });

  async function pipeStream(stream: ReadableStream<Uint8Array>, out: { write(chunk: Uint8Array): void }) {
    const reader = stream.getReader();
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      out.write(value);
      await appendFile(logFile, value);
    }
  }

  console.log(`> ${basename(exePath)} ${args.join(" ")}`);
  await Promise.all([pipeStream(proc.stdout, process.stdout), pipeStream(proc.stderr, process.stderr)]);
  return await proc.exited;
}

interface RuleStat {
  count: number;
  samples: string[];
}

interface Diag {
  file: string;
  rule: string;
  severity: "warning" | "error";
  text: string;
}

// cl.exe diagnostics reach the log in two shapes: with a source location
// ("src\foo.cpp(123,4): warning C6387: ...") and file-level ("warning C28251: ...").
// Linker diagnostics ("Foo.obj : error LNK2001: ...", "... .exe : fatal error LNK1120:
// ...") have no source location at all, so they need their own pattern — without it a
// failed link is reported as a clean run. MSBuild also prefixes each line with the node
// id ("10>") and repeats every diagnostic once more in its trailing warning summary, so
// lines are de-prefixed and de-duplicated.
const locatedRx =
  /^(?<file>[^(]+)\((?<line>\d+)(?:,(?<col>\d+))?\)\s*:\s*(?<sev>warning|error)\s+(?<rule>[A-Za-z]+\d+)\s*:\s*(?<msg>.+)$/;
const fileLessRx = /^(?<sev>warning|error)\s+(?<rule>[A-Za-z]+\d+)\s*:\s*(?<msg>.+)$/;
const linkRx = /:\s*(?<sev>error|fatal error)\s+(?<rule>LNK\d+|LNK)\s*:\s*(?<msg>.+)$/;

function parseDiagnostics(log: string): Diag[] {
  const diags: Diag[] = [];
  const seen = new Set<string>();
  for (const rawLine of log.split(/\r?\n/)) {
    // Node prefix ("     1>") + indentation: the analysis report at the end of the log
    // indents the same lines the compile step printed flush.
    const line = rawLine.replace(/^\s*\d+>\s*/, "").trim();
    const hasDiagnostic = line.includes(": warning ") || line.includes(": error ") || line.includes(": fatal error ");
    if (!hasDiagnostic) continue;
    // Every diagnostic shows up twice: once from the compile step (with a trailing
    // " [C:\...\Foo.vcxproj]" project annotation) and again in the analysis report at the
    // end (without it). Strip the annotation so the two copies collapse into one.
    const key = line.replace(/\s*\[[A-Za-z]:\\[^\]]*\]$/, "");
    const located = locatedRx.exec(key);
    const fileLess = located ? null : fileLessRx.exec(key);
    const link = located || fileLess ? null : linkRx.exec(key);
    const m = located ?? fileLess ?? link;
    if (!m || !m.groups) continue;
    if (seen.has(key)) continue;
    seen.add(key);
    diags.push({
      file: (m.groups["file"] ?? (link ? "<linker>" : "")).trim().replace(/\\/g, "/"),
      rule: m.groups["rule"],
      severity: m.groups["sev"] === "warning" ? "warning" : "error",
      text: key,
    });
  }
  return diags;
}

function isVendored(file: string): boolean {
  return /(^|\/)(ext|mupdf|packages)\//.test(file);
}

// src/base/*.cpp compile into base.vcxproj; the rest of src/ belongs to the app project.
function projectFor(file: string): string {
  return file.replace(/\\/g, "/").startsWith("src/base/") ? String.raw`vs2022\base.vcxproj` : projectPath;
}

function rank(diags: Diag[]): [string, RuleStat][] {
  const byRule = new Map<string, RuleStat>();
  for (const d of diags) {
    const stat = byRule.get(d.rule) ?? { count: 0, samples: [] };
    stat.count++;
    if (stat.samples.length < 3) stat.samples.push(d.text);
    byRule.set(d.rule, stat);
  }
  return [...byRule.entries()].sort((a, b) => b[1].count - a[1].count);
}

async function main() {
  const args = process.argv.slice(2);
  const fileArgs: string[] = [];
  let withDeps = false;
  let parseOnly = false;

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === "-files") {
      // accept "-files a;b", "-files a b", "-files a,b"
      while (i + 1 < args.length && !args[i + 1].startsWith("-")) {
        fileArgs.push(...args[++i].split(/[;,]/).filter((f) => f.length > 0));
      }
    } else if (arg === "-with-deps") {
      withDeps = true;
    } else if (arg === "-parse-only") {
      parseOnly = true;
    } else {
      throw new Error(`unknown argument: ${arg} (usage: bun cmd/analyze.ts [-files a;b] [-with-deps] [-parse-only])`);
    }
  }

  if (!existsSync("src") || !existsSync(projectPath) || !existsSync(slnPath)) {
    throw new Error(`run me from sumatrapdf-src (src/ + ${projectPath} + ${slnPath} must exist)`);
  }
  if (fileArgs.length > 0 && withDeps) {
    throw new Error("-files and -with-deps are mutually exclusive");
  }
  if (parseOnly && !existsSync(logFile)) {
    throw new Error(`-parse-only needs an existing ${logFile}`);
  }
  if (!parseOnly && !withDeps && !existsSync(outputExe)) {
    console.warn(
      `warning: ${outputExe} is missing — dependencies may not be built yet.\n` +
        `         run a normal Release build first (bun cmd/build.ts is Debug; use: ` +
        `msbuild ${slnPath} /t:TumatraPDF /p:Configuration=Release;Platform=x64 /m)`,
    );
  }

  // -files: touch the listed files (MSBuild skips up-to-date TUs, so nothing would be
  // analyzed) and compile just the projects owning them. SelectedFiles is deliberately
  // NOT used: it is a global property, so it also filters every other project's items
  // and the RC task then dies with MSB4044.
  const jobs: { project: string; target: string }[] = [];
  if (fileArgs.length > 0) {
    const now = new Date();
    const projects = new Set<string>();
    for (const file of fileArgs) {
      if (!existsSync(file)) {
        throw new Error(`file not found: ${file}`);
      }
      utimesSync(file, now, now);
      projects.add(projectFor(file));
    }
    for (const project of projects) {
      jobs.push({ project, target: `/t:Build` });
    }
  } else if (withDeps) {
    jobs.push({ project: slnPath, target: `/t:TumatraPDF:Rebuild` });
  } else {
    // Rebuild, not Build: Build finds every TU up to date and analyzes nothing.
    jobs.push({ project: projectPath, target: `/t:Rebuild` });
  }

  await mkdir(outDir, { recursive: true });

  const { msbuildPath } = detectVisualStudio();
  const props = [
    `/p:Configuration=${config};Platform=${platform}`,
    `/p:RunCodeAnalysis=true`,
    `/p:SDLCheck=true`,
    ...(withDeps ? [] : [`/p:BuildProjectReferences=false`]),
    `/m`,
  ];

  const start = performance.now();
  let exitCode = 0;
  if (!parseOnly) {
    await writeFile(logFile, "");
    for (const job of jobs) {
      const code = await runAndLog(msbuildPath, [job.project, job.target, ...props]);
      if (code !== 0 && exitCode === 0) {
        exitCode = code;
      }
    }
  }
  const elapsed = ((performance.now() - start) / 1000).toFixed(1);

  const log = await readFile(logFile, "utf-8");
  const diags = parseDiagnostics(log);
  const forkDiags = diags.filter((d) => !isVendored(d.file));
  const vendoredDiags = diags.filter((d) => isVendored(d.file));

  const count = (list: Diag[], severity: "warning" | "error") => list.filter((d) => d.severity === severity).length;
  const forkRanked = rank(forkDiags);
  const vendoredRanked = rank(vendoredDiags);

  const lines: string[] = [];
  lines.push(`MSVC /analyze summary (${new Date().toISOString()})`);
  lines.push(
    `build: ${parseOnly ? "(parse-only, existing log)" : jobs.map((j) => `${basename(j.project)} ${j.target.replace("/t:", "")}`).join(", ")}`,
  );
  lines.push(`props: ${props.filter((p) => p !== "/m").join(" ")}`);
  lines.push(`files: ${fileArgs.length > 0 ? fileArgs.join(" ") : "(all TumatraPDF TUs)"}`);
  lines.push(`deps analyzed: ${withDeps ? "yes (-with-deps)" : "no (BuildProjectReferences=false)"}`);
  lines.push(`msbuild exit code: ${exitCode}, elapsed: ${elapsed}s`);
  lines.push(
    `fork sources:  ${count(forkDiags, "warning")} warnings, ${count(forkDiags, "error")} errors, ` +
      `${forkRanked.length} distinct rules`,
  );
  lines.push(
    `vendored:      ${count(vendoredDiags, "warning")} warnings, ${count(vendoredDiags, "error")} errors, ` +
      `${vendoredRanked.length} distinct rules (informational)`,
  );
  lines.push("");
  lines.push("=== fork sources, by rule ===");
  for (const [rule, stat] of forkRanked) {
    lines.push(`${String(stat.count).padStart(6)}  ${rule}`);
    for (const sample of stat.samples) {
      lines.push(`        ${sample}`);
    }
  }
  if (vendoredRanked.length > 0) {
    lines.push("");
    lines.push("=== vendored (counts only, not fixable here) ===");
    for (const [rule, stat] of vendoredRanked) {
      lines.push(`${String(stat.count).padStart(6)}  ${rule}`);
    }
  }
  await writeFile(summaryFile, lines.join("\n") + "\n", "utf-8");

  console.log(
    `\n=== MSVC /analyze: fork ${count(forkDiags, "warning")} warnings / ` +
      `${count(forkDiags, "error")} errors, vendored ${count(vendoredDiags, "warning")} warnings ` +
      `(${elapsed}s) ===`,
  );
  for (const [rule, stat] of forkRanked.slice(0, 20)) {
    console.log(`${String(stat.count).padStart(6)}  ${rule}`);
  }
  console.log(`\nFull log: ${logFile}\nSummary: ${summaryFile}`);

  if (exitCode !== 0) {
    console.error(`msbuild failed with exit code ${exitCode}`);
    if (/error LNK2001|error LNK1120/.test(log)) {
      console.error(
        "hint: unresolved externals usually mean a dependency library is stale or was\n" +
          "      rebuilt from a filtered item list (seen: base.lib holding only BasePch.obj).\n" +
          `      rebuild it, e.g.: msbuild ${String.raw`vs2022\base.vcxproj`} /t:Rebuild ` +
          `/p:Configuration=${config};Platform=${platform} /m`,
      );
    }
    process.exit(exitCode);
  }
}

await main();
