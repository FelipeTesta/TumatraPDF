import { launchSumatra } from "./win-automation";
import { waitForFrame } from "./win-automation";
import { getClassName, getWindowRect, sleep, getWindow, sendMessage } from "./winapi";
import { runStandalone } from "./util";

const GW_CHILD = 5;
const GW_HWNDNEXT = 2;
const WM_USER = 0x0400;
const TB_BUTTONCOUNT_MSG = WM_USER + 0x1A;

function tbButtonCount(hwnd: number): number {
  const res = sendMessage(hwnd, TB_BUTTONCOUNT_MSG, 0n, 0n);
  return Number(res);
}

export async function testit(): Promise<void> {
  const pdf = "ext/a-zlib/zlib.3.pdf";
  const proc = launchSumatra([pdf]);
  const pid = proc.pid;
  
  const fhwnd = await waitForFrame(pid);
  console.log(`Frame HWND: ${fhwnd}`);
  
  if (fhwnd) {
    // Walk frame children
    console.log("\n=== Frame children ===");
    let child = getWindow(fhwnd, GW_CHILD);
    let idx = 0;
    while (child && idx < 50) {
      const ccls = getClassName(child);
      const cr = getWindowRect(child);
      const count = tbButtonCount(child);
      console.log(`  Child ${idx}: hwnd=0x${child.toString(16)} class="${ccls}" rect=(${cr.left},${cr.top},${cr.right},${cr.bottom}) btnCount=${count}`);
      
      // Walk grandchildren
      let grandchild = getWindow(child, GW_CHILD);
      let gidx = 0;
      while (grandchild && gidx < 20) {
        const gcls = getClassName(grandchild);
        const gr = getWindowRect(grandchild);
        const gcount = tbButtonCount(grandchild);
        console.log(`    Grandchild ${gidx}: hwnd=0x${grandchild.toString(16)} class="${gcls}" rect=(${gr.left},${gr.top},${gr.right},${gr.bottom}) btnCount=${gcount}`);
        grandchild = getWindow(grandchild, GW_HWNDNEXT);
        gidx++;
      }
      
      child = getWindow(child, GW_HWNDNEXT);
      idx++;
    }
  }

  proc.kill();
  console.log("\nDone.");
}

if (import.meta.main) {
  await runStandalone(testit);
}
