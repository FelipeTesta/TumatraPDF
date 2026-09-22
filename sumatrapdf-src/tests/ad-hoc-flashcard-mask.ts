// Debug visual: abre PDF, seleciona texto via mouse drag, cria cloze (S), ativa study, screenshots.
import { launchSumatra, waitForFrame, findCanvas, sendCommand } from "./win-automation";
import { captureWindowToPng, sleep, postMessage, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE, WM_KEYDOWN, packCoords } from "./winapi";
import { cmdId, tmpPath } from "./util";

export async function testit(): Promise<void> {
    const pdf = "G:\\Meu Drive\\3 Arquivos\\Obsidian\\UNLP\\3 Ano\\Semiologia\\LIVROS_SEMIOLOGIA\\Semiologia Cardiaca - Caino Sanchez (1996).pdf";
    const proc = launchSumatra(["-page", "22", pdf]);
    const frame = await waitForFrame(proc.pid);
    await sleep(4000);
    const canvas = findCanvas(frame);

    // drag-select um trecho de texto visível
    postMessage(canvas, WM_LBUTTONDOWN, 1, packCoords(120, 430));
    await sleep(100);
    for (let x = 130; x <= 380; x += 25) {
        postMessage(canvas, WM_MOUSEMOVE, 1, packCoords(x, 430));
        await sleep(30);
    }
    postMessage(canvas, WM_LBUTTONUP, 0, packCoords(380, 430));
    await sleep(800);
    captureWindowToPng(canvas, tmpPath("fc-sel.png"));

    // flashcards ON, depois 'S' cria cloze
    sendCommand(frame, cmdId("CmdFlashcardToggle"));
    await sleep(1200);
    postMessage(frame, WM_KEYDOWN, 0x53, 0); // 'S'
    await sleep(1200);
    captureWindowToPng(canvas, tmpPath("fc-added.png"));

    sendCommand(frame, cmdId("CmdFlashcardStudy"));
    await sleep(1200);
    captureWindowToPng(canvas, tmpPath("fc-study.png"));

    sendCommand(frame, cmdId("CmdFlashcardReveal"));
    await sleep(1200);
    captureWindowToPng(canvas, tmpPath("fc-reveal.png"));

    proc.kill();
}

if (import.meta.main) {
    await testit();
}
