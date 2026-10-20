// Build the in-process libFuzzer harnesses for the mupdf pipelines and copy the
// ASan runtime DLL next to them:
//   tests/fuzz/MupdfFuzzer.cpp -> out/fuzz/fuzz-mupdf.exe   (PDF open + render)
//   tests/fuzz/ImageFuzzer.cpp -> out/fuzz/fuzz-images.exe  (image decode + render)
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
const mupdfLib = join("out", "rel64", "obj-s", "mupdf.lib");

// (harness cpp, obj, exe, link response file, link batch)
const targets = [
  ["MupdfFuzzer.cpp", "MupdfFuzzer.obj", "fuzz-mupdf.exe", "mupdf-link.rsp", "mupdf-link.cmd"],
  ["ImageFuzzer.cpp", "ImageFuzzer.obj", "fuzz-images.exe", "images-link.rsp", "images-link.cmd"],
  ["HtmlMdFuzzer.cpp", "HtmlMdFuzzer.obj", "fuzz-htmlmd.exe", "htmlmd-link.rsp", "htmlmd-link.cmd"],
];

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

  for (const [cppName, objName, exeName, rspName, batchName] of targets) {
    const obj = join(outDir, objName);
    const exe = join(outDir, exeName);

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
      join("tests", "fuzz", cppName),
      `/Fo:${obj}`,
    ]);

    // step 2: MSVC link (the only linker that consumes the /GL bitcode objects in
    // mupdf.lib); sanitizer runtimes come from LLVM's clang resource dir. link.exe
    // needs the vcvars environment (LIB for the SDK/UCRT libs), so it runs inside a
    // temp batch that calls vcvars64.bat first; args go through a response file to
    // avoid quoting issues.
    const { vsRoot } = detectVisualStudio();
    const vcvars = join(vsRoot, "VC", "Auxiliary", "Build", "vcvars64.bat");
    const rsp = join(outDir, rspName);
    const batch = join(outDir, batchName);
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
      // mupdf's own dependency archives (freetype, harfbuzz, ...); unreferenced
      // members are skipped by the linker, so listing them all is harmless
      join("out", "rel64", "obj-s", "freetype.lib"),
      join("out", "rel64", "obj-s", "harfbuzz.lib"),
      join("out", "rel64", "obj-s", "a-jbig2dec.lib"),
      join("out", "rel64", "obj-s", "lcms2.lib"),
      join("out", "rel64", "obj-s", "libjpeg-turbo.lib"),
      join("out", "rel64", "obj-s", "libwebp.lib"),
      join("out", "rel64", "obj-s", "a-openjpeg.lib"),
      join("out", "rel64", "obj-s", "a-zlib.lib"),
      join("out", "rel64", "obj-s", "a-mujs.lib"),
      join("out", "rel64", "obj-s", "brotli.lib"),
      join("out", "rel64", "obj-s", "a-extract.lib"),
      join("out", "rel64", "obj-s", "a-gumbo.lib"),
      join("out", "rel64", "obj-s", "libarchive.lib"),
      "user32.lib",
      "gdi32.lib",
      "advapi32.lib",
      "shell32.lib",
      "uuid.lib",
      "ole32.lib",
      "windowscodecs.lib",
    ];
    writeFileSync(rsp, args.map((a) => (a.includes(" ") ? `"${a}"` : a)).join(" ") + "\n");
    writeFileSync(batch, `@echo off\r\ncall "${vcvars}" >nul\r\nlink.exe "@${rsp}"\r\n`);
    await runLogged("cmd.exe", ["/c", batch]);
    console.log(`exe: ${exe}`);
  }
  copyAsanRuntime();
}

await main();
