// Clean Release build for TumatraPDF
//
// This script performs a CLEAN Release build (x64) by:
// 1. Removing out/rel64 and the Release obj directory (stale incremental objects
//    after a header change broke the GlobalPrefs layout and crashed the app at startup)
// 2. Running the same msbuild command the CI uses
// 3. Printing the resulting exe path, size, and SHA-256
//
// The stale-object trap: incremental builds mix object files compiled against
// different header layouts (e.g. Settings.h / GlobalPrefs.h). A clean build
// guarantees all TUs see the same headers. Run this before any Release test or
// before packaging a release asset.

import { join } from "node:path";
import { existsSync, rmSync, statSync } from "node:fs";
import { createHash } from "node:crypto";
import { detectVisualStudio, runLogged } from "./util";
import { clearDirPreserveSettings } from "./clean";

async function main() {
  const timeStart = performance.now();

  const outDir = join("out", "rel64");
  const finalExeDir = join("..", "Compiled");
  const srcExe = join(outDir, "TumatraPDF.exe");

  // 1. Clean Release outputs (out/rel64 + Release obj dir)
  console.log("Cleaning Release build outputs...");
  clearDirPreserveSettings(outDir, true);
  clearDirPreserveSettings(finalExeDir, true);

  // Also clean the VS obj directory for Release/x64 (stale .obj files live there)
  const objDir = join("vs2022", "TumatraPDF", "Release");
  if (existsSync(objDir)) {
    rmSync(objDir, { recursive: true, force: true });
    console.log(`Deleted obj dir: ${objDir}`);
  }

  // 2. Run msbuild (same command CI uses)
  console.log("Building Release x64...");
  const { msbuildPath } = detectVisualStudio();
  const sln = String.raw`vs2022\TumatraPDF.sln`;
  const t = `/t:TumatraPDF`;
  const p = `/p:Configuration=Release;Platform=x64`;
  await runLogged(msbuildPath, [sln, t, p, `/m`]);

  // 3. Verify and report
  if (!existsSync(srcExe)) {
    throw new Error(`Build succeeded but exe not found at ${srcExe}`);
  }

  const stats = statSync(srcExe);
  const sizeMiB = (stats.size / (1024 * 1024)).toFixed(2);

  // SHA-256
  const fileBuffer = require("node:fs").readFileSync(srcExe);
  const sha256 = createHash("sha256").update(fileBuffer).digest("hex");

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`\n=== Clean Release build complete (${elapsed}s) ===`);
  console.log(`Exe:     ${srcExe}`);
  console.log(`Size:    ${sizeMiB} MiB (${stats.size} bytes)`);
  console.log(`SHA-256: ${sha256}`);
}

await main();