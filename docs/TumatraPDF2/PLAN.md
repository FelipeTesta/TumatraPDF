# TumatraPDF 2 — Plano de Rebuild (Rust)

> **Status:** 📌 Referência futura — *não* está sendo executado. Criado em 2026-08-16.
> Objetivo: documentar a stack otimizada e a arquitetura modular caso o projeto seja refeito do zero.

## Contexto

TumatraPDF v1 = fork C++/Win32/MuPDF de SumatraPDF. Features exclusivas já maduras:
AutoScroll+ETA, CropView/Trim, ContrastOverlay, Recolor/Inversão, themes, AI chat.

Rebuild do zero **perde** o valor de merge com upstream (MERGE.md) e ~1M linhas de
renderização/formatos. Decisão é hipotética/adiada — este plano preserva o desenho.

## Stack recomendada

| Camada | Escolha | Motivo |
|---|---|---|
| Linguagem | Rust (1.94+ stable) | Memory-safe, `cargo`, 1 binário |
| Engine PDF | MuPDF via `mupdf-rs` | Herda PDF/EPUB/MOBI/CBZ + perf |
| UI | egui (glow/wgpu) | Immediate mode, ideal para crop/overlay |
| Config | serde + TOML | Tipada, `#[serde(default)]` migra versões |
| Testes | `cargo test` + snapshot de render | Regressão visual no CI |
| Build/CI | GitHub Actions, target windows-msvc | Sem VS |

### Alternativa (maior facilidade IA, porém mais pesada)
Tauri v2 + React/TS + PDF.js — RAM ~150-250MB, render 2-3x mais lento. Contradiz a leveza.

## Arquitetura modular

```
tumatra/
├── Cargo.toml              # [workspace]
├── crates/
│   ├── core/               # Document, ViewState, pipeline (sem UI, testável)
│   ├── ui/                 # egui app shell (thin)
│   ├── render/             # passes: Crop/Recolor/Contrast
│   ├── scroll/             # autoscroll state machine + ETA
│   ├── settings/           # serde TOML versionado
│   └── ai-chat/            # trait ChatProvider (Claude/Grok/Anthropic)
└── tests/
    └── snapshot/           # golden images de render
```

### Pipeline de render (correção de bug por construção)

Bug conhecido v1: `RenderCache::Paint` tinha 2 paths (cached/não-cached), só o cached
chamava `RecolorPixmap` → inversão perdida com trim on. No v2, **um único pipeline**:

```
RenderPage(engine, page, opts) -> Bitmap
  1. MuPDF rasteriza (mupdf-rs)
  2. CropPass    (margins L/R/T/B + inter-column gap)  <- CropView/Trim
  3. RecolorPass (inversão / tema dark)                <- Theme + Invert
  4. ContrastPass (contraste/brilho overlay)           <- ContrastOverlay
```

Cada passo: `trait RenderPass { fn apply(&self, img: &mut Image, ctx: &PassCtx) }`.
Config via `RenderOptions` (serde). Sem globals (`gRenderCache` some).

### Mapeamento das features v1 → crates v2

| Feature v1 | Crate v2 | Ganho |
|---|---|---|
| CropView + TrimConfigDialog | `render` (CropPass) + `ui` | geometria = teste unit |
| AutoScroll + ETA label | `scroll` | state machine pura, timer injetado |
| ContrastOverlay | `render` (ContrastPass) | WS_EX_TRANSPARENT some |
| Recolor/Inversão/themes | `render` (RecolorPass) | 1 path só |
| AI chat (Claude/Grok/AntiGravity) | `ai-chat` | novo provider = 1 impl |
| Settings (registry/txt) | `settings` | TOML versionado |

### Camadas e contratos
- `core` não conhece UI: consome `Document` (Arc imutável) + `ViewState { page, zoom, viewport, crop, scroll }`
- UI fina declarativa (egui)
- Eventos: `enum AppEvent { ScrollTick, PageChanged, UserClick{x,y}, ChatMsg }` — 1 loop de update

## Fases de migração

### Fase 0 — Spike do engine (maior risco)
1. `cargo new` workspace + crate `core`
2. Integrar `mupdf-rs` (ou `pdfium-render`) → render PDF fixture → PNG
3. Prova dos passes Crop+Recolor+Contrast no bitmap
4. **Go/no-go:** página A4 < 200ms, binário < 20MB, output ≈ SumatraPDF
5. Entregável: CLI `tumatra render in.pdf -o out.png --crop --invert`

### Fase 1 — Workspace + Pipeline
Crater `core`/`render`/`settings`/`scroll`/`ai-chat`/`ui`. `trait RenderPass` + `RenderOptions`.
1 path de render único. Testes unit + snapshot.

### Fase 2 — UI egui leitura básica
Page up/down, zoom, scroll, dark theme. ViewState pura.

### Fase 3 — Portar features (ordem de risco)
CropView/Trim → Recolor/Theme → Contrast → AutoScroll/ETA → AI chat.

### Fase 4 — Settings + release
TOML versionado, `--release`, CI (cargo test + snapshot diff).

## Decisões pendentes (quando executar)
1. **Engine:** `mupdf-rs` (formatos+perf, build do C MuPDF — recomendado) vs `pdfium-render` (DLL pré-compilada, Windows fácil, menos formatos)
2. **UI:** `egui` (recomendado) vs `iced`
3. **Escopo MVP:** só PDF + features atuais vs paridade completa de formatos
4. **Local:** pasta irmã `TumatraPDF2\` vs subpasta no repo atual

## Ferramentas verificadas (2026-08-16)
- cargo 1.94.1, rustc 1.94.1, target stable-x86_64-pc-windows-msvc
- Git 2.53.0