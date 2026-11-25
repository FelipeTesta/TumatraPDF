import { withControlledSumatra, ControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  // Test at 3 widths: 1200, 900, 640
  for (const width of [1200, 900, 640]) {
    console.log(`\n=== Testing at ${width}px width ===`);
    
    await withControlledSumatra(EXE, async (client) => {
      const res = await client.request(ControlCommand.TestHomeGeometry);
      console.log(JSON.stringify(res, null, 2));
      return res;
    }, ["-for-testing", "-appdata", "C:\\Users\\Testa\\AppData\\Local\\Temp\\tumatra-test-empty"]);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}