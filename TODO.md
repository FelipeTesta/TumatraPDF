# TODO

## Two Columns v2 (Crop View) — PLANO DE MIGRAÇÃO (em progresso)

### Objetivo

Criar um **segundo modo** "Two Column v2" (`CmdViewportCropV2Toggle`) **totalmente independente** do existente (`CmdViewportCropToggle`), para poder excluir o v1 depois sem conflitos. O v2 implementa o conceito do cropview: cada metade da página vira uma **"página virtual" empilhada verticalmente** (`1L → 1R → 2L → 2R → ...`), largura total da tela, para o autoscroll fluir como texto contínuo **sem salto**.

### Estado atual (v1)

- v1 = manipulação de viewport (`ApplyViewportCrop` em `DisplayModel.cpp:250`): zoom ×2 + shift horizontal (`viewPort.x`) entre colunas. `PageCount()` = N físico, **sem duplicação**.
- Bug central: em `ScrollYBy()` (`DisplayModel.cpp:2165`) ao fim da coluna esquerda faz `viewPort.y = colTop` (snap pro topo da direita) = **salto** descrito no TODO.
- Autoscroll → `MoveDocBy` (`WindowTab.cpp:169`) → `ScrollYBy(dy,false)` → mesmo salto.
- Render: caminho não-cache `RenderCache.cpp:1140-1195` já corta sub-retângulo via `RenderPageArgs(...,&area)` (`:1182`); cacheado usa `req.pageRect` (`RenderCache.cpp:984`). `EngineMupdf::RenderPage` (`:5024`) corta pelo `pageRect`. **Base pronta para coluna.**

### Decisões do usuário

1. Botão **separado** "Two Column 2" (não substitui o v1).
2. v2 **o mais independente possível** (facilitar exclusão do v1 depois).
3. Plano **o mais detalhado possível**.

### Design — Independência v2

- Campos novos em `DisplayModel`: `viewportCropV2Enabled`, `viewportCropV2QuickToggled` (NÃO reusar campos v1).
- Métodos novos: `ApplyViewportCropV2()`, `QuickToggleViewportCropV2()`, `VirtualToPhysical()`, `ColumnOfVirtual()`, `VirtualPageCount()`.
- Comando novo `CmdViewportCropV2Toggle` (independente do `CmdViewportCropToggle`).
- v1 e v2 **mutuamente excludentes** (ligar um desliga o outro).
- Render: cortar coluna via `pageRect`/`area` + `physicalPageNo`; cache key por **virtual pageNo** (resolve esq/dir sem tocar `BitmapCacheEntry`).

### Arquivos afetados

`DisplayModel.{h,cpp}`, `DocumentLayout.{h,cpp}`, `RenderCache.{h,cpp}`, `Canvas.cpp`, `SumatraPDF.cpp`, `cmd/gen-commands.ts`, `Toolbar.cpp`, `Menu.cpp`, `CommandAvailability.cpp`, `Accelerators.cpp`. (Settings do v2: reusar `ViewportCrop` — margens são do PDF, não do modo.)

### Fases (cada fase: `bun cmd/build.ts` 0 err/0 warn + clang-format + smoke manual `-for-testing`)

#### FASE 0 — Infra paralela (comando + estado + botão) — EM ANDAMENTO

- [x] `cmd/gen-commands.ts`: adicionado `CmdViewportCropV2Toggle` (antes de `CmdNone`). → `Commands.h: CmdViewportCropV2Toggle = 512`.
- [x] `bun cmd/gen-code.ts`: Commands.h/Commands.cpp regenerados. _(Erro `cl` não achado no final é só da parte virt-keys; ignorar, ver Accelerators na FASE 5)_.
- [x] `DisplayModel.h`: adicionados campos `viewportCropV2Enabled`, `viewportCropV2QuickToggled` + métodos `ApplyViewportCropV2()`, `QuickToggleViewportCropV2()`, `VirtualToPhysical()`, `ColumnOfVirtual()`, `VirtualPageCount()`.
- [x] `DisplayModel.cpp`: implementar `VirtualPageCount()` (2N se v2 on & contínuo, senão N) e helpers `VirtualToPhysical`/`ColumnOfVirtual` (`:112-131`).
- [x] `SumatraPDF.cpp`: handler `case CmdViewportCropV2Toggle:` (`:11177`, clonado de v1, exclusão mútua, commitado).
- [x] `Toolbar.cpp`: botão novo `{TbIcon::Text, CmdViewportCropV2Toggle, _TRN("Two Column 2")}` (`:106`) + `BTNS_CHECK` (`:395`).
- [x] `Menu.cpp`: item de menu novo (`:631`).
- [x] `CommandAvailability.cpp`: gating (`:232`,`:262`).
- [x] Build + teste: toggle liga/desliga **sem mudança visual** (v2 ainda no-off). ✅ FASE 0 completa (commitada em fc50439).

