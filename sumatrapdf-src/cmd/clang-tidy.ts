import { Glob } from "bun";
import { unlink, appendFile, writeFile, readFile, mkdir } from "node:fs/promises";
import { existsSync } from "node:fs";
import { basename, join } from "node:path";
import { detectVisualStudio } from "./util";
import { buildIncludes, cxxStandard, defines } from "./clang-flags";

const includes = buildIncludes();

const clangTidyLogFile = "clangtidy.out.txt";
const summaryFile = join("out", "lint", "clang-tidy-summary.txt");

// logFile: where the raw clang-tidy output is also appended (the -parse-only baseline).
// Pass undefined for one-off -files runs so they do not pollute the full sweep log.
async function runAndLog(exePath: string, args: string[], logFile?: string): Promise<number> {
  const proc = Bun.spawn([exePath, ...args], {
    stdout: "pipe",
    stderr: "pipe",
  });

  async function pipeStream(stream: ReadableStream<Uint8Array>, out: { write(chunk: Uint8Array): void }) {
    const reader = stream.getReader();
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      out.write(value);
      if (logFile) await appendFile(logFile, value);
    }
  }

  console.log(`> ${basename(exePath)} ${args.join(" ")}`);
  await Promise.all([pipeStream(proc.stdout, process.stdout), pipeStream(proc.stderr, process.stderr)]);

  return await proc.exited;
}

// Parallel-sweep variant: buffers the whole per-file output (clang-tidy prints source
// excerpts, so it is chunky) instead of streaming, so N files can run at once without
// their diagnostics interleaving in the log.
async function runCapture(exePath: string, args: string[]): Promise<{ exitCode: number; output: string }> {
  const proc = Bun.spawn([exePath, ...args], { stdout: "pipe", stderr: "pipe" });
  const [stdout, stderr] = await Promise.all([
    Bun.readableStreamToText(proc.stdout),
    Bun.readableStreamToText(proc.stderr),
  ]);
  const exitCode = await proc.exited;
  return { exitCode, output: stdout + stderr };
}

// Checks, HeaderFilterRegex/ExcludeHeaderFilterRegex and FormatStyle all come from
// .clang-tidy -- don't pass --checks/--header-filter here or that file stops being
// the single source of truth. -fix only adds --fix on top.
function clangTidyFileArgs(path: string): string[] {
  return [`-extra-arg=${cxxStandard}`, path, "--", ...includes, ...defines];
}

function clangTidyFixArgs(path: string): string[] {
  return ["--fix", `-extra-arg=${cxxStandard}`, path, "--", ...includes, ...defines];
}

const whitelisted = [
  "resource.h",
  "version.h",
  // vendored PCH shim: a lone #include "hb.hh", needs harfbuzz-only include dirs
  "harfbuzzpch.cpp",
  "translationlangs.cpp",
  "doc.cpp",
  "ebookcontroller.cpp",
  "ebookcontrols.cpp",
  "ebookformatter.cpp",
  "engineebook.cpp",
  "htmlformatter.cpp",
  "stresstesting.cpp",
  "tester.cpp",
];

function isWhitelisted(path: string): boolean {
  const name = basename(path).toLowerCase();
  if (name.endsWith(".h")) return true;
  return whitelisted.includes(name);
}

async function globFiles(patterns: string[]): Promise<string[]> {
  const files: string[] = [];
  for (const pattern of patterns) {
    const glob = new Glob(pattern);
    for await (const file of glob.scan({ dot: false })) {
      files.push(file);
    }
  }
  return files;
}

interface CheckStat {
  count: number;
  samples: string[];
}

// clang-tidy diagnostics are "path:line:col: warning: message [check-name]"; compiler
// diagnostics carry no check name and are bucketed as "(compiler)".
const diagnosticRx = /^(?<path>\S.*?):\d+:\d+:\s+(?<sev>warning|error):\s+.*?(?:\[(?<check>[A-Za-z0-9_.-]+)\])?\s*$/;

// Mirrors ExcludeHeaderFilterRegex in .clang-tidy: vendored headers still leak through
// clang-tidy 23's header filter (e.g. ext/darkmodelib/include/DarkModeSubclass.h), so
// drop their diagnostics here too.
const vendoredRx = /[/\\](mupdf|ext|packages)[/\\]/;

interface SweepStats {
  byCheck: Map<string, CheckStat>;
  warnings: number;
  errors: number;
  vendored: number;
}

