# mOS - oráculo do tradutor arm64: compila o monitor bare-metal (build\a64oracle\mon.elf)
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force "$root\build\a64oracle" | Out-Null
& $zig cc -target aarch64-freestanding-none -mcpu=cortex_a76 -O2 -g0 -ffreestanding -nostdlib -static -fno-pic `
    -fno-stack-protector -mgeneral-regs-only -fno-builtin -Wall -Wno-unused-function `
    "-Wl,-T,$PSScriptRoot\mon.ld" "$PSScriptRoot\mon.S" "$PSScriptRoot\mon.c" -o "$root\build\a64oracle\mon.elf" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'monitor do oraculo FAILED' -ForegroundColor Red; exit 1 }
Write-Host "ok: build\a64oracle\mon.elf"
