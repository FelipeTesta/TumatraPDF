import { launchSumatra } from "./win-automation";
import { waitForFrame } from "./win-automation";
import { getClassName, getWindowRect, sleep, getWindow } from "./winapi";
import { runStandalone } from "./util";

const GW_CHILD = 5;
const GW_HWNDNEXT = 2;

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const pid = proc.pid;
  console.log(`PID: ${pid}`);
  
  const fhwnd = await waitForFrame(pid);
  console.log(`Frame HWND: ${fhwnd}`);
  
  if (fhwnd) {
    const cls = getClassName(fhwnd);
    console.log(`Frame class: "${cls}"`);
    const r = getWindowRect(fhwnd);
    console.log(`Frame rect: (${r.left},${r.top},${r.right},${r.bottom})`);
    
    // Walk children using GetWindow
    let child = getWindow(fhwnd, GW_CHILD);
    let idx = 0;
    while (child && idx < 50) {
      const ccls = getClassName(child);
      const cr = getWindowRect(child);
      console.log(`  Child ${idx}: hwnd=0x${child.toString(16)} class="${ccls}" rect=(${cr.left},${cr.top},${cr.right},${cr.bottom})`);
      child = getWindow(child, GW_HWNDNEXT);
      idx++;
    }
    if (idx >= 50) console.log("  ... (stopped at 50 children)");
  }

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
