# mOS - teste do tradutor arm64 no QEMU (frente RUNTIME, docs/tradutor-arm64.md): põe na imagem o runtime, o libSystem
# arm64 e os programas de teste (userland\tests-a64rt), e jobs do launchd que os rodam no boot com cada motor. A saída
# vai para o log serial com o prefixo "[interp]" / "[jit]" ("a64rt-teste: N falhas" no fim de cada um).
#   .\build.ps1 -OutDir build-a64
#   .\tools\build-a64rt-teste.ps1 -OutDir build-a64
#   .\run.ps1 -Headless -Memory 1536 -Image build-a64/mOS.img -Serial build-a64/serial.log
#   -Motores interp,jit (padrão)   -Real <Mach-O arm64 ou universal>: também roda esse binário (forçado arm64) com --version
param([Parameter(Mandatory)][string]$OutDir, [string]$Motores = 'interp,jit', [string]$Real = '', [string]$RealArgs = '--version',
      [int]$SizeMiB = 400)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path (Join-Path $_.FullName 'zig.exe') } | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
& "$PSScriptRoot\build-a64rt.ps1" -Parcial
if ($LASTEXITCODE -ne 0) { exit 1 }
if (-not (Test-Path "$root\build\libSystem-arm64.mos")) { Write-Host 'falta build\libSystem-arm64.mos (tools\build-libsystem-arm64.ps1)' -ForegroundColor Red; exit 1 }
$a64 = "$esp\mOS\Sistema\Biblioteca\a64"
New-Item -ItemType Directory -Force $a64 | Out-Null
Copy-Item "$root\build\a64rt.mos" "$a64\a64rt.mos" -Force
Copy-Item "$root\build\libSystem-arm64.mos" "$a64\libSystem.mos" -Force

$t = "$esp\mOS\Sistema\Programas\a64teste"
New-Item -ItemType Directory -Force $t | Out-Null
$src = "$root\userland\tests-a64rt"
"#include <stdio.h>`nint main(int c, char **v) { printf(""hello %s: argc=%d\n"", v[0], c); return 7; }" | Set-Content -Encoding ascii "$root\build\a64rt-hello.c"
& $zig cc -target aarch64-macos -O1 "$root\build\a64rt-hello.c" -o "$t\hello-arm64" '-Wl,-undefined,dynamic_lookup' 2>&1 | ForEach-Object { "$_" }
& $zig cc -target x86_64-macos -O1 "$root\build\a64rt-hello.c" -o "$t\hello-x86" '-Wl,-undefined,dynamic_lookup' 2>&1 | ForEach-Object { "$_" }
& $zig cc -target x86_64-macos -O1 "$src\exec-from-x86.c" -o "$t\exec-from-x86" '-Wl,-undefined,dynamic_lookup' 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'exec-from-x86: FAILED' -ForegroundColor Red; exit 1 }
& $zig c++ -target aarch64-macos -O1 "$src\main.cpp" -o "$t\a64rt-teste" '-Wl,-undefined,dynamic_lookup' 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'testes arm64: FAILED' -ForegroundColor Red; exit 1 }

$ld = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons"
New-Item -ItemType Directory -Force $ld | Out-Null
Get-ChildItem $ld -Filter 'com.mos.a64teste.*' -ErrorAction SilentlyContinue | Remove-Item -Force
function Write-Job([string]$label, [string[]]$argv, [hashtable]$envs) {
    $a = ($argv | ForEach-Object { "<string>$_</string>" }) -join ''
    $e = ($envs.GetEnumerator() | ForEach-Object { "<key>$($_.Key)</key><string>$($_.Value)</string>" }) -join ''
    @"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>Label</key><string>$label</string>
<key>ProgramArguments</key><array>$a</array>
<key>EnvironmentVariables</key><dict>$e</dict>
<key>RunAtLoad</key><true/>
</dict></plist>
"@ | Set-Content -Encoding ascii "$ld\$label.plist"
}
# hello-x86: posix_spawn arm64→x86. hello-arm64: arm64→arm64. exec-from-x86: x86 faz exec do hello-arm64.
$spawn = '/Sistema/Programas/a64teste/hello-x86:7;/Sistema/Programas/a64teste/hello-arm64:7;/Sistema/Programas/a64teste/exec-from-x86:7'
foreach ($m in $Motores.Split(',')) {
    Write-Job "com.mos.a64teste.$m" @('/Sistema/Programas/a64teste/a64rt-teste') @{ MOS_A64_ENGINE = $m; A64RT_TESTE_SPAWN = $spawn }
}
if ($Real) {
    Copy-Item $Real "$t\real" -Force
    Write-Job 'com.mos.a64teste.real' (@('/Sistema/Programas/a64teste/real') + $RealArgs.Split(' ')) @{ MOS_ARCH = 'arm64' }
}
. "$PSScriptRoot\overlay-teste.ps1"
$ovt = New-OverlayTeste $root $OutDir
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img com os testes do tradutor arm64 no boot ($Motores)" -ForegroundColor Green
