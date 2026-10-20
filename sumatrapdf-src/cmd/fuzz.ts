// App-level fuzzing driver: feeds mutated inputs to the ASan build of TumatraPDF.exe
// through its headless entry points (-render for the full engine+render path,
// -extract-text for load+text extraction, -dump-exif for the EXIF parser) and records
// any crash together with the input that caused it.
//
// Two-tier fuzzing setup: base/ text parsers are fuzzed in-process by the native
// libFuzzer harness (cmd/build-fuzz-harness.ts -> out/fuzz/fuzz-parsers.exe,
// ~13k execs/s); this driver fuzzes the full ENGINE paths, which cannot run
// in-process (mupdf has global state that cannot reset between loads). Each exec
// pays ~100-200 ms of process startup, so runs are bounded by -n (iterations) and
// -seconds rather than relying on libFuzzer's own loop.
//
// Flag-semantics notes: -render/-extract-text take the PAGE NUMBER (-render 1)
// and the document is a positional arg; -dump-exif takes the file as its param.
// The render mode only exercises the engine lifecycle (EngineMupdf::RenderPage
// returns null headless - GetFzPageInfo's full path needs GUI-side init), so
// extract-text is the deep headless exercise path.
//
// Requirements: build the ASan exe first (bun cmd/build-asan.ts); out/dbg64_asan/
// TumatraPDF.exe must exist.
//
// Usage:
//   bun cmd/fuzz.ts                          # 100 iterations over ext/*.pdf seeds
//   bun cmd/fuzz.ts -n 500 -seconds 600      # more iterations, 10 min budget
//   bun cmd/fuzz.ts -mode exif               # -dump-exif over *.jpg seeds
//   bun cmd/fuzz.ts -seeds <dir>             # custom seed dir (default: ext/ + tests/, recursive)
//   bun cmd/fuzz.ts -seed <file>             # single-seed campaign

import { appendFile, copyFile, mkdir, readdir, rm, writeFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join } from "node:path";

interface FuzzMode {
  // how the app is invoked for one input; returns [exeArgs, fileFilter]
  argsFor(file: string): string[];
  seeds(pattern: string): string[];
}

const modes: Record<string, FuzzMode> = {
  // engine load + page render + teardown, all before window creation; exercises
  // mupdf xref/object parsing, the engine wrappers and the render pipeline
  render: {
    argsFor: (file) => [file, "-render", "1"],
    seeds: () =>
      gatherSeeds(
        ["ext", "tests"],
        [".pdf", ".xps", ".epub", ".mobi", ".fb2", ".cbz", ".cbr", ".jpg", ".png", ".jxl", ".heic", ".tga"],
      ),
  },
  // engine load + text extraction + teardown (no rendering)
  extract: {
    argsFor: (file) => [file, "-extract-text", "1"],
    seeds: () => gatherSeeds(["ext", "tests"], [".pdf", ".xps", ".epub", ".mobi", ".fb2", ".chm"]),
  },
  // EXIF/JPEG metadata parser only; fast, focused
  exif: {
    argsFor: (file) => ["-dump-exif", file],
    seeds: () => gatherSeeds(["ext", "tests"], [".jpg", ".jpeg", ".tif", ".tiff"]),
  },
};

// walk the given repo-relative roots (recursively) and return every file whose
// extension matches; this is the D2 seed corpus ("ext/*.pdf + tests/ regression files")
async function gatherSeeds(roots: string[], exts: string[]): Promise<string[]> {
  const out: string[] = [];
  for (const root of roots) {
    if (!existsSync(root)) continue;
    const walk = async (dir: string): Promise<void> => {
      for (const name of await readdir(dir, { withFileTypes: true })) {
        const p = join(dir, name.name);
        if (name.isDirectory()) {
          if (name.name === "tmp" || name.name === "node_modules") continue;
          await walk(p);
        } else if (exts.some((e) => name.name.toLowerCase().endsWith(e))) {
          out.push(p);
        }
      }
    };
    await walk(root);
  }
  return out;
}

// a deterministic byte-level mutator: enough structure preservation to reach deep
// parser paths (only a few mutations per input), enough chaos to escape shallow checks
function mutate(u8: Uint8Array, rng: () => number): Uint8Array {
  const out = new Uint8Array(u8);
  const nMut = 1 + (rng() % 4);
  for (let i = 0; i < nMut; i++) {
    const kind = rng() % 5;
    const pos = rng() % out.length;
    switch (kind) {
      case 0: // bit flip
        out[pos] ^= 1 << (rng() % 8);
        break;
      case 1: // random byte
        out[pos] = rng() % 256;
        break;
      case 2: // interesting byte
        out[pos] = [0, 0xff, 0x7f, 0x80, 0x00][rng() % 5];
        break;
      case 3: {
        // truncate
        const cut = 1 + (rng() % out.length);
        return out.slice(0, cut);
      }
      case 4: {
        // repeat a slice elsewhere (chunk splice); copyWithin clips safely
        const len = Math.min(64, out.length - pos);
        const dst = rng() % out.length;
        out.copyWithin(dst, pos, pos + len);
        break;
      }
    }
  }
  return out;
}