#### FASE 1 — Duplicar layout para 2N

- [x] `DisplayModel::PageCount()` (`:102`): retorna `VirtualPageCount()` (2N se v2 on & contínuo; senão N). `ValidPageNo` aceita 1..VirtualPageCount.
- [x] `ValidPageNo` (`:150`): aceitar virtual (1..VirtualPageCount()). ✅
- [x] `Relayout()` (`:1277`): `layout.Reset(VirtualPageCount())`, iterar 2N preenchendo `physicalPageNo=VirtualToPhysical(v)`, `cropColumn=ColumnOfVirtual(v)`, mediaBox/zoomReal da página física.
- [x] `BuildPagesInfo()` (`:738`): aloca `VirtualPageCount()` e agora é re-callable (libera array existente) — necessário quando v2 alterna N↔2N.
- [x] `CopyDocumentLayoutToPageInfo` (`:685`): copia `physicalPageNo`+`cropColumn`.
- [x] `DocumentLayoutPage` (`DocumentLayout.h:13`): + `int physicalPageNo` + `int cropColumn = -1`.
- [x] `PageInfo` (`DisplayModel.h:22`): + `int physicalPageNo` + `int cropColumn = -1`.
- [x] Roteamento engine: `PageMediaBox`, `PageSizeAfterRotation`, `GetContentBox`, `CvtToScreen`/`CvtFromScreen`, `ZoomRealFromVirtualForPage` (contentBox loop), `ScrollTo` → `VirtualToPhysical(pageNo)`.
- [x] Render: `ShouldCacheRendering` = false quando v2 (força não-cache, igual trim); `RenderCache::Paint` mapeia `renderPageNo = dm->VirtualToPhysical(pageNo)`.
- [x] `ApplyViewportCropV2` agora chama `BuildPagesInfo()` + `Relayout()` (toggle liga/desliga rebuilda pagesInfo 2N/N). Handler OFF em SumatraPDF.cpp usa `ApplyViewportCropV2`.
- [x] `Canvas.cpp`: `PageMediabox` em trim-drag/erase/full-image → `VirtualToPhysical(pageNo)`.
- [x] Build 0 err/0 warn (75s) + clang-format + smoke estável (zlib.3.pdf 3 páginas, sem crash). ✅ FASE 1 completa — 2N linhas empilhadas, ainda sem corte de coluna.

#### FASE 2 — Corte de coluna no render

- [ ] Caminho não-cache `RenderCache.cpp:1162`: quando `pi->cropColumn>=0`, computar rect da coluna sobre mediabox físico (`esq={0,0,w/2,h}`, `dir={w/2,0,w/2,h}`), deslocar `area` por `colX`, clampar largura, render com `physicalPageNo`.
- [ ] Caminho cacheado (tile): aplicar offset de coluna em `GetTileRectUser`/`pageRect` OU forçar não-cache no v2 (como trim já faz, `RenderCache.cpp:1169`). **Decidir no build**: MVP = forçar não-cache.
- [ ] Cache: usar **virtual pageNo** como chave (já funciona; esq/dir não colidem).
- [ ] `Canvas.cpp` loop `DrawDocument` (`:3288`): iterar virtual; largura da "página" = largura da coluna.
- [ ] Build + teste: cada linha mostra só metade (esq/dir), largura total da tela.

#### FASE 3 — Fluxo contínuo (sem salto) — CRITÉRIO CENTRAL

- [ ] `ScrollYBy()` (`:2155-2198`): no caminho v2, **remover** o snap `viewPort.y = colTop` e o bloqueio `colBottom - viewPort.dy`. Com 2N empilhadas, o scroll avança verticalmente de forma natural.
- [ ] `GoToNextPage`/`GoToPrevPage` operando sobre virtual (`2k-1→2k→2k+1`).
- [ ] Autoscroll (`MoveDocBy`→`ScrollYBy`): flui sem interrupção.
- [ ] **Teste de aceite**: `autoscroll=on` + Two Column 2 lê `1L→1R→2L→2R` como texto único, sem salto.

#### FASE 4 — Conversões virtual↔físico no engine

