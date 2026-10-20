// Build the in-process libFuzzer harness for the mupdf document pipeline
// (tests/fuzz/MupdfFuzzer.cpp) and copy the ASan runtime DLL next to it.
// Output: out/fuzz/fuzz-mupdf.exe
//
// Unlike the base-parsers harness (cmd/build-fuzz-harness.ts), the mupdf objects
// are NOT compiled into the harness: rel64 mupdf.lib is MSVC /GL bitcode that
// lld-link cannot consume, and rebuilding mupdf with clang is out of scope. So
// clang-cl compiles the instrumented harness object, then
// MSVC link.exe (which handles /GL via LTCG) links it against mupdf.lib plus the
// clang sanitizer runtimes picked by hand (LLVM 23 Windows ASan is dynamic-only:
// import lib + loader thunk + the DLL copied next to the exe).
//
// Usage:
//   bun cmd/build-fuzz-harness-mupdf.ts
//   out\fuzz\fuzz-mupdf.exe out\fuzz\corpus-mupdf -max_total_time=60

import { copyFileSync, existsSync, mkdirSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { detectVisualStudio, runLogged } from "./util";
import { defines, includeDirs } from "./clang-flags";

const llvmRoot = join("C:", "Program Files", "LLVM");
const clangCl = join(llvmRoot, "bin", "clang-cl.exe");
const outDir = join("out", "fuzz");
const obj = join(outDir, "MupdfFuzzer.obj");
const exe = join(outDir, "fuzz-mupdf.exe");
const mupdfLib = join("out", "rel64", "obj-s", "mupdf.lib");

function copyAsanRuntime(): void {
  const dllDir = join(llvmRoot, "lib", "clang");
  for (const ver of readdirSafe(dllDir)) {
    const winDir = join(dllDir, ver, "lib", "windows");
    if (!existsSync(winDir)) continue;
    for (const name of readdirSafe(winDir)) {
      if (name === "clang_rt.asan_dynamic-x86_64.dll") {
        copyFileSync(join(winDir, name), join(outDir, name));
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

function findClangRtLib(name: string): string {
  const root = join(llvmRoot, "lib", "clang");
  for (const ver of readdirSafe(root)) {
    const p = join(root, ver, "lib", "windows", name);
    if (existsSync(p)) return p;
  }
  throw new Error(`clang runtime lib '${name}' not found under ${root}`);
}

async function main() {
  if (!existsSync(clangCl)) {
    throw new Error(`clang-cl not found at '${clangCl}'`);
  }
  if (!existsSync(mupdfLib)) {
    throw new Error(`mupdf.lib not found at '${mupdfLib}'; build a Release config first`);
  }
  mkdirSync(outDir, { recursive: true });

  // step 1: instrumented harness object (coverage + ASan checks, no linking)
  await runLogged(clangCl, [
    "/nologo",
    "/EHsc",
    "/MD",
    "/Z7",
    "/clang:-std=c++23",
    "-fsanitize=address,fuzzer-no-link",
    ...includeDirs.map((d) => `-I${d}`),
    ...defines,
    "/c",
    join("tests", "fuzz", "MupdfFuzzer.cpp"),
    `/Fo:${obj}`,
  ]);

  // step 2: MSVC link (the only linker that consumes the /GL bitcode objects in
  // mupdf.lib); sanitizer runtimes come from LLVM's clang resource dir. link.exe
  // needs the vcvars environment (LIB for the SDK/UCRT libs), so it runs inside a
  // temp batch that calls vcvars64.bat first; args go through a response file to
  // avoid quoting issues.
  const { vsRoot } = detectVisualStudio();
  const vcvars = join(vsRoot, "VC", "Auxiliary", "Build", "vcvars64.bat");
  const rsp = join(outDir, "mupdf-link.rsp");
  const batch = join(outDir, "mupdf-link.cmd");
  const args = [
    "/nologo",
    `/OUT:${exe}`,
    "/SUBSYSTEM:CONSOLE",
    "/MACHINE:X64",
    obj,
    findClangRtLib("clang_rt.fuzzer-x86_64.lib"),
    // LLVM 23 on Windows ships ASan dynamic-only: import lib + loader thunk
    findClangRtLib("clang_rt.asan_dynamic-x86_64.lib"),
    findClangRtLib("clang_rt.asan_dynamic_runtime_thunk-x86_64.lib"),
    mupdfLib,
    "user32.lib",
    "gdi32.lib",
    "advapi32.lib",
    "shell32.lib",
    "uuid.lib",
  ];
  await writeFile(rsp, args.map((a) => (a.includes(" ") ? `"${a}"` : a)).join(" ") + "\n");
  await writeFile(batch, `@echo off\r\ncall "${vcvars}" >nul\r\nlink.exe "@${rsp}"\r\n`);
  await runLogged("cmd.exe", ["/c", batch]);
  copyAsanRuntime();
  console.log(`exe: ${exe}`);
}

await main();
