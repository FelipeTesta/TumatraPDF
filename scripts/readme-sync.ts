// Checks that README.es.md was re-synced after the last change to README.md.
//
// The translated README carries a machine-readable token right under its header:
//   <!-- readme-sync: source-blob=<git blob sha of README.md> version=<app version> synced=<date> -->
// `source-blob` is the hash of the exact README.md content that was translated, so any later
// English edit makes the two files drift and this check fails until an agent updates the
// translation (and the token).
//
// Usage:
//   bun scripts/readme-sync.ts            # check, exit 1 on drift
//   bun scripts/readme-sync.ts --update   # rewrite the token from the current README.md
//   bun scripts/readme-sync.ts --update --version 1.0.21

import { execFileSync } from "node:child_process";
import { readFileSync, writeFileSync } from "node:fs";

const SRC = "README.md";
const TRANSLATED = "README.es.md";
const TOKEN_RE = /<!--\s*readme-sync:(.*?)-->/s;

function readToken(text: string): { blob?: string; version?: string; synced?: string } {
  const m = text.match(TOKEN_RE);
  if (!m) return {};
  const fields: Record<string, string> = {};
  for (const part of m[1].trim().split(/\s+/)) {
    const eq = part.indexOf("=");
    if (eq > 0) fields[part.slice(0, eq)] = part.slice(eq + 1);
  }
  return { blob: fields["source-blob"], version: fields.version, synced: fields.synced };
}

// hash-object applies the same clean filters git uses on commit, so the value is identical
// on a Windows working tree and on a fresh CI checkout.
function blobOf(path: string): string {
  return execFileSync("git", ["hash-object", "--path", path, path], { encoding: "utf8" }).trim();
}

function latestChangelogVersion(): string {
  const text = readFileSync("CHANGELOG.md", "utf8");
  const m = text.match(/^##\s*v?(\d+\.\d+(?:\.\d+)?)/m);
  return m ? m[1] : "0.0.0";
}

function argValue(name: string, fallback: string): string {
  const i = process.argv.indexOf(name);
  if (i > 0 && i + 1 < process.argv.length) return process.argv[i + 1];
  return fallback;
}

const translated = readFileSync(TRANSLATED, "utf8");
const token = readToken(translated);
const current = blobOf(SRC);

if (process.argv.includes("--update")) {
  const version = argValue("--version", latestChangelogVersion());
  const today = new Date().toISOString().slice(0, 10);
  const next = `<!-- readme-sync: source-blob=${current} version=${version} synced=${today} -->`;
  const updated = TOKEN_RE.test(translated)
    ? translated.replace(TOKEN_RE, next)
    : translated.replace(/\n\n/, `\n\n${next}\n`);
  writeFileSync(TRANSLATED, updated, "utf8");
  console.log(`README.es.md token updated: source-blob=${current} version=${version} synced=${today}`);
  process.exit(0);
}

const problems: string[] = [];
if (!token.blob) problems.push(`${TRANSLATED} has no readme-sync token`);
else if (token.blob !== current)
  problems.push(
    `${SRC} changed since the last sync (translated ${token.blob.slice(0, 12)}, now ${current.slice(0, 12)})`,
  );

if (problems.length) {
  console.error("::error::README translation is out of sync");
  for (const p of problems) console.error(`  - ${p}`);
  console.error(`  Fix: update ${TRANSLATED} to match ${SRC}, then run 'bun scripts/readme-sync.ts --update'`);
  process.exit(1);
}

console.log(`README.es.md in sync with README.md (${current.slice(0, 12)})`);