- [ ] Auditar call-sites de `PageCount()` e engine calls; rotear por `physicalPageNo`:
  - `CurrentPageNo()`, `GetPageNoByPoint`, `CvtToScreen`/`CvtScreenToPage`, `GetContentBox`, `GoToPage`, `GetScrollState`/`SetScrollState`, tile math (`GetTileRectUser/Device`), `PageMediabox`, `ShouldCacheRendering`.
  - Search/Selection/DDE/uia `PageProvider`.
- [ ] Decidir definitivamente `PageCount()` (virtual p/ layout) vs `VirtualPageCount()` (engine usa físico) e corrigir consumidores engine-bound.
- [ ] Build + teste: seleção de texto, busca, links e cliques corretos na 2L/2R.

#### FASE 5 — Estado, navegação & labels

- [ ] `ScrollState.page` (`DisplayModel.h:63`): armazenar virtual pageNo; restaurar mapeia para coluna.
- [ ] NavigationModel/histórico: 2N páginas.
- [ ] Page labels: `1L/1R/2L/2R` (derivar de `ColumnOfVirtual`).
- [ ] `QuickToggleViewportCropV2`: lembrar **página+coluna** ao alternar.
- [ ] `Accelerators.cpp` (`// @gen-start virt-keys-num`): adicionar `CmdViewportCropV2Toggle` (rodar gen-code com VS no PATH, ou editar via gen-data).
- [ ] Build + teste: ir/voltar, histórico, thumbnails coerentes.

#### FASE 6 — Trim + R2L + colGap

- [ ] **Trim** (`marginTrimEnabled`): integrar com corte de coluna (ambos atuam no `area`/`pageRect`; testar ordem em `RenderCache.cpp:1169`).
- [ ] **R2L** (`displayR2L`): inverter esq/dir em docs RTL.
- [ ] **Largura de coluna**: 50/50 vs `colGap` (`Settings.h:32` `ViewportCrop.colGap`).
- [ ] Build + teste combos: `TwoColumn2 + Trim`, `TwoColumn2 + R2L`.

#### FASE 7 — Polish & regressões

- [ ] Reset ao trocar aba/fechar; persistência estado v2.
- [ ] Testar modos não-contínuos (SinglePage/Facing) — v2 só faz sentido em contínuo (ou desabilitar lá).
- [ ] `bun cmd/run-unit-tests.ts -dbg` + smoke.
- [ ] Build final + deploy `Compiled/TumatraPDF.exe`.

### NOTAS

- `cl.exe` (VS) não está no PATH nesta sessão: `bun cmd/gen-code.ts` falha só na geração de virt-keys (`Accelerators.cpp`). Para regenerar Commands.h/Commands.cpp, roda o gen-code e ignora o erro `cl` (o enum/arrays já são gravados antes).
- Forçar não-cache no v2 é aceitável para o MVP (mesma premissa do trim), mas pode re-renderizar ao rolar — avaliar perf na FASE 2/7.

---

## Two Columns (Crop View)

- O botão deve se comportar dessa forma:

**Crop view OFF:**

```
| ? largura total da tela ou zoom usuário ? |
|           página 1 (original)             |
|    coluna esquerda | coluna direita      |
|                 página 2                  |
|    coluna esquerda | coluna direita      |
```

**Crop view ON:**

```
| ? largura total da tela ? |
|        página 1           |
|      coluna esquerda      |
|         página 1          |
|       coluna direita      |
|        página 2           |
|       coluna esquerda     |
|        página 2           |
|       coluna direita      |
```

**Problema atual:** ao "tocar" o limite inferior da página, o app pula automaticamente para o topo direito, não dando tempo de ler o texto de forma fluida como continuidade.

**Solução:** a próxima página renderizada deve ser (1) a mesma página novamente focada na metade direita, depois (2) a página seguinte focada na metade esquerda. Assim, com autoscroll=on, o texto flui de forma contínua sem saltos, como se fosse um texto de única coluna.

## Margin

Configuração de margens:

```
______________
| margem esquerda | margem topo | margem direita |
| margem esquerda | COLUNA 1 | gap entre colunas | COLUNA 2 | margem direita |
| margem esquerda | margem inferior | margem direita |
______________
```

Quando `trim=on`, deve recortar do zoom esses pixels, otimizando o uso da tela.

## TRIM

Desenvolver funções do TRIM sem afetar o CROP por enquanto.

Dois botões: `[Trim]` (on/off) e `[Trim Config]`:

