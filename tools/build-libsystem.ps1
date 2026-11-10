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

# libunwind do LLVM (o do zig, Apache-2.0 com excecao LLVM): _Unwind_*/unw_* de verdade (excecoes C++ e ObjC, objc_exc.c).
# C/C++ no modo Apple (x86_64-apple-macos11-elf: as tabelas das imagens Mach-O por _dyld_find_unwind_sections, dlfcn.c),
# .S sem __APPLE__; o .eh_frame do proprio libSystem vem de unwind_ls.c (tools\patch-ehframe.ps1, depois do link).
# Zig 0.14.1: `zig cc -target x86_64-apple-macos11-elf` responde
#   error: unable to parse target query 'x86_64-apple-macos11-elf': UnknownOperatingSystem
# e `zig clang --target=x86_64-apple-macos11-elf` (o que este script chama), com o include do 0.14
# (any-macos-any; any-darwin-any ainda não existe), responde
#   error: backend data layout 'e-m:e-...' does not match expected target description 'e-m:o-...'
# Sem esse include, o clang cai em /usr/include e falta bits/wordsize.h. O contorno, só nesse caso,
# compila C/C++ com --target=x86_64-unknown-none-elf e -D__APPLE__ -D__MACH__ -D__ELF__ (objeto ELF).
$uw = Join-Path $zigdir 'lib\libunwind'
$uwo = "$root\build\libunwind-x86"
New-Item -ItemType Directory -Force $uwo | Out-Null
$dar = Join-Path $zigdir 'lib\libc\include\any-darwin-any'
$mac = Join-Path $zigdir 'lib\libc\include\any-macos-any'
if (Test-Path -LiteralPath $dar) { $sysinc = $dar }
elseif (Test-Path -LiteralPath $mac) { $sysinc = $mac }
else { Write-Host "libunwind: falta any-darwin-any e any-macos-any em $zigdir" -ForegroundColor Red; exit 1 }
$uwCommon = @('-O2', '-g0', '-fPIC', '-mno-red-zone', '-fno-stack-protector', '-funwind-tables',
    '-D_LIBUNWIND_IS_NATIVE_ONLY', '-DNDEBUG', '-w', "-I$uw\include", '-isystem', (Join-Path $zigdir 'lib\include'), '-isystem', $sysinc)
$appleTarget = @('--target=x86_64-apple-macos11-elf')
$fallbackTarget = @('--target=x86_64-unknown-none-elf', '-D__APPLE__', '-D__MACH__', '-D__ELF__')
$useFallback = $false
$probeLog = & $zig clang @appleTarget @uwCommon -std=c11 -c (Join-Path $uw 'src\UnwindLevel1.c') -o (Join-Path $uwo 'probe.o') 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) {
    $text = ($probeLog | Out-String)
    [IO.File]::WriteAllText("$root\build\libunwind-alvo-erro.txt", $text)
    if ($text -match 'data layout' -or $text -match 'UnknownOperatingSystem' -or $text -match 'wordsize\.h') {
        Write-Host 'libunwind: x86_64-apple-macos11-elf falhou neste zig. C/C++ usam x86_64-unknown-none-elf com -D__APPLE__ -D__MACH__ -D__ELF__.' -ForegroundColor Yellow
        Write-Host $text
        $useFallback = $true
        Remove-Item (Join-Path $uwo 'probe.o') -ErrorAction SilentlyContinue
    } else {
        Write-Host $text
        Write-Host 'libunwind (sonda) FAILED' -ForegroundColor Red
        exit 1
    }
}
foreach ($f in 'libunwind.cpp', 'UnwindLevel1.c', 'UnwindLevel1-gcc-ext.c', 'UnwindRegistersSave.S', 'UnwindRegistersRestore.S') {
    $o = "$uwo\$f.o"
    if ((Test-Path $o) -and (Get-Item $o).LastWriteTime -gt (Get-Item -LiteralPath $zig).LastWriteTime -and $f -ne 'UnwindLevel1.c') { $src += $o; continue }
    $x = switch -Wildcard ($f) { '*.cpp' { @('-std=c++17', '-fno-exceptions', '-fno-rtti', '-nostdinc++') } '*.S' { @('-U__APPLE__', '-U__MACH__') } default { @('-std=c11') } }
    $tgt = if ($f -like '*.S' -or -not $useFallback) { $appleTarget } else { $fallbackTarget }
    & $zig clang @tgt @uwCommon @x -c "$uw\src\$f" -o $o 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0 -and $f -like '*.S') {
        & $zig clang --target=x86_64-unknown-none-elf @uwCommon @x -c "$uw\src\$f" -o $o 2>&1 | ForEach-Object { "$_" }
    }
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
