# mOS - gera userland\libsystem\vision_sjis.inc: Shift-JIS (JIS X 0208) -> Unicode no índice de 13 bits do modo kanji do
# QR Code (ISO/IEC 18004 §7.4.6), com o Encoding 932 do .NET. Rode de novo só se o formato mudar.
$root = Split-Path $PSScriptRoot -Parent
$enc = [System.Text.Encoding]::GetEncoding(932)
$n = 0x2000
$vals = New-Object 'int[]' $n
for ($v = 0; $v -lt $n; $v++) {
    $hi = [math]::Floor($v / 0xC0); $lo = $v % 0xC0
    $sj = ($hi * 0x100 + $lo) + 0x8140
    if ($sj -gt 0x9FFC) { $sj = ($hi * 0x100 + $lo) + 0xC140 }
    if ($sj -gt 0xEBBF) { continue }
    $b1 = [byte](($sj -shr 8) -band 0xFF); $b2 = [byte]($sj -band 0xFF)
    if ($b2 -lt 0x40 -or $b2 -eq 0x7F -or $b2 -gt 0xFC) { continue }
    $s = $enc.GetString([byte[]]@($b1, $b2))
    if ($s.Length -eq 1 -and ($s[0] -ne [char]0x30FB -or ($b1 -eq 0x81 -and $b2 -eq 0x45))) { $vals[$v] = [int]$s[0] }
}
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('/* incluído por vision.c — Shift-JIS (JIS X 0208) -> Unicode para o modo kanji do QR Code (ISO/IEC 18004 §7.4.6).')
[void]$sb.AppendLine(' * Índice = valor de 13 bits do QR. Gerado por tools/gen-sjis-table.ps1 (Encoding 932 do .NET); 0 = sem caractere. */')
[void]$sb.AppendLine("static const unsigned short SJIS_QR[0x$('{0:X}' -f $n)] = {")
for ($i = 0; $i -lt $n; $i += 16) { [void]$sb.AppendLine('    ' + (($vals[$i..($i + 15)] | ForEach-Object { '0x{0:x}' -f $_ }) -join ', ') + ',') }
[void]$sb.AppendLine('};')
[IO.File]::WriteAllText("$root\userland\libsystem\vision_sjis.inc", $sb.ToString(), [Text.UTF8Encoding]::new($false))
Write-Host ("vision_sjis.inc: {0} caracteres" -f ($vals | Where-Object { $_ -ne 0 }).Count)