- Abre janela com botões "margin-top" / "margin-bottom"
- Clicar em qualquer um exibe linha vermelha horizontal 2px com distância 0px do topo/bottom
- Linha permite clicar e arrastar para baixo/cima
- Arrastar calcula automaticamente a distância do topo até nova posição da linha
- Clicar em ✓ salva as distâncias

**Status:** ✅ IMPLEMENTADO (2026-08-15) — clip-based top/bottom elimination + Trim Config dialog (margin-top/margin-bottom buttons, draggable red 2px line, ✓ save). Left/right/column-gap NÃO implementados (decisão do usuário).

## AUTOSCROLL TIMER

Incluir na barra de ferramentas um Checkbox, logo após o autoscroll: `[✓ Timer | Input Numérico]`.

**Ideia:** quando timer=✓, o autoscroll desliga sozinho após N minutos (input numérico — padrão 30min).

## ARCH TOOLS (Scale + Measure)

Nova aba "Arch Tools" no sidebar esquerdo (nova aba ao lado de Conteúdo/Favoritos).

Duas funções, INDEPENDENTES POR DOCUMENTO (cada aba/doc mantém seu próprio estado):

### Scale

- Ativar modo Scale
- Selecionar uma LINHA VETORIAL existente (hit-test nos segmentos extraídos do PDF) OU desenhar uma linha (2 cliques) com SNAP nos pontos/endpoints vetoriais
- Informar a medida real daquela linha (ex: "5 m") num input
- Calcula fator de escala = comprimentoEmPagina / medidaReal
- O desenho "assume a escala" no documento (escala por documento, ancorada na linha desenhada)

### Measure

- Requer escala definida (senão, avisa para definir Scale)
- Clicar 2x para desenhar linha temporária (com SNAP)
- Calcula (pela escala): largura (x = |pageX|), altura (y = |pageY|), comprimento linear (√(x²+y²))
- Mostra readout (x/y/comprimento) junto à linha + lista na aba

### Técnico

- Extração vetorial: fz_device custom (stroke_path) coletando segmentos por página (page coords) → cache em DisplayModel/EngineMupdf
- SNAP: endpoint mais próximo dentro de threshold (px tela)
- Coordenadas: converter tela↔página via DisplayModel (CvtScreenToPage/CvtPageToScreen) → medições em page units, zoom-independentes
- Estado por doc: FileState (archScaleFactor, archScaleAnchorX/Y, archUnit, archScaleSet) + MainWindow (modo, pontos, lista de medições)
- Overlay: desenhar em OnPaintDocument (padrão TrimConfigDialog/ContrastOverlay) com GDI+
- Persistência: UpdateTabFileDisplayStateForTab / SetDisplayState

**Status:** ✅ EM TESTE (2026-08-28) — Fases 1-4 build-verificadas (infra+shell, extração vetorial+SNAP, overlay+interação+math Scale/Measure, persistência FileState+polish); aguardando validação runtime do usuário (Scale/Measure interativos).

**Refinamentos:**

- Fase 5: janela flutuante Scale, clique-clique+arrastar, tabela Measure + Limpar linhas — aguardando validação runtime
- Fase 5b: logging automático [arch] + correção bug Desenhar linha (arme modo, não pre-set archDragLine=1) — aguardando validação runtime
- Fase 6: [Limpar linhas] visível, label medida na linha scale, cursor crosshair no desenho, Esc cancela modo, minidump-on-crash (sumatrapdfcrash.dmp) — aguardando validação runtime
- Fase 6b: corrigido crash OnOk (atof null) — valida linha desenhada + float>0 antes de atof — aguardando validação runtime
- Fase 7: UI redesign — removido sidebar, botão 'Arch Tools' na toolbar abre 2a toolbar (Scale/Measure/Reset Scale); linhas visíveis só com Arch Tools=on; modo apagar (segurar 'e' = cursor vermelho + click apaga linha individual); fix flicker — aguardando validação runtime
- Modularidade: gated by global pref archToolsEnabled (default on); off → zero UI + zero processing cost
- Fase 8: Scale dialog ativa linha existente + mostra comprimento real no edit; label bg mede texto (sem vazar); erase 'e' funciona com Arch Tools on (cursor vermelho + overlay); barra 2a tem 'Limpar linhas' (CmdArchClear) + 'Reset Scale' (scale-only) — aguardando validação runtime
- Fase 9: 'Limpar linhas' limpa só medidas (não escala); erase 'e' funciona com Arch Tools on (fora do gate draw mode); cursor 'e' sem fundo preto (overlay vermelho no canvas) — aguardando validação runtime
- Fase 10: Scale simplificado — sem clicar na linha; digitar nova medida + OK recalcula escala (usa linha existente); desenhar nova linha substitui anterior; pre-fill do comprimento atual no dialog — aguardando validação runtime
- Fase 11: editar escala funciona (OnOk usa archScaleSet; toggle não zera archScaleLineDefined); erase 'e' hit-test direto nas linhas armazenadas (não segmento PDF); underline branco desenha p/ toggle ativo (sem early-out gAnyToggleChecked) — aguardando validação runtime
- Fase 12: corrigido regressão OnOk (guarda archScaleLineDefined||archScaleSet, permitia desenhar nova linha de escala); corrigido underline invertido do Measure (removido BTNS_CHECK auto-toggle, estado gerenciado por UpdateToolbar2State); renomeado 'Limpar linhas'→'Clean lines' — aguardando validação runtime
- Fase 13: fator escala canônico (metros) + label respeita unidade atual + lembrar última unidade (ArchUnit) + separador decimal configurável (ArchDecimalSeparator, default ',') + Shift constrain horizontal/vertical — aguardando validação runtime
- Fase 13b: escala e medições independentes por documento (HashMap archMeasurementsByDoc, save/restore no switch de aba, descartado ao fechar) — aguardando validação runtime

