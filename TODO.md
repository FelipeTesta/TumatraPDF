# TODO
## Crop View
o botão deve se comportar dessa forma:
cropview off:
| ← largura total da tela ou zoom usuário → |
|           página 1 (original)             |
|    coluna esquerda | | colunadireita      |

|                 página 2                  |
|    coluna esquerda | |  colunadireita     |

cropview on:
| ← largura total da tela → |
|        página 1           |
|      coluna esquerda      |

|         página 1          |
|       colunadireita       |

|        página 2           |
|       coluna esquerda     |

|        página 2           |
|       colunadireita       |

## Margin
margin config:
______________
|←margin left | ↑margin top | margin right →|
|←margin left | COLUMN 1 |←margin between columns→| COLUMNS 2 |margin right →|
|←margin left | ↓margin bottom | margin right →|
______________
Quando {trim}=on deve recortar do zoom esses pixels, otimizando o uso da tela.

## TRIM
vamos começar a desenvolver os funções do TRIM, mas sem afetar o CROP por enquanto.
vamos ter dois botões: [Trim](on/off) e
[Trim Config]:
abre uma janela com botões "margin-top" "margin-bottom"→clicar em qualquer um→exibe uma linha vermelha horizontal 2px com distancia 0px do topo/bottom; essa linha permite clicar e arrastar para baixo/cima→arrastar calcula automaticamente a distancia do topo até a nova posição da linha→clicar em ✅ salva as distancia.
[Trim]=on:
o leitor contínuo ELIMINA do render esse pedaço das páginas;

Status: ✅ IMPLEMENTADO (2026-08-15) — clip-based top/bottom elimination + Trim Config dialog (margin-top/margin-bottom buttons, draggable red 2px line, ✅ save). Left/right/column-gap NOT implemented (per user decision).

## AUTOSCROLL TIMER
Incluir na barra de ferramentas um Checkbox, logo após o autoscroll: "🔳 Timer |Input Numerico|";
A ideia é: quando timer:check, o autoscroll desliga sozinho apos N minutos (input numerico - padrão 30min);

## Arch Tools
Vamos criar uma aba "Arch Tools";
dentro dessa aba vamos ter 2 funções: scale, measure;
ambas são funções independentes POR documento, ou seja, o que eu definir em um documento deverá se restringir a esse documento e não refletir nas outras abas (outros documentos abertos); antes de iniciar, registre o conceito dessas ferramentas no TODO.md e só vamos sinalizar concluido quando estiverem 100% funcionais;
ambas ferramentas vão depender de interagir com LINHAS vetoriais dos documentos (são desenhos de arquitetura em pdf)

funcionalidade:
1. scale: eu vou selecionar uma linha OU desenhar uma linha (considerar uma função SNAP em pontos), e depois eu vou definir uma medida para essa linha. Após definir essa medida, o desenho deve assumir a escala da medida no local onde eu desenhei a linha;

2. measure: a partir da escala definida, eu vou usar clicar (duas vezes) para definir/desenhar outra linha temporária, e quero que calcule (de acordo a escala definida pela ferramenta anterior): altura (y), largura (x) e comprimento linear dessa linha que eu desenhei;

antes de iniciar, pesquise se há algo opensource similar para se basear e se há skills ou ferramentas úteis para auxiliar no desenvolvimento.

Se nós conseguíssemos fazer esse Arch Tools ser uma espécie de Add-on ou extension do Tumatra, seria interessante, ou de forma modular para caso seja desativado não pese o app;

## ARCH TOOLS (Scale + Measure)
Nova aba "Arch Tools" no sidebar esquerdo (nova tab ao lado de Conteúdo/Favoritos).
Duas funções, INDEPENDENTES POR DOCUMENTO (cada aba/doc mantém seu próprio estado):

### Scale
- Ativar modo Scale.
- Selecionar uma LINHA VETORIAL existente (hit-test nos segmentos extraídos do PDF) OU desenhar uma linha (2 cliques) com SNAP nos pontos/endpoints vetoriais.
- Informar a medida real daquela linha (ex: "5 m") num input.
- Calcula fator de escala = comprimentoEmPagina / medidaReal.
- O desenho "assume a escala" no documento (escala por documento, ancorada na linha desenhada).

