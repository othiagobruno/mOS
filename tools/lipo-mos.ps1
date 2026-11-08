# mOS - junta Mach-O de arquiteturas diferentes num binário universal (o "lipo -create" da Apple), para as bibliotecas que o
# mOS instala servirem aos processos x86_64 e aos arm64 do tradutor (docs/tradutor-arm64.md): o carregador
# (kernel/compat/macho.c) escolhe o slice da arquitetura do processo.
#   .\tools\lipo-mos.ps1 -Out build\universal\usr\lib\libc++.1.dylib build\libcxx\libc++.1.dylib build\libcxx-arm64\libc++.1.dylib
#   .\tools\lipo-mos.ps1 -Info arquivo     # lista os slices
[CmdletBinding(PositionalBinding = $false)]
param([string]$Out, [switch]$Info, [Parameter(ValueFromRemainingArguments = $true)][string[]]$In)

function BE32([byte[]]$b, [int]$o) { return ([long]$b[$o] -shl 24) + ([long]$b[$o + 1] -shl 16) + ([long]$b[$o + 2] -shl 8) + [long]$b[$o + 3] }
function Put32([System.IO.Stream]$s, [long]$v) { foreach ($sh in 24, 16, 8, 0) { $s.WriteByte([byte](($v -shr $sh) -band 255)) } }
function Name([long]$c) { switch ($c) { 0x01000007 { 'x86_64' } 0x0100000C { 'arm64' } default { 'cpu 0x{0:X}' -f $c } } }

if ($Info) {
    $b = [IO.File]::ReadAllBytes((Resolve-Path $In[0]).Path)
    if ((BE32 $b 0) -eq 0xCAFEBABEL) {
        $n = BE32 $b 4
        for ($i = 0; $i -lt $n; $i++) { $o = 8 + 20 * $i; "{0,-7} +0x{1:X} {2:N0} bytes (alinhado a 2^{3})" -f (Name (BE32 $b $o)), (BE32 $b ($o + 8)), (BE32 $b ($o + 12)), (BE32 $b ($o + 16)) }
    } elseif ([BitConverter]::ToUInt32($b, 0) -eq 0xFEEDFACFL) { "fino: $(Name ([BitConverter]::ToUInt32($b, 4)))" }
    else { 'nao e Mach-O' }
    exit 0
}
if (-not $Out -or -not $In) { Write-Host 'uso: lipo-mos.ps1 -Out saida entrada1 entrada2 ...' -ForegroundColor Red; exit 1 }
$slices = @()
foreach ($f in $In) {
    $b = [IO.File]::ReadAllBytes((Resolve-Path $f).Path)
    if ([BitConverter]::ToUInt32($b, 0) -ne 0xFEEDFACFL) { Write-Host "$f nao e Mach-O de 64 bits fino" -ForegroundColor Red; exit 1 }
    $cpu = [long][BitConverter]::ToUInt32($b, 4); $sub = [long][BitConverter]::ToUInt32($b, 8)
    $align = if ($cpu -eq 0x0100000C) { 14 } else { 12 }        # 16 KiB no arm64, 4 KiB no x86_64 (como o lipo da Apple)
    $slices += [pscustomobject]@{ cpu = $cpu; sub = $sub; data = $b; align = $align; off = 0L }
}
if (@($slices | Group-Object cpu | Where-Object Count -gt 1).Count) { Write-Host 'duas entradas da mesma arquitetura' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force (Split-Path $Out -Parent) | Out-Null
$off = [long](8 + 20 * $slices.Count)
foreach ($x in $slices) { $a = [long](1 -shl $x.align); $off = [long]([math]::Ceiling($off / $a) * $a); $x.off = $off; $off += $x.data.Length }
$s = [IO.File]::Create($Out)
try {
    Put32 $s 0xCAFEBABEL; Put32 $s $slices.Count
    foreach ($x in $slices) { Put32 $s $x.cpu; Put32 $s $x.sub; Put32 $s $x.off; Put32 $s $x.data.Length; Put32 $s $x.align }
    foreach ($x in $slices) { if ($s.Position -lt $x.off) { $pad = New-Object byte[] ($x.off - $s.Position); $s.Write($pad, 0, $pad.Length) }; $s.Write($x.data, 0, $x.data.Length) }
} finally { $s.Close() }
Write-Host ("{0}: universal ({1})" -f $Out, (($slices | ForEach-Object { Name $_.cpu }) -join ' + '))
