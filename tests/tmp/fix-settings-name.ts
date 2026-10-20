// Rename the settings file written by tests to the fork's actual name
// (AppSettings.cpp returns "TumatraPDF-settings.txt"; tests still used the
// upstream name, so any settings they wrote were silently ignored).
import { readdirSync, readFileSync, writeFileSync } from "fs";
import { join } from "path";

const DIR = "sumatrapdf-src/tests";
const FROM = "SumatraPDF-settings.txt";
const TO = "TumatraPDF-settings.txt";

let changed = 0;
for (const e of readdirSync(DIR, { withFileTypes: true })) {
    if (!e.isFile() || !e.name.endsWith(".ts")) continue;
    const p = join(DIR, e.name);
    const buf = readFileSync(p);
    const s = buf.toString("latin1");
    if (!s.includes(FROM)) continue;
    const n = s.split(FROM).join(TO);
    writeFileSync(p, Buffer.from(n, "latin1"));
    changed++;
    console.log(`updated ${e.name}`);
}
console.log(`files changed: ${changed}`);
