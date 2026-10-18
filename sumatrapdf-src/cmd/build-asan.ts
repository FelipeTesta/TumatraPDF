import { copyFileSync, existsSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { detectVisualStudio, runLogged } from "./util";
import { clearDirPreserveSettings } from "./clean";

const asanDllName = "clang_rt.asan_dynamic-x86_64.dll";

function findAsanDll(vsRoot: string): string | undefined {
  const candidates = [join(vsRoot, String.raw`VC\Tools\MSVC`), join(vsRoot, String.raw`VC\Tools\Llvm\x64\lib\clang`)];
  for (const base of candidates) {
    if (!existsSync(base)) continue;
    const walk = (dir: string): string | undefined => {
      for (const name of readdirSync(dir, { withFileTypes: true })) {
        const p = join(dir, name.name);
        if (name.isDirectory()) {
          const hit = walk(p);
          if (hit) return hit;
        } else if (name.name === asanDllName) {
          return p;
        }
      }
      return;
    };
    const hit = walk(base);
    if (hit) return hit;
  }
  return;
}

// The project files already copy the ASan runtime next to the exe (a PostBuildEvent on
// every x64_asan configuration), so this is only a safety net for an incremental build
// that skipped the link step.
function copyAsanRuntime(vsRoot: string, outDir: string): void {
  const src = findAsanDll(vsRoot);
  if (!src) {
    console.log(`warning: ${asanDllName} not found under ${vsRoot}`);
    return;
  }
  copyFileSync(src, join(outDir, asanDllName));
  console.log(`copied ${asanDllName}`);
}

// Build TumatraPDF.exe with MSVC AddressSanitizer (x64_asan platform).
// Output: out/dbg64_asan/TumatraPDF.exe or out/rel64_asan/TumatraPDF.exe
//
// Does NOT run premake: bin/premake5.exe regenerates vs2022/*.vcxproj from the upstream
// premake5.lua, which doesn't know the fork-added sources, silently stripping them (the
// link then fails much later with LNK1120 unresolved externals). Committed projects are
// used as-is.
//
// Usage:
//   bun cmd/build-asan.ts         # Debug ASan build
//   bun cmd/build-asan.ts -rel    # Release ASan build
//   bun cmd/build-asan.ts -clean  # wipe out/<dir> first

const argv = process.argv;
const isRelease = argv.includes("-release") || argv.includes("-rel");
const clean = argv.includes("-clean") || argv.includes("--clean");
const config = isRelease ? "Release" : "Debug";
const outDir = isRelease ? join("out", "rel64_asan") : join("out", "dbg64_asan");

async function main() {
  const timeStart = performance.now();
  console.log(`${config} ASan build (TumatraPDF.exe, x64_asan)`);

  if (clean) {
    clearDirPreserveSettings(outDir);
  }

  const { msbuildPath, vsRoot } = detectVisualStudio();
  await runLogged(msbuildPath, [
    String.raw`vs2022\TumatraPDF.sln`,
    `/t:TumatraPDF`,
    `/p:Configuration=${config};Platform=x64_asan`,
    `/m`,
  ]);
  copyAsanRuntime(vsRoot, outDir);

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`build took ${elapsed}s`);
  console.log(`exe: ${join(outDir, "TumatraPDF.exe")}`);
}

await main();
