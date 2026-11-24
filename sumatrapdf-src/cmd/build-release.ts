// Clean Release build for TumatraPDF
//
// This script performs a CLEAN Release build (x64) by:
// 1. Removing out/rel64 (the Release object files live under out/rel64/obj,
//    removed by the clean step) and the Compiled folder
// 2. Running the same msbuild command the CI uses
// 3. Deploying the exe to ../Compiled/ under two names (same as CI):
//    TumatraPDF-portable-win64.exe AND TumatraPDF-install-win64.exe
//    (they are the SAME file - IsInstallerOrUninstallerExe() flips it at runtime)
// 4. Printing both destination paths with their size and SHA-256
//
// The stale-object trap: incremental builds mix object files compiled against
// different header layouts (e.g. Settings.h / GlobalPrefs.h). A clean build
// guarantees all TUs see the same headers. Run this before any Release test or
// before packaging a release asset.

import { join } from "node:path";
import { existsSync, rmSync, statSync, readFileSync, copyFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { detectVisualStudio, runLogged } from "./util";
import { clearDirPreserveSettings } from "./clean";

async function main() {
  const timeStart = performance.now();

  const outDir = join("out", "rel64");
  const finalExeDir = join("..", "Compiled");
  const srcExe = join(outDir, "TumatraPDF.exe");
  const portableExe = join(finalExeDir, "TumatraPDF-portable-win64.exe");
  const installExe = join(finalExeDir, "TumatraPDF-install-win64.exe");

  // 1. Clean Release outputs (out/rel64 + Compiled folder)
  console.log("Cleaning Release build outputs...");
  clearDirPreserveSettings(outDir, true);
  clearDirPreserveSettings(finalExeDir, true);

  // 2. Run msbuild (same command CI uses)
  console.log("Building Release x64...");
  const { msbuildPath } = detectVisualStudio();
  const sln = String.raw`vs2022\TumatraPDF.sln`;
  const t = `/t:TumatraPDF`;
  const p = `/p:Configuration=Release;Platform=x64`;
  await runLogged(msbuildPath, [sln, t, p, `/m`]);

  // 3. Verify and deploy
  if (!existsSync(srcExe)) {
    throw new Error(`Build succeeded but exe not found at ${srcExe}`);
  }

  // Deploy: copy same exe to two names in Compiled/ (same as CI)
  copyFileSync(srcExe, portableExe);
  copyFileSync(srcExe, installExe);
  console.log(`Deployed: ${portableExe}`);
  console.log(`Deployed: ${installExe}`);

  // 4. Report both files (same size, same hash)
  const stats = statSync(portableExe);
  const sizeMiB = (stats.size / (1024 * 1024)).toFixed(2);

  const fileBuffer = readFileSync(portableExe);
  const sha256 = createHash("sha256").update(fileBuffer).digest("hex");

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`\n=== Clean Release build complete (${elapsed}s) ===`);
  console.log(`Portable: ${portableExe}`);
  console.log(`Install:  ${installExe}`);
  console.log(`Size:     ${sizeMiB} MiB (${stats.size} bytes)`);
  console.log(`SHA-256:  ${sha256}`);
}

await main();