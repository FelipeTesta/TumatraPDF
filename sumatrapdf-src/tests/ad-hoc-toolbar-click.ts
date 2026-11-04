import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone, cmdId } from "./util";
import { sleep } from "./winapi";
import { launchSumatra, waitForFrame, sendCommand, findChildByClass, captureWindowToPng, clickAt } from "./win-automation";
import { getClientRect } from "./winapi";

type Button = { idx: number; cmd: number; hidden: boolean; enabled: boolean; rect: string; text: string };
type Tool = { uid: number; rect: string };

function parseButtonLine(line: string): Button | null {
  const ti = line.indexOf(" text=");
  const head = ti >= 0 ? line.slice(0, ti) : line;
  const text = ti >= 0 ? line.slice(ti + " text=".length) : "";
  const f = new Map<string, string>();
  for (const tok of head.split(" ")) {
    const i = tok.indexOf("=");
    if (i > 0) {
      f.set(tok.slice(0, i), tok.slice(i + 1));
    }
  }
  if (!f.has("idx") || !f.has("cmd") || !f.has("rect")) {
    return null;
  }
  return {
    idx: +(f.get("idx") ?? 0),
    cmd: +(f.get("cmd") ?? 0),
    hidden: f.get("hidden") === "1",
    enabled: f.get("enabled") === "1",
    rect: f.get("rect") ?? "",
    text,
  };
}

function parse(dump: string): { buttons: Button[]; tools: Tool[] } {
  const buttons: Button[] = [];
  const tools: Tool[] = [];
  for (const line of dump.split("\n")) {
    if (line.startsWith("idx=")) {
      const b = parseButtonLine(line);
      if (b) {
        buttons.push(b);
      }
      continue;
    }
    const m = /^tool=\d+ uid=(-?\d+) rect=(\S+)$/.exec(line);
    if (m) {
      tools.push({ uid: +m[1]!, rect: m[2]! });
    }
  }
  return { buttons, tools };
}

async function dumpButtons(client: { request: (cmd: number, args: unknown[]) => Promise<unknown[]> }): Promise<string> {
  const res = await client.request(ControlCommand.TestToolbarButtons, []);
  return String(res[1] ?? "");
}

function parseRect(rectStr: string): { left: number; top: number; right: number; bottom: number } | null {
  const parts = rectStr.split(",").map(Number);
  if (parts.length !== 4) return null;
  return { left: parts[0], top: parts[1], right: parts[2], bottom: parts[3] };
}

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  // Test Arch Tools buttons
  console.log("=== Test: Arch Tools buttons (click Scale, Measure) ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  await sleep(3000);
  
  // Toggle Arch Tools ON
  console.log("Toggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(2000);
  
  // Capture screenshot before clicking
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\arch-tools-before.png");
  
  // Find the Arch Tools toolbar (child of frame, class "ToolbarWindow32")
  const archToolbar = findChildByClass(frame, "ToolbarWindow32");
  console.log(`Arch Tools toolbar: ${archToolbar}`);
  
  if (archToolbar) {
    // Get its rect
    const tbRect = getClientRect(archToolbar);
    console.log(`Toolbar rect: ${tbRect.left},${tbRect.top},${tbRect.right},${tbRect.bottom}`);
    
    // Click in the toolbar area where Arch buttons should be
    // Arch Tools buttons: Scale, Measure, Erase, Clean, Reset Scale
    // They should be on the second row (y > 30)
    const clickX = tbRect.left + 50;
    const clickY = tbRect.top + 40; // second row
    console.log(`Clicking at toolbar client (${clickX}, ${clickY})`);
    await clickAt(archToolbar, clickX, clickY);
    await sleep(1000);
    
    // Try clicking Measure button area
    const clickX2 = tbRect.left + 150;
    const clickY2 = tbRect.top + 40;
    console.log(`Clicking at toolbar client (${clickX2}, ${clickY2})`);
    await clickAt(archToolbar, clickX2, clickY2);
    await sleep(1000);
    
    captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\arch-tools-after-click.png");
  }
  
  // Now test Edit toolbar
  console.log("\n=== Test: Edit toolbar buttons (click +Text, +Img) ===");
  
  // Toggle Arch Tools OFF first (conflict gate)
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(1000);
  
  // Toggle Edit ON
  console.log("Toggling Edit ON...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(2000);
  
  captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\edit-toolbar-before.png");
  
  // Find Edit toolbar (second toolbar window)
  const editToolbar = findChildByClass(frame, "ToolbarWindow32");
  console.log(`Edit toolbar: ${editToolbar}`);
  
  if (editToolbar && editToolbar !== archToolbar) {
    const tbRect = getClientRect(editToolbar);
    console.log(`Edit toolbar rect: ${tbRect.left},${tbRect.top},${tbRect.right},${tbRect.bottom}`);
    
    // Click +Text button area
    const clickX = tbRect.left + 30;
    const clickY = tbRect.top + 15;
    console.log(`Clicking +Text at (${clickX}, ${clickY})`);
    await clickAt(editToolbar, clickX, clickY);
    await sleep(1000);
    
    // Click +Img button area
    const clickX2 = tbRect.left + 90;
    const clickY2 = tbRect.top + 15;
    console.log(`Clicking +Img at (${clickX2}, ${clickY2})`);
    await clickAt(editToolbar, clickX2, clickY2);
    await sleep(1000);
    
    captureWindowToPng(frame, "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\edit-toolbar-after-click.png");
  }
  
  proc.kill();
  
  console.log("\n✅ Button click tests completed!");
  console.log("Check screenshots in tests/tmp/");
}

if (import.meta.main) {
  await runStandalone(testit);
}