function collectCheckStats(log: string): SweepStats {
  const byCheck = new Map<string, CheckStat>();
  let warnings = 0;
  let errors = 0;
  let vendored = 0;

  for (const rawLine of log.split(/\r?\n/)) {
    const line = rawLine.trimEnd();
    const m = diagnosticRx.exec(line);
    if (!m || !m.groups) continue;
    if (vendoredRx.test(m.groups["path"])) {
      vendored++;
      continue;
    }
    if (m.groups["sev"] === "warning") warnings++;
    else errors++;

    const check = m.groups["check"] ?? "(compiler)";
    const stat = byCheck.get(check) ?? { count: 0, samples: [] };
    stat.count++;
    if (stat.samples.length < 2) stat.samples.push(line.trim());
    byCheck.set(check, stat);
  }
  return { byCheck, warnings, errors, vendored };
}

// Real bug paths, as opposed to the stylistic bulk: only these clang-analyzer packages
// model execution and point at an actual defect, so they are worth listing with locations.
const analyzerFindingsRx =
  /^(?<path>\S.*?):(?<line>\d+):\d+:\s+(?:warning|error):\s+(?<msg>.*?)\s*\[(?<check>clang-analyzer-(?:core|cplusplus|security|deadcode|unix)\.[A-Za-z0-9_.-]+)\]\s*$/;

function relPath(p: string): string {
  const idx = p.replace(/\\/g, "/").indexOf("sumatrapdf-src/");
  return idx < 0 ? p : p.slice(idx + "sumatrapdf-src/".length);
}

function buildSummary(log: string, stats: SweepStats, processed: number, failures: string[]): string {
  const { byCheck, warnings, errors, vendored } = stats;
  const ranked = [...byCheck.entries()].sort((a, b) => b[1].count - a[1].count);
  const out: string[] = [];

  out.push("=== clang-tidy sweep summary ===");
  out.push(`files analyzed    : ${processed}${failures.length > 0 ? ` (${failures.length} with non-zero exit)` : ""}`);
  out.push(`user diagnostics  : ${warnings + errors} (${warnings} warnings, ${errors} errors)`);
  out.push(`vendored ignored  : ${vendored}`);
  out.push(`distinct checks   : ${byCheck.size}`);
  out.push("");
  out.push("-- ranked checks (full) --");
  for (const [check, stat] of ranked) {
    out.push(`${String(stat.count).padStart(6)}  ${check}`);
  }

  const analyzer: string[] = [];
  for (const rawLine of log.split(/\r?\n/)) {
    const m = analyzerFindingsRx.exec(rawLine.trimEnd());
    if (!m || !m.groups) continue;
    if (vendoredRx.test(m.groups["path"])) continue;
    analyzer.push(
      `${relPath(m.groups["path"])}:${m.groups["line"]}  [${m.groups["check"]}]  ${m.groups["msg"].trim()}`,
    );
  }
  out.push("");
  out.push(`-- clang-analyzer path findings (${analyzer.length}) --`);
  for (const a of analyzer) out.push(a);

  if (failures.length > 0) {
    out.push("");
    out.push(`-- files with non-zero clang-tidy exit (${failures.length}) --`);
    for (const file of failures) out.push(`  ${file}`);
  }
  return out.join("\n");
}

// The clang-tidy that ships inside Visual Studio (VC\Tools\Llvm, 19.1.x here) crashes
// with an access violation (0xC0000005, exit 5) on this codebase's TUs, so the standalone
// LLVM install is preferred when present. Override with the CLANG_TIDY env var.
function pickClangTidy(): string {
  const override = process.env["CLANG_TIDY"];
  if (override) return override;
  const candidates: (string | undefined)[] = [
    join(process.env["ProgramFiles"] ?? "C:\\Program Files", "LLVM", "bin", "clang-tidy.exe"),
    detectVisualStudio().clangTidyPath,
  ];
  for (const candidate of candidates) {
    if (candidate && existsSync(candidate)) return candidate;
  }
  return "clang-tidy.exe";
}

