# TumatraPDF 2 — Rebuild Plan (Rust)

> **Status:** 📌 Future reference — *not* being executed. Created 2026-08-16.
> Goal: document the optimized stack and modular architecture in case the project is rebuilt from scratch.

## Context

TumatraPDF v1 = C++/Win32/MuPDF fork of SumatraPDF. Mature exclusive features:
AutoScroll+ETA, CropView/Trim, ContrastOverlay, Recolor/Inversion, themes, AI chat.

Rebuilding from zero **loses** upstream merge value (MERGE.md) and ~1M lines of
rendering/formats. Decision hypothetical/deferred — this plan preserves the design.

## Recommended stack

| Layer | Choice | Reason |
|---|---|---|
| Language | Rust (1.94+ stable) | Memory-safe, `cargo`, 1 binary |
| PDF engine | MuPDF via `mupdf-rs` | Inherits PDF/EPUB/MOBI/CBZ + perf |
| UI | egui (glow/wgpu) | Immediate mode, ideal for crop/overlay |
| Config | serde + TOML | Typed, `#[serde(default)]` migrates versions |
| Tests | `cargo test` + render snapshot | Visual regression in CI |
| Build/CI | GitHub Actions, target windows-msvc | No VS |

### Alternative (easier for AI, but heavier)
Tauri v2 + React/TS + PDF.js — RAM ~150-250MB, 2-3x slower render. Contradicts lightness.

## Modular architecture

```
tumatra/
├── Cargo.toml              # [workspace]
├── crates/
│   ├── core/               # Document, ViewState, pipeline (no UI, testable)
│   ├── ui/                 # egui app shell (thin)
│   ├── render/             # passes: Crop/Recolor/Contrast
│   ├── scroll/             # autoscroll state machine + ETA
│   ├── settings/           # versioned serde TOML
│   └── ai-chat/            # trait ChatProvider (Claude/Grok/Anthropic)
└── tests/
    └── snapshot/           # render golden images
```

### Render pipeline (bug-free by construction)

Known v1 bug: `RenderCache::Paint` had 2 paths (cached/non-cached), only cached
called `RecolorPixmap` → inversion lost with trim on. In v2, **one single pipeline**:

```
RenderPage(engine, page, opts) -> Bitmap
  1. MuPDF rasterizes (mupdf-rs)
  2. CropPass    (margins L/R/T/B + inter-column gap)  <- CropView/Trim
  3. RecolorPass (inversion / dark theme)              <- Theme + Invert
  4. ContrastPass (contrast/brightness overlay)        <- ContrastOverlay
```

Each step: `trait RenderPass { fn apply(&self, img: &mut Image, ctx: &PassCtx) }`.
Config via `RenderOptions` (serde). No globals (`gRenderCache` gone).

### Mapping v1 features → v2 crates

| v1 feature | v2 crate | Gain |
|---|---|---|
| CropView + TrimConfigDialog | `render` (CropPass) + `ui` | geometry = unit test |
| AutoScroll + ETA label | `scroll` | pure state machine, injected timer |
| ContrastOverlay | `render` (ContrastPass) | WS_EX_TRANSPARENT gone |
| Recolor/Inversion/themes | `render` (RecolorPass) | 1 path only |
| AI chat (Claude/Grok/AntiGravity) | `ai-chat` | new provider = 1 impl |
| Settings (registry/txt) | `settings` | versioned TOML |

### Layers and contracts
- `core` never touches UI: consumes `Document` (immutable Arc) + `ViewState { page, zoom, viewport, crop, scroll }`
- Thin declarative UI (egui)
- Events: `enum AppEvent { ScrollTick, PageChanged, UserClick{x,y}, ChatMsg }` — 1 update loop

## Migration phases

### Phase 0 — Engine spike (biggest risk)
1. `cargo new` workspace + `core` crate
2. Integrate `mupdf-rs` (or `pdfium-render`) → render PDF fixture → PNG
3. Prove Crop+Recolor+Contrast passes on bitmap
4. **Go/no-go:** A4 page < 200ms, binary < 20MB, output ≈ SumatraPDF
5. Deliverable: CLI `tumatra render in.pdf -o out.png --crop --invert`

### Phase 1 — Workspace + Pipeline
Create `core`/`render`/`settings`/`scroll`/`ai-chat`/`ui`. `trait RenderPass` + `RenderOptions`.
1 single render path. Unit + snapshot tests.

### Phase 2 — Basic egui reading UI
Page up/down, zoom, scroll, dark theme. Pure ViewState.

### Phase 3 — Port features (risk order)
CropView/Trim → Recolor/Theme → Contrast → AutoScroll/ETA → AI chat.

### Phase 4 — Settings + release
Versioned TOML, `--release`, CI (cargo test + snapshot diff).

## Pending decisions (when executing)
1. **Engine:** `mupdf-rs` (formats+perf, builds C MuPDF — recommended) vs `pdfium-render` (prebuilt DLL, easy Windows, fewer formats)
2. **UI:** `egui` (recommended) vs `iced`
3. **MVP scope:** PDF only + current features vs full format parity
4. **Location:** sibling `TumatraPDF2\` folder vs subfolder in current repo

## Verified tooling (2026-08-16)
- cargo 1.94.1, rustc 1.94.1, target stable-x86_64-pc-windows-msvc
- Git 2.53.0