// ad-hoc E2E: update dialog button visibility probe
// Fires TestShowUpdateDialog (modal), then from the SAME process enumerates the dialog's
// child windows and asserts the two buttons exist, are visible, enabled, have non-empty
// rects INSIDE the dialog's client rect, and have the expected texts.
// THEN asserts PIXELS: each button's rect must contain non-background colors (painted).
// Then posts BM_CLICK to the Install button and asserts the dialog closes (pipe answers).

import { EXE, runStandalone, cmdId, tmpPath } from "./util";
import { ControlClient, ControlCommand, withControlledSumatra, uniquePipeName } from "../cmd/control";
import {
  enumWindows,
  enumChildWindows,
  getClassName,
  getWindowThreadProcessId,
  getClientRect,
  getWindowRect,
  getWindowText,
  getControlText,
  getWindowLong,
  isWindowVisible,
  postMessage,
  captureWindowToPng,
  captureWindowPixels,
  BM_CLICK,
} from "./winapi";

const WS_DISABLED = 0x08000000;
const GWL_STYLE = -16;

// Assert that a rect (in window coordinates) contains non-background pixels.
// We capture the dialog's CLIENT area pixels (captureWindowPixels uses getClientRect internally)
// and sample the button rects converted to client coordinates using frame offsets.
function assertButtonPainted(
  dialogHwnd: number,
  dialogWindowRect: { left: number; top: number; right: number; bottom: number },
  btnRect: { left: number; top: number; right: number; bottom: number },
  btnName: string,
): void {
  // Capture dialog client area pixels (captureWindowPixels uses getClientRect internally)
  const pixels = captureWindowPixels(dialogHwnd);
  if (!pixels) {
    throw new Error(`${btnName}: failed to capture dialog client pixels`);
  }
  const { w, h, data } = pixels;

  // Get dialog's actual client rect (always 0,0,clientW,clientH)
  const dialogClientRect = getClientRect(dialogHwnd);
  const clientW = dialogClientRect.right;
  const clientH = dialogClientRect.bottom;

  // Frame offsets: window -> client
  const winW = dialogWindowRect.right - dialogWindowRect.left;
  const winH = dialogWindowRect.bottom - dialogWindowRect.top;
  const frameX = (winW - clientW) / 2;
  const frameY = winH - clientH - frameX; // caption + top frame

  // Button rect is in WINDOW/SCREEN coordinates. Convert to CLIENT coordinates:
  const clientLeft = btnRect.left - dialogWindowRect.left - frameX;
  const clientTop = btnRect.top - dialogWindowRect.top - frameY;
  const clientRight = btnRect.right - dialogWindowRect.left - frameX;
  const clientBottom = btnRect.bottom - dialogWindowRect.top - frameY;

  // Validate converted coords are inside captured image
  if (clientLeft < 0 || clientTop < 0 || clientRight > w || clientBottom > h) {
    throw new Error(`${btnName}: converted button rect (${clientLeft},${clientTop},${clientRight},${clientBottom}) outside client image (${w}x${h})`);
  }

  // Sample multiple points in the button rect
  const samplePoints: [number, number][] = [
    [clientLeft + 2, clientTop + 2],
    [clientLeft + 2, clientBottom - 2],
    [clientRight - 2, clientTop + 2],
    [clientRight - 2, clientBottom - 2],
    [(clientLeft + clientRight) / 2, (clientTop + clientBottom) / 2],
  ];

  const colors = new Set<number>();
  for (const [x, y] of samplePoints) {
    if (x >= 0 && x < w && y >= 0 && y < h) {
      const idx = (y * w + x) * 4;
      const b = data[idx];
      const g = data[idx + 1];
      const r = data[idx + 2];
      const a = data[idx + 3];
      if (a > 0) {
        colors.add((r << 16) | (g << 8) | b);
      }
    }
  }

  // Must have at least 2 distinct colors (button face + border/text) or distinct from background
  if (colors.size < 2) {
    // Sample dialog background (top-left corner of client area)
    const bgIdx = 0;
    const bgB = data[bgIdx];
    const bgG = data[bgIdx + 1];
    const bgR = data[bgIdx + 2];
    const bgColor = (bgR << 16) | (bgG << 8) | bgB;

    // Check if all sampled colors are just the background
    const allBackground = Array.from(colors).every(c => c === bgColor);
    if (allBackground || colors.size === 0) {
      throw new Error(`${btnName}: button rect appears unpainted (uniform background, ${colors.size} colors sampled, bg=#${bgColor.toString(16).padStart(6, '0')})`);
    }
  }

  console.log(`[probe] ${btnName}: PAINTED (${colors.size} distinct colors sampled, bg=#${((data[2]<<16)|(data[1]<<8)|data[0]).toString(16).padStart(6,'0')})`);
}

