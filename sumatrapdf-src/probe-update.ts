// probe: does CmdCheckUpdate on the DEBUG build show any UI (notification)?
import { launchSumatra, waitForFrame, sendCommand } from "./tests/win-automation";
import { cmdId } from "./tests/util";
import { captureWindowToPng } from "./tests/winapi";

const p = launchSumatra(["-for-testing", "ext/a-zlib/zlib.3.pdf"]);
const f = await waitForFrame(p.pid);
console.log("frame", f, "pid", p.pid);
sendCommand(f, cmdId("CmdCheckUpdate"));
await new Promise((r) => setTimeout(r, 6000));
const out = process.env.TEMP + "\\dbg-update-probe.png";
const okShot = captureWindowToPng(f, out);
console.log("captured:", okShot, out);
sendCommand(f, cmdId("CmdExit"));
