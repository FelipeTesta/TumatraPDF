// Sync sumatrapdf-src/mupdf to the upstream 1.28.5 tree.
// - copies every file from SRC to DST
// - deletes DST files not present in SRC (the 4 removals)
// - normalizes text files to CRLF (repo uses `* -text`, mupdf stored as CRLF)
// - binary detection: 0x00 in first 8 KiB => copy bytes as-is
import { readdirSync, readFileSync, writeFileSync, mkdirSync, rmSync, statSync } from "fs";
import { join, dirname } from "path";

const SRC = "tests/tmp/sp-1285/ext/mupdf";
const DST = "sumatrapdf-src/mupdf";

function walk(root: string, rel = ""): string[] {
    const out: string[] = [];
    for (const e of readdirSync(join(root, rel), { withFileTypes: true })) {
        const r = rel ? `${rel}/${e.name}` : e.name;
        if (e.isDirectory()) out.push(...walk(root, r));
        else out.push(r);
    }
    return out;
}

function toCrlf(buf: Buffer): { out: Buffer; text: boolean } {
    const probe = buf.subarray(0, Math.min(buf.length, 8192));
    if (probe.includes(0)) return { out: buf, text: false };
    const parts: Buffer[] = [];
    let start = 0;
    for (let i = 0; i < buf.length; i++) {
        if (buf[i] === 0x0a && (i === 0 || buf[i - 1] !== 0x0d)) {
            parts.push(buf.subarray(start, i), Buffer.from([0x0d, 0x0a]));
            start = i + 1;
        }
    }
    if (parts.length === 0) return { out: buf, text: true };
    parts.push(buf.subarray(start));
    return { out: Buffer.concat(parts), text: true };
}

const srcFiles = walk(SRC);
const dstFiles = new Set(walk(DST));
const srcSet = new Set(srcFiles);

let copied = 0;
let normalized = 0;
let added = 0;
let removed = 0;

for (const rel of srcFiles) {
    const srcPath = join(SRC, rel);
    const dstPath = join(DST, rel);
    mkdirSync(dirname(dstPath), { recursive: true });
    const raw = readFileSync(srcPath);
    const { out, text } = toCrlf(raw);
    writeFileSync(dstPath, out);
    copied++;
    if (text) normalized++;
    if (!dstFiles.has(rel)) added++;
}

for (const rel of dstFiles) {
    if (!srcSet.has(rel)) {
        rmSync(join(DST, rel), { force: true });
        removed++;
    }
}

console.log(`copied=${copied} (text-normalized=${normalized}) added=${added} removed=${removed}`);
