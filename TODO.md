# TODO
## Crop View
o bot�o deve se comportar dessa forma:
cropview off:
| ? largura total da tela ou zoom usu�rio ? |
|           p�gina 1 (original)             |
|    coluna esquerda | | colunadireita      |

|                 p�gina 2                  |
|    coluna esquerda | |  colunadireita     |

cropview on:
| ? largura total da tela ? |
|        p�gina 1           |
|      coluna esquerda      |

|         p�gina 1          |
|       colunadireita       |

|        p�gina 2           |
|       coluna esquerda     |

|        p�gina 2           |
|       colunadireita       |

## Margin
margin config:
______________
|?margin left | ?margin top | margin right ?|
|?margin left | COLUMN 1 |?margin between columns?| COLUMNS 2 |margin right ?|
|?margin left | �margin bottom | margin right ?|
______________
Quando {trim}=on deve recortar do zoom esses pixels, otimizando o uso da tela.

## TRIM
vamos come�ar a desenvolver as fun��es do TRIM, mas sem afetar o CROP por enquanto.
vamos ter dois bot�es: [Trim](on/off) e
[Trim Config]:
abre uma janela com bot�es "margin-top" "margin-bottom"?clicar em qualquer um?exibe uma linha vermelha horizontal 2px com dist�ncia 0px do topo/bottom; essa linha permite clicar e arrastar para baixo/cima?arrastar calcula automaticamente a dist�ncia do topo at� a nova posi��o da linha?clicar em ? salva as dist�ncia.
[Trim]=on:
o leitor cont�nuo ELIMINA do render essa parte das p�ginas;

Status: ? IMPLEMENTADO (2026-08-15) � clip-based top/bottom elimination + Trim Config dialog (margin-top/margin-bottom buttons, draggable red 2px line, ? save). Left/right/column-gap NOT implemented (per user decision).

## AUTOSCROLL TIMER
Incluir na barra de ferramentas um Checkbox, logo ap�s o autoscroll: "? Timer |Input Num�rico|";
A ideia �: quando timer:check, o autoscroll desliga sozinho ap�s N minutos (input num�rico - padr�o 30min);

## ARCH TOOLS (Scale + Measure)
Nova aba "Arch Tools" no sidebar esquerdo (nova tab ao lado de Conte�do/Favoritos).
Duas fun��es, INDEPENDENTES POR DOCUMENTO (cada aba/doc mant�m seu pr�prio estado):

### Scale
- Ativar modo Scale.
- Selecionar uma LINHA VETORIAL existente (hit-test nos segmentos extra�dos do PDF) OU desenhar uma linha (2 cliques) com SNAP nos pontos/endpoints vetoriais.
- Informar a medida real daquela linha (ex: "5 m") num input.
- Calcula fator de escala = comprimentoEmPagina / medidaReal.
- O desenho "assume a escala" no documento (escala por documento, ancorada na linha desenhada).

### Measure
- Requer escala definida (sen�o, avisa para definir Scale).
- Clicar 2x para desenhar linha tempor�ria (com SNAP).
- Calcula (pela escala): largura (x = |?pageX|), altura (y = |?pageY|), comprimento linear (v(x�+y�)).
- Mostra readout (x/y/comprimento) junto � linha + lista na aba.

### T�cnico
- Extra��o vetorial: fz_device custom (stroke_path) coletando segmentos por p�gina (page coords) -> cache em DisplayModel/EngineMupdf.
- SNAP: endpoint mais pr�ximo dentro de threshold (px tela).
- Coordenadas: converter tela<->p�gina via DisplayModel (CvtScreenToPage/CvtPageToScreen) -> medi��es em page units, zoom-independentes.
- Estado por doc: FileState (archScaleFactor, archScaleAnchorX/Y, archUnit, archScaleSet) + MainWindow (modo, pontos, lista de medi��es).
- Overlay: desenhar em OnPaintDocument (padr�o TrimConfigDialog/ContrastOverlay) com GDI+.
- Persist�ncia: UpdateTabFileDisplayStateForTab / SetDisplayState.

