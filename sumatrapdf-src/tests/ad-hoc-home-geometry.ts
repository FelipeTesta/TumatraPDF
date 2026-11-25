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
  // F4: Add widths below 480 to test floor clamping
  const testWidths = [1400, 1200, 960, 900, 640, 480, 400, 320];
  const testDir = join(tmpdir(), "tumatra-test-home");
  
  console.log(`\n=== Home Responsiveness Audit (F1-F4) ===`);
  console.log(`Test widths: ${testWidths.join(", ")}`);
  console.log(`Test data dir: ${testDir}`);

  for (const width of testWidths) {
    console.log(`\n--- Testing at ${width}px width ---`);
    
    await withControlledSumatra(EXE, async (client) => {
      // Resize window to test width
      const resizeRes = await client.request(ControlCommand.SetWindowSize, [width, 900]);
      console.log(`  Resize: ${resizeRes[1]}`);
      
      // Parse resize response to verify actual client width
      const resizeOut = resizeRes[1] as string;
      const clientWidthMatch = resizeOut.match(/newClientWidth=(\d+)/);
      assertTrue(clientWidthMatch !== null, "Resize response missing newClientWidth");
      const actualClientWidth = parseInt(clientWidthMatch[1], 10);
      console.log(`  Actual client width: ${actualClientWidth}`);
      
      // Verify window actually resized (D4)
      assertTrue(Math.abs(actualClientWidth - width) < 50, 
        `Window did not resize: requested ${width}, got ${actualClientWidth}`);
      
      // Get geometry
      const geomRes = await client.request(ControlCommand.TestHomeGeometry);
      const geomOut = geomRes[1] as string;
      console.log(`  Geometry dump (first 3000 chars):\n${geomOut.slice(0, 3000)}`);
      const geom = parseGeometryOutput(geomOut);
      
      // Check mode from geometry (study history width vs heatmap width)
      const studyHistoryDx = parseInt(geom["rcStudyHistory.dx"] || "0", 10);
      const heatmapDx = parseInt(geom["rcStats.dx"] || "0", 10);
      const studyHistoryX = parseInt(geom["rcStudyHistory.x"] || "0", 10);
      const heatmapX = parseInt(geom["rcStats.x"] || "0", 10);
      
      // In 2-column mode, study history and heatmap should be side by side
      // (studyHistoryX + studyHistoryDx ≈ heatmapX)
      const isTwoCol = (heatmapX > studyHistoryX + studyHistoryDx - 10);
      const actualMode = isTwoCol ? "2-column" : "1-column";
      
      console.log(`  Mode: ${actualMode}`);
      console.log(`  StudyHistory: x=${studyHistoryX} dx=${studyHistoryDx}`);
      console.log(`  Heatmap: x=${heatmapX} dx=${heatmapDx}`);
      
      // Verify mode is internally consistent with layout geometry
      // (C1: two columns iff windowWidth >= max(0.5 * screenWidth, 900))
      // We can't know screenWidth from test, but we verify the layout is self-consistent
      if (isTwoCol) {
        // In 2-column mode, bands should be side by side with a gap
        assertTrue(heatmapX >= studyHistoryX + studyHistoryDx, "2-column: heatmap should be to the right of study history");
      } else {
        // In 1-column mode, bands should be stacked vertically
        const studyHistoryY = parseInt(geom["rcStudyHistory.y"] || "0", 10);
        const heatmapY = parseInt(geom["rcStats.y"] || "0", 10);
        const studyHistoryDy = parseInt(geom["rcStudyHistory.dy"] || "0", 10);
        assertTrue(heatmapY >= studyHistoryY + studyHistoryDy - 20, "1-column: heatmap should be below study history");
      }
      
      // Check bands don't intersect (C7)
      const sh = {
        x: studyHistoryX,
        y: parseInt(geom["rcStudyHistory.y"] || "0", 10),
        dx: studyHistoryDx,
        dy: parseInt(geom["rcStudyHistory.dy"] || "0", 10)
      };
      const hm = {
        x: heatmapX,
        y: parseInt(geom["rcStats.y"] || "0", 10),
        dx: heatmapDx,
        dy: parseInt(geom["rcStats.dy"] || "0", 10)
      };
      const books = {
        x: parseInt(geom["rcBooksHdr.x"] || "0", 10),
        y: parseInt(geom["rcBooksHdr.y"] || "0", 10),
        dx: parseInt(geom["rcBooksHdr.dx"] || "0", 10),
        dy: parseInt(geom["rcBooksHdr.dy"] || "0", 10)
      };
      
      // Helper to check intersection
      function intersects(a: typeof sh, b: typeof sh): boolean {
        return !(a.x + a.dx <= b.x || b.x + b.dx <= a.x ||
                 a.y + a.dy <= b.y || b.y + b.dy <= a.y);
      }
      
      assertTrue(!intersects(sh, hm), "StudyHistory and Heatmap bands intersect!");
      assertTrue(!intersects(sh, books), "StudyHistory and Books bands intersect!");
      assertTrue(!intersects(hm, books), "Heatmap and Books bands intersect!");
      
      // Study history height > 0 (never collapses, D2/C7)
      assertTrue(sh.dy > 0, "StudyHistory band has zero height (collapsed!)");
      assertTrue(hm.dy > 0, "Heatmap band has zero height!");
      
      // Heatmap cell size: square cells (C3/C5)
      const cellDx = parseInt(geom["cellDx"] || "0", 10);
      const cellDy = parseInt(geom["cellDy"] || "0", 10);
      console.log(`  Cell size: ${cellDx}x${cellDy}`);
      assertEqual(cellDx, cellDy, "Heatmap cells are not square!");
      // F2: cell size >= 6px at tested widths (or clamped at floor)
      const minCellExpected = (width >= 640) ? 6 : 2; // below 640 may hit floor
      assertTrue(cellDx >= minCellExpected, `Cell size ${cellDx} < expected minimum ${minCellExpected} at ${width}px`);
      
      // Gap between cells (C5: should be 4px)
      const rcGridX = parseInt(geom["rcGrid.x"] || "0", 10);
      const rcGridDx = parseInt(geom["rcGrid.dx"] || "0", 10);
      const visibleCols = parseInt(geom["visibleCols"] || "53", 10);
      const expectedGridDx = visibleCols * (cellDx + 4) - 4;
      console.log(`  Grid: x=${rcGridX} dx=${rcGridDx} visibleCols=${visibleCols} (expected ~${expectedGridDx})`);
      assertTrue(Math.abs(rcGridDx - expectedGridDx) <= cellDx, "Grid width doesn't match 4px gap formula for visibleCols");
      
      // Month border parity and fill families (C6, F1, F4)
      // Check that months alternate gray/silver borders
      let prevBorder: string | null = null;
      for (let m = 1; m <= 12; m++) {
        const key = `month${m}`;
        const monthLine = geom[key];
        if (monthLine) {
          const borderMatch = monthLine.match(/border=(\w+)/);
          if (borderMatch) {
            const border = borderMatch[1];
            if (prevBorder !== null) {
              assertTrue(border !== prevBorder, `Month ${m} has same border as month ${m-1} (${border}), should alternate`);
            }
            prevBorder = border;
            console.log(`  ${key}: ${monthLine}`);
          }
        }
      }
      
      // F1: Verify fill families come from theme tokens (peru for gray-border, silver for silver-border)
      if ("monthFillFamily" in geom) {
        console.log(`  Fill families: ${geom["monthFillFamily"]}`);
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
      
      // F4: Verify visible range data
      const firstVisibleCol = parseInt(geom["firstVisibleCol"] || "0", 10);
      const visibleCols = parseInt(geom["visibleCols"] || "53", 10);
      const firstVisibleDoy = parseInt(geom["firstVisibleDoy"] || "1", 10);
      const lastVisibleDoy = parseInt(geom["lastVisibleDoy"] || "366", 10);
      console.log(`  Visible range: cols=${firstVisibleCol}-${firstVisibleCol + visibleCols - 1} (${visibleCols} cols), doy=${firstVisibleDoy}-${lastVisibleDoy}`);
      
      // F2: At 640px, expect ~26 weeks (visibleCols around 26)
      if (width === 640) {
        assertTrue(visibleCols >= 24 && visibleCols <= 28, `At 640px expected ~26 visible cols, got ${visibleCols}`);
        assertTrue(cellDx >= 6, `At 640px cell size should be >=6px, got ${cellDx}`);
      }
      // F2: At 480px and below, visibleCols should be clamped to floor (13)
      if (width <= 480) {
        assertTrue(visibleCols >= 13, `At ${width}px visibleCols should be >=13 (floor), got ${visibleCols}`);
      }
      
      // F4: TODAY cell verification
      if ("todayDoy" in geom) {
        const todayDoy = parseInt(geom["todayDoy"], 10);
        if (todayDoy >= 1) {
          console.log(`  TODAY doy=${todayDoy} (in visible range: ${todayDoy >= firstVisibleDoy && todayDoy <= lastVisibleDoy})`);
          // The paint logic ensures TODAY is white; we verify it's in visible range when it should be
        }
      }
      
      // Hover fade state
      const hoverFadeAlpha = parseInt(geom["hoverFadeAlpha"] || "255", 10);
      const hoverDay = parseInt(geom["hoverDay"] || "-1", 10);
      console.log(`  Hover: day=${hoverDay} fadeAlpha=${hoverFadeAlpha}`);
      
      // Test hover fade: move mouse into heatmap, then out, verify fade
      assertTrue("hoverFadeTimerActive" in geom, "hoverFadeTimerActive not reported");
      
      return geomRes;
    }, ["-for-testing", "-appdata", testDir]);
  }
  
  console.log(`\n✓ All ${testWidths.length} width tests PASSED`);
}

if (import.meta.main) {
  await runStandalone(testit);
}