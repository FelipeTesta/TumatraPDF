// Keeps FLOW/*.dot maps honest: their references are checked against the code.
//
// A .dot map says "F9 -> CmdAutoScrollToggle -> AutoScrollContinuousTick (AutoScroll.cpp:338)".
// When the code is renamed or deleted, the map keeps claiming a flow that no longer exists and
// nobody notices. This script extracts every file / symbol reference from the maps and verifies
// it still exists, so staleness becomes a red CI check instead of folklore.
//
// Modes:
//   bun scripts/flow-sync.ts                 check (CI) - fails only on NEW staleness (ratchet)
//   bun scripts/flow-sync.ts --baseline      rewrite FLOW/.sync-baseline.json from the current state
//   bun scripts/flow-sync.ts --stamp         refresh "// dot-sync: verified=... refs=..." lines
//   bun scripts/flow-sync.ts --strip-lines   drop ":123" line numbers from file references
//
// Ratchet: FLOW/.sync-baseline.json records the staleness that already existed when the check was
// introduced. Only dangling references NOT in the baseline fail the build, so the map debt can be
// paid down over time without blocking work today. --baseline refuses to grow an entry.
//
// Allowlist: FLOW/.sync-allow.txt lists tokens that are deliberately not in this repo (Win32 /
// WebView2 / mupdf API names, node ids of a map describing another project). One token per line,
// optionally scoped as "some-map.dot: Token". '#' starts a comment.

import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { readFileSync, readdirSync, writeFileSync, existsSync } from "node:fs";
import { basename, join, relative, sep } from "node:path";

const ROOT = process.cwd();
const FLOW = join(ROOT, "FLOW");
const SRC = join(ROOT, "sumatrapdf-src", "src");
const BASELINE = join(FLOW, ".sync-baseline.json");
const ALLOWLIST = join(FLOW, ".sync-allow.txt");
const STAMP_RE = /^\/\/ dot-sync: verified=(\d{4}-\d{2}-\d{2}) refs=([0-9a-f]+)?\s*$/m;

// Directories that never contain referenceable code, or that are too large to index.
const SKIP_DIRS = new Set(["out", ".vs", "packages", ".work", "docs", "tests", "node_modules", ".git"]);
const CODE_EXT = /\.(cpp|h|c|cc|hpp)$/;
// Node ids and Win32 control classes are not symbols of this codebase.
const NOT_SYMBOLS = new Set([
  "ReBarWindow32",
  "SysTabControl32",
  "SysListView32",
  "SysTreeView32",
  "ComboBoxEx32",
  "ToolbarWindow32",
]);

type MapInfo = {
  path: string; // repo-relative, forward slashes
  text: string;
  fileRefs: string[];
  symbols: string[];
};

function rel(p: string): string {
  return relative(ROOT, p).split(sep).join("/");
}

