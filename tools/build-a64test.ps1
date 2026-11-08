# mOS - harness do tradutor arm64 no Windows: build\a64test.exe + testes em build\a64tests\*.elf
#   -Jit : liga o JIT (A64_HAVE_JIT, compila userland\a64\jit*.c)
#   -Teste nome : compila só tools\a64tests\<nome>.c|.S
#   -SoHarness : só o a64test.exe (e o fuzzer/monitor), sem os testes
# Também compila o monitor do oráculo QEMU (build\a64oracle\mon.elf) e o fuzzer (build\a64fuzz.exe).
# Testes: ELF AArch64 estático, base 0x200000000, -mcpu=apple_m2, crt.S + a64t.h. As seções
# .a64fzcode/.a64fzscr dos testes de regressão do fuzzer (regress_*) ficam nos endereços fixos de
# fz.h (linker script tools\a64oracle\regress.ld).
param([switch]$Jit, [string]$Teste = '', [switch]$SoHarness)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force "$root\build\a64tests" | Out-Null
# outras sessões podem estar rodando o a64test/a64fuzz: um .exe em uso não pode ser sobrescrito,
# mas pode ser renomeado (o processo que roda continua com a cópia antiga)
function Move-InUse([string]$exe) {
    if (-not (Test-Path $exe)) { return }
    try { [IO.File]::Open($exe, 'Open', 'ReadWrite', 'None').Close() } catch {
        $old = Join-Path "$root\build\old" ("{0}-{1}.exe" -f [IO.Path]::GetFileNameWithoutExtension($exe), [DateTime]::Now.ToString('HHmmssfff'))
        New-Item -ItemType Directory -Force "$root\build\old" | Out-Null
        Move-Item $exe $old -Force
    }
    Get-ChildItem "$root\build\old\*.exe" -ErrorAction SilentlyContinue | ForEach-Object { Remove-Item $_.FullName -ErrorAction SilentlyContinue }
}
Move-InUse "$root\build\a64test.exe"
Move-InUse "$root\build\a64fuzz.exe"
$src = Get-ChildItem "$root\userland\a64\*.c" | Where-Object { $Jit -or $_.Name -notlike 'jit*' } | ForEach-Object FullName
$defs = @(); if ($Jit) { $defs += '-DA64_HAVE_JIT' }
& $zig cc -target x86_64-windows-gnu -O2 -g -mcx16 -Wall -Wno-unused-function -fno-strict-aliasing $defs `
    "$root\tools\a64test.c" $src -o "$root\build\a64test.exe" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'a64test build FAILED' -ForegroundColor Red; exit 1 }

& $zig cc -target x86_64-windows-gnu -O2 -g -Wall -Wno-unused-function "$root\tools\a64fuzz.c" -o "$root\build\a64fuzz.exe" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'a64fuzz build FAILED' -ForegroundColor Red; exit 1 }
& powershell -NoProfile -ExecutionPolicy Bypass -File "$root\tools\a64oracle\build.ps1" | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host 'monitor do oraculo FAILED' -ForegroundColor Red; exit 1 }
if ($SoHarness) { Write-Host 'ok: build\a64test.exe, build\a64fuzz.exe, build\a64oracle\mon.elf'; exit 0 }

$tests = Get-ChildItem "$root\tools\a64tests\*" -Include *.c, *.S | Where-Object { $_.Name -ne 'crt.S' -and ($Teste -eq '' -or $_.BaseName -eq $Teste) }
$fails = 0
foreach ($t in $tests) {
    $lay = if ($t.BaseName -like 'regress_*') { @("-Wl,-T,$root\tools\a64oracle\regress.ld", '-Wl,-e,_start') } else { @('-Wl,--image-base=0x200000000', '-Wl,-e,_start') }
    & $zig cc -target aarch64-freestanding-none -O1 -g0 -ffreestanding -nostdlib -static -fno-pic -fno-stack-protector `
        -mcpu=apple_m2 "-I$root\tools\a64tests" $lay `
        "$root\tools\a64tests\crt.S" $t.FullName `
        -o "$root\build\a64tests\$($t.BaseName).elf" 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host "teste $($t.Name) FAILED" -ForegroundColor Red; $fails++ }
}
if ($fails) { Write-Host "$fails teste(s) nao compilaram" -ForegroundColor Red; exit 1 }
Write-Host "ok: build\a64test.exe + $($tests.Count) teste(s)"
