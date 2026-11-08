# mOS - compila o libSystem (biblioteca de usuario dos apps do macOS) em build\libSystem.mos
# ELF estatico x86-64 System V, ligado em 0x400000100000 (USER_BASE + 1 MiB). O kernel o embute
# (kernel\compat\libsystem_blob.S) e o mapeia em cada processo Mach-O.
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
if ($zig) { $zigdir = $zig.FullName; $zig = Join-Path $zigdir 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG; $zigdir = Split-Path $zig } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source; $zigdir = Split-Path $zig }
if (-not $zig) { Write-Host 'zig nao encontrado (.tools\, MOS_ZIG ou PATH)' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force "$root\build" | Out-Null
$src = Get-ChildItem "$root\userland\libsystem\*" -Include *.c, *.S | ForEach-Object FullName

# libunwind do LLVM (o do zig, Apache-2.0 com excecao LLVM): _Unwind_*/unw_* de verdade (excecoes C++ e ObjC, objc_exc.c).
# C/C++ no modo Apple (x86_64-apple-macos11-elf: as tabelas das imagens Mach-O por _dyld_find_unwind_sections, dlfcn.c),
# .S sem __APPLE__; o .eh_frame do proprio libSystem vem de unwind_ls.c (tools\patch-ehframe.ps1, depois do link).
$uw = "$zigdir\lib\libunwind"
$uwo = "$root\build\libunwind-x86"
New-Item -ItemType Directory -Force $uwo | Out-Null
$uwflags = @('--target=x86_64-apple-macos11-elf', '-O2', '-g0', '-fPIC', '-mno-red-zone', '-fno-stack-protector', '-funwind-tables',
    '-D_LIBUNWIND_IS_NATIVE_ONLY', '-DNDEBUG', '-w', "-I$uw\include", '-isystem', "$zigdir\lib\include", '-isystem', "$zigdir\lib\libc\include\any-darwin-any")
foreach ($f in 'libunwind.cpp', 'UnwindLevel1.c', 'UnwindLevel1-gcc-ext.c', 'UnwindRegistersSave.S', 'UnwindRegistersRestore.S') {
    $o = "$uwo\$f.o"
    if ((Test-Path $o) -and (Get-Item $o).LastWriteTime -gt (Get-Item $zig).LastWriteTime) { $src += $o; continue }
    $x = switch -Wildcard ($f) { '*.cpp' { @('-std=c++17', '-fno-exceptions', '-fno-rtti', '-nostdinc++') } '*.S' { @('-U__APPLE__', '-U__MACH__') } default { @('-std=c11') } }
    & $zig clang @uwflags @x -c "$uw\src\$f" -o $o 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host "libunwind ($f) FAILED" -ForegroundColor Red; exit 1 }
    $src += $o
}

& $zig cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie `
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header `
    "-I$root\userland\libsystem\kshim" `
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