// xorshift: deterministic across runs so a crash is always reproducible by rerunning
// with the same -seed-offset
function makeRng(seed: number): () => number {
  let s = seed || 0x9e3779b9;
  return () => {
    s ^= s << 13;
    s ^= s >>> 17;
    s ^= s << 5;
    return s >>> 0;
  };
}

const argv = process.argv;
function argVal(name: string): string | undefined {
  const i = argv.indexOf(name);
  return i >= 0 ? argv[i + 1] : undefined;
}
const modeName = argVal("-mode") ?? "render";
const exeRel = argVal("-exe") ?? join("out", "dbg64_asan", "TumatraPDF.exe");
const seedsDir = argVal("-seeds") ?? "ext";
const seedFile = argVal("-seed");
const iterations = Number.parseInt(argVal("-n") ?? "100", 10) || 100;
const secondsBudget = Number.parseInt(argVal("-seconds") ?? "600", 10) || 600;
const seedOffset = Number.parseInt(argVal("-seed-offset") ?? "1", 10) || 1;

const workDir = join("tests", "tmp", "fuzz-run");
const crashesDir = join("tests", "tmp", "fuzz-crashes");
const logFile = join(crashesDir, "fuzz-log.txt");

async function main() {
  const mode = modes[modeName];
  if (!mode) {
    console.log(`unknown -mode '${modeName}' (valid: ${Object.keys(modes).join(", ")})`);
    process.exit(1);
  }
  const exe = join(process.cwd(), exeRel);
  if (!existsSync(exe)) {
    console.log(`missing ASan exe '${exe}' - run: bun cmd/build-asan.ts`);
    process.exit(1);
  }

  const seeds = seedFile ? [seedFile] : await mode.seeds(seedsDir);
  if (seeds.length === 0) {
    console.log(`no seed files found in '${seedsDir}'`);
    process.exit(1);
  }

  await rm(workDir, { recursive: true, force: true });
  await mkdir(workDir, { recursive: true });
  await mkdir(crashesDir, { recursive: true });

  const inputPath = join(workDir, `input${modeName === "exif" ? ".jpg" : ".pdf"}`);
  const start = performance.now();
  let crashes = 0;
  let execs = 0;

  for (let i = 0; i < iterations; i++) {
    if ((performance.now() - start) / 1000 > secondsBudget) {
      console.log(`time budget (${secondsBudget}s) reached`);
      break;
    }
    const rng = makeRng((seedOffset + i) * 2654435761);
    const seed = seeds[rng() % seeds.length];
    const raw = new Uint8Array(await Bun.file(seed).arrayBuffer());
    if (raw.length === 0) continue;
    const data = mutate(raw, rng);
    await writeFile(inputPath, data);

    const res = Bun.spawnSync([exe, "-for-testing", ...mode.argsFor(inputPath)]);
    execs++;
    const crashed = res.exitCode !== 0 && res.exitCode !== 1;
    const outText = `${res.stdout}\n${res.stderr}`;
    const asanHit = /AddressSanitizer|ERROR: libFuzzer|SEGV|runtime error/i.test(outText);
    if (crashed || asanHit) {
      crashes++;
      const tag = `${Date.now()}-iter${i}`;
      const crashInput = join(crashesDir, `${tag}${modeName === "exif" ? ".jpg" : ".pdf"}`);
      await copyFile(inputPath, crashInput);
      await appendFile(join(crashesDir, `${tag}.out`), outText.slice(0, 64 * 1024));
      await appendFile(logFile, `CRASH iter=${i} seed=${seed} exit=${res.exitCode} input=${crashInput}\n`);
      console.log(`CRASH iter=${i} exit=${res.exitCode} -> ${crashInput}`);
    }
    if ((i + 1) % 25 === 0) {
      const rate = (execs / ((performance.now() - start) / 1000)).toFixed(1);
      console.log(`[${i + 1}/${iterations}] execs=${execs} crashes=${crashes} (${rate} exec/s)`);
    }
  }

  const secs = ((performance.now() - start) / 1000).toFixed(0);
  console.log(`done: ${execs} execs in ${secs}s, ${crashes} crash(es); inputs kept in ${crashesDir}`);
}

await main();
