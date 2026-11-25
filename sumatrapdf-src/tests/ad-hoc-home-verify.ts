import { withControlledSumatra, ControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  console.log("\n=== Testing Home Verify ===");
  
  await withControlledSumatra(EXE, async (client) => {
    // First get initial geometry
    let res = await client.request(ControlCommand.TestHomeGeometry);
    console.log("Initial:", res[1]);
    
    // Verify the graph toggle works
    const toggleRes = await client.request(ControlCommand.TestStatsToggleGraph);
    console.log("After toggle to graph:", toggleRes[1]);
    
    // Toggle back
    const toggleRes2 = await client.request(ControlCommand.TestStatsToggleGraph);
    console.log("After toggle back to grid:", toggleRes2[1]);
    
    return res;
  }, ["-for-testing", "-appdata", "C:\\Users\\Testa\\AppData\\Local\\Temp\\tumatra-test-empty"]);
}

if (import.meta.main) {
  await runStandalone(testit);
}