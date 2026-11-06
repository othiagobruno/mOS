# mOS - compila o libSystem (biblioteca de usuario dos apps do macOS) em build\libSystem.mos
# ELF estatico x86-64 System V, ligado em 0x400000100000 (USER_BASE + 1 MiB). O kernel o embute
# (kernel\compat\libsystem_blob.S) e o mapeia em cada processo Mach-O.
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado (.tools\, MOS_ZIG ou PATH)' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force "$root\build" | Out-Null
$src = Get-ChildItem "$root\userland\libsystem\*" -Include *.c, *.S | ForEach-Object FullName
& $zig cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie `
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header `
    "-Wl,--image-base=0x400000100000" "-Wl,-e,cx_start" "-Wl,--no-gc-sections" `
    $src -o "$root\build\libSystem.mos" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'libSystem build FAILED' -ForegroundColor Red; exit 1 }

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