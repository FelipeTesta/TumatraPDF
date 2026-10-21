// B4: coordinate-namespace + virtual/physical pageNo invariant tests, driven
// through the -dbg-control TestCvtRoundTrip command against the real
// DisplayModel layout.
//
// Invariants asserted (in the app, see SumatraTest.cpp CvtRoundTripResultTemp):
//   1. for every page and N deterministic random page points:
//      CvtFromScreen(CvtToScreen(pageNo, pt)) == pt within ~0.5 px / zoom
//   2. the TC2 virtual->physical mapping is consistent (monotonic physical
//      pages, column in 1..2) whenever the doc has virtual pages
//
// Run: bun tests/ad-hoc-b4-coordinate-invariants.ts [--no-build]
import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";

// EXE = <root>/out/dbg64/TumatraPDF.exe: three ".." to get back to the repo
// root (one pops the filename, one pops dbg64, one pops out)
const ROOT = join(EXE, "..", "..", "..");
const ZLIB_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const MULTIPAGE_PDF = join(ROOT, "ext", "brotli", "docs", "brotli-comparison-study-2015-09-22.pdf");

// parse the "OK pages=... maxErr=..." / FAIL line the app returns
// (response args: [exitCode, resultText])
function parseResult(res: ControlArg[]): Record<string, string> {
  const exitCode = res[0] as number;
  const text = ((res[1] as string) ?? "").trim();
  const kv: Record<string, string> = {};
  const first = text.split("\n")[0] ?? "";
  for (const m of first.matchAll(/(\w+)=([^\s]+)/g)) {
    kv[m[1]!] = m[2]!;
  }
  kv["__exitCode"] = String(exitCode);
  kv["__firstLine"] = first;
  return kv;
}

async function runRoundTrip(
  docArgs: string[],
  zoom: number,
  samples: number,
  seed: number,
): Promise<Record<string, string>> {
  const args: ControlArg[] = [zoom, samples, seed];
  const res = await runControlCommand(EXE, ControlCommand.TestCvtRoundTrip, args, docArgs);
  const kv = parseResult(res);
  const text = kv["__firstLine"]!;
  if (!text.startsWith("OK") && !text.startsWith("FAIL") && !text.startsWith("NOTREADY")) {
    throw new Error(`unexpected result: ${text}`);
  }
  return kv;
}

export async function testit(): Promise<void> {
  // 1. single-page PDF at several zooms: the round-trip must hold everywhere
  for (const zoom of [25, 100, 400, 1600]) {
    const kv = await runRoundTrip([ZLIB_PDF], zoom, 16, 42);
    if (kv["__firstLine"]!.startsWith("NOTREADY")) {
      throw new Error(`doc not loaded: ${kv["__firstLine"]}`);
    }
    if (kv["__firstLine"]!.startsWith("FAIL")) {
      throw new Error(`round-trip failed at zoom ${zoom}: ${kv["__firstLine"]}`);
    }
    const maxErr = Number(kv["maxErr"]);
    const pages = Number(kv["pages"]);
    if (!(pages >= 1)) {
      throw new Error(`expected at least 1 page, got ${pages}`);
    }
    if (!(maxErr <= 1.25 / (zoom / 100) + 0.001)) {
      throw new Error(`maxErr ${maxErr} exceeds tolerance at zoom ${zoom}`);
    }
    console.log(`zoom ${zoom}: pages=${pages} maxErr=${maxErr.toFixed(3)} OK`);
  }

  // 2. different seed (different sample points) must also hold
  const kv2 = await runRoundTrip([ZLIB_PDF], 100, 24, 0x5eed);
  if (kv2["__firstLine"]!.startsWith("FAIL")) {
    throw new Error(`round-trip failed with seed 0x5eed: ${kv2["__firstLine"]}`);
  }
  console.log(`seed 0x5eed: maxErr=${Number(kv2["maxErr"]).toFixed(3)} OK`);

  // 3. multi-page document: every page covered
  const kv3 = await runRoundTrip([MULTIPAGE_PDF], 100, 8, 7);
  if (kv3["__firstLine"]!.startsWith("FAIL")) {
    throw new Error(`round-trip failed on multi-page doc: ${kv3["__firstLine"]}`);
  }
  console.log(`multi-page: pages=${kv3["pages"]} maxErr=${Number(kv3["maxErr"]).toFixed(3)} OK`);

  // 4. VirtWnd local<->root bridge: no doc, so the home virtual tree exists.
  // The tree is laid out by the home paint path; poll until NOTREADY clears
  const deadline4 = Date.now() + 20_000;
  let text4 = "";
  for (;;) {
    const res4 = await runControlCommand(EXE, ControlCommand.TestVirtWndHitTest, [8, 42]);
    text4 = ((res4[1] as string) ?? "").trim();
    if (!text4.startsWith("NOTREADY")) {
      break;
    }
    if (Date.now() > deadline4) {
      throw new Error(`virt-wnd tree never became ready: ${text4}`);
    }
    await new Promise((r) => setTimeout(r, 500));
  }
  if (text4.startsWith("FAIL")) {
    throw new Error(`virt-wnd bridge failed: ${text4}`);
  }
  const kv4 = parseResult([0, text4]);
  if (!(Number(kv4["tested"]) > 0)) {
    throw new Error(`virt-wnd bridge tested nothing: ${text4}`);
  }
  console.log(`virt-wnd bridge: ${text4.split("\n")[0]}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