function walk(dir: string, out: string[]) {
  let entries;
  try {
    entries = readdirSync(dir, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const e of entries) {
    if (e.isDirectory()) {
      if (!SKIP_DIRS.has(e.name)) walk(join(dir, e.name), out);
    } else if (CODE_EXT.test(e.name)) {
      out.push(join(dir, e.name));
    }
  }
  return out;
}

// Index code under src/, vendored mupdf/ext headers and the code generators. The generators are
// where setting / command / flag names are declared (`field("AutoScrollSpeedMultiplier", ...)` in
// cmd/gen-settings.ts), so a map citing the user-facing setting name is correct even though the
// C++ struct field is spelled autoScrollSpeedMultiplier.
function buildCodeIndex(): { text: string; byName: Map<string, string[]> } {
  const files = walk(SRC, []);
  for (const extra of ["mupdf", "ext"]) {
    walk(join(ROOT, "sumatrapdf-src", extra), files);
  }
  const genDir = join(ROOT, "sumatrapdf-src", "cmd");
  for (const e of readdirSync(genDir, { withFileTypes: true })) {
    if (e.isFile() && e.name.startsWith("gen-") && e.name.endsWith(".ts")) files.push(join(genDir, e.name));
  }
  const byName = new Map<string, string[]>();
  let text = "";
  for (const f of files) {
    const name = basename(f);
    if (!byName.has(name)) byName.set(name, []);
    byName.get(name)!.push(f);
    try {
      text += "\n" + readFileSync(f, "utf8");
    } catch {
      /* unreadable file: skip */
    }
  }
  return { text, byName };
}

function readAllowlist(): { global: Set<string>; scoped: Map<string, Set<string>> } {
  const global = new Set<string>();
  const scoped = new Map<string, Set<string>>();
  if (!existsSync(ALLOWLIST)) return { global, scoped };
  for (const raw of readFileSync(ALLOWLIST, "utf8").split(/\r?\n/)) {
    const line = raw.replace(/#.*$/, "").trim();
    if (!line) continue;
    const m = line.match(/^([^\s:]+\s*:\s*)?(.+)$/);
    if (!m) continue;
    if (m[1]) {
      const map = m[1].replace(/\s*:\s*$/, "");
      if (!scoped.has(map)) scoped.set(map, new Set());
      scoped.get(map)!.add(m[2].trim());
    } else {
      global.add(m[2].trim());
    }
  }
  return { global, scoped };
}

// Removes dot escape sequences (\n, \l, \r used as label line breaks) and hex colors, which
// otherwise glue onto the following token ("\nMainWindow.cpp" -> "nMainWindow.cpp").
function cleanDotText(raw: string): string {
  return raw
    .replace(/\\[nlr]/g, " ")
    .replace(/#[0-9A-Fa-f]{6}\b/g, " ")
    .replace(/\b[A-Fa-f][0-9A-Fa-f]{5}\b(?=[",\]\s])/g, " ");
}

function loadMaps(): MapInfo[] {
  const maps: MapInfo[] = [];
  for (const e of readdirSync(FLOW, { withFileTypes: true })) {
    // FLOW/DEBUG holds one-off investigation maps; they describe bugs that were fixed long ago
    // and are excluded from the check on purpose.
    if (!e.isFile() || !e.name.endsWith(".dot")) continue;
    const full = join(FLOW, e.name);
    const text = cleanDotText(readFileSync(full, "utf8"));
    const nodeIds = new Set<string>();
    for (const m of text.matchAll(/^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\[/gm)) nodeIds.add(m[1]);
    for (const m of text.matchAll(/^(?:di)?graph\s+"?([A-Za-z0-9_]+)"?/gm)) nodeIds.add(m[1]);
    const fileRefs = new Set<string>();
    for (const m of text.matchAll(/(?<![A-Za-z0-9_])([A-Za-z_][A-Za-z0-9_]*\.(?:cpp|h|c))(?![A-Za-z0-9_])/g))
      fileRefs.add(m[1]);
    const symbols = new Set<string>();
    for (const m of text.matchAll(/(?<![A-Za-z0-9_])([A-Z][a-z0-9]+(?:[A-Z][a-z0-9]+){2,})(?![A-Za-z0-9_])/g)) {
      if (!nodeIds.has(m[1]) && !NOT_SYMBOLS.has(m[1])) symbols.add(m[1]);
    }
    maps.push({ path: rel(full), text, fileRefs: [...fileRefs], symbols: [...symbols] });
  }
  return maps;
}

function stampFor(map: MapInfo, byName: Map<string, string[]>): string {
  const blobs: string[] = [];
  for (const ref of map.fileRefs) {
    for (const abs of byName.get(ref) ?? []) {
      blobs.push(execFileSync("git", ["hash-object", "--path", rel(abs), rel(abs)], { encoding: "utf8" }).trim());
    }
  }
  return createHash("sha1").update(blobs.sort().join("")).digest("hex").slice(0, 12);
}

function danglingOf(
  map: MapInfo,
  code: { text: string; byName: Map<string, string[]> },
  allow: ReturnType<typeof readAllowlist>,
) {
  const name = basename(map.path);
  const files = map.fileRefs.filter(
    (r) => !code.byName.has(r) && !allow.global.has(r) && !allow.scoped.get(name)?.has(r),
  );
  const symbols = map.symbols.filter(
    (s) => !code.text.includes(s) && !allow.global.has(s) && !allow.scoped.get(name)?.has(s),
  );
  return { files, symbols };
}

type Baseline = Record<string, { files: string[]; symbols: string[] }>;

function readBaseline(): Baseline {
  if (!existsSync(BASELINE)) return {};
  const parsed = JSON.parse(readFileSync(BASELINE, "utf8")) as { maps?: Baseline };
  return parsed.maps ?? {};
}

const args = process.argv.slice(2);
const code = buildCodeIndex();
const allow = readAllowlist();
const maps = loadMaps();

if (args.includes("--strip-lines")) {
  let total = 0;
  let touched = 0;
  const files: string[] = [];
  (function collect(dir: string) {
    for (const e of readdirSync(dir, { withFileTypes: true })) {
      const p = join(dir, e.name);
      if (e.isDirectory()) collect(p);
      else if (e.name.endsWith(".dot")) files.push(p);
    }
  })(FLOW);
  for (const f of files) {
    const raw = readFileSync(f, "utf8");
    const next = raw.replace(/([A-Za-z0-9_]+\.(?:cpp|h|c)):\d+(?:-\d+)?/g, (_, file: string) => {
      total++;
      return file;
    });
    if (next !== raw) {
      writeFileSync(f, next, "utf8");
      touched++;
    }
  }
  console.log(`stripped ${total} line references in ${touched}/${files.length} maps`);
  process.exit(0);
}

if (args.includes("--stamp")) {
  let touched = 0;
  for (const map of maps) {
    const full = join(ROOT, map.path);
    const raw = readFileSync(full, "utf8");
    const stamp = `// dot-sync: verified=${new Date().toISOString().slice(0, 10)} refs=${stampFor(map, code.byName)}`;
    const next = STAMP_RE.test(raw) ? raw.replace(STAMP_RE, stamp) : `${stamp}\n${raw}`;
    if (next !== raw) {
      writeFileSync(full, next, "utf8");
      touched++;
    }
  }
  console.log(`stamped ${touched} maps`);
  process.exit(0);
}

if (args.includes("--baseline")) {
  const previous = readBaseline();
  const next: Baseline = {};
  for (const map of maps) {
    const d = danglingOf(map, code, allow);
    if (!d.files.length && !d.symbols.length) continue;
    const prev = previous[basename(map.path)];
    const grew = [...d.files, ...d.symbols].filter(
      (t) => !prev || ![...(prev.files ?? []), ...(prev.symbols ?? [])].includes(t),
    );
    if (prev && grew.length) {
      console.error(`refusing to grow baseline for ${map.path}: ${grew.join(", ")}`);
      console.error("Fix the map (or add an allowlist entry) instead of hiding new staleness.");
      process.exit(1);
    }
    next[basename(map.path)] = { files: d.files.sort(), symbols: d.symbols.sort() };
  }
  writeFileSync(
    BASELINE,
    `${JSON.stringify(
      {
        note: "Known-stale references recorded when the FLOW check was introduced. CI fails only on NEW staleness. Pay down over time; --baseline never lets an entry grow.",
        maps: next,
      },
      null,
      2,
    )}\n`,
    "utf8",
  );
  const count = Object.keys(next).length;
  console.log(
    `baseline written: ${count} maps with known debt (${Object.values(next).reduce((a, m) => a + m.files.length + m.symbols.length, 0)} refs)`,
  );
  process.exit(0);
}

// Check mode: ratchet against the baseline, plus advisory warnings for stale review stamps.
const baseline = readBaseline();
const regressions: string[] = [];
const shrunk: string[] = [];
const staleStamps: string[] = [];

for (const map of maps) {
  const key = basename(map.path);
  const d = danglingOf(map, code, allow);
  const known = baseline[key] ?? { files: [], symbols: [] };
  const knownAll = new Set([...known.files, ...known.symbols]);
  const current = [...d.files, ...d.symbols];
  const fresh = current.filter((t) => !knownAll.has(t));
  if (fresh.length) regressions.push(`${map.path}: ${fresh.join(", ")}`);
  if (knownAll.size > current.length)
    shrunk.push(`${map.path} (${knownAll.size - current.length} fixed entries can be removed from the baseline)`);

  const m = map.text.match(STAMP_RE);
  if (m && m[2]) {
    const actual = stampFor(map, code.byName);
    if (m[2] !== actual) staleStamps.push(`${map.path} (verified ${m[1]}, referenced code changed since)`);
  }
}

if (staleStamps.length) {
  for (const s of staleStamps) console.warn(`::warning file=${s.split(" ")[0]}::map not re-verified: ${s}`);
  console.warn(`${staleStamps.length} map(s) reference code changed since their last review stamp`);
}

if (shrunk.length) {
  console.log(`note: ${shrunk.length} map(s) improved - run --baseline to shrink the debt record`);
  for (const s of shrunk) console.log(`  ${s}`);
}

if (regressions.length) {
  console.error("::error::FLOW/*.dot maps reference symbols/files that no longer exist");
  for (const r of regressions) console.error(`  ${r}`);
  console.error("  Fix: update the map to the current code, add an allowlist entry in FLOW/.sync-allow.txt");
  console.error("       for intentional external names, then run 'bun scripts/flow-sync.ts --baseline'.");
  process.exit(1);
}

console.log(`FLOW maps OK (${maps.length} checked, ${Object.keys(baseline).length} with known debt)`);
