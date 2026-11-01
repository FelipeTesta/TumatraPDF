import { launchSumatra, waitForFrame } from "./win-automation";
import { isWindowVisible, getWindowRect, sleep } from "./winapi";
import { runStandalone } from "./util";

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame: ${frame}`);
  console.log(`t=0: Frame visible:`, isWindowVisible(frame), "rect:", getWindowRect(frame));
  
  for (let i = 1; i <= 10; i++) {
    await sleep(500);
    console.log(`t=${i * 0.5}s: Frame visible:`, isWindowVisible(frame), "rect:", getWindowRect(frame));
  }
  
  proc.kill();
}

if (import.meta.main) {
  await runStandalone(testit);
}