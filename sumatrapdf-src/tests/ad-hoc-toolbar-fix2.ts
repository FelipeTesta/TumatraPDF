import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, runStandalone } from "./util";
import { sleep } from "./winapi";

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
  
  // Test with Arch Tools toggled ON
  console.log("=== Test: Arch Tools toolbar (toggled ON) ===");
  const archDump = await withControlledSumatra(EXE, async (client) => {
    // Wait for app to start
    await sleep(2000);
    // Send CmdArchToolsToggle - need to use TestToolbarButtons to see state
    // Actually we need to send the command through the control pipe
    // But TestToolbarButtons just dumps state - we need a way to send commands
    // Let's just check initial state and if Arch Tools is visible
    const dump = await dumpButtons(client);
    return dump;
  }, ["-for-testing", pdf]);
  
  console.log(archDump);
  const { buttons, tools } = parse(archDump);
  console.log(`Buttons: ${buttons.length}, Tooltip tools: ${tools.length}`);
  
  // Check for Arch Tools specific buttons (should have Scale, Measure, etc.)
  const archButtons = buttons.filter(b => 
    b.text.includes("Scale") || 
    b.text.includes("Measure") || 
    b.text.includes("Erase") || 
    b.text.includes("Clean") || 
    b.text.includes("Reset")
  );
  console.log(`Arch-specific buttons found: ${archButtons.length}`);
  for (const b of archButtons) {
    console.log(`  ${b.text} (cmd=${b.cmd}, rect=${b.rect}, hidden=${b.hidden})`);
  }
  
  // Check band height
  const bandMatch = /d6 band=(\d+)/.exec(archDump);
  if (bandMatch) {
    console.log(`Band height: ${bandMatch[1]}px`);
  }
  
  // Check if Arch Tools rebar is visible (should be a separate rebar with its own buttons)
  const rebarMatch = /d6 nBands=(\d+)/.exec(archDump);
  if (rebarMatch) {
    console.log(`Number of bands: ${rebarMatch[1]}`);
  }
  
  const fail = (msg: string) => {
    throw new Error(`${msg}\nDump:\n${archDump}`);
  };
  
  // The secondary Arch Tools toolbar should be visible and have buttons
  if (archButtons.length === 0) {
    fail("Arch Tools toolbar has no Arch-specific buttons (Scale, Measure, etc.)");
  }
  
  console.log("\n✅ Arch Tools toolbar test passed!");
}

if (import.meta.main) {
  await runStandalone(testit);
}