## Fase 15 — Autoscroll Timer (BUILD OK)

- [x] `cmd/gen-commands.ts`: adicionar `CmdAutoScrollTimerToggle = 494`, `CmdAutoScrollTimerEdit = 495`; rodar `bun cmd/gen-commands.ts`
- [x] Verificar `cmd/gen-settings.ts` tem `AutoScrollTimerMinutes` + `AutoScrollTimerEnabled`; regen `Settings.h` se necessário
- [x] Rebuild (0 err / 0 warn)
- [x] Deploy `out\dbg64\TumatraPDF.exe` → `Compiled\TumatraPDF.exe`
- [x] Smoke test launch (`-for-testing -console`)
- [ ] Verificação UI: `[checkbox][Timer:][input]` antes de Autoscroll; timer para autoscroll após N min

## Fase 15 Fixes (BUILD OK)

- [x] Autoscroll speed clamp 0.1f → 0.008f (AutoScroll.cpp:74)
- [x] Lembrar última velocidade: persist FileState + defaults 0.0167f → 0.008f (MainWindow.h, SumatraPDF.cpp, gen-settings.ts)
- [x] Timer visual: design tokens kCtrlGapX/kCtrlH + TimerInfoId 130 + fix bug slot→r (Toolbar.cpp)
- [x] Build fix: revert build.ts:37 `..\vs2022` → `vs2022\TumatraPDF.sln`; build 0 err/0 warn; deploy Compiled\TumatraPDF.exe; smoke limpo
- [x] BUILD.md criado (método de build documentado separado)
- [ ] Teste UI prático pelo usuário (amanhã): timer control, speed mínimo, persistência, alinhamento

## Fase 16 — Modularização (refactoring, baseline commit 968b4ec)

### 16A — Design tokens Toolbar.cpp (risco baixo, payoff visual)

- [x] Substituir medidas ad-hoc DpiScale(4/8/10/12/50/70/110) por tokens nomeados (kCtrlGapX/kCtrlH existem; adicionar kPadX, kLabelW, kSlotW)
- [x] Corrigir checkbox 18px hardcoded → token kCtrlH
- [x] Corrigir gaps inconsistentes (2/20/22/64px) entre checkbox/label/edit
- [x] Build + smoke test

<!-- 16A DONE 2026-08-30: tokens kPadX/kGapX/kTextAnchor/kPagePad/kEtaW/kSpeedSlotW/kSlotW/kLabelW/kToolbarPadY/kToolbarPadY2 added; checkbox 18px→kCtrlH; build 0/0, deploy, smoke clean. -->

### 16B — Structs MainWindow.h (risco médio)

- [x] Agrupar ~274 membros em structs por domínio (AutoScrollState, ArchToolsState, TimerState)
- [x] Build + smoke test

### 16C — Split SumatraPDF.cpp (12.743 linhas, alto valor, incremental)

- [x] C1: Mapear clusters de handlers (grep `case Cmd` + helpers estáticos por domínio)
- [x] C2: Extrair AutoScrollCommands (menor, bem conhecido)
- [x] C3: Extrair ArchToolsCommands
- [x] C4: Extrair ViewCommands
- [x] C5: Extrair FileCommands → 24 HandleCmdXxx wrappers em SumatraPDF.cpp, dispatch convertido, stubs Commands_File.cpp limpos (2026-09-01)
- [x] Build + smoke test após CADA extração (dispatch permanece em SumatraPDF.cpp)

