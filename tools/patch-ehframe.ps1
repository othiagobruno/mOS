# mOS - grava no blob do libSystem (x86_64 ou arm64) onde fica o próprio .eh_frame: cx_ls_ehframe[2..3] (unwind_ls.c)
# recebe o deslocamento a partir do cabeçalho ELF e o tamanho. O blob carregado não tem cabeçalhos de seção, e é por
# aqui que o libunwind do libSystem desenrola quadros do próprio libSystem (exceção C++/ObjC atravessando um qsort, um
# bloco do dispatch, o NSInvocation). Usado por tools\build-libsystem.ps1 e tools\build-libsystem-arm64.ps1.
#   .\tools\patch-ehframe.ps1 build\libSystem.mos
param([Parameter(Mandatory = $true)][string]$Path)
$b = [IO.File]::ReadAllBytes((Resolve-Path $Path).Path)
$shoff = [BitConverter]::ToUInt64($b, 40); $shentsize = [BitConverter]::ToUInt16($b, 58); $shnum = [BitConverter]::ToUInt16($b, 60)
$shstr = [BitConverter]::ToUInt16($b, 62)
$strOff = [BitConverter]::ToUInt64($b, [int]($shoff + $shstr * $shentsize + 24))
$ehAddr = 0; $ehSize = 0
for ($i = 0; $i -lt $shnum; $i++) {
    $sh = [int]($shoff + $i * $shentsize)
    $no = [int]($strOff + [BitConverter]::ToUInt32($b, $sh))
    $e = $no; while ($b[$e]) { $e++ }
    if ([Text.Encoding]::ASCII.GetString($b, $no, $e - $no) -eq '.eh_frame') { $ehAddr = [BitConverter]::ToUInt64($b, $sh + 16); $ehSize = [BitConverter]::ToUInt64($b, $sh + 32) }
}
$magic = [BitConverter]::GetBytes([UInt64]0x4d4f5345485f4652) + [BitConverter]::GetBytes([UInt64]0x4152344d34365f41)
$hit = -1
for ($k = 0; $k -le $b.Length - 32; $k += 8) {
    $m = $true; for ($j = 0; $j -lt 16; $j++) { if ($b[$k + $j] -ne $magic[$j]) { $m = $false; break } }
    if ($m) { $hit = $k; break }
}
if ($hit -lt 0 -or -not $ehSize) { Write-Host "${Path}: .eh_frame ou marcador cx_ls_ehframe nao encontrado" -ForegroundColor Red; exit 1 }
[BitConverter]::GetBytes([UInt64]$ehAddr).CopyTo($b, $hit + 16)
[BitConverter]::GetBytes([UInt64]$ehSize).CopyTo($b, $hit + 24)
# os aliases unw_* do libunwind (montados fora do modo Apple) saem WEAK; o carregador só indexa GLOBAL
$nprom = 0
for ($i = 0; $i -lt $shnum; $i++) {
    $sh = [int]($shoff + $i * $shentsize)
    if ([BitConverter]::ToUInt32($b, $sh + 4) -ne 2) { continue }      # SHT_SYMTAB
    $off = [BitConverter]::ToUInt64($b, $sh + 24); $size = [BitConverter]::ToUInt64($b, $sh + 32); $ent = [BitConverter]::ToUInt64($b, $sh + 56)
    $str = [BitConverter]::ToUInt64($b, [int]($shoff + [BitConverter]::ToUInt32($b, $sh + 40) * $shentsize + 24))
    for ($k = 1; $k -lt $size / $ent; $k++) {
        $s = [int]($off + $k * $ent)
        if (($b[$s + 4] -shr 4) -ne 2 -or [BitConverter]::ToUInt16($b, $s + 6) -eq 0) { continue }
        $no = [int]($str + [BitConverter]::ToUInt32($b, $s)); $e = $no; while ($b[$e]) { $e++ }
        $nm = [Text.Encoding]::ASCII.GetString($b, $no, $e - $no)
        if ($nm -like 'unw_*' -or $nm -like '_Unwind_*' -or $nm -like '__unw_*') { $b[$s + 4] = [byte]((1 -shl 4) -bor ($b[$s + 4] -band 15)); $nprom++ }
    }
}
[IO.File]::WriteAllBytes((Resolve-Path $Path).Path, $b)
Write-Host ("eh_frame: +0x{0:X} ({1:N0} bytes)" -f $ehAddr, $ehSize)
