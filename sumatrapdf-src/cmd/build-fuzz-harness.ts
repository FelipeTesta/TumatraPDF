// Build the in-process libFuzzer harness for the base text parsers
// (tests/fuzz/BaseParsersFuzzer.cpp) with clang-cl + ASan, and copy the ASan
// runtime DLL next to it. Output: out/fuzz/fuzz-parsers.exe
//
// The harness is built with the standalone LLVM clang-cl (LLVM 23), NOT the MSVC
// toolset: libFuzzer requires clang's sanitizer-coverage instrumentation, which
// MSVC's /fsanitize=coverage does not provide. The parser sources are compiled
// directly into the harness (so they get instrumented + covered) and everything
// else they reference comes from the MSVC-built release base.lib (built with the
// static CRT /MT, so the harness is /MT too to keep the RuntimeLibrary match).
//
// Usage:
//   bun cmd/build-fuzz-harness.ts           # build (requires out/rel64/obj-s/base.lib)
//   out\fuzz\fuzz-parsers.exe -help=1       # libFuzzer options
//   out\fuzz\fuzz-parsers.exe out\fuzz\corpus -max_total_time=60

import { copyFileSync, existsSync, mkdirSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { runLogged } from "./util";
import { defines, includeDirs } from "./clang-flags";

const llvmBin = join(process.env["ProgramFiles"] ?? "C:\\Program Files", "LLVM", "bin");
const clangCl = join(llvmBin, "clang-cl.exe");
const outDir = join("out", "fuzz");
const exe = join(outDir, "fuzz-parsers.exe");

function copyAsanRuntime(): void {
  // clang-cl /MD links the dynamic ASan runtime; find it in LLVM's resource dir
  const clangLibRoot = join("C:", "Program Files", "LLVM", "lib", "clang");
  if (!existsSync(clangLibRoot)) {
    console.log(`warning: ${clangLibRoot} not found; the exe may fail to start`);
    return;
  }
  for (const ver of readdirSafe(clangLibRoot)) {
    const dllDir = join(clangLibRoot, ver, "lib", "windows");
    if (!existsSync(dllDir)) continue;
    for (const name of readdirSafe(dllDir)) {
      if (name.startsWith("clang_rt.asan_") && name.endsWith(".dll")) {
        copyFileSync(join(dllDir, name), join(outDir, name));
      }
    }
  }
}

function readdirSafe(dir: string): string[] {
  try {
    return readdirSync(dir);
  } catch {
    return [];
  }
}

async function main() {
  if (!existsSync(clangCl)) {
    throw new Error(`clang-cl not found at '${clangCl}'`);
  }
  mkdirSync(outDir, { recursive: true });

  // system libs the base/ code pulls in transitively
  const sysLibs = ["user32.lib", "gdi32.lib", "shell32.lib", "ole32.lib", "oleaut32.lib", "advapi32.lib", "uuid.lib"];

  await runLogged(clangCl, [
    "/nologo",
    "/EHsc",
    "/MT",
    "/Z7",
    "/clang:-std=c++23",
    "-fsanitize=address,fuzzer",
    ...includeDirs.map((d) => `-I${d}`),
    ...defines,
    join("tests", "fuzz", "BaseParsersFuzzer.cpp"),
    join("src", "base", "SquareTreeParser.cpp"),
    join("src", "base", "JsonParser.cpp"),
    // the base objects the parsers pull out of base.lib; compiled here because
    // the release base.lib contains /GL bitcode objects lld-link cannot consume
    join("src", "base", "Str.cpp"),
    join("src", "base", "Strconv.cpp"),
    join("src", "base", "StrUtf8.cpp"),
    join("src", "base", "StrFormatParse.cpp"),
    join("src", "base", "Arena.cpp"),
    join("src", "base", "Arena_win.cpp"),
    join("src", "base", "Base.cpp"),
    join("src", "base", "Base_win.cpp"),
    ...sysLibs,
    `/Fe:${exe}`,
    `/Fo:${outDir}\\`,
  ]);
  copyAsanRuntime();
  console.log(`exe: ${exe}`);
}

await main();
