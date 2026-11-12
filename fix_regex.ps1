$content = Get-Content 'D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\PDF Tools\TumatraPDF\sumatrapdf-src\src\EngineMupdf.cpp' -Raw
$oldPattern = 'appliedInMemory = true;\s+return true;'
$newPattern = 'appliedInMemory = true;`n    return true;}'
$content = $content -replace $oldPattern, $newPattern
Set-Content -Path 'D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\PDF Tools\TumatraPDF\sumatrapdf-src\src\EngineMupdf.cpp' -Value $content -Encoding UTF8