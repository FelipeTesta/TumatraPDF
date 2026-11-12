$c = Get-Content 'D:\1 Principal\4 Trabalhos\1 Projetos\00 EXECUTANDO\APPS\PDF Tools\TumatraPDF\sumatrapdf-src\src\EngineMupdf.cpp'
$line1 = $c[7524]  # 0-based, so line 7525
$line2 = $c[7525]  # 0-based, so line 7526
Write-Host "Line 7525 chars:"
$line1.ToCharArray() | ForEach-Object { [int]$_ }
Write-Host ""
Write-Host "Line 7526 chars:"
$line2.ToCharArray() | ForEach-Object { [int]$_ }