### 16D — Accessors tipados gGlobalPrefs (por último)

- [ ] Avaliar necessidade (152 acessos em SumatraPDF.cpp) — só com testes

<!-- 16B DONE 2026-08-30: AutoScrollState + ArchToolsState structs created in MainWindow.h; ~90% of .cpp refs renamed via mechanical PowerShell; bare identifiers in measurement functions fixed with this->; build 0 err/0 warn, deploy, smoke clean. -->
<!-- 16C-2 DONE 2026-08-30: Markdown contrast (light bg + dark gray text), Arch Tools hidden for .md, AutoScroll commands extracted → Commands_AutoScroll.{h,cpp}; build 0 err/0 warn, deploy, smoke clean. -->
<!-- 16C-C5 DONE 2026-09-01: 24 HandleCmdXxx wrappers in SumatraPDF.cpp, dispatch converted, stubs cleared, build 0/0 -->

---

## Análise Esforço x Resultado — Próximos Passos (2026-09-01)

### Matriz de Decisão

| Ação                                                           | Esforço                                                         | Resultado                                          | Risco                                       | ROI   | Prioridade |
| -------------------------------------------------------------- | --------------------------------------------------------------- | -------------------------------------------------- | ------------------------------------------- | ----- | ---------- |
| **16C-F6: Extrair init/window creation do SumatraPDF.cpp**     | Médio (pattern C2-C5 já estabelecido)                           | Alto (-800~1200 linhas, god file ~10k)             | Médio (static deps, mas pattern comprovado) | ????? | 1          |
| **Canvas.cpp: Extrair event handling**                         | Alto (5091 linhas, mouse/keyboard entrelaçados com rendering)   | Alto (2o maior arquivo)                            | Alto (rendering code, sem pattern)          | ????? | 2          |
| **16D: Encapsular gGlobalPrefs**                               | Alto (152 acessos em SumatraPDF.cpp, toca muitos arquivos)      | Alto (reduz acoplamento global significativamente) | Alto (muitos arquivos, fácil de quebrar)    | ????? | 3          |
| **EngineMupdf.cpp: Desacoplar helpers de rendering**           | Médio (extrair helpers por formato)                             | Médio (isola dependência MuPDF)                    | Médio (acoplamento profundo)                | ????? | 4          |
| ~~MainWindow.h: FileState struct~~ → **SelectionState struct** | Baixo (pattern estabelecido com AutoScrollState/ArchToolsState) | Médio (organiza god class)                         | Baixo                                       | ????? | 5          |
| **Toolbar.cpp: Refinar mais**                                  | Baixo (2368 linhas, já tem design tokens)                       | Baixo (já modular o suficiente)                    | Baixo                                       | ????? | 6          |

### Recomendação: Caminho ideal

**Fase 16C-F6** (Extração restante SumatraPDF.cpp):

- Mantém momentum das extrações C2-C5
- Pattern já validado (HandleCmdXxx wrappers)
- Reduz god file de ~11.200 para ~10.000 linhas
- Esforço: médio — reaproveita infra existente

**Depois: FileState struct** (MainWindow.h):

- Quick win a baixo risco, melhoria organizacional
- Complementa AutoScrollState/ArchToolsState já feitos

**Evitar por agora:**

- Canvas.cpp extraction (alto risco, sem pattern, rendering entrelaçado)
- gGlobalPrefs encapsulation (muito disruptivo sem testes unitários)

### Ordem Sugerida de Execução

1. [x] 16C-F6: Extrair init/window creation do SumatraPDF.cpp (reduzido de 14,212 para ~13,830 linhas) — 2026-09-08
2. [x] MainWindow.h: SelectionState struct (15 membros selection/touch agrupados) — 2026-09-08
3. [ ] 16D: gGlobalPrefs typed accessors (só quando houver testes)
4. [ ] Canvas.cpp: Mapear dependências antes de extrair
5. [ ] EngineMupdf.cpp: Desacoplar helpers de rendering por formato

## Design System Rules (2026-09-08)

### Botões e Inputs — Centralização de Conteúdo

- **Regra:** Botões e inputs devem centralizar conteúdo (texto/ícones) horizontal E verticalmente dentro dos seus bounds
- Aplica-se a: controles ToolbarLayout, botões toolbar, inputs timer/speed, e quaisquer novos elementos UI
- **Status:** ⚠️ Regra definida, pendente implementação

