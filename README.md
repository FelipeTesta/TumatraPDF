# TumatraPDF

Leitor de PDF baseado no [SumatraPDF](https://github.com/sumatrapdfreader/sumatrapdf), um projeto open source (GPLv3) desenvolvido por [Krzysztof Kowalczyk](https://github.com/kjk) e contribuidores.

## Objetivo

Criar uma versão melhorada do SumatraPDF com funcionalidades adicionais focadas em leitura de livros e documentos longos, mantendo compatibilidade com atualizações upstream.

## Créditos

Este projeto é um fork do **SumatraPDF** — um leitor de PDF leve, rápido e open source para Windows.

- **Repositório original:** [github.com/sumatrapdfreader/sumatrapdf](https://github.com/sumatrapdfreader/sumatrapdf)
- **Autor principal:** [Krzysztof Kowalczyk (kjk)](https://github.com/kjk)
- **Licença:** [GNU General Public License v3.0](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/COPYING)
- **Site oficial:** [sumatrapdfreader.org](https://www.sumatrapdfreader.org)
- **Versão base utilizada:** Pre-release `3.7.20958`

Agradecemos ao Krzysztof e a todos os contribuidores pelo excelente trabalho no SumatraPDF.

## Funcionalidades Planejadas

### 1. Rolagem Automática com Timer
- Scroll vertical progressivo e contínuo (linhas/segundo)
- Velocidade ajustável via atalhos (F7/F8)
- Pausa automática ao pressionar Ctrl ou clicar
- Timer de segurança: desliga após 30 minutos
- Baseado na implementação AutoHotkey existente: `sumatra-autoscroll-v3.ahk`

### 2. Recorte da Viewport para Livros em Duas Colunas
- Detecção/cropping de meia-página (coluna esquerda/direita)
- Navegação coluna por coluna (Page Down = próxima coluna)
- Suporte para PDFs escaneados (layout fixo de duas colunas)
- Atalhos: teclas para coluna esquerda/direita, próxima página

### 3. Filtro de Contraste
- Ajuste de contraste e luminosidade via pós-processamento
- Pipeline: MuPDF render → filtro de contraste → display
- Perfis salvos por documento
- Atalhos: Ctrl+Shift+C (contraste+), Ctrl+Shift+D (contraste-)

## Estratégia de Modularidade

Para permitir atualizações do SumatraPDF upstream sem quebrar nossas funcionalidades:

```
TumatraPDF/
├── sumatrapdf-src/        # Código original do SumatraPDF (intocado)
├── src/                   # Nossas extensões e hooks
│   ├── features/          # Funcionalidades modulares
│   │   ├── autoscroll/    # Rolagem automática
│   │   ├── viewport/      # Recorte de viewport
│   │   └── contrast/      # Filtro de contraste
│   └── hooks/             # Pontos de integração com upstream
├── docs/                  # Documentação
├── FLOW/                  # Diagramas de planejamento
├── README.md
├── LOG.md
└── MERGE.md               # Instruções para merge upstream
```

**Princípios:**
- Código upstream em `sumatrapdf-src/` NUNCA é modificado diretamente
- Features implementadas em `src/features/` como módulos isolados
- Integração via hooks/patching no build system (Premake5)
- `MERGE.md` documenta o processo de atualização upstream

## Sistema de Verificação de Atualizações

- Monitora versões pre-release do SumatraPDF via `updatecheck-pre-release.txt`
- Verificação automática ao iniciar (configurável)
- Notifica quando nova versão upstream está disponível
- Script `scripts/check-updates.ps1` para verificação manual
- Processo de merge documentado em `MERGE.md`

## Arquitetura

Baseado no SumatraPDF:
- **Linguagem:** C, C++
- **Build:** Premake5 → Visual Studio 2022
- **UI:** Win32 API nativa
- **Motor PDF:** MuPDF
- **Formatos:** PDF, EPUB, MOBI, CBZ/CBR, FB2, CHM, XPS, DjVu
- **Auto-update nativo:** `src/UpdateCheck.cpp` (SumatraPDF)

## Status

🟡 Em planejamento — repositório upstream clonado, arquitetura documentada.