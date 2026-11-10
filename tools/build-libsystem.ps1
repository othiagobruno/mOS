# mOS - compila o libSystem (biblioteca de usuario dos apps do macOS) em build\libSystem.mos
# ELF estatico x86-64 System V, ligado em 0x400000100000 (USER_BASE + 1 MiB). O kernel o embute
# (kernel\compat\libsystem_blob.S) e o mapeia em cada processo Mach-O.
$root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\stage-host.ps1"
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
$zigdir = $mosZigPick.Dir
New-Item -ItemType Directory -Force "$root\build" | Out-Null
$src = Get-ChildItem "$root\userland\libsystem\*" -Include *.c, *.S | ForEach-Object FullName

# x86_64: _Unwind_* e a personality C++ estão em userland/libsystem/cxx_unwind.c.
# Não ligar os .o do libunwind do zig: os nomes _Unwind_* batem. arm64 segue em build-libsystem-arm64.ps1.

& $zig cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie `
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header `
    "-I$root\userland\libsystem\kshim" "-I$root\userland\libsystem\fsinc" `
    "-isystem", (Join-Path $zigdir 'lib\include') `
    "-Wl,--image-base=0x400000100000" "-Wl,-e,cx_start" "-Wl,--no-gc-sections" `
    $src -o "$root\build\libSystem.mos" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'libSystem build FAILED' -ForegroundColor Red; exit 1 }
& "$PSScriptRoot\patch-ehframe.ps1" "$root\build\libSystem.mos" | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host 'libSystem: .eh_frame nao gravado' -ForegroundColor Red; exit 1 }

# o .incbin em kernel\compat\libsystem_blob.S nao entra na chave de cache do zig: grava o hash do
# libSystem no proprio .S para o kernel ser recompilado sempre que a biblioteca mudar
$hash = (Get-FileHash "$root\build\libSystem.mos" -Algorithm SHA256).Hash.Substring(0, 16)
$blob = "$root\kernel\compat\libsystem_blob.S"
$text = [IO.File]::ReadAllText($blob)
$line = "/* libSystem.mos sha256: $hash */"
if ($text -notmatch [regex]::Escape($line)) {
    $text = [regex]::Replace($text, '(?m)^/\* libSystem\.mos sha256: [0-9A-F]+ \*/\r?\n?', '')
    [IO.File]::WriteAllText($blob, $line + "`n" + $text, [Text.Encoding]::ASCII)
}