async function main() {
  const args = process.argv.slice(2);
  const fix = args.includes("-fix");
  // -sweep: keep going when one file fails and print a per-check summary at the end
  // (whole-tree runs); without it the first non-zero clang-tidy exit aborts the run.
  const sweep = args.includes("-sweep");

  // -parse-only: summarize an existing clangtidy.out.txt without running clang-tidy, so
  // the baseline can be re-derived (and refreshed after a partial sweep). Writes the
  // full ranking + the clang-analyzer locations to out/lint/clang-tidy-summary.txt.
  const parseOnly = args.includes("-parse-only");
  if (parseOnly) {
    const log = await readFile(clangTidyLogFile, "utf-8");
    const stats = collectCheckStats(log);
    // clang prints one "N warnings generated." line per TU that produced diagnostics.
    const processed = (log.match(/\d+ warnings generated\./g) ?? []).length;
    const text = buildSummary(log, stats, processed, []);
    console.log(text);
    await mkdir(join("out", "lint"), { recursive: true });
    await writeFile(summaryFile, text + "\n");
    console.log(`\nwrote '${summaryFile}'`);
    return;
  }

  const exePath = pickClangTidy();
  console.log(`using '${exePath}'`);

  // -files a.cpp b.cpp: check just these files (fast triage loop, ~15-20 s per TU)
  // instead of the whole-tree patterns. Runs sequentially and leaves the full sweep log
  // alone: that log is the -parse-only baseline (~60 min to regenerate).
  const filesIdx = args.indexOf("-files");
  const fileArgs = filesIdx >= 0 ? args.slice(filesIdx + 1).filter((a) => !a.startsWith("-")) : [];

  // -jobs N: run N clang-tidy processes at once. A single TU takes ~15-20 s (Windows +
  // mupdf headers), so the 275-file sweep is ~90 min sequential and ~20 min at 6 jobs.
  // -fix stays sequential: concurrent --fix runs would race on shared headers.
  const jobsIdx = args.indexOf("-jobs");
  const jobs =
    fix || fileArgs.length > 0 || jobsIdx < 0 ? 1 : Math.max(1, Number.parseInt(args[jobsIdx + 1] ?? "", 10) || 1);

  if (fileArgs.length === 0) {
    await unlink(clangTidyLogFile).catch(() => {});
    await writeFile(clangTidyLogFile, "");
  }

  const patterns = [
    "src/*.cpp",
    "src/mui/*.cpp",
    "src/base/*.cpp",
    "src/wingui/*.cpp",
    "src/uia/*.cpp",
    "src/previewer/*.cpp",
    "src/ifilter/*.cpp",
  ];

  const files = fileArgs.length > 0 ? fileArgs : await globFiles(patterns);
  const toProcess = files.filter((f) => !isWhitelisted(f));

  const failures: string[] = [];
  let processed = 0;

  async function processFile(file: string): Promise<void> {
    const tidyArgs = fix ? clangTidyFixArgs(file) : clangTidyFileArgs(file);
    let exitCode: number;
    if (jobs > 1) {
      const res = await runCapture(exePath, tidyArgs);
      await appendFile(clangTidyLogFile, res.output);
      exitCode = res.exitCode;
    } else {
      exitCode = await runAndLog(exePath, tidyArgs, fileArgs.length === 0 ? clangTidyLogFile : undefined);
    }
    processed++;
    if (exitCode !== 0) {
      if (!sweep) {
        console.error(`clang-tidy exited with code ${exitCode}`);
        process.exit(exitCode);
      }
      console.error(`clang-tidy exited with code ${exitCode} for ${file} (continuing)`);
      failures.push(file);
    } else if (jobs > 1) {
      console.log(`[${processed}/${toProcess.length}] done ${file}`);
    }
  }

  if (jobs > 1) {
    let next = 0;
    console.log(`running up to ${jobs} clang-tidy jobs in parallel over ${toProcess.length} files`);
    await Promise.all(
      Array.from({ length: Math.min(jobs, toProcess.length) }, async () => {
        for (let idx = next++; idx < toProcess.length; idx = next++) {
          await processFile(toProcess[idx]);
        }
      }),
    );
  } else {
    for (const file of toProcess) {
      if (sweep) console.log(`[${processed + 1}/${toProcess.length}] ${file}`);
      await processFile(file);
    }
  }

  if (sweep) {
    const log = await readFile(clangTidyLogFile, "utf-8");
    const stats = collectCheckStats(log);
    const text = buildSummary(log, stats, processed, failures);
    console.log(`\n${text}`);
    await mkdir(join("out", "lint"), { recursive: true });
    await writeFile(summaryFile, text + "\n");
    console.log(`\nwrote '${summaryFile}'`);
  }

  if (fileArgs.length > 0) {
    console.log(`\nchecked ${processed} file(s); '${clangTidyLogFile}' left untouched`);
  } else {
    console.log(`\nLogged output to '${clangTidyLogFile}'`);
  }
}

await main();
