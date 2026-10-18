import { Glob } from "bun";
import { existsSync } from "node:fs";
import { writeFile } from "node:fs/promises";
import { buildIncludes, cxxStandard, defines } from "./clang-flags";

// Generates compile_commands.json, the compilation database that clangd (and every
// clangd-backed tool: Serena, mcp-cpp, `clangd --check`, clang-tidy via a database)
// uses to parse each translation unit with the right flags.
//
// The VS build goes through MSBuild / premake, neither of which can emit a database,
// so the flags come from cmd/clang-flags.ts — the same include dirs and defines
// cmd/clang-tidy.ts uses. Every TU gets the identical flag set, which is also what
// makes clangd's "nearest entry" heuristic work for the .h files.
//
// Regenerate after adding or removing a .cpp under src/ (or after a WebView2 bump):
//   bun cmd/gen-compile-commands.ts
//
// Usage: bun cmd/gen-compile-commands.ts [output.json]

// Same set of translation units cmd/clang-tidy.ts lints: fork + upstream Windows
// code that shares the flag set. Excluded on purpose: src/mac/** (Objective-C++,
// different toolchain), src/testcode, src/tools, src/regress (test helpers).
const patterns = [
  "src/*.cpp",
  "src/base/*.cpp",
  "src/wingui/*.cpp",
  "src/uia/*.cpp",
  "src/previewer/*.cpp",
  "src/ifilter/*.cpp",
];

async function findFiles(): Promise<string[]> {
  const files = new Set<string>();
  for (const pattern of patterns) {
    const glob = new Glob(pattern);
    for await (const file of glob.scan({ dot: false })) {
      // Glob yields OS separators; the database must be portable.
      files.add(file.replaceAll("\\", "/"));
    }
  }
  return [...files].sort();
}

function makeEntry(file: string, includes: string[], directory: string) {
  const command = ["clang++", "-x", "c++", cxxStandard, "-c", file, ...includes, ...defines].join(" ");
  return { directory, file, command };
}

async function main() {
  if (!existsSync("src/base/Base.h")) {
    console.error("error: run this from the sumatrapdf-src directory (bun cmd/gen-compile-commands.ts)");
    process.exit(1);
  }

  const outPath = process.argv[2] || "compile_commands.json";
  const directory = process.cwd().replaceAll("\\", "/");
  const includes = buildIncludes();
  const files = await findFiles();
  const entries = files.map((f) => makeEntry(f, includes, directory));

  await writeFile(outPath, JSON.stringify(entries, null, 2) + "\n");
  console.log(`\nwrote ${entries.length} entries to '${outPath}' (directory: ${directory})`);
}

await main();
