import { join } from "node:path";
import { copyFileSync } from "node:fs";
import { detectVisualStudio, runLogged } from "./util";

// Builds Release x64 and produces BOTH flavors in ../Compiled:
// - TumatraPDF.exe              (portable asset -> TumatraPDF-portable-win64.exe)
// - TumatraPDF-install-win64.exe (installer asset: SAME exe; the "install" in the
//   name flips it to installer mode via IsInstallerOrUninstallerExe()).
// The payload (InstallerData.dat: libsumatrapdf.dll, PdfFilter.dll, PdfPreview.dll,
// sumatrapdf-tool.exe) is already embedded by the TumatraPDF pre-build step.
// CI (.github/workflows/release.yml) does the same renames from out/rel64.
const outDir = join("out", "rel64");
const finalExeDir = join("..", "Compiled");

async function main() {
    const timeStart = performance.now();
    console.log("Release x64 build (portable + installer)");
    const { msbuildPath } = detectVisualStudio();
    const sln = String.raw`vs2022\TumatraPDF.sln`;
    await runLogged(msbuildPath, [sln, `/t:TumatraPDF`, `/p:Configuration=Release;Platform=x64`, `/m`]);

    const srcExe = join(outDir, "TumatraPDF.exe");
    for (const name of ["TumatraPDF.exe", "TumatraPDF-portable-win64.exe", "TumatraPDF-install-win64.exe"]) {
        const dst = join(finalExeDir, name);
        try {
            copyFileSync(srcExe, dst);
            console.log(`Copied ${srcExe} -> ${dst}`);
        } catch (e) {
            console.warn(`Failed to copy ${name}: ${e}`);
        }
    }
    const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
    console.log(`build took ${elapsed}s`);
}

await main();
