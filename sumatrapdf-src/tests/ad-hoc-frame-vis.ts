import { launchSumatra, waitForFrame } from "./win-automation";
import { isWindowVisible, getWindowRect, sleep } from "./winapi";
import { runStandalone } from "./util";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame: ${frame}`);
  console.log(`Frame rect:`, getWindowRect(frame));
  console.log(`Frame visible:`, isWindowVisible(frame));
  
  await sleep(3000);
  console.log(`After sleep - Frame visible:`, isWindowVisible(frame));
  console.log(`After sleep - Frame rect:`, getWindowRect(frame));
  
  proc.kill();
}

if (import.meta.main) {
  await runStandalone(testit);
}