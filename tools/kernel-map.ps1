# mOS - recompila só o kernel com o mapa de símbolos do linker (para traduzir RIPs de pânico em funções).
#   .\build.ps1 -OutDir build-exec ; .\tools\kernel-map.ps1 -OutDir build-exec
#   depois: .\tools\kernel-map.ps1 -OutDir build-exec -Addr 0x1234   (RIP-base do log "*** kernel em ...")
# Usa as mesmas flags do build.ps1; grava $OutDir\BOOTX64.EFI (copiado para o esp) e $OutDir\kernel.map.
param([string]$OutDir = 'build-exec', [string[]]$Addr = @())
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$map = "$OutDir\kernel.map"
if (-not $Addr) {
    $zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Select-Object -First 1
    $zig = Join-Path $zig.FullName 'zig.exe'
    $sources = Get-ChildItem "$root\kernel" -Recurse -Include *.c, *.S | ForEach-Object { $_.FullName.Substring($root.Length + 1) }
    $cflags = @('-target', 'x86_64-uefi-msvc', '-O2', '-g0', '-ffreestanding', '-fno-stack-protector', '-fno-stack-check',
        '-mno-stack-arg-probe', '-mno-red-zone', '-fshort-wchar', '-fno-sanitize=all', '-fno-strict-aliasing', '-nostdlib',
        '-Ikernel/include', '-I.', '-std=gnu11', '-w', "-Wl,/map:$map")
    & $zig cc @cflags @sources -o "$OutDir/BOOTX64.EFI" 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host 'kernel FAILED' -ForegroundColor Red; exit 1 }
    Remove-Item "$OutDir/BOOTX64.lib", "$OutDir/BOOTX64.pdb" -ErrorAction SilentlyContinue
    Copy-Item -Force "$OutDir/BOOTX64.EFI" "$OutDir\esp\EFI\BOOT\BOOTX64.EFI"
    Write-Host "OK: $OutDir\BOOTX64.EFI + $map (rode o mkimg ou o stage para a imagem)" -ForegroundColor Green
    exit 0
}
# tradução: cada endereço (RVA) -> símbolo com o maior RVA <= endereço. Linhas do mapa do lld-link:
#   Address  Size  Align Out In Symbol   (endereços virtuais com a base preferida)
$lines = Get-Content $map
$base = 0
foreach ($l in $lines) { if ($l -match 'Preferred load address is ([0-9a-fA-F]+)') { $base = [Convert]::ToUInt64($matches[1], 16) } }
$syms = foreach ($l in $lines) {
    if ($l -match '^\s*([0-9a-fA-F]{8,16})\s+[0-9a-fA-F]+\s+\d+\s+(\S+)\s*$') {
        [pscustomobject]@{ Va = [Convert]::ToUInt64($matches[1], 16); Name = $matches[2] }
    }
}
$syms = $syms | Sort-Object Va
foreach ($a in $Addr) {
    $rva = [Convert]::ToUInt64(($a -replace '^0x', ''), 16)
    $va = $base + $rva
    $best = $null
    foreach ($s in $syms) { if ($s.Va -le $va) { $best = $s } else { break } }
    if ($best) { "{0} -> {1}+0x{2:x}" -f $a, $best.Name, ($va - $best.Va) } else { "$a -> ?" }
}