## Known Bugs (2026-09-09)

### BUG-1: Invert=on — Clipboard image copy broken

- **Status:** ✅ Resolvido (2026-09-10)
- **Descrição:** Quando Invert (contrast overlay) está ativado, não é possível copiar imagens para o clipboard
- **Fix:** Adicionado `case WM_NCHITTEST: return HTTRANSPARENT;` ao WndProcContrastOverlay — overlay agora é click-through

### BUG-2: Trim=on — Text selection displaced

- **Status:** ✅ Resolvido (2026-09-10)
- **Descrição:** Quando Trim está ativado, a seleção de texto fica deslocada da posição renderizada do texto
- **Fix:** Adicionado trim.top offset em SelectionOnPage::GetRect() antes de CvtToScreen — seleção agora segue posição de renderização

### BUG-3: Right-click image context menu — maioria das funções broken

- **Status:** Pendente
- **Descrição:** Menu contexto botão direito em imagens: apenas Copy to clipboard funciona; outras opções (Save as, Copy link, etc.) não funcionam
- **Nota:** Pode não ter sido implementado no SumatraPDF original — investigar antes de corrigir

## Sistema de Flashcards (Cloze sobre texto existente)

### Modelo (decidido 2026-09-16)

- **1 highlight = 1 carta cloze.** A seleção de texto vira um Highlight MuPDF = máscara posicional sobre o texto ORIGINAL do PDF.
- **SEM duplicação de conteúdo:** o texto original já está no PDF. A carta guarda só `annotId`, `pageNo`, `bounds` (rect do highlight). **Content do highlight fica VAZIO** (não grava `"Q: texto"`).
- **Study=on:** máscara opaca cobre o rect do cloze → `[_____]`. **Reveal:** remove a máscara → texto original visível.
- Múltiplos clozes na mesma frase = múltiplos highlights/cartas (cada seleção = 1 carta).
- **Futuro (pós-MVP):** selecionar várias frases → 1 cloze único (preservar numerais de lista fora do cloze). Não bloqueia o MVP.

### Matriz de render (Canvas) — study on/off NUNCA cobre texto no momento errado

| Estado        | flashcard.on | studyMode | revealMode | Ação sobre cada carta                                                        |
| ------------- | ------------ | --------- | ---------- | ---------------------------------------------------------------------------- |
| Off           | false        | —         | —          | nada (zero custo)                                                            |
| Leitura       | true         | false     | false      | marcador translúcido sutil (`30,128,128,128`) em TODAS — texto visível       |
| Estudo        | true         | true      | false      | máscara opaca (`200,100,100,100`) SÓ na carta atual — texto oculto           |
| Estudo+Reveal | true         | true      | true       | SEM máscara na carta atual — texto original visível (highlight PDF 40% fica) |

> Regra: **máscara opaca só em `studyMode && !revealMode` e só no rect da carta atual.** Fora disso, nunca cobrir texto. O bug atual (FC-A) desenha a máscara em todas as cartas e não a remove no reveal.

### Status atual do código (referência)

- Comandos: 11 IDs (CmdFlashcardToggle..CmdFlashcardAddTip) via gen-commands.ts
- `Flashcard.cpp`: `LoadFromDocument` (parse `Q:`/`T:` — a remover), SM-2, study save/load (load **nunca chamado** — bug FC-B)
- `FlashcardToolbar.cpp`: toolbar secundária (Study/Back/Lista/Filter + contador via botão idx 0)
- `Commands_Flashcard.cpp`: handlers
- `FlashcardSidebar.cpp`: sidebar Lista flutuante
- `Canvas.cpp:3390-3418`: overlay study/reading (reveal quebrado)
- `EditAnnotations.cpp:1300-1313`: filtro `Author == TumatraPDF-Flashcard` (✅ já feito)

### Plano de Implementação (Cloze posicional)

#### Passo 1 — Render: matriz study/reveal correta (Canvas.cpp:3390-3418) [CRÍTICO]

- Reestruturar o loop de cartas: máscara opaca **só na carta atual** (`studyOrder[currentCardIdx]`) quando `studyMode && !revealMode`.
- Reveal: **não** desenhar máscara na carta atual (texto original aparece).
- Demais cartas em study: marcador translúcido (não opaco) — senão todos os clozes aparecem mascarados.
- Leitura (on, !study): marcador translúcido em todas.
- **Verificar:** `texto → study=on → [_____] → reveal → 'texto'` (exemplo do usuário).

