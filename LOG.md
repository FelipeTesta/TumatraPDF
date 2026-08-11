# TumatraPDF — Log de Desenvolvimento

## 2026-08-11 — Início do Projeto

### Pesquisa da Arquitetura SumatraPDF
- **Repositório:** github.com/sumatrapdfreader/sumatrapdf
- **Build:** Premake5 gera solução Visual Studio 2022
- **UI:** Win32 API nativa (sem MFC, sem wxWidgets)
- **Motor PDF:** MuPDF (parsing + rendering)
- **Linguagens:** C, C++
- **Estrutura de diretórios:** `src/` (código principal), `mupdf/` (engine PDF), `ext/` (libs externas), `vs2022/` (solução VS)
- **Formatos suportados:** PDF, EPUB, MOBI, CBZ/CBR, FB2, CHM, XPS, DjVu
- **Autor:** Krzysztof Kowalczyk (kjk)
- **Licença:** GPLv3

### Versão Pre-release
- **Versão atual:** `3.7.20958`
- **NÃO possui sistema de plugins/módulos** — app monolítico Win32
- **Possui auto-update nativo:** `src/UpdateCheck.cpp`, manifest `updatecheck-pre-release.txt`
- **Configuração:** `src/Version.h`, `src/BuildConfig.h`, `src/AppSettings.h`

### Funcionalidades Planejadas
1. **Rolagem automática com timer** — scroll contínuo com velocidade ajustável
2. **Recorte da viewport para duas colunas** — cropping de meia-página para livros com layout de duas colunas
3. **Filtro de contraste** — ajuste de contraste/luminosidade via pós-processamento

### Implementação AutoHotkey Existente
- **Arquivo:** `Sumatra Tools\sumatra-autoscroll-v3.ahk`
- Já implementa: auto-scroll (F9 toggle, F7/F8 velocidade), navegação 2 colunas (numpad), pause Ctrl, timer 30min
- Servirá como referência para implementação nativa em C++

### Decisões de Design
- Manter base Win32 para compatibilidade e leveza
- Integrar com MuPDF como engine de renderização
- **Código upstream em `sumatrapdf-src/` intocado** — features em `src/features/`
- Modularidade via hooks no build system Premake5
- Merge upstream documentado em `MERGE.md`

### Estratégia de Atualização Upstream
- Verificação automática de novas versões pre-release
- Script `scripts/check-updates.ps1` para verificação manual
- Processo: baixar nova versão → diff → merge manual nos hooks → rebuild

### Estrutura do Projeto
```
TumatraPDF/
├── README.md              # Visão geral e créditos
├── LOG.md                 # Este arquivo
├── MERGE.md               # Instruções de merge upstream (a criar)
├── FLOW/                  # Diagramas de planejamento
│   └── tumatrapdf.dot
├── sumatrapdf-src/        # Código original SumatraPDF (6353 arquivos)
├── src/                   # Nossas extensões (a criar)
│   ├── features/
│   │   ├── autoscroll/
│   │   ├── viewport/
│   │   └── contrast/
│   └── hooks/
└── scripts/               # Scripts auxiliares (a criar)
    └── check-updates.ps1
```

### Próximos Passos
- [x] Clonar repositório SumatraPDF
- [x] Documentar arquitetura e créditos
- [x] Inicializar repositório Git local
- [ ] Criar repositório privado no GitHub
- [ ] Configurar ambiente de build (VS2022 + Premake5)
- [ ] Compilar versão base sem modificações
- [ ] Criar MERGE.md com processo de atualização
- [ ] Implementar scripts/check-updates.ps1
- [ ] Planejar pontos de integração para as 3 features
- [ ] Implementar feature 1: rolagem automática (portar do AHK)
- [ ] Implementar feature 2: recorte de viewport
- [ ] Implementar feature 3: filtro de contraste