Status: ? EM TESTE (2026-08-28) � Fases 1-4 build-verificadas (infra+shell, extra��o vetorial+SNAP, overlay+intera��o+math Scale/Measure, persist�ncia FileState+polish); aguardando valida��o runtime do usu�rio (Scale/Measure interativos).
Refinamento Fase 5 (janela flutuante Scale, clique-clique+arrastar, tabela Measure + Limpar linhas) implementado; aguardando valida��o runtime.
Fase 5b: logging autom�tico [arch] + corre��o bug Desenhar linha (arme modo, n�o pre-set archDragLine=1); aguardando valida��o runtime.
Fase 6: [Limpar linhas] vis�vel, label medida na linha scale, cursor crosshair no desenho, Esc cancela modo, minidump-on-crash (sumatrapdfcrash.dmp); aguardando valida��o runtime.
Fase 6b: corrigido crash OnOk (atof null) - valida linha desenhada + float>0 antes de atof; aguardando valida��o runtime.
Fase 7: UI redesign - removido sidebar, bot�o 'Arch Tools' na toolbar abre 2a toolbar (Scale/Measure/Reset Scale); linhas vis�veis s� com Arch Tools=on; modo apagar (segurar 'e' = cursor vermelho + click apaga linha individual); fix flicker; aguardando valida��o runtime.
Modularidade: gated by global pref archToolsEnabled (default on); off => zero UI + zero processing cost.

Fase 8: Scale dialog ativa linha existente + mostra comprimento real no edit; label bg mede texto (sem vazar); erase 'e' funciona com Arch Tools on (cursor vermelho + overlay); barra 2a tem 'Limpar linhas' (CmdArchClear) + 'Reset Scale' (scale-only); aguardando valida��o runtime.

Fase 9: 'Limpar linhas' limpa s� medidas (n�o escala); erase 'e' funciona com Arch Tools on (fora do gate draw mode); cursor 'e' sem fundo preto (overlay vermelho no canvas); aguardando valida��o runtime.

Fase 10: Scale simplificado - sem clicar na linha; digitar nova medida + OK recalcula escala (usa linha existente); desenhar nova linha substitui anterior; pre-fill do comprimento atual no dialog; aguardando valida��o runtime.

Fase 11: editar escala funciona (OnOk usa archScaleSet; toggle n�o zera archScaleLineDefined); erase 'e' hit-test direto nas linhas armazenadas (n�o segmento PDF); underline branco desenha p/ toggle ativo (sem early-out gAnyToggleChecked); aguardando valida��o runtime.

Fase 12: corrigido regressao OnOk (guarda archScaleLineDefined||archScaleSet, permitia desenhar nova linha de escala); corrigido underline invertido do Measure (removido BTNS_CHECK auto-toggle, estado gerenciado por UpdateToolbar2State); renomeado 'Limpar linhas'->'Clean lines'; aguardando valida��o 

Fase 13: fator escala canonico (metros) + label respeita unidade atual + lembrar �ltima unidade (ArchUnit) + separador decimal configuravel (ArchDecimalSeparator, default ',') + Shift constrain horizontal/vertical; aguardando valida��o runtime.

Fase 13b: escala e medi��es independentes por documento (HashMap archMeasurementsByDoc, save/restore no switch de aba, descartado ao fechar); aguardando valida��o runtime.

## Fase 15 � Autoscroll Timer (BUILD OK)
- [x] `cmd/gen-commands.ts`: adicionar `CmdAutoScrollTimerToggle = 494`, `CmdAutoScrollTimerEdit = 495`; rodar `bun cmd/gen-commands.ts`
- [x] Verificar `cmd/gen-settings.ts` tem `AutoScrollTimerMinutes` + `AutoScrollTimerEnabled`; regen `Settings.h` se necess�rio
- [x] Rebuild (0 err / 0 warn)
- [x] Deploy `out\dbg64\TumatraPDF.exe` ? `Compiled\TumatraPDF.exe`
- [x] Smoke test launch (`-for-testing -console`)
- [ ] Verifica��o UI: `[checkbox][Timer:][input]` antes de Autoscroll; timer para autoscroll ap�s N min

## Fase 15 Fixes (BUILD OK)

- [x] Autoscroll speed clamp 0.1f ? 0.008f (AutoScroll.cpp:74)
- [x] Lembrar �ltima velocidade: persist FileState + defaults 0.0167f ? 0.008f (MainWindow.h, SumatraPDF.cpp, gen-settings.ts)
- [x] Timer visual: design tokens kCtrlGapX/kCtrlH + TimerInfoId 130 + fix bug slot?r (Toolbar.cpp)
- [x] Build fix: revert build.ts:37 `..\vs2022` ? `vs2022\TumatraPDF.sln`; build 0 err/0 warn; deploy Compiled\TumatraPDF.exe; smoke limpo
- [x] BUILD.md criado (m�todo de build documentado separado)
- [ ] Teste UI pr�tico pelo usu�rio (amanh�): timer control, speed m�nimo, persist�ncia, alinhamento

