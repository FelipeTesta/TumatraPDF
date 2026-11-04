// OPT-6 perf baseline: render-cache page-back navigation latency via control pipe + WM_COMMAND nav.
// NO new C++ control commands — uses existing TestGetToc (doc load) + CmdGoToNextPage/CmdGoToPrevPage
// (real WM_COMMAND route) instrumentation only.
// Register ONLY in tests/before-release.ts (not tests/all.ts).
// Run: bun tests/ad-hoc-perf-cache-behavior.ts [--no-build]
//
// Honest scope note: captures BASELINE back-nav hop latency for trend comparison.
// Tolerances LOOSE, sanity ceiling only — NOT a CI fail-gate. Real gates need new
// C++ control-pipe commands (cache hit rate, relayout frequency) — residual in OPT-6.
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { waitForFrame } from "./win-automation.ts";
import { EXE, cmdId, runStandalone } from "./util.ts";
import { postMessage } from "./winapi.ts";

const WM_COMMAND = 0x0111;
const FIXTURE = join(EXE, "..", "..", "..", "ext", "a-zlib", "zlib.3.pdf");
const NAV_HOPS = 6; // >=5 back-nav hops, avg reported
const BACK_NAV_CEIL_MS_PER_HOP = 200; // generous sanity ceiling, NOT a gate

function perf(label: string, ms: number): void {
  console.log(`PERF: ${label}=${ms.toFixed(1)} ms`);
}

export async function testit(): Promise<void> {
  const cmdNext = cmdId("CmdGoToNextPage");
  const cmdPrev = cmdId("CmdGoToPrevPage");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const pid = typeof proc.pid === "number" ? proc.pid : 0;
      if (!pid) throw new Error("cache-behavior: no proc pid");
      const frame = await waitForFrame(pid, 20000);
      if (!frame) throw new Error("cache-behavior: frame window not found");

      // load fixture via existing control command
      await client.request(ControlCommand.TestGetToc, [FIXTURE]);
      await new Promise((r) => setTimeout(r, 500));

      // advance a few pages forward so we have pages to go back from
      for (let i = 0; i < 4; i++) {
        postMessage(frame, WM_COMMAND, cmdNext, 0);
        await new Promise((r) => setTimeout(r, 150));
      }

      // measure repeated page-back navigation latency (cached back-nav)
      const hops: number[] = [];
      for (let i = 0; i < NAV_HOPS; i++) {
        const t0 = performance.now();
        postMessage(frame, WM_COMMAND, cmdPrev, 0);
        // settle: let the app process the nav + begin render
        await new Promise((r) => setTimeout(r, 120));
        const t1 = performance.now();
        hops.push(t1 - t0);
      }
      const avgHop = hops.reduce((a, b) => a + b, 0) / hops.length;
      perf("cache.backNavAvgMsPerHop", avgHop);
      console.log(`PERF: cache.backNavHops=${hops.join(",")}`);

      if (avgHop > BACK_NAV_CEIL_MS_PER_HOP) {
        console.log(
          `NOTE: back-nav avg above sanity ceiling ${BACK_NAV_CEIL_MS_PER_HOP} ms/hop (baseline only, not a gate)`,
        );
      }

      return "cache-behavior baseline captured";
    },
    [FIXTURE, "-log"],
    { allowedExitCodes: [0] },
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