### Measure
- Requer escala definida (senão, avisa para definir Scale).
- Clicar 2x para desenhar linha temporária (com SNAP).
- Calcula (pela escala): largura (x = |ΔpageX|), altura (y = |ΔpageY|), comprimento linear (√(x²+y²)).
- Mostra readout (x/y/comprimento) junto à linha + lista na aba.

### Técnico
- Extração vetorial: fz_device custom (stroke_path) coletando segmentos por página (page coords) -> cache em DisplayModel/EngineMupdf.
- SNAP: endpoint mais próximo dentro de threshold (px tela).
- Coordenadas: converter tela<->página via DisplayModel (CvtScreenToPage/CvtPageToScreen) -> medições em page units, zoom-independentes.
- Estado por doc: FileState (archScaleFactor, archScaleAnchorX/Y, archUnit, archScaleSet) + MainWindow (modo, pontos, lista de medições).
- Overlay: desenhar em OnPaintDocument (padrão TrimConfigDialog/ContrastOverlay) com GDI+.
- Persistência: UpdateTabFileDisplayStateForTab / SetDisplayState.

Status: 🟡 EM TESTE (2026-08-28) — Fases 1-4 build-verificadas (infra+shell, extração vetorial+SNAP, overlay+interação+math Scale/Measure, persistência FileState+polish); aguardando validação runtime do usuário (Scale/Measure interativos).
Refinamento Fase 5 (janela flutuante Scale, clique-clique+arrastar, tabela Measure + Limpar linhas) implementado; aguardando validação runtime.
Fase 5b: logging automático [arch] + correção bug Desenhar linha (arme modo, não pre-set archDragLine=1); aguardando validação runtime.
Fase 6: [Limpar linhas] visível, label medida na linha scale, cursor crosshair no desenho, Esc cancela modo, minidump-on-crash (sumatrapdfcrash.dmp); aguardando validação runtime.
Fase 6b: corrigido crash OnOk (atof null) - valida linha desenhada + float>0 antes de atof; aguardando validacao runtime.
Fase 7: UI redesign - removido sidebar, botao 'Arch Tools' na toolbar abre 2a toolbar (Scale/Measure/Reset Scale); linhas visiveis so com Arch Tools=on; modo apagar (segurar 'e' = cursor vermelho + click apaga linha individual); fix flicker; aguardando validacao runtime.
Modularidade: gated by global pref archToolsEnabled (default on); off => zero UI + zero processing cost.

Fase 8: Scale dialog ativa linha existente + mostra comprimento real no edit; label bg mede texto (sem vazar); erase 'e' funciona com Arch Tools on (cursor vermelho + overlay); barra 2a tem 'Limpar linhas' (CmdArchClear) + 'Reset Scale' (scale-only); aguardando validacao runtime.

Fase 9: 'Limpar linhas' limpa so medidas (nao escala); erase 'e' funciona com Arch Tools on (fora do gate draw mode); cursor 'e' sem fundo preto (overlay vermelho no canvas); aguardando validacao runtime.

Fase 10: Scale simplificado - sem clicar na linha; digitar nova medida + OK recalcula escala (usa linha existente); desenhar nova linha substitui anterior; pre-fill do comprimento atual no dialog; aguardando validacao runtime.

Fase 11: editar escala funciona (OnOk usa archScaleSet; toggle nao zera archScaleLineDefined); erase 'e' hit-test direto nas linhas armazenadas (nao segmento PDF); underline branco desenha p/ toggle ativo (sem early-out gAnyToggleChecked); aguardando validacao runtime.

Fase 12: corrigido regressao OnOk (guarda archScaleLineDefined||archScaleSet, permitia desenhar nova linha de escala); corrigido underline invertido do Measure (removido BTNS_CHECK auto-toggle, estado gerenciado por UpdateToolbar2State); renomeado 'Limpar linhas'->'Clean lines'; aguardando validacao 
Fase 13: fator escala canonico (metros) + label respeita unidade atual + lembrar ultima unidade (ArchUnit) + separador decimal configuravel (ArchDecimalSeparator, default ',') + Shift constrain horizontal/vertical; aguardando validacao runtime.

Fase 13b: escala e medicoes independentes por documento (HashMap archMeasurementsByDoc, save/restore no switch de aba, descartado ao fechar); aguardando validacao runtime.

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