## Fase 16 � Modulariza��o (refactoring, baseline commit 968b4ec)

### 16A � Design tokens Toolbar.cpp (risco baixo, payoff visual)
- [x] Substituir medidas ad-hoc DpiScale(4/8/10/12/50/70/110) por tokens nomeados (kCtrlGapX/kCtrlH existem; adicionar kPadX, kLabelW, kSlotW)
- [x] Corrigir checkbox 18px hardcoded ? token kCtrlH
- [x] Corrigir gaps inconsistentes (2/20/22/64px) entre checkbox/label/edit
- [x] Build + smoke test
<!-- 16A DONE 2026-08-30: tokens kPadX/kGapX/kTextAnchor/kPagePad/kEtaW/kSpeedSlotW/kSlotW/kLabelW/kToolbarPadY/kToolbarPadY2 added; checkbox 18px?kCtrlH; build 0/0, deploy, smoke clean. -->

### 16B � Structs MainWindow.h (risco m�dio)
- [x] Agrupar ~274 membros em structs por dom�nio (AutoScrollState, ArchToolsState, TimerState)
- [x] Build + smoke test

### 16C � Split SumatraPDF.cpp (12.743 linhas, alto valor, incremental)
- [x] C1: Mapear clusters de handlers (grep `case Cmd` + helpers est�ticos por dom�nio)
- [x] C2: Extrair AutoScrollCommands (menor, bem conhecido)
- [x] C3: Extrair ArchToolsCommands
- [x] C4: Extrair ViewCommands
- [x] C5: Extrair FileCommands � 24 HandleCmdXxx wrappers em SumatraPDF.cpp, dispatch convertido, stubs Commands_File.cpp limpos (2026-09-01)
- [x] Build + smoke test ap�s CADA extra��o (dispatch permanece em SumatraPDF.cpp)

### 16D � Accessors tipados gGlobalPrefs (por �ltimo)
- [ ] Avaliar necessidade (152 acessos em SumatraPDF.cpp) � s� com testes

<!-- 16B DONE 2026-08-30: AutoScrollState + ArchToolsState structs created in MainWindow.h; ~90% of .cpp refs renamed via mechanical PowerShell; bare identifiers in measurement functions fixed with this->; build 0 err/0 warn, deploy, smoke clean. -->
<!-- 16C-2 DONE 2026-08-30: Markdown contrast (light bg + dark gray text), Arch Tools hidden for .md, AutoScroll commands extracted ? Commands_AutoScroll.{h,cpp}; build 0 err/0 warn, deploy, smoke clean. -->
<!-- 16C-C5 DONE 2026-09-01: 24 HandleCmdXxx wrappers in SumatraPDF.cpp, dispatch converted, stubs cleared, build 0/0 -->

---

## An�lise Esfor�o x Resultado � Pr�ximos Passos (2026-09-01)

### Matriz de Decis�o

| A��o | Esfor�o | Resultado | Risco | ROI | Prioridade |
|---|---|---|---|---|---|
| **16C-F6: Extrair init/window creation do SumatraPDF.cpp** | M�dio (pattern C2-C5 j� estabelecido) | Alto (-800~1200 linhas, god file ~10k) | M�dio (static deps, mas pattern comprovado) | ????? | ?? 1 |
| **Canvas.cpp: Extrair event handling** | Alto (5091 linhas, mouse/keyboard entrela�ados com rendering) | Alto (2o maior arquivo) | Alto (rendering code, sem pattern) | ????? | ?? 2 |
| **16D: Encapsular gGlobalPrefs** | Alto (152 acessos em SumatraPDF.cpp, toca muitos arquivos) | Alto (reduz acoplamento global significativamente) | Alto (muitos arquivos, f�cil de quebrar) | ????? | ?? 3 |
| **EngineMupdf.cpp: Desacoplar helpers de rendering** | M�dio (extrair helpers por formato) | M�dio (isola depend�ncia MuPDF) | M�dio (acoplamento profundo) | ????? | ?? 4 |
| ~~MainWindow.h: FileState struct~~ ? **SelectionState struct** ? | Baixo (pattern estabelecido com AutoScrollState/ArchToolsState) | M�dio (organiza god class) | Baixo | ????? | ?? 5 ? |
| **Toolbar.cpp: Refinar mais** | Baixo (2368 linhas, j� tem design tokens) | Baixo (j� modular o suficiente) | Baixo | ????? | ? 6 |

