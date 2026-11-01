# TumatraPDF Debugging Guide — Win32 Toolbar & UI Automation

## Toolchain Overview

| Tool | Purpose | Location | Key APIs |
|------|---------|----------|----------|
| **winapi.ts** | Raw Win32 FFI bindings | `tests/winapi.ts` | `dlopen("user32")`, `SendMessage`, `PostMessage`, `GetWindowRect`, `PrintWindow` |
| **win-automation.ts** | High-level GUI actions | `tests/win-automation.ts` | `launchSumatra`, `waitForFrame`, `sendCommand`, `captureWindowToPng` |
| **control.ts** | In-process debug commands | `cmd/control.ts` | `withControlledSumatra`, `ControlCommand.TestToolbarButtons` |
| **util.ts** | Shared test helpers | `tests/util.ts` | `EXE`, `cmdId`, `runStandalone`, `tmpPath` |

---

## Debugging Secondary Toolbars (Arch Tools, Edit, Flashcard)

### The Core Problem
- **Main toolbar** = rebar1 + toolbar1 (always present)
- **Secondary toolbars** = separate rebar+toolbar pairs created on-demand:
  - Arch Tools: rebar2 (`IDC_ARCH_REBAR=1115`) + toolbar2 (`IDC_ARCH_TOOLBAR=1116`)
  - Flashcard: rebar (`IDC_FLASHCARD_REBAR`) + toolbar
  - Edit: rebar (`IDC_EDIT_REBAR`) + toolbar

- **TestToolbarButtons** only dumps main toolbar (Toolbar.cpp:ToolbarButtonsResultTemp)
- Secondary toolbars are invisible to the control pipe

---

### Verified Debug Pattern

```typescript
// 1. Launch fresh instance with -for-testing (isolated settings)
const proc = launchSumatra([pdfPath]); // Bun.spawn
const frame = await waitForFrame(proc.pid); // Find SUMATRA_PDF_FRAME
await sleep(3000); // UI settle

// 2. Toggle secondary toolbar via cross-process WM_COMMAND
sendCommand(frame, cmdId("CmdArchToolsToggle")); // Posts WM_COMMAND to frame
await sleep(2000); // Rebar shows, toolbar wraps

// 3. VISUAL VERIFICATION (primary oracle)
const png = captureWindowToPng(frame); // BitBlt from DWM surface
writeFileSync("debug-toolbar.png", png);

// 4. STATE VERIFICATION via control pipe (separate instance)
const dump = await withControlledSumatra(EXE, async (client) => {
  const res = await client.request(ControlCommand.TestToolbarButtons, []);
  return String(res[1] ?? "");
}, ["-for-testing", pdfPath]);
// Parses main toolbar state + band height

// 5. Cleanup
proc.kill();
```

---

### Command IDs (from Commands.h)

```
CmdArchToolsToggle = 493
CmdFlashcardToggle = 498  
CmdEditToggle = 518
CmdViewportCropV2Toggle = 487
CmdMarginTrimToggle = 488
```

---

### Debug Dump Format (Toolbar.cpp lines ~2700-2800)

```
d6 band=<bandH> nBands=<n> toolbarDy=<dy> toolbarClientDx=<dx> rebarWinDy=<dy> rebarClientDy=<dy> packAvail=<w> packRebar=<w> packRows=<n> marks=<i1>,<i2>,<i3>,<i4>
d6b autoscroll style=0xXX cx=<w> needed=<w> set=<w>
d6c trio check=<x>,<y> <w>x<h> label=<x>,<y> <w>x<h> edit=<x>,<y> <w>x<h> vis=<c>/<l>/<e>
d6d anchor=<x>,<y> <dx>x<dy>
idx=<i> cmd=<cmd> hidden=<0/1> enabled=<0/1> checked=<0/1> rect=<l,t,r,b> style=0xXX cx=<w> text=<label>
tool=<i> uid=<cmd> rect=<l,t,r,b>
```

---

### Key Metrics

| Metric | Meaning | Healthy Value | Broken Value |
|--------|---------|---------------|--------------|
| **band** | Current rebar band height (px) | Main: ~30px, Arch: ~30-60px | Arch: 90px (stacked) |
| **toolbarDy** | Toolbar window height | Matches band | >band (overflow) |
| **rebarWinDy** | Rebar window height | Matches band | >band |
| **packRows** | Comctl wrap row count | 1-3 | -1 (not wrapped) |
| **marks** | Wrap break indices | Valid indices | -1,-1,-1,-1 |

---

### Common Failure Modes

| Symptom | Root Cause | Fix |
|---------|------------|-----|
| Band height 90px | Toolbar created at width 0, comctl stacked buttons, never re-wrapped | Size rebar to frame width AFTER `RB_INSERTBAND` (FlashcardToolbarCreate recipe) |
| White band / no buttons | `TBSTYLE_TRANSPARENT` on non-default theme | Remove `TBSTYLE_TRANSPARENT` from all rebar-hosted toolbars |
| Buttons missing | ID collision (Arch rebar used same ID as main) | Unique IDs (`IDC_ARCH_REBAR=1115`, `IDC_ARCH_TOOLBAR=1116`) |
| Dim overlay accumulation | GDI+ POSTPAINT dim on every repaint | Removed `DrawToolbarDisabledOverlay` / `DimDisabledToolbarButtons` |

---

### Traps

1. **Control pipe spawns NEW process** — `withControlledSumatra` launches separate instance
2. **sendCommand targets EXISTING process** — `PostMessage` to running frame HWND
3. **DPI virtualization** — `sendCommand` uses physical coords; control pipe dumps physical rects
4. **Conflict gates** — Arch Tools and Flashcard mutually exclusive (SumatraPDF.cpp ~9921)
5. **Comctl AV on cross-thread TB_*** — `TestToolbarButtons` from background thread crashes comctl

---

### Verified Probes

| Probe | Purpose | Method |
|-------|---------|--------|
| `tests/ad-hoc-toolbar-fix4.ts` | Arch Tools visual verification | `captureWindowToPng` + toggle command |
| `tests/issue-5869.ts` | Main toolbar tooltip rects | `dumpUntilStable` polling |
| `tests/issue-5866.ts` | Chrome strip stability | `topChromePixels` + `countDifferingPixels` |
| `tests/s36ax-fctb.ts` | Flashcard toolbar | `captureWindowToPng` + `BM_CLICK` + sumlog witness |

---

### Next Debug Steps for Arch Tools

1. Run `tests/ad-hoc-toolbar-fix4.ts` to capture screenshot
2. Verify secondary toolbar appears in screenshot (below main toolbar)
3. Check band height in dump (should be 30px main, NOT 90px)
4. If screenshot shows Arch toolbar but dump doesn't → control pipe limitation confirmed