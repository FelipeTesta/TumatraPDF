$c = Get-Content 'D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\PDF Tools\TumatraPDF\sumatrapdf-src\src\EngineMupdf.cpp'
# Find and add } after return true that follows appliedInMemory = true
$newContent = ''
for ($i = 0; $i -lt $c.Length; $i++) {
    $line = $c[$i]
    if ($line -match 'appliedInMemory = true' -and $i + 1 -lt $c.Length -and $c[$i+1].Trim() -eq 'return true') {
        $newContent += $line + "`n" + $c[$i+1] + '}`n'
        $i++
    } else {
        $newContent += $line + "`n"
    }
}
# Also need to remove the duplicate return true that was added
# Actually let me just rewrite more carefully
$lines = Get-Content 'D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\PDF Tools\TumatraPDF\sumatrapdf-src\src\EngineMupdf.cpp'
$output = ''
$skipNext = $false
for ($i = 0; $i -lt $lines.Length; $i++) {
    $skipNext = $false
    if ($skipNext) {
        $skipNext = $false
        continue
    }
    if ($lines[$i] -match 'appliedInMemory = true' -and $i + 1 -lt $lines.Length -and $lines[$i+1].Trim() -eq 'return true') {
        $output += $lines[$i] + "`n"
        $output += '    return true;}' + "`n"
        $skipNext = $true
    } else {
        $output += $lines[$i] + "`n"
    }
}
Set-Content -Path 'D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\PDF Tools\TumatraPDF\sumatrapdf-src\src\EngineMupdf.cpp' -Value $output -Encoding UTF8