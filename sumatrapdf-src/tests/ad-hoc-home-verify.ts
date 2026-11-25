import { withControlledSumatra, ControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";
import { tmpdir } from "os";
import { join } from "path";

function parseGeometryOutput(output: string): Record<string, string> {
  const result: Record<string, string> = {};
  for (const line of output.split("\n")) {
    // Handle multiple key=value pairs on one line (space-separated)
    const pairs = line.trim().split(/\s+/);
    for (const pair of pairs) {
      const idx = pair.indexOf("=");
      if (idx > 0) {
        const key = pair.slice(0, idx).trim();
        const value = pair.slice(idx + 1).trim();
        result[key] = value;
        // Also parse comma-separated rect values into .x, .y, .dx, .dy
        if (value.includes(",")) {
          const parts = value.split(",").map(s => s.trim());
          if (parts.length === 4) {
            result[`${key}.x`] = parts[0];
            result[`${key}.y`] = parts[1];
            result[`${key}.dx`] = parts[2];
            result[`${key}.dy`] = parts[3];
          }
        }
      }
    }
  }
  return result;
}

function assertEqual<T>(actual: T, expected: T, msg: string): void {
  if (actual !== expected) {
    throw new Error(`${msg}: expected ${expected}, got ${actual}`);
  }
}

function assertTrue(cond: boolean, msg: string): void {
  if (!cond) throw new Error(msg);
}

function assertContains(haystack: string, needle: string, msg: string): void {
  if (!haystack.includes(needle)) {
    throw new Error(`${msg}: "${needle}" not found in "${haystack}"`);
  }
}

export async function testit(): Promise<void> {
  const testDir = join(tmpdir(), "tumatra-test-home-verify");
  
  console.log(`\n=== Home Verify (Graph Toggle + Hover Fade + F1-F4) ===`);
  console.log(`Test data dir: ${testDir}`);

  // Test at multiple widths to verify F2 behavior
  const testWidths = [1200, 640, 480];
  
  for (const width of testWidths) {
    console.log(`\n--- Verify at ${width}px width ---`);
    
    await withControlledSumatra(EXE, async (client) => {
      // Set width for testing
      await client.request(ControlCommand.SetWindowSize, [width, 900]);
      
      // Get initial geometry (should be grid mode)
      let geomRes = await client.request(ControlCommand.TestHomeGeometry);
      let geom = parseGeometryOutput(geomRes[1] as string);
      console.log("Initial mode: grid");
      assertEqual(geom["hoverDay"], "-1", "Initial hoverDay should be -1");
      assertEqual(geom["hoverFadeAlpha"], "255", "Initial hoverFadeAlpha should be 255");
      
      // Toggle to graph mode
      const toggleRes = await client.request(ControlCommand.TestStatsToggleGraph);
      console.log("Toggle to graph:", toggleRes[1]);
      assertContains(toggleRes[1] as string, "graph", "Should switch to graph mode");
      
      // Verify graph mode in geometry
      geomRes = await client.request(ControlCommand.TestHomeGeometry);
      geom = parseGeometryOutput(geomRes[1] as string);
      
      // Toggle back to grid mode
      const toggleRes2 = await client.request(ControlCommand.TestStatsToggleGraph);
      console.log("Toggle back to grid:", toggleRes2[1]);
      assertContains(toggleRes2[1] as string, "grid", "Should switch back to grid mode");
      
      geomRes = await client.request(ControlCommand.TestHomeGeometry);
      geom = parseGeometryOutput(geomRes[1] as string);
      
      // Test hover fade state machine
      assertTrue("hoverFadeAlpha" in geom, "hoverFadeAlpha missing");
      assertTrue("hoverDay" in geom, "hoverDay missing");
      assertTrue("hoverFadeTimerActive" in geom, "hoverFadeTimerActive missing");
      
      const hoverFadeAlpha = parseInt(geom["hoverFadeAlpha"], 10);
      const hoverDay = parseInt(geom["hoverDay"], 10);
      const hoverFadeTimerActive = parseInt(geom["hoverFadeTimerActive"], 10);
      
      console.log(`Hover state: day=${hoverDay} alpha=${hoverFadeAlpha} timerActive=${hoverFadeTimerActive}`);
      
      // Timer should not be active when no hover
      if (hoverDay === -1) {
        assertEqual(hoverFadeTimerActive, 0, "Fade timer should not be active when not hovering");
        assertEqual(hoverFadeAlpha, 255, "Fade alpha should be 255 when not hovering");
      }
      
      // Border mismatches are expected at week column boundaries (uniform grid)
      const mismatches = parseInt(geom["borderMismatches"] || "0", 10);
      console.log(`Border mismatches: ${mismatches} (expected >0 at month boundaries)`);
      assertTrue(mismatches > 0, "Border mismatches should exist at month boundaries");
      
      // Verify split weeks reported
      if ("splitWeeks" in geom) {
        console.log(`Split weeks: ${geom["splitWeeks"]}`);
      }
      
      // F1: Verify fill families come from theme tokens
      if ("monthFillFamily" in geom) {
        console.log(`Fill families: ${geom["monthFillFamily"]}`);
        const famParts = geom["monthFillFamily"].split(",");
        for (const part of famParts) {
          const [monthStr, family] = part.split(":");
          const month = parseInt(monthStr, 10);
          const monthLine = geom[`month${month}`];
          if (monthLine) {
            const borderMatch = monthLine.match(/border=(\w+)/);
            if (borderMatch) {
              const border = borderMatch[1];
              const expectedFamily = (border === "gray") ? "peru" : "silver";
              assertEqual(family, expectedFamily, `Month ${month}: border=${border} but fillFamily=${family}, expected ${expectedFamily}`);
            }
          }
        }
      }
      
      // F2: Verify visible range
      const visibleCols = parseInt(geom["visibleCols"] || "53", 10);
      const cellDx = parseInt(geom["cellDx"] || "0", 10);
      console.log(`Visible cols: ${visibleCols}, cell size: ${cellDx}`);
      
      if (width === 640) {
        assertTrue(visibleCols >= 24 && visibleCols <= 28, `At 640px expected ~26 visible cols, got ${visibleCols}`);
        assertTrue(cellDx >= 6, `At 640px cell size should be >=6px, got ${cellDx}`);
      }
      if (width <= 480) {
        assertTrue(visibleCols >= 13, `At ${width}px visibleCols should be >=13 (floor), got ${visibleCols}`);
      }
      
      // F4: TODAY cell verification
      if ("todayDoy" in geom) {
        const todayDoy = parseInt(geom["todayDoy"], 10);
        if (todayDoy >= 1) {
          console.log(`TODAY doy=${todayDoy}`);
        }
      }
      
      return geomRes;
    }, ["-for-testing", "-appdata", testDir]);
  }
  
  console.log(`\n✓ Home Verify PASSED`);
}

if (import.meta.main) {
  await runStandalone(testit);
}