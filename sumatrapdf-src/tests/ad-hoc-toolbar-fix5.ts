import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone, cmdId } from "./util";
import { sleep } from "./winapi";
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import { captureWindowToPng } from "./winapi";

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

export async function testit(): Promise<void> {
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  
  // Test Edit toolbar
  console.log("=== Test: Edit toolbar (toggled ON via sendCommand) ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  await sleep(3000);
  
  // Toggle Edit ON
  console.log("Toggling Edit ON...");
  sendCommand(frame, cmdId("CmdEditToolsToggle"));
  await sleep(2000);
  
  // Capture screenshot
  const screenshotPath = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\edit-toolbar.png";
  const ok = captureWindowToPng(frame, screenshotPath);
  console.log(`Screenshot saved: ${ok} (${screenshotPath})`);
  
  // Dump main toolbar state via control pipe
  const dump = await withControlledSumatra(EXE, async (client) => {
    return dumpButtons(client);
  }, ["-for-testing", pdf]);
  
  console.log(dump);
  const { buttons, tools } = parse(dump);
  console.log(`Main toolbar: ${buttons.length} buttons, ${tools.length} tooltip tools`);
  
  // Check band height
  const bandMatch = /d6 band=(\d+)/.exec(dump);
  if (bandMatch) {
    console.log(`Main band height: ${bandMatch[1]}px`);
  }
  
  proc.kill();
  
  console.log("\n✅ Edit toolbar toggle sent successfully!");
  console.log("Check screenshot at tests/tmp/edit-toolbar.png");
}

if (import.meta.main) {
  await runStandalone(testit);
}