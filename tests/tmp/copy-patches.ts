// Copy upstream ext/patches (patch files + README) into sumatrapdf-src/ext/patches
// so the vendored-mupdf changes are recorded in-repo (F3 provenance).
import { readdirSync, readFileSync, writeFileSync, mkdirSync } from "fs";

const SRC = "tests/tmp/sp-1285/ext/patches";
const DST = "sumatrapdf-src/ext/patches";

mkdirSync(DST, { recursive: true });
let n = 0;
for (const e of readdirSync(SRC, { withFileTypes: true })) {
    if (!e.isFile()) continue;
    writeFileSync(`${DST}/${e.name}`, readFileSync(`${SRC}/${e.name}`));
    n++;
}
console.log(`copied ${n} files to ${DST}`);
