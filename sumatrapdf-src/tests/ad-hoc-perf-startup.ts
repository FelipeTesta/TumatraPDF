// OPT-6 perf baseline: startup timing via -dbg-control pipe.
// NO new C++ control commands — uses existing Ping + TestGetToc instrumentation only.
// Register ONLY in tests/before-release.ts (not tests/all.ts).
// Run: bun tests/ad-hoc-perf-startup.ts [--no-build]
//
// Honest scope note: this captures BASELINE numbers (startup + first-doc-load
// wall-clock) for future trend comparison. Tolerances are LOOSE sanity ceilings,
// NOT CI fail-gates. Real perf gates (cache hit rate, relayout frequency) need
// new C++ control-pipe commands — recorded as residual in OPT-6 TODO/LOG.
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone } from "./util.ts";

// small single-page PDF already vendored in the tree (used by other ad-hoc tests)
const FIXTURE = join(EXE, "..", "..", "..", "ext", "a-zlib", "zlib.3.pdf");

// generous, machine-noise-tolerant sanity ceiling — NOT a perf gate
const STARTUP_CEIL_MS = 15000;
const DOCLOAD_CEIL_MS = 15000;

function perf(label: string, ms: number): void {
  console.log(`PERF: ${label}=${ms.toFixed(1)} ms`);
}

export async function testit(): Promise<void> {
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const tStart = performance.now();

      // t0: process start -> Ping OK
      await client.request(ControlCommand.Ping, []);
      const tPingOk = performance.now();
      perf("startup.startToPingOk", tPingOk - tStart);

      // t1: Ping OK -> first doc load OK (TestGetToc opens the fixture)
      await client.request(ControlCommand.TestGetToc, [FIXTURE]);
      const tDocLoadOk = performance.now();
      perf("startup.pingOkToDocLoadOk", tDocLoadOk - tPingOk);

      // sanity ceilings only (record actuals, do not fail hard on noise)
      if (tPingOk - tStart > STARTUP_CEIL_MS) {
        console.log(`NOTE: startup above sanity ceiling ${STARTUP_CEIL_MS} ms (baseline only, not a gate)`);
      }
      if (tDocLoadOk - tPingOk > DOCLOAD_CEIL_MS) {
        console.log(`NOTE: doc load above sanity ceiling ${DOCLOAD_CEIL_MS} ms (baseline only, not a gate)`);
      }

      return "startup baseline captured";
    },
    [FIXTURE, "-log"],
    { allowedExitCodes: [0] },
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
