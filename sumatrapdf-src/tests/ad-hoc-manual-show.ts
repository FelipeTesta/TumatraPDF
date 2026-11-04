import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import { isWindowVisible, getWindowRect, showWindow, SW_RESTORE, sleep } from "./winapi";
import { runStandalone } from "./util";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame: ${frame}`);
  console.log(`Initial Frame rect:`, getWindowRect(frame));
  console.log(`Initial Frame visible:`, isWindowVisible(frame));
  
  await sleep(5000);
  console.log(`After 5s - Frame visible:`, isWindowVisible(frame));
  console.log(`After 5s - Frame rect:`, getWindowRect(frame));
  
  // Try to manually show the window
  console.log(`\n>>> Manually calling showWindow(5 = SW_SHOW)...`);
  showWindow(frame, 5);
  await sleep(1000);
  console.log(`After ShowWindow - Frame visible:`, isWindowVisible(frame));
  console.log(`After ShowWindow - Frame rect:`, getWindowRect(frame));
  
  proc.kill();
}

if (import.meta.main) {
  await runStandalone(testit);
}