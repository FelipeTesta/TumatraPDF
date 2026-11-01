import { launchSumatra, waitForFrame, sleep } from "./win-automation";
import { isWindowVisible, getWindowRect, getWindowLong, getWindowThreadProcessId, sleep as sleepApi } from "./winapi";
import { runStandalone } from "./util";

const GWL_STYLE = -16;
const GWL_EXSTYLE = -20;
const GWLP_HWNDPARENT = -8;

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame: ${frame}`);
  console.log(`Frame rect:`, getWindowRect(frame));
  console.log(`Frame visible:`, isWindowVisible(frame));
  
  const style = getWindowLong(frame, GWL_STYLE);
  const exStyle = getWindowLong(frame, GWL_EXSTYLE);
  console.log(`Frame style: 0x${style.toString(16)} (${style})`);
  console.log(`Frame exStyle: 0x${exStyle.toString(16)} (${exStyle})`);
  
  // Check if it's a child window
  const isChild = (style & 0x40000000) !== 0; // WS_CHILD
  const isPopup = (style & 0x80000000) !== 0; // WS_POPUP
  const isVisible = (style & 0x10000000) !== 0; // WS_VISIBLE
  const isOverlapped = (style & 0x00000000) !== 0; // WS_OVERLAPPED
  console.log(`WS_CHILD: ${isChild}`);
  console.log(`WS_POPUP: ${isPopup}`);
  console.log(`WS_VISIBLE: ${isVisible}`);
  console.log(`WS_OVERLAPPED: ${isOverlapped}`);
  
  // Get parent
  const parent = getWindowLong(frame, GWLP_HWNDPARENT);
  console.log(`Parent hwnd: ${parent}`);
  
  if (parent !== 0) {
    const parentRect = getWindowRect(parent);
    console.log(`Parent rect:`, parentRect);
    console.log(`Parent visible:`, isWindowVisible(parent));
  }
  
  // Get process ID
  const pid = getWindowThreadProcessId(frame);
  console.log(`Frame PID: ${pid}`);
  console.log(`Proc PID: ${proc.pid}`);
  
  await sleepApi(3000);
  
  proc.kill();
}

if (import.meta.main) {
  await runStandalone(testit);
}