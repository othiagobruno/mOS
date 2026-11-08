# mOS - oraculo FP do tradutor arm64: gera vetores, roda no QEMU (-cpu max) e compara com o
# interpretador (a64_exec_fp + a64_fp_*).
#   -N vetores-por-modelo (padrao 4000)   -Semente n   -Filtro texto (so modelos com esse nome)
#   -MaxFalhas n (falhas mostradas por modelo)   -SoChecar (reusa in.bin/out.bin ja gerados)
param([int]$N = 4000, [string]$Semente = '1', [string]$Filtro = '', [int]$MaxFalhas = 3, [switch]$SoChecar)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$root = (Resolve-Path "$here\..\..\..").Path
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } else { $zig = (Get-Command zig).Source }
$qemu = 'C:\Program Files\qemu\qemu-system-aarch64.exe'
$out = "$root\build\fp_oracle"
New-Item -ItemType Directory -Force $out | Out-Null
$a64 = "$root\userland\a64"
$fpsrc = @("$a64\a64_fp.c", "$a64\interp_fp.c")

& $zig cc -target x86_64-windows-gnu -O2 -g -Wall -Wno-unused-function "$here\fp_check.c" $fpsrc -o "$out\fp_check.exe"
if ($LASTEXITCODE -ne 0) { throw 'fp_check build' }
if (-not $SoChecar) {
    & $zig cc -target x86_64-windows-gnu -O2 -g -Wall -Wno-unused-function "$here\fp_gen.c" $fpsrc -o "$out\fp_gen.exe"
    if ($LASTEXITCODE -ne 0) { throw 'fp_gen build' }
    & $zig cc -target aarch64-freestanding-none -nostdlib -static -fno-pic -ffreestanding `
        "-Wl,--image-base=0x40000000" "-Wl,-e,_start" "$here\runner.S" -o "$out\runner.elf"
    if ($LASTEXITCODE -ne 0) { throw 'runner build' }
    $outq = ("$out\out.bin" -replace '\\', '/')
    $ga = @("$out\in.bin", $outq, "$N", $Semente)
    if ($Filtro) { $ga += $Filtro }
    & "$out\fp_gen.exe" @ga
    if ($LASTEXITCODE -ne 0) { throw 'fp_gen' }
    Remove-Item "$out\out.bin" -ErrorAction SilentlyContinue
    $t0 = Get-Date
    & $qemu -M virt -cpu max -m 3G -display none -monitor none -serial none -nodefaults `
        -semihosting-config enable=on,target=native -kernel "$out\runner.elf" `
        -device "loader,file=$out\in.bin,addr=0x48000000,force-raw=on"
    Write-Host ("qemu: {0:N1} s" -f ((Get-Date) - $t0).TotalSeconds)
    if (-not (Test-Path "$out\out.bin")) { throw 'qemu nao gerou out.bin' }
}
& "$out\fp_check.exe" "$out\in.bin" "$out\out.bin" "$MaxFalhas"
exit $LASTEXITCODE
