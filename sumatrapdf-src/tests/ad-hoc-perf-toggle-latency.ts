// OPT-6 perf baseline: Trim toggle (CmdMarginTrimToggle) end-to-end latency ×3 toggles.
// NO new C++ control commands — uses existing CmdMarginTrimToggle WM_COMMAND route only.
// ALSO captures existing RenderCache sumlog lines as evidence for future metric work.
// Register ONLY in tests/before-release.ts (not tests/all.ts).
// Run: bun tests/ad-hoc-perf-toggle-latency.ts [--no-build]
//
// Honest scope note: precise end-to-end toggle latency needs new C++ instrumentation
// (cache-invalidate/complete signal) — recorded as residual. Here we measure
// dispatch wall-clock around each toggle + capture RenderCache sumlog lines as
// evidence. Tolerances LOOSE, sanity ceiling only — NOT a CI fail-gate. Baseline
// capture for future trend comparison.
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { waitForFrame } from "./win-automation.ts";
import { EXE, cmdId, runStandalone, tmpPath } from "./util.ts";
import { postMessage } from "./winapi.ts";

const WM_COMMAND = 0x0111;
const FIXTURE = join(EXE, "..", "..", "..", "ext", "a-zlib", "zlib.3.pdf");
const TOGGLES = 3; // full on/off cycles
const TOGGLE_CEIL_MS = 5000; // generous sanity ceiling, NOT a gate
const SUMLOG = join(EXE, "..", "sumlog.txt"); // debug build writes sumlog.txt next to exe

function perf(label: string, ms: number): void {
  console.log(`PERF: ${label}=${ms.toFixed(1)} ms`);
}

function readSumlogRenderCacheLines(): string[] {
  let text = "";
  try {
    text = readFileSync(SUMLOG, "utf8");
  } catch {
    return [`(no sumlog at ${SUMLOG})`];
  }
  const lines = text.split(/\r?\n/);
  const rc = lines.filter((l) => /RenderCache/i.test(l));
  return rc.slice(0, 5).length > 0 ? rc.slice(0, 5) : ["(no RenderCache lines in sumlog)"];
}

export async function testit(): Promise<void> {
  const cmdTrim = cmdId("CmdMarginTrimToggle");
  const toggleLatencies: number[] = [];

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const pid = typeof proc.pid === "number" ? proc.pid : 0;
      if (!pid) throw new Error("toggle-latency: no proc pid");
      const frame = await waitForFrame(pid, 20000);
      if (!frame) throw new Error("toggle-latency: frame window not found");

      await client.request(ControlCommand.TestGetToc, [FIXTURE]);
      await new Promise((r) => setTimeout(r, 500));

      for (let i = 0; i < TOGGLES; i++) {
        // full toggle cycle: off->on->off (2 commands), time the pair
        const t0 = performance.now();
        postMessage(frame, WM_COMMAND, cmdTrim, 0);
        await new Promise((r) => setTimeout(r, 250));
        postMessage(frame, WM_COMMAND, cmdTrim, 0);
        await new Promise((r) => setTimeout(r, 250));
        const t1 = performance.now();
        toggleLatencies.push(t1 - t0);
        perf(`toggle.cycle${i + 1}Ms`, t1 - t0);
      }

      return "toggle-latency baseline captured";
    },
    [FIXTURE, "-log"],
    { allowedExitCodes: [0] },
  );

  const avgToggle = toggleLatencies.length ? toggleLatencies.reduce((a, b) => a + b, 0) / toggleLatencies.length : 0;
  perf("toggle.avgCycleMs", avgToggle);
  if (avgToggle > TOGGLE_CEIL_MS) {
    console.log(`NOTE: toggle avg above sanity ceiling ${TOGGLE_CEIL_MS} ms (baseline only, not a gate)`);
  }

  // evidence: top-5 RenderCache sumlog lines for future metric work
  console.log("EVIDENCE: top-5 RenderCache sumlog lines:");
  for (const line of readSumlogRenderCacheLines()) {
    console.log(`  ${line}`);
  }

  if (avgToggle > TOGGLE_CEIL_MS) {
    // still pass: baseline capture, not a gate
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
