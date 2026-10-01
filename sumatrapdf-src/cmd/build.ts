import { join } from "node:path";
import { execFileSync } from "node:child_process";
import { existsSync } from "node:fs";
import { detectVisualStudio, runLogged } from "./util";
import { clearDirPreserveSettings } from "./clean";

// Format every modified (tracked-changes or new) C/C++ file under src/ with
// clang-format before building — keeps the build on the house style without
// a manual step. Vendored trees (src/mupdf, src/ext) are never touched.
// Design_Guidelines.md improvement #7.
function formatModifiedCpp() {
  let files: string[] = [];
  try {
    const tracked = execFileSync("git", ["diff", "--name-only", "HEAD", "--", "src"], { encoding: "utf8" });
    const untracked = execFileSync("git", ["ls-files", "--others", "--exclude-standard", "--", "src"], {
      encoding: "utf8",
    });
    files = `${tracked}\n${untracked}`.split(/\r?\n/);
  } catch {
    return; // git unavailable (or not a repo): skip silently
  }
  const isCpp = (f: string) => /\.(cpp|c|h)$/.test(f);
  const isVendored = (f: string) => f.startsWith("src/mupdf/") || f.startsWith("src/ext/");
  files = files.filter((f) => isCpp(f) && !isVendored(f) && existsSync(f));
  files = [...new Set(files)];
  if (files.length === 0) {
    return;
  }
  console.log(`clang-format: ${files.length} modified file(s)`);
  try {
    execFileSync("clang-format", ["-i", ...files], { stdio: "inherit" });
  } catch {
    console.warn("clang-format not on PATH — skipping format step");
  }
}

let clean = false;
let config = "Debug";
let platform = "x64";
let outDir = join("out", "dbg64");
let finalExeDir = join("..", "Compiled");

let t = `/t:TumatraPDF`;

for (const arg of process.argv.slice(2)) {
  if (arg === "-clean") {
    clean = true;
  } else if (arg === "-rel-32") {
    config = "Release";
    platform = "Win32";
    outDir = join("out", "rel32");
  } else {
    throw new Error(`unknown argument: ${arg}`);
  }
}

async function main() {
  const timeStart = performance.now();

  formatModifiedCpp();

  console.log(`${config} ${platform} build`);
  if (clean) {
    const dirs = [outDir, finalExeDir];
    for (const dir of dirs) {
      clearDirPreserveSettings(dir);
    }
  }

  const { msbuildPath } = detectVisualStudio();
  const sln = String.raw`vs2022\TumatraPDF.sln`;
  // const t = `/t:SumatraPDF;test_util`;
  const p = `/p:Configuration=${config};Platform=${platform}`;
  await runLogged(msbuildPath, [sln, t, p, `/m`]);

  // Copy freshly compiled exe to final folder
  const srcExe = join(outDir, "TumatraPDF.exe");
  const dstExe = join(finalExeDir, "TumatraPDF.exe");
  try {
    require("fs").copyFileSync(srcExe, dstExe);
    console.log(`Copied ${srcExe} to ${dstExe}`);
  } catch (e) {
    console.warn(`Failed to copy exe: ${e}`);
  }

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`build took ${elapsed}s`);
}

await main();
