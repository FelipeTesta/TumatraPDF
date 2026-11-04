// Runs all regular tests (tests/all.ts) plus ad-hoc / pre-release checks.
//
// Run:  bun tests/before-release.ts [--no-build]
//
// Use before a release to re-verify EXIF parsing and other occasional checks
// that are too slow or need external repos and are not in tests/all.ts.

import { buildApp, formatDuration, runTest } from "./util.ts";
import { testit as allTests } from "./all.ts";
import { testit as adHocExif } from "./ad-hoc-exif.ts";
import { testit as adHocSelectionTranslate } from "./ad-hoc-selection-translate.ts";
import { testit as adHocTripleClickLine } from "./ad-hoc-triple-click-line.ts";
import { testit as adHocTocPaletteSync } from "./ad-hoc-toc-palette-sync.ts";
import { testit as adHocJpegDecode } from "./ad-hoc-jpeg-decode.ts";
import { testit as adHocPasteImageAnnot } from "./ad-hoc-paste-image-annot.ts";
import { testit as adHocUia } from "./ad-hoc-uia.ts";
import { testit as adHocUpdate } from "./ad-hoc-update.ts";
import { testit as adHocFlashcardCrossPage } from "./ad-hoc-flashcard-crosspage.ts";
import { testit as adHocB4CoordinateInvariants } from "./ad-hoc-b4-coordinate-invariants.ts";
import { testit as latexTests } from "./latex.ts";
import { testit as issueChmLzx } from "./issue-chm-lzx.ts";
import { testit as adHocSettingsMigration } from "./ad-hoc-settings-migration.ts";
import { testit as adHocProfileLiveWrites } from "./ad-hoc-profile-live-writes.ts";
import { testit as adHocTabSession } from "./ad-hoc-tab-session.ts";
import { testit as adHocTabMenu } from "./ad-hoc-tab-menu.ts";
import { testit as adHocTabSessionE2E } from "./ad-hoc-tab-session-e2e.ts";
import { testit as adHocAnnotSaveIntegrity } from "./ad-hoc-annot-save-integrity.ts";
import { testit as adHocRestoreNoFileStates } from "./ad-hoc-restore-no-filestates.ts";
import { testit as adHocPerfStartup } from "./ad-hoc-perf-startup.ts";
import { testit as adHocPerfCacheBehavior } from "./ad-hoc-perf-cache-behavior.ts";
import { testit as adHocPerfToggleLatency } from "./ad-hoc-perf-toggle-latency.ts";

const adHocTests: [string, () => void | Promise<void>][] = [
  ["ad-hoc-exif", adHocExif],
  ["ad-hoc-selection-translate", adHocSelectionTranslate],
  ["ad-hoc-triple-click-line", adHocTripleClickLine],
  ["ad-hoc-toc-palette-sync", adHocTocPaletteSync],
  ["ad-hoc-jpeg-decode", adHocJpegDecode],
  ["ad-hoc-paste-image-annot", adHocPasteImageAnnot],
  ["ad-hoc-uia", adHocUia],
  ["ad-hoc-update", adHocUpdate],
  ["ad-hoc-flashcard-crosspage", adHocFlashcardCrossPage],
  ["ad-hoc-b4-coordinate-invariants", adHocB4CoordinateInvariants],
  ["issue-chm-lzx", issueChmLzx],
  ["ad-hoc-settings-migration", adHocSettingsMigration],
  ["ad-hoc-profile-live-writes", adHocProfileLiveWrites],
  ["ad-hoc-tab-session", adHocTabSession],
  ["ad-hoc-tab-menu", adHocTabMenu],
  ["ad-hoc-tab-session-e2e", adHocTabSessionE2E],
  ["ad-hoc-annot-save-integrity", adHocAnnotSaveIntegrity],
  ["ad-hoc-restore-no-filestates", adHocRestoreNoFileStates],
  ["ad-hoc-perf-startup", adHocPerfStartup],
  ["ad-hoc-perf-cache-behavior", adHocPerfCacheBehavior],
  ["ad-hoc-perf-toggle-latency", adHocPerfToggleLatency],
];

export async function testit(): Promise<void> {
  const t0 = performance.now();
  await allTests();

  console.log("\n========== latex ==========");
  await runTest("latex", latexTests);

  for (const [name, fn] of adHocTests) {
    console.log(`\n========== ${name} ==========`);
    await runTest(name, fn);
  }
  console.log(
    `\n✅ before-release checks passed (all.ts + latex.ts + ${adHocTests.length} ad-hoc test(s)) in ${formatDuration(performance.now() - t0)}`,
  );
}

if (import.meta.main) {
  if (!process.argv.includes("--no-build")) {
    buildApp();
  }
  try {
    await testit();
  } catch (e) {
    console.error(`\n❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
  process.exit(0);
}
