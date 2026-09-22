// Repro ETA: liga autoscroll, observa log [as-eta] ao atravessar páginas.
import { launchSumatra, waitForFrame, findCanvas, sendCommand } from "./win-automation";
import { captureWindowToPng, sleep } from "./winapi";
import { cmdId, tmpPath } from "./util";

export async function testit(): Promise<void> {
    const pdf = "G:\\Meu Drive\\3 Arquivos\\Obsidian\\UNLP\\3 Ano\\Semiologia\\LIVROS_SEMIOLOGIA\\Semiologia Cardiaca - Caino Sanchez (1996).pdf";
    const proc = launchSumatra(["-page", "22", pdf]);
    const frame = await waitForFrame(proc.pid);
    await sleep(4000);
    const canvas = findCanvas(frame);

    // ups o vel primeiro (aumentar multiplicador) e liga autoscroll
    for (let i = 0; i < 10; i++) {
        sendCommand(frame, cmdId("CmdAutoScrollSpeedUp"));
        await sleep(60);
    }
    await sleep(300);
    sendCommand(frame, cmdId("CmdAutoScrollToggle"));
    // deixa rolar ~150s para observar várias transições
    for (let i = 0; i < 15; i++) {
        await sleep(10000);
        captureWindowToPng(canvas, tmpPath(`as-${i}.png`));
    }
    proc.kill();
}

if (import.meta.main) {
    await testit();
}
