// Run the freeze test multiple times and collect stats
import { buildApp, runStandalone, formatDuration } from "./util.ts";

const testName = "ad-hoc-flashcard-study-freeze";

async function runOnce(runNum: number): Promise<{ passed: boolean; time: string; output: string }> {
  const t0 = performance.now();
  let passed = false;
  let output = "";
  try {
    // Import and run the test
    const mod = await import(`./${testName}.ts`);
    await mod.testit();
    passed = true;
  } catch (e) {
    output = (e as Error)?.message ?? String(e);
  }
  const elapsed = formatDuration(performance.now() - t0);
  return { passed, time: elapsed, output };
}

export async function testit(): Promise<void> {
  console.log("Running freeze test 5 times...\n");
  
  let passCount = 0;
  let failCount = 0;
  
  for (let i = 1; i <= 5; i++) {
    console.log(`\n========== RUN ${i} ==========`);
    const result = await runOnce(i);
    if (result.passed) {
      console.log(`✅ RUN ${i} PASSED in ${result.time}`);
      passCount++;
    } else {
      console.log(`❌ RUN ${i} FAILED in ${result.time}: ${result.output}`);
      failCount++;
    }
  }
  
  console.log(`\n========== SUMMARY ==========`);
  console.log(`Passed: ${passCount}/5`);
  console.log(`Failed: ${failCount}/5`);
  console.log(`Failure rate: ${(failCount / 5 * 100).toFixed(1)}%`);
  
  if (failCount > 0) {
    throw new Error(`${failCount}/5 runs failed — freeze is intermittent`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}