#### Passo 2 — Chave estável da carta (Flashcard.h/cpp) [pré-persistência]

- `annotId = pdf_to_num(...)` é **instável** (MuPDF renumera ao salvar).
- Nova chave: `u64 key = hash(pageNo, bounds)` (bounds arredondados), usada para casar `studyDoc` e ordenar.
- Manter `annotId` só como referência de anotação, não como chave.

#### Passo 3 — Persistência: carregar studyDoc no Toggle ON (Commands_Flashcard.cpp) [FC-B]

- `CmdFlashcardToggle` ON (após `FlashcardLoadFromDocument`): chamar `win->flashcard.studyDoc = FlashcardStudyLoad(tab->filePath.s)`.
- Garante `newCount`/`dueCount` corretos após reinício.
- **Verificar:** cria carta, estuda, fecha, reabre → contador mantém estado.

#### Passo 4 — Criação sem duplicação (Commands_Flashcard.cpp, CmdFlashcardAdd) [FC-D/G]

- Parar de gravar `content = "Q: <texto>"` → content vazio (highlight é só máscara).
- Ajustar `FlashcardLoadFromDocument` para carregar por `author == TumatraPDF-Flashcard` (sem depender do prefixo `Q:`), com retrocompat para cartas antigas com `Q:`.
- Após criar, **recarregar `cards`** (FC-G) para contador/lista refletirem a nova carta.
- Decidir persistência: salvar o PDF após adicionar (paridade com AddTip) OU depender do save automático do leitor — alinhar os dois caminhos.

#### Passo 5 — SRS real: filtrar por due (Commands_Flashcard.cpp, BuildFilteredStudyOrder) [FC-C]

- `studyOrder` = cartas com `rating==0` (novas) OU `nextReviewAt <= now` (devidas).
- rating==1 (Again): **reinserir a carta na fila atual** (relearn, igual NoteAnki) em vez de descartar.
- `FlashcardSm2Update` (Flashcard.cpp): cap `easeFactor` em `[1.3, 2.5]`.

#### Passo 6 — Atalhos de teclado (Accelerators.cpp) [alinhar NoteAnki]

- `Space`/`Enter` = CmdFlashcardReveal (Space já revela; confirmar/acrescentar Enter).
- `1`-`4` = CmdFlashcardRate1-4 (rating; hoje só via toolbar).
- Manter `S` = Add (já existe, L148-149).

#### Passo 7 — Podar código morto [FC-H]

- Remover `hwndCardCount` de `FlashcardState` (contador virou botão idx 0).
- Remover/implementar `CmdFlashcardNext` (no-op), `studyModeType`, `history` (decidir undo real ou remover).
- Remover parse de `T:`/`tip` se o modelo position-only não usar (CmdFlashcardAddTip placeholder `(hint)`).

#### Passo 8 — Testes (tests/ + test_util)

- `FlashcardSm2Update`: intervalos, cap ease, relearn rating==1.
- Parser JSON study (save/load roundtrip).
- Filtro due (novas + devidas).
- (GUI opcional) máscara some no reveal via screenshot.

#### Passo 9 — Build + deploy + LOG

- `bun cmd/build.ts` → `Compiled/TumatraPDF.exe` (0 err / 0 warn).
- Atualizar LOG.md + README (matriz de render, modelo cloze).

### Arquivos a Modificar

| Arquivo                      | Mudança                                                              |
| ---------------------------- | -------------------------------------------------------------------- |
| `src/Canvas.cpp`             | Matriz render study/reveal (Passo 1)                                 |
| `src/Flashcard.h`            | Campo `key` estável; remover `tip`/`text` se não usar                |
| `src/Flashcard.cpp`          | Load por author sem `Q:`; key hash; SM-2 cap/relearn                 |
| `src/Commands_Flashcard.cpp` | Add sem duplicação; load studyDoc; due filter; relearn; podar mortos |
| `src/MainWindow.h`           | Remover `hwndCardCount`/mortos                                       |
| `src/Accelerators.cpp`       | Space/Enter reveal + 1-4 rating                                      |
| `src/FlashcardToolbar.cpp`   | (se aplicável) ajustes de contador                                   |

### Referências

- NoteAnki Cloze system: `D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\NoteAnki\CLOZE.md` (máscara sobre texto-fonte, sem duplicação)
- AnnotCreateArgs: `src/Annotation.h:72-93` (opacity 0-100, col, bgColor)
