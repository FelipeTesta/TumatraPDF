# MERGE.md — Atualização do SumatraPDF Upstream

Este documento descreve o processo para integrar novas versões do SumatraPDF ao TumatraPDF.

## Estrutura Modular

```
TumatraPDF/
├── sumatrapdf-src/        # Código upstream — NUNCA modificado
├── src/                   # Nossas extensões (features + hooks)
├── MERGE.md               # Este documento
└── scripts/               # Scripts auxiliares
    └── check-updates.ps1  # Verificador de novas versões
```

**Princípio fundamental:** O código em `sumatrapdf-src/` nunca é alterado diretamente. Todas as nossas modificações ficam em `src/features/` e são integradas via hooks no build system (Premake5).

## Verificação de Atualizações

### Automática (SumatraPDF nativo)
O SumatraPDF já possui verificação de atualizações nativa em `src/UpdateCheck.cpp`. As versões pre-release verificam o manifest:
```
https://www.sumatrapdfreader.org/updatecheck-pre-release.txt
```

### Manual (nosso script)
```powershell
# Verificar se há nova versão pre-release
.\scripts\check-updates.ps1
```

O script compara a versão atual (definida em `sumatrapdf-src/src/Version.h`) com a versão mais recente disponível no site.

## Processo de Merge

### 1. Verificar nova versão
```powershell
.\scripts\check-updates.ps1
```

### 2. Fazer backup do estado atual
```powershell
git checkout -b merge/vX.Y.Z
git push -u origin merge/vX.Y.Z
```

### 3. Baixar nova versão do SumatraPDF
```powershell
# Fazer backup do sumatrapdf-src atual
Copy-Item -Recurse sumatrapdf-src sumatrapdf-src.backup

# Clonar nova versão
Remove-Item -Recurse sumatrapdf-src
git clone --depth 1 --branch <tag> https://github.com/sumatrapdfreader/sumatrapdf.git sumatrapdf-src
Remove-Item -Recurse sumatrapdf-src\.git
```

### 4. Analisar diferenças
```powershell
# Comparar com backup
git diff --stat sumatrapdf-src.backup sumatrapdf-src
```

Foco nas áreas que afetam nossos hooks:
- `src/Theme.cpp` / `src/Theme.h` — pipeline de cores (Feature 3)
- `src/SumatraPDF.cpp` — main window, comandos (Feature 1)
- `src/Canvas.cpp` — renderização (Features 2, 3)
- `src/DisplayModel.cpp` — modelo de exibição (Feature 2)
- `src/AppSettings.cpp` — configurações
- `premake5.lua` / `premake5.files.lua` — build system

### 5. Ajustar hooks e features
Se houver conflitos com nossas features:
1. Identificar o que mudou no upstream
2. Adaptar nossos hooks em `src/hooks/`
3. Testar cada feature individualmente
4. Rodar build completo: `bun cmd/build.ts` (ou msbuild direto)

### 6. Atualizar documentação
- `README.md` — nova versão base
- `LOG.md` — registro do merge
- `FLOW/tumatrapdf.dot` — se necessário

### 7. Testar e commitar
```powershell
# Build e teste
bun cmd/run-unit-tests.ts -dbg

# Commit
git add -A
git commit -m "merge: atualizar SumatraPDF upstream para vX.Y.Z"
```

### 8. Merge na master
```powershell
git checkout master
git merge merge/vX.Y.Z
git push
```

## Pontos de Integração (Hooks)

Nossas features se conectam ao upstream nestes pontos:

| Feature | Arquivo(s) upstream | Tipo de hook |
|---|---|---|
| AutoScroll | `SumatraPDF.cpp` (loop de eventos), `Canvas.cpp` | Timer + scroll |
| Viewport Crop | `DisplayModel.cpp`, `Canvas.cpp` | Render pipeline |
| Contraste | `Theme.cpp`, `PdfDarkModeDevice.cpp` | Post-process |

## Comandos Úteis

```powershell
# Build rápido (Debug x64)
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" sumatrapdf-src\vs2022\SumatraPDF.sln /t:SumatraPDF /p:Configuration=Debug /p:Platform=x64 /m

# Executar para teste
.\sumatrapdf-src\out\dbg64\SumatraPDF.exe -for-testing

# Limpar build
Remove-Item -Recurse sumatrapdf-src\out\ -ErrorAction SilentlyContinue
```

## Troubleshooting

| Problema | Solução |
|---|---|
| `couldn't find vs 2026 msbuild.exe` | Usar msbuild direto em vez de `bun cmd/build.ts` |
| Erro de link | Rodar `bun cmd/premake.ts` para regenerar .vcxproj |
| Conflito de merge | Comparar com `sumatrapdf-src.backup` |
| Feature quebrada após merge | Verificar hooks em `src/hooks/` |
