$lines = Get-Content 'D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\PDF Tools\TumatraPDF\sumatrapdf-src\src\EngineMupdf.cpp'
for ($i = 0; $i -lt $lines.Length; $i++) {
    $line = $lines[$i]
    if ($line -match '^\s+return true;$' -and $i -gt 0 -and $lines[$i-1] -match 'appliedInMemory') {
        $lines[$i] = $line.TrimEnd() + '}'
    }
}
Set-Content -Path 'D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\PDF Tools\TumatraPDF\sumatrapdf-src\src\EngineMupdf.cpp' -Value $lines -Encoding UTF8