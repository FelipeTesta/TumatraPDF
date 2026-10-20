// Copy upstream's src/mupdf additions we need into sumatrapdf-src/src/mupdf,
// normalizing to CRLF (repo uses `* -text`).
import { readFileSync, writeFileSync, mkdirSync } from "fs";

const SRC = "tests/tmp/sp-1285/src/mupdf";
const DST = "sumatrapdf-src/src/mupdf";
const files = ["pkcs7-windows.c", "pkcs7-windows.h", "README.md"];

function toCrlf(buf: Buffer): Buffer {
    const parts: Buffer[] = [];
    let start = 0;
    for (let i = 0; i < buf.length; i++) {
        if (buf[i] === 0x0a && (i === 0 || buf[i - 1] !== 0x0d)) {
            parts.push(buf.subarray(start, i), Buffer.from([0x0d, 0x0a]));
            start = i + 1;
        }
    }
    if (parts.length === 0) return buf;
    parts.push(buf.subarray(start));
    return Buffer.concat(parts);
}

mkdirSync(DST, { recursive: true });
for (const f of files) {
    const buf = toCrlf(readFileSync(`${SRC}/${f}`));
    writeFileSync(`${DST}/${f}`, buf);
    console.log(`${f}: ${buf.length} bytes`);
}
