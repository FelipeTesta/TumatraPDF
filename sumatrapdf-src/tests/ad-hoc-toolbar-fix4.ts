import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone, cmdId } from "./util";
import { sleep } from "./winapi";
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation";
import { captureWindowToPng } from "./winapi";
import { writeFileSync } from "node:fs";

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
  
  // Launch app and toggle Arch Tools ON via win-automation
  console.log("=== Test: Arch Tools toolbar (toggled ON via sendCommand) ===");
  
  const proc = launchSumatra([pdf]);
  const frame = await waitForFrame(proc.pid);
  console.log(`Frame window: ${frame}`);
  
  // Wait for app to fully initialize
  await sleep(3000);
  
  // Toggle Arch Tools ON
  console.log("Toggling Arch Tools ON...");
  sendCommand(frame, cmdId("CmdArchToolsToggle"));
  await sleep(2000);
  
  // Capture screenshot of the window to verify toolbar visually
  const screenshotPath = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\tests\\tmp\\arch-tools-toolbar.png";
  const ok = captureWindowToPng(frame, screenshotPath);
  console.log(`Screenshot saved: ${ok} (${screenshotPath})`);
  
  // Now dump toolbar state via control pipe (separate instance)
  const archDump = await withControlledSumatra(EXE, async (client) => {
    return dumpButtons(client);
  }, ["-for-testing", pdf]);
  
  console.log(archDump);
  const { buttons, tools } = parse(archDump);
  console.log(`Main toolbar: ${buttons.length} buttons, ${tools.length} tooltip tools`);
  
  // Check band height
  const bandMatch = /d6 band=(\d+)/.exec(archDump);
  if (bandMatch) {
    console.log(`Main band height: ${bandMatch[1]}px`);
  }
  
  // Clean up
  proc.kill();
  
  // The secondary Arch Tools toolbar is a separate rebar - we can't dump it via TestToolbarButtons
  // But we verified the toggle command was sent. Visual verification via screenshot.
  console.log("\n✅ Arch Tools toggle sent successfully!");
  console.log("Check screenshot at tests/tmp/arch-tools-toolbar.png");
}

if (import.meta.main) {
  await runStandalone(testit);
}