async function sleep(ms: number): Promise<void> {
  await new Promise((r) => setTimeout(r, ms));
}

interface ChildInfo {
  hwnd: number;
  className: string;
  text: string;
  rect: { left: number; top: number; right: number; bottom: number };
  visible: boolean;
  disabled: boolean;
  insideClient: boolean;
}

export async function testit(): Promise<void> {
  const pipeName = uniquePipeName("upd-dialog-probe");
  const proc = Bun.spawn([EXE, "-for-testing", "-dbg-control", pipeName, "-console"], {
    stdout: "pipe",
    stderr: "pipe",
  });

  let client: ControlClient | undefined;
  try {
    client = await ControlClient.connect(pipeName, 10000);
    console.log("[probe] connected to control pipe");

    // Fire the command WITHOUT awaiting - the dialog is modal and runs its own pump
    // so the request won't return until the dialog closes.
    // We use a promise that we DON'T await immediately.
    const requestPromise = client.request(ControlCommand.TestShowUpdateDialog, [1]); // installerExists = 1

    // Give the dialog time to appear
    await sleep(500);

    // Find the update dialog window (class TUMATRA_UPDATE_DIALOG)
    let dlgHwnd = 0;
    const pid = proc.pid;
    enumWindows((hwnd) => {
      if (getWindowThreadProcessId(hwnd) === pid && getClassName(hwnd) === "TUMATRA_UPDATE_DIALOG") {
        dlgHwnd = hwnd;
        return false;
      }
      return true;
    });

    if (!dlgHwnd) {
      // Dump all windows for debugging
      const all: string[] = [];
      enumWindows((hwnd) => {
        if (getWindowThreadProcessId(hwnd) === pid) {
          all.push(`hwnd=${hwnd} class=${getClassName(hwnd)} text="${getWindowText(hwnd)}"`);
        }
        return true;
      });
      console.error("[probe] Dialog not found. All windows:", all.join(" | "));
      throw new Error("TUMATRA_UPDATE_DIALOG window not found");
    }

    console.log("[probe] Found dialog hwnd:", dlgHwnd);

    // Get dialog client rect
    const dlgClientRect = getClientRect(dlgHwnd);
    const dlgWindowRect = getWindowRect(dlgHwnd);
    console.log("[probe] Dialog client rect:", dlgClientRect);
    console.log("[probe] Dialog window rect:", dlgWindowRect);

    // Capture dialog to PNG
    const pngPath = tmpPath("update-dialog-probe.png");
    captureWindowToPng(dlgHwnd, pngPath);
    console.log("[probe] Dialog captured to:", pngPath);

    // Enumerate child windows
    const children: ChildInfo[] = [];
    // Pre-compute frame offsets for window->client conversion
    const winW = dlgWindowRect.right - dlgWindowRect.left;
    const winH = dlgWindowRect.bottom - dlgWindowRect.top;
    const frameX = (winW - dlgClientRect.right) / 2;
    const frameY = winH - dlgClientRect.bottom - frameX;

    enumChildWindows(dlgHwnd, (hwnd) => {
      const cls = getClassName(hwnd);
      const txt = getControlText(hwnd);
      const rc = getWindowRect(hwnd);
      const visible = isWindowVisible(hwnd);
      // Use getWindowLong from winapi.ts (which uses GetWindowLongPtrW internally)
      const style = getWindowLong(hwnd, GWL_STYLE);
      const disabled = (style & WS_DISABLED) !== 0;

      // Convert child window rect (screen coords) to dialog client coords
      const clientLeft = rc.left - dlgWindowRect.left - frameX;
      const clientTop = rc.top - dlgWindowRect.top - frameY;
      const clientRight = rc.right - dlgWindowRect.left - frameX;
      const clientBottom = rc.bottom - dlgWindowRect.top - frameY;

      // Check if child rect is inside dialog client rect (in client coordinates)
      const insideClient =
        clientLeft >= 0 &&
        clientTop >= 0 &&
        clientRight <= dlgClientRect.right &&
        clientBottom <= dlgClientRect.bottom;

      children.push({
        hwnd,
        className: cls,
        text: txt,
        rect: rc,
        clientRect: { left: clientLeft, top: clientTop, right: clientRight, bottom: clientBottom },
        visible,
        disabled,
        insideClient,
      });
      return true;
    });

    console.log("[probe] Child windows:");
    for (const c of children) {
      console.log(
        `  hwnd=${c.hwnd} class=${c.className} text="${c.text}" rect=(${c.rect.left},${c.rect.top},${c.rect.right},${c.rect.bottom}) ` +
          `visible=${c.visible} disabled=${c.disabled} inside=${c.insideClient}`,
      );
    }

    // Find the two buttons
    const buttons = children.filter(
      (c) => c.className === "Button" || c.className.toLowerCase().includes("button"),
    );
    console.log("[probe] Button-like children:", buttons.length);

    // Assertions
    if (buttons.length < 2) {
      throw new Error(`Expected at least 2 buttons, found ${buttons.length}`);
    }

    // Check each button
    let dontInstallBtn: ChildInfo | null = null;
    let installBtn: ChildInfo | null = null;

    for (const btn of buttons) {
      if (btn.text.includes("Don't install") || btn.text.includes("Não instalar")) {
        dontInstallBtn = btn;
      } else if (
        btn.text.includes("Install and relaunch") ||
        btn.text.includes("Instalar e reiniciar") ||
        btn.text.includes("Download update") ||
        btn.text.includes("Baixar atualização")
      ) {
        installBtn = btn;
      }
    }

    if (!dontInstallBtn) {
      throw new Error("'Don't install' button not found");
    }
    if (!installBtn) {
      throw new Error("'Install and relaunch' / 'Download update' button not found");
    }

    console.log("[probe] 'Don't install' button:", dontInstallBtn);
    console.log("[probe] 'Install' button:", installBtn);

    // Assert visibility, enabled, non-empty rect, inside client
    for (const [name, btn] of [
      ["Don't install", dontInstallBtn],
      ["Install", installBtn],
    ] as const) {
      if (!btn!.visible) {
        throw new Error(`${name} button is not visible`);
      }
      if (btn!.disabled) {
        throw new Error(`${name} button is disabled`);
      }
      const { left, top, right, bottom } = btn!.rect;
      if (right <= left || bottom <= top) {
        throw new Error(`${name} button has empty/zero rect: (${left},${top},${right},${bottom})`);
      }
      if (!btn!.insideClient) {
        throw new Error(`${name} button rect is outside dialog client area`);
      }
    }

    console.log("[probe] All button assertions PASSED");

    // Verify buttons are fully inside client area (layout fix - buttons must not be clipped)
    for (const [name, btn] of [
      ["Don't install", dontInstallBtn!],
      ["Install", installBtn!],
    ] as const) {
      const clientRect = btn.clientRect!;
      if (clientRect.bottom > dlgClientRect.bottom) {
        throw new Error(`${name} button bottom (${clientRect.bottom}) exceeds client height (${dlgClientRect.bottom}) - CLIPPED`);
      }
      if (clientRect.top < 0) {
        throw new Error(`${name} button top (${clientRect.top}) < 0 - CLIPPED`);
      }
      if (clientRect.right > dlgClientRect.right || clientRect.left < 0) {
        throw new Error(`${name} button horizontally outside client area`);
      }
      console.log(`[probe] ${name} button LAYOUT OK: client rect=(${clientRect.left},${clientRect.top},${clientRect.right},${clientRect.bottom}) inside client (0,0,${dlgClientRect.right},${dlgClientRect.bottom})`);
    }

    // Post BM_CLICK to the Install button
    console.log("[probe] Posting BM_CLICK to Install button...");
    postMessage(installBtn!.hwnd, BM_CLICK, 0, 0);

    const result = await Promise.race([
      requestPromise,
      new Promise<never>((_, reject) => setTimeout(() => reject(new Error("control pipe timeout after 15s")), 15000)),
    ]);
    console.log("[probe] Control pipe response:", result);

    // The response should be [exitCode, resultString]
    const exitCode = result[0] as number;
    const resultStr = result[1] as string;
    console.log("[probe] Exit code:", exitCode, "Result:", resultStr);

    if (exitCode !== 101) {
      throw new Error(`Expected exit code 101 (Install), got ${exitCode}: ${resultStr}`);
    }

    console.log("[probe] SUCCESS: Dialog closed with Install button (101)");
  } finally {
    if (client) {
      try {
        await Promise.race([
          client.quit(),
          new Promise((_, reject) => setTimeout(() => reject(new Error("quit timeout")), 5000)),
        ]);
      } catch {}
      client.close();
    } else {
      proc.kill();
    }
    // Wait for process to exit
    await Promise.race([
      proc.exited,
      new Promise((resolve) => setTimeout(resolve, 5000)),
    ]);
  }
}

