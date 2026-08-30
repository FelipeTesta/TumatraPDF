# Build — TumatraPDF

Guia de compilação. **Leia antes de tentar buildar.**

## Método correto (CRÍTICO)

O build DEVE rodar de DENTRO de `sumatrapdf-src`. NÃO da raiz do repo (`TumatraPDF\`).

```powershell
cd sumatrapdf-src
bun cmd/build.ts
```

Saída: `sumatrapdf-src\out\dbg64\TumatraPDF.exe`

### Por que
`cmd/build.ts` (linha 37) usa `vs2022\TumatraPDF.sln` (versão comitada/correta). Esse
caminho é relativo ao cwd. Rodando de `sumatrapdf-src` → `sumatrapdf-src\vs2022\TumatraPDF.sln` ✓.

Um agente introduziu regressão mudando para `..\vs2022\TumatraPDF.sln` (relativo a cwd=sumatrapdf-src
→ pasta pai de sumatrapdf-src = arquivo inexistente → MSB1009). Revertido para `vs2022\TumatraPDF.sln`.

Rodar `bun cmd/build.ts` da raiz do repo (`TumatraPDF\`) dá:
`bun : error: Module not found "cmd/build.ts"` (o script está em `sumatrapdf-src\cmd\`, não `TumatraPDF\cmd\`).

O build copia o exe automaticamente p/ `..\Compiled\TumatraPDF.exe` (finalExeDir = join("..","Compiled")).
O passo de deploy manual abaixo é redundante mas seguro.

Subagentes falham o build por rodar da raiz ou por timeout. O arquiteto (bash) sempre faz `cd sumatrapdf-src` primeiro.

## Deploy (copiar pro Compiled)
```powershell
Copy-Item -Path sumatrapdf-src\out\dbg64\TumatraPDF.exe -Destination Compiled\TumatraPDF.exe -Force
```

## Smoke test
```powershell
Compiled\TumatraPDF.exe -for-testing -console
```
Fechar janela → saída limpa, sem crash dump em `%LOCALAPPDATA%\SumatraPDF-data\<hash>\crashinfo`.

## Fallback: MSBuild direto
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" sumatrapdf-src\vs2022\TumatraPDF.sln /t:TumatraPDF /p:Configuration=Debug /p:Platform=x64 /m
```

## Arquivos novos em src/
Se adicionar .cpp/.h, rodar `bun cmd/premake.ts` antes (regenera `vs2022/*.vcxproj`).

## Comandos/settings gerados
Ao adicionar command ID: editar `cmd/gen-commands.ts` e rodar `bun cmd/gen-commands.ts`
(regenera `src/Commands.h`). Ao adicionar setting: editar `cmd/gen-settings.ts` e rodar
`bun cmd/gen-settings.ts` (regenera `src/Settings.h`). NUNCA editar à mão os arquivos `@gen`.

## Problemas conhecidos
- **LNK1201 (PDB lock)**: `libsumatrapdf.pdb` travado por VS Code/OneDrive. Renomear o PDB e rebuildar.
- **build-asan.ts / .vscode F5 quebrados**: referem `SumatraPDF.sln` antigo. Não usar.
- **build_log.txt**: erro `Module not found` = cwd errado (raiz do repo). Usar `cd sumatrapdf-src`.
- **MSB1009 (sln não encontrado)**: `build.ts:37` com `..\vs2022` é regressão. Deve ser `vs2022\TumatraPDF.sln`. Reverter se aparecer.
```