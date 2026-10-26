// #5869: several custom toolbar buttons that resolve to the same command shared
// one command id. The Win32 toolbar identifies a button - and registers its
// tooltip with its tooltip control - by command id, so with duplicate ids only
// one tooltip tool stayed alive: it got the last button's rect but the first
// button's text, and the other buttons had no tooltip region at all
// (rect 0,0,0,0). Three `Cmd = CmdNone` Shortcuts entries reproduce it.
//
// Also checks that toolbar buttons coming from ExternalViewers keep the order
// they're listed in (they used to come out reversed, gFirstCustomCommand is a
// prepend-only list).
//
// Uses -appdata so it never touches the user's real settings.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "../cmd/control";
import { EXE, ROOT, runStandalone, tmpPath } from "./util";

const SHORTCUT_NAMES = ["SVG test dummy A", "SVG test dummy B", "SVG test dummy C"];
const VIEWER_NAMES = ["Viewer One", "Viewer Two"];

function svg(letter: string): string {
  return (
    `<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none">` +
    `<path d="M2 4h20a1 1 0 0 1 1 1v3a12 12 0 0 1-12 12H2a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1Z" fill="#ff8"/>` +
    `<text x="3.5" y="15" font-family="Arial" font-size="8" font-weight="bold" fill="#000">${letter}</text></svg>`
  );
}

// all three use the same command (CmdNone) on purpose: that's what made them
// share a command id
function shortcut(key: string, name: string, letter: string): string {
  return `\t[
\t\tCmd = CmdNone
\t\tKey = ${key}
\t\tName = ${name}
\t\tToolbarSvgIcon = ${svg(letter)}
\t]`;
}

function externalViewer(name: string): string {
  return `\t[
\t\tCommandLine = notepad.exe "%1"
\t\tName = ${name}
\t\tToolbarText = ${name}
\t]`;
}

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
Shortcuts [
${shortcut("d", SHORTCUT_NAMES[0]!, "A")}
${shortcut("e", SHORTCUT_NAMES[1]!, "B")}
${shortcut("f", SHORTCUT_NAMES[2]!, "C")}
]
ExternalViewers [
${externalViewer(VIEWER_NAMES[0]!)}
${externalViewer(VIEWER_NAMES[1]!)}
]
`;

type Button = { idx: number; cmd: number; hidden: boolean; enabled: boolean; rect: string; text: string };
type Tool = { uid: number; rect: string };

// dump line (see ToolbarButtonsResultTemp in src/Toolbar.cpp):
//   idx=%d cmd=%d hidden=%d enabled=%d rect=%d,%d,%d,%d style=0x%02X cx=%d text=%s
// `text=` is last and button labels contain spaces, and the key=value fields
// before it keep growing (style/cx were added, then enabled) — so parse by key
// instead of a rigid regex, which silently broke the test twice already.
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

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5869");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "TumatraPDF-settings.txt"), SETTINGS);

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const res = await withControlledSumatra(EXE, (client) => client.request(ControlCommand.TestToolbarButtons, []), [
    "-appdata",
    dir,
    pdf,
  ]);
  const dump = String(res[1] ?? "");
  const { buttons, tools } = parse(dump);
  const fail = (msg: string) => {
    throw new Error(`${msg}\ntoolbar dump:\n${dump}`);
  };
  if (buttons.length === 0) {
    fail("no toolbar buttons reported");
  }

  // the custom buttons are the ones whose text is one of our names
  const wanted = [...SHORTCUT_NAMES, ...VIEWER_NAMES];
  const custom = buttons.filter((b) => wanted.includes(b.text));
  if (custom.length !== wanted.length) {
    fail(`expected ${wanted.length} custom toolbar buttons, got ${custom.length}`);
  }

  // buttons must be in the order the settings list them
  const order = custom.map((b) => b.text);
  if (order.join("|") !== wanted.join("|")) {
    fail(`custom toolbar buttons out of order: ${JSON.stringify(order)} != ${JSON.stringify(wanted)}`);
  }

  // this is the actual bug: the three CmdNone shortcuts shared one command id
  const ids = custom.map((b) => b.cmd);
  if (new Set(ids).size !== ids.length) {
    fail(`custom toolbar buttons share command ids: ${JSON.stringify(ids)}`);
  }

  // and therefore each one must have its own tooltip tool, covering its own
  // button rect (a duplicate id left tools with an empty 0,0,0,0 rect)
  for (const b of custom) {
    const matching = tools.filter((t) => t.uid === b.cmd);
    if (matching.length !== 1) {
      fail(`button '${b.text}' (cmd ${b.cmd}) has ${matching.length} tooltip tools, want 1`);
    }
    if (matching[0]!.rect !== b.rect) {
      fail(`tooltip for '${b.text}' covers ${matching[0]!.rect}, but the button is at ${b.rect}`);
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
