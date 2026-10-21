// Clang flags shared by cmd/clang-tidy.ts and cmd/gen-compile-commands.ts.
import { readdirSync, existsSync } from "node:fs";
import { join } from "node:path";

// Keep in sync with AdditionalIncludeDirectories in vs2022/SumatraPDF-static.vcxproj.
// A missing dir isn't fatal: clang reports "file not found" and keeps parsing with
// that header's declarations unknown, which silently degrades every check in the
// files that include it (this used to affect 25 files, SumatraPDF.cpp among them).
export const includeDirs = [
  // keep in sync with the AdditionalIncludeDirectories of the app + mupdf
  // vcxproj (post F1: src/mupdf carries the fork's mupdf patches, cmark-gfm
  // is a real dep of mupdf's md.c)
  "mupdf/include",
  "mupdf/scripts/freetype",
  "src",
  "src/base",
  "src/mupdf",
  "src/wingui",
  "ext/chmdec",
  "ext/djvudec",
  "ext/a-zlib",
  "ext/synctex",
  "ext/lzma/C",
  "ext/libwebp/src",
  "ext/freetype/include",
  "ext/libarchive",
  "ext/a-zopfli",
  "ext/cmark-gfm/src",
  "ext/cmark-gfm/extensions",
  "mupdf/scripts/cmark-gfm",
  "ext/heicdec",
  "ext/jxldec",
  "ext/darkmodelib/include",
];

// resolved at runtime so bumping the NuGet package doesn't silently re-break WebView.cpp
export function webView2IncludeDir(): string | null {
  const packages = "packages";
  if (!existsSync(packages)) {
    return null;
  }
  const dir = readdirSync(packages).find((d) => d.startsWith("Microsoft.Web.WebView2."));
  if (!dir) {
    return null;
  }
  const inc = join(packages, dir, "build/native/include");
  return existsSync(inc) ? inc : null;
}

export function buildIncludes(): string[] {
  const dirs = [...includeDirs];
  const wv2 = webView2IncludeDir();
  if (wv2) {
    dirs.push(wv2);
  } else {
    console.log("warning: WebView2 package not found, WebView.cpp will not fully parse");
  }
  for (const d of dirs) {
    if (!existsSync(d)) {
      console.log(`warning: include dir '${d}' does not exist`);
    }
  }
  // forward slashes: the same list is embedded in compile_commands.json, where a
  // backslash is an escape character in JSON and in clang's own response files
  return dirs.map((d) => d.replaceAll("\\", "/")).flatMap((d) => ["-I", d]);
}

export const defines = [
  "-DUNICODE",
  "-DWIN32",
  "-D_WIN32",
  "-D_CRT_SECURE_NO_WARNINGS",
  "-DWINVER=0x0a00",
  "-D_WIN32_WINNT=0x0a00",
  "-DPRE_RELEASE_VER=3.3",
];

// The VS projects use <LanguageStandard>stdcpplatest</LanguageStandard>; tools that
// parse the same sources have to use the same dialect or they reject valid code.
export const cxxStandard = "-std=c++23";