### Recomenda��o: Caminho �timo

**Fase 16C-F6** (Extra��o restante SumatraPDF.cpp):
- Mant�m momentum das extra��es C2-C5
- Pattern j� validado (HandleCmdXxx wrappers)
- Reduz god file de ~11.200 para ~10.000 linhas
- Esfor�o: m�dio � reaproveita infra existente

**Depois: FileState struct** (MainWindow.h):
- Quick win � baixo risco, melhoria organizacional
- Complementa AutoScrollState/ArchToolsState j� feitos

**Evitar por agora:**
- Canvas.cpp extraction (alto risco, sem pattern,Rendering entrela�ado)
- gGlobalPrefs encapsulation (muito disruptivo sem testes unit�rios)

### Ordem Sugerida de Execu��o

1. [x] 16C-F6: Extrair init/window creation do SumatraPDF.cpp (reduzido de 14,212 para ~13,830 linhas) ? 2026-09-08
2. [x] MainWindow.h: SelectionState struct (15 membros selection/touch agrupados) ? 2026-09-08
3. [ ] 16D: gGlobalPrefs typed accessors (s� quando houver testes)
4. [ ] Canvas.cpp: Mapear depend�ncias antes de extrair
5. [ ] EngineMupdf.cpp: Desacoplar helpers de rendering por formato

## Design System Rules (2026-09-08)

### Bot�es e Inputs � Centraliza��o de Conte�do
- **Regra**: Bot�es e inputs devem centralizar conte�do (texto/�cones) horizontal E verticalmente dentro dos seus bounds
- Aplica-se a: controles ToolbarLayout, bot�es toolbar, inputs timer/speed, e quaisquer novos elementos UI
- Status: ?? Regra definida, pendente implementa��o
## Known Bugs (2026-09-09)

### BUG-1: Invert=on - Clipboard image copy broken
- **Status**: ✅ Resolvido (2026-09-10)
- **Descricao**: Quando Invert (contrast overlay) esta ativado, nao e possivel copiar imagens para o clipboard
- **Fix**: Added `case WM_NCHITTEST: return HTTRANSPARENT;` to WndProcContrastOverlay — overlay is now click-through

### BUG-2: Trim=on - Text selection displaced
- **Status**: ✅ Resolvido (2026-09-10)
- **Descricao**: Quando Trim esta ativado, a selecao de texto fica deslocada da posicao renderizada do texto
- **Fix**: Added trim.top offset in SelectionOnPage::GetRect() before CvtToScreen — selection now follows render position

### BUG-3: Right-click image context menu -多数功能 broken
- **Status**: Pendente
- **Descricao**: Menu contexto botao direito em imagens: apenas Copy to clipboard funciona; outras opcoes (Save as, Copy link, etc.) nao funcionam
- **Nota**: Pode nao ter sido implementado no SumatraPDF original - investigar antes de corrigir

## Sistema de Flashcards
Tasks:
1. Pesquisar sistemas de flashcard opensource: `Anki` (Temos baixado em ...\Documents); `supermemo`, etc.

### Funcionamento
Vamos usar algo similar ao Annotations atual (criar um novo painel similar, nao usar o mesmo); selecionar o texto e marcar como "Flashcard" vai incluir um highlight bem SUTIL de cinza (responsivo ao {Invert=on/off}) atrás do texto selecionado; essa marcação terá um id;
Criar um Botão {Flashcard}, similar ao {Arch Tools}, vai abrir uma nova barra de ferramentas: |Tipo de Estudo: `In order`/`Random`| Min Time: {num input} |Return | Next |

Quando Flashcard=on, a ideia é o app saltar de marcação em marcação (card a card), usando a ordem definida pelo usuário; o highlight que antes era sutil, agora vai ser sólido, preto (quando invert=off) ou gray (quando invert=on) e cobrir o texto selecionado, quando usuário apertar `enter`/`space`, revela o texto e o usuário da uma nota de 1 a 4 (numpads), muito similar ao sistema do ANKI; Não queremos criar algo tão complexo quanto o Anki já que não temos como armazenar tanto histórico nem otimizar, mas vamos tentar criar algo similar de forma leve; estude uma forma que seja praticável usando essas referências; preveja como armazenar o estudo do usuário em arquivos temporários sem pesar demais o cache, talvez salvar em um arquivo de texto local;