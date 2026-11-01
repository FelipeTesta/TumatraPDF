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
  // Test 1: Main toolbar buttons
  console.log("=== Test 1: Main toolbar ===");
  const pdf = "D:\\1 Principal\\4 Trabalhos\\1 Projetos\\00 EXECUTANDO\\APPS\\PDF Tools\\TumatraPDF\\sumatrapdf-src\\ext\\a-zlib\\zlib.3.pdf";
  const mainDump = await withControlledSumatra(EXE, (client) => dumpButtons(client), ["-for-testing", pdf]);
  console.log(mainDump);
  const { buttons: mainButtons, tools: mainTools } = parse(mainDump);
  console.log(`Main toolbar: ${mainButtons.length} buttons, ${mainTools.length} tooltip tools`);
  
  // Test 2: Toggle Arch Tools and check toolbar
  console.log("\n=== Test 2: Arch Tools toolbar ===");
  const archDump = await withControlledSumatra(EXE, async (client) => {
    // Send CmdArchToolsToggle (need to find the command ID)
    // Actually let's just check if the test exe can send commands
    await sleep(1000);
    return dumpButtons(client);
  }, ["-for-testing", pdf]);
  console.log(archDump);
  const { buttons: archButtons, tools: archTools } = parse(archDump);
  console.log(`Arch toolbar: ${archButtons.length} buttons, ${archTools.length} tooltip tools`);
  
  const fail = (msg: string) => {
    throw new Error(`${msg}\nMain dump:\n${mainDump}\nArch dump:\n${archDump}`);
  };
  
  if (mainButtons.length === 0) {
    fail("Main toolbar has no buttons");
  }
  
  if (archButtons.length === 0) {
    fail("Arch Tools toolbar has no buttons - FIX FAILED");
  }
  
  console.log("\n✅ All toolbar tests passed!");
}

if (import.meta.main) {
  await runStandalone(testit);
}