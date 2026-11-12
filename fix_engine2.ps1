$lines = Get-Content 'D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\PDF Tools\TumatraPDF\sumatrapdf-src\src\EngineMupdf.cpp'
for ($i = 0; $i -lt $lines.Length; $i++) {
    if ($lines[$i] -match 'appliedInMemory = true' -and $i+1 -lt $lines.Length -and $lines[$i+1].Trim() -eq 'return true') {
        $lines[$i+1] = $lines[$i+1].Trim() + '}'
    }
}
Set-Content -Path 'D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\PDF Tools\TumatraPDF\sumatrapdf-src\src\EngineMupdf.cpp' -Value $lines -Encoding UTF8