// Run both variants: installerExists=0 ("Download update") and installerExists=1 ("Install and relaunch")
async function runBothVariants() {
  const variants = [
    { installerExists: 0, installText: "Download update" },
    { installerExists: 1, installText: "Install and relaunch" },
  ];

  for (const variant of variants) {
    console.log(`\n[probe] === Testing variant: installerExists=${variant.installerExists} (${variant.installText}) ===`);
    const pipeName = uniquePipeName(`upd-dialog-probe-${variant.installerExists}`);
    const proc = Bun.spawn([EXE, "-for-testing", "-dbg-control", pipeName, "-console"], {
      stdout: "pipe",
      stderr: "pipe",
    });

    let client: ControlClient | undefined;
    try {
      client = await Promise.race([
        ControlClient.connect(pipeName, 10000),
        new Promise<never>((_, reject) => setTimeout(() => reject(new Error("connect timeout after 10s")), 10000)),
      ]);
      console.log("[probe] connected to control pipe");

      const requestPromise = client.request(ControlCommand.TestShowUpdateDialog, [variant.installerExists]);

      await sleep(500);

      // Children array for both dialog instances
      const children: ChildInfo[] = [];

      // Find the update dialog window (class TUMATRA_UPDATE_DIALOG)
      let dlgHwnd = 0;
      const pid = proc.pid;
      enumWindows((hwnd) => {
        if (getWindowThreadProcessId(hwnd) === pid && getClassName(hwnd) === "TUMATRA_UPDATE_DIALOG") {
          dlgHwnd = hwnd;
          return false;
        }
        return true;
      });

      if (!dlgHwnd) {
        throw new Error("TUMATRA_UPDATE_DIALOG window not found");
      }

      console.log("[probe] Found dialog hwnd:", dlgHwnd);

      const dlgClientRect = getClientRect(dlgHwnd);
      const dlgWindowRect = getWindowRect(dlgHwnd);
      console.log("[probe] Dialog client rect:", dlgClientRect);
      console.log("[probe] Dialog window rect:", dlgWindowRect);

      const pngPath = tmpPath(`update-dialog-probe-${variant.installerExists}.png`);
      captureWindowToPng(dlgHwnd, pngPath);
      console.log("[probe] Dialog captured to:", pngPath);

      // Wait for dialog to be fully created and ready
      await sleep(500);

      // Find dialog again (it was recreated after first request)
      let dlgHwnd2 = 0;
      enumWindows((hwnd) => {
        if (getWindowThreadProcessId(hwnd) === pid && getClassName(hwnd) === "TUMATRA_UPDATE_DIALOG") {
          dlgHwnd2 = hwnd;
          return false;
        }
        return true;
      });

      if (!dlgHwnd2) throw new Error("Dialog not found on second attempt");

      const dlgClientRect2 = getClientRect(dlgHwnd2);
      const dlgWindowRect2 = getWindowRect(dlgHwnd2);

      const pngPath2 = tmpPath(`update-dialog-probe-${variant.installerExists}-final.png`);
      captureWindowToPng(dlgHwnd2, pngPath2);
      const winW2 = dlgWindowRect2.right - dlgWindowRect2.left;
      const winH2 = dlgWindowRect2.bottom - dlgWindowRect2.top;
      const frameX2 = (winW2 - dlgClientRect2.right) / 2;
      const frameY2 = winH2 - dlgClientRect2.bottom - frameX2;

      enumChildWindows(dlgHwnd2, (hwnd) => {
        const cls = getClassName(hwnd);
        const txt = getControlText(hwnd);
        const rc = getWindowRect(hwnd);
        const visible = isWindowVisible(hwnd);
        const style = getWindowLong(hwnd, GWL_STYLE);
        const disabled = (style & WS_DISABLED) !== 0;

        const clientLeft = rc.left - dlgWindowRect2.left - frameX2;
        const clientTop = rc.top - dlgWindowRect2.top - frameY2;
        const clientRight = rc.right - dlgWindowRect2.left - frameX2;
        const clientBottom = rc.bottom - dlgWindowRect2.top - frameY2;

        const insideClient =
          clientLeft >= 0 &&
          clientTop >= 0 &&
          clientRight <= dlgClientRect2.right &&
          clientBottom <= dlgClientRect2.bottom;

        children.push({
          hwnd,
          className: cls,
          text: txt,
          rect: rc,
          clientRect: { left: clientLeft, top: clientTop, right: clientRight, bottom: clientBottom },
          visible,
          disabled,
          insideClient,
        });
        return true;
      });

      const buttons = children.filter(
        (c) => c.className === "Button" || c.className.toLowerCase().includes("button"),
      );

      if (buttons.length < 2) throw new Error(`Expected 2 buttons, found ${buttons.length}`);

      let dontInstallBtn: ChildInfo | null = null;
      let installBtn: ChildInfo | null = null;

      for (const btn of buttons) {
        if (btn.text.includes("Don't install") || btn.text.includes("Não instalar")) {
          dontInstallBtn = btn;
        } else if (
          btn.text.includes("Install and relaunch") ||
          btn.text.includes("Instalar e reiniciar") ||
          btn.text.includes("Download update") ||
          btn.text.includes("Baixar atualização")
        ) {
          installBtn = btn;
        }
      }

      if (!dontInstallBtn) throw new Error("'Don't install' button not found");
      if (!installBtn) throw new Error(`'${variant.installText}' button not found`);

      // Assert visibility, enabled, inside client
      for (const [name, btn] of [
        ["Don't install", dontInstallBtn!],
        [variant.installText, installBtn!],
      ] as const) {
        if (!btn.visible) throw new Error(`${name} button not visible`);
        if (btn.disabled) throw new Error(`${name} button disabled`);
        if (!btn.insideClient) throw new Error(`${name} button outside client area`);
      }

      // Verify buttons are fully inside client area (layout fix - buttons must not be clipped)
      for (const [name, btn] of [
        ["Don't install", dontInstallBtn!],
        [variant.installText, installBtn!],
      ] as const) {
        const clientRect = btn.clientRect!;
        if (clientRect.bottom > dlgClientRect2.bottom) {
          throw new Error(`${name} button bottom (${clientRect.bottom}) exceeds client height (${dlgClientRect2.bottom}) - CLIPPED`);
        }
        if (clientRect.top < 0) {
          throw new Error(`${name} button top (${clientRect.top}) < 0 - CLIPPED`);
        }
        if (clientRect.right > dlgClientRect2.right || clientRect.left < 0) {
          throw new Error(`${name} button horizontally outside client area`);
        }
        console.log(`[probe] ${name} button LAYOUT OK: client rect=(${clientRect.left},${clientRect.top},${clientRect.right},${clientRect.bottom}) inside client (0,0,${dlgClientRect2.right},${dlgClientRect2.bottom})`);
      }

      postMessage(installBtn!.hwnd, BM_CLICK, 0, 0);

      const result = await Promise.race([
        requestPromise,
        new Promise<never>((_, reject) => setTimeout(() => reject(new Error("control pipe timeout")), 15000)),
      ]);
      const exitCode = result[0] as number;
      if (exitCode !== 101) throw new Error(`Expected exit code 101, got ${exitCode}`);

      console.log(`[probe] Variant ${variant.installerExists} PASS`);
    } finally {
      if (client) {
        try { await Promise.race([client.quit(), new Promise((_, reject) => setTimeout(() => reject(new Error("quit timeout")), 5000))]); } catch {}
        client.close();
      } else {
        proc.kill();
      }
      await Promise.race([proc.exited, new Promise((r) => setTimeout(r, 5000))]);
    }
  }
}

if (import.meta.main) {
  await runStandalone(runBothVariants);
}