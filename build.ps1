# mOS - build script (Windows / PowerShell 5.1+)
#   .\build.ps1          builds the kernel and creates $OutDir\mOS.img + build\esp\
#   .\build.ps1 -Run     builds and boots in QEMU
#   .\build.ps1 -Clean   removes build\
#   .\build.ps1 -Tests   also runs the user-mode self-test (/Sistema/Programas/teste) at boot
#   .\build.ps1 -UnitTests  kernel unit tests at boot (-DMOS_UNIT_TESTS: RAM disks only; results on the serial log as "unit:")
param([switch]$Run, [switch]$Clean, [switch]$Debug, [string]$OutDir = 'build', [switch]$Tests, [switch]$UnitTests, [string]$MonitorExtra = '')
$ErrorActionPreference = 'Continue'
$root = $PSScriptRoot
Set-Location $root

$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Error "Zig not found. Download from https://ziglang.org/download/ and extract into .tools\"; exit 1 }

if ($Clean) { Remove-Item -Recurse -Force "$root\$OutDir" -ErrorAction SilentlyContinue; exit 0 }
New-Item -ItemType Directory -Force "$root\$OutDir" | Out-Null

$sources = Get-ChildItem "$root\kernel" -Recurse -Include *.c, *.S | ForEach-Object { $_.FullName.Substring($root.Length + 1) }
$opt = if ($Debug) { '-O1' } else { '-O2' }
$cflags = @(
    '-target', 'x86_64-uefi-msvc', $opt, '-g0',
    '-ffreestanding', '-fno-stack-protector', '-fno-stack-check', '-mno-stack-arg-probe',
    '-mno-red-zone', '-fshort-wchar', '-fno-sanitize=all', '-fno-strict-aliasing',
    '-nostdlib', '-Ikernel/include', '-I.', '-std=gnu11',
    '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-unused-function', '-Wno-sign-compare',
    '-Wno-missing-field-initializers', '-Wno-unused-command-line-argument'
)
if ($UnitTests) { $cflags += '-DMOS_UNIT_TESTS' }
Write-Host "==> Building libSystem.mos (apps do macOS)" -ForegroundColor Cyan
& "$root\tools\build-libsystem.ps1"
if ($LASTEXITCODE -ne 0) { Write-Host "libSystem build FAILED" -ForegroundColor Red; exit 1 }
Write-Host "==> Building the Nucleo kernel ($($sources.Count) files)" -ForegroundColor Cyan
& $zig cc @cflags @sources -o $OutDir/BOOTX64.EFI 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host "kernel build FAILED" -ForegroundColor Red; exit 1 }
Remove-Item $OutDir/BOOTX64.lib, $OutDir/BOOTX64.pdb -ErrorAction SilentlyContinue

if (-not (Test-Path $OutDir/mkimg.exe) -or (Get-Item tools/mkimg.c).LastWriteTime -gt (Get-Item $OutDir/mkimg.exe).LastWriteTime -or
    (Get-Item kernel/include/ed25519_impl.h).LastWriteTime -gt (Get-Item $OutDir/mkimg.exe).LastWriteTime) {
    Write-Host "==> Building mkimg" -ForegroundColor Cyan
    & $zig cc -O2 -target x86_64-windows-gnu tools/mkimg.c -o $OutDir/mkimg.exe -lbcrypt 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host "mkimg build FAILED" -ForegroundColor Red; exit 1 }
}

# ESP tree: EFI\BOOT\BOOTX64.EFI + \mOS\ (system and user files)
$esp = "$root\$OutDir\esp"
Remove-Item -Recurse -Force $esp -ErrorAction SilentlyContinue
if (Test-Path $esp) { Start-Sleep -Milliseconds 500; Remove-Item -Recurse -Force $esp -ErrorAction SilentlyContinue }
# fail-closed: um staging que sobrou (arquivo em uso, resíduo de build antigo, contas/logs de uma sessão) NUNCA pode virar payload
if (Test-Path $esp) { Write-Host "ERRO: build\esp não pôde ser apagado por inteiro (arquivo em uso?). Feche o que o estiver usando e rode de novo; o build não continua com um staging sujo." -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force "$esp\EFI\BOOT", "$esp\mOS" | Out-Null
Copy-Item $OutDir/BOOTX64.EFI "$esp\EFI\BOOT\BOOTX64.EFI" -Force
# o conteúdo de root\mOS para dentro de esp\mOS (com "root\mOS" sem \*, uma esp\mOS que sobrou viraria esp\mOS\mOS)
Copy-Item -Recurse -Force "$root\root\mOS\*" "$esp\mOS\"
Remove-Item -Recurse -Force "$esp\mOS\mOS" -ErrorAction SilentlyContinue   # resto de builds antigos
# root\mOS\Users só com pastas vazias (sobra de quando os exemplos moravam ali; hoje estão em root-live\mOS\Users, e o git
# não apaga pastas que ficaram presas): fica fora do staging. Com qualquer arquivo dentro ela vai, e o mkimg recusa a imagem.
if ((Test-Path "$esp\mOS\Users") -and -not (Get-ChildItem "$esp\mOS\Users" -Recurse -File -Force -ErrorAction SilentlyContinue)) {
    Write-Host "aviso: root\mOS\Users só tem pastas vazias; fora da imagem (os exemplos ficam em root-live\mOS\Users)" -ForegroundColor Yellow
    Remove-Item -Recurse -Force "$esp\mOS\Users"
}

# device firmware (Intel Wi-Fi/Bluetooth from linux-firmware, redistributable; licenses alongside)
if (Test-Path "$root\firmware") {
    New-Item -ItemType Directory -Force "$esp\mOS\Sistema\Firmware" | Out-Null
    Copy-Item -Recurse "$root\firmware\*" "$esp\mOS\Sistema\Firmware\"
}

# libc++ do LLVM como /usr/lib/libc++.1.dylib (módulos nativos dos apps Electron; tools\build-libcxx.ps1)
& "$root\tools\build-libcxx.ps1"
if ($LASTEXITCODE -ne 0) { Write-Host 'libc++ build FAILED' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force "$esp\mOS\usr\lib" | Out-Null
Copy-Item "$root\build\libcxx\libc++.1.dylib" "$esp\mOS\usr\lib\libc++.1.dylib" -Force
# SQLite como /usr/lib/libsqlite3.dylib (WhatsApp, Core Data; tools\build-sqlite.ps1, se o amálgama estiver em .tools)
& "$root\tools\build-sqlite.ps1"
if ($LASTEXITCODE -ne 0) { Write-Host 'libsqlite3 build FAILED' -ForegroundColor Red; exit 1 }
if (Test-Path "$root\build\sqlite\libsqlite3.dylib") { Copy-Item "$root\build\sqlite\libsqlite3.dylib" "$esp\mOS\usr\lib\libsqlite3.dylib" -Force }
# ICU4C como /usr/lib/libicucore.A.dylib, com os dados em /usr/share/icu (Intl do Claude Code; tools\build-icu.ps1, se o
# código-fonte estiver em .tools\icu-dl). O carregador prefere esta à parte de IDN do libSystem (kernel/compat/macho.c).
& "$root\tools\build-icu.ps1"
if ($LASTEXITCODE -ne 0) { Write-Host 'libicucore build FAILED' -ForegroundColor Red; exit 1 }
if (Test-Path "$root\build\icu\libicucore.A.dylib") {
    Copy-Item "$root\build\icu\libicucore.A.dylib" "$esp\mOS\usr\lib\libicucore.A.dylib" -Force
    New-Item -ItemType Directory -Force "$esp\mOS\usr\share\icu" | Out-Null
    Copy-Item "$root\build\icu\icudt74l.dat" "$esp\mOS\usr\share\icu\icudt74l.dat" -Force
    Copy-Item "$root\build\icu\LICENSE" "$esp\mOS\usr\share\icu\LICENSE" -Force -ErrorAction SilentlyContinue
}
# zlib (licença zlib) do bottle do Homebrew em /usr/local/opt/zlib/lib, para onde o carregador manda /usr/lib/libz.1.dylib
# (kernel/compat/macho.c). Módulos nativos a ligam: o file_service.node do Cursor (busca de arquivos) caía sem crc32/inflate.
$zl = Get-ChildItem "$root\build\brew\x\zlib\*\lib\libz.1.*.dylib" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zl) {
    New-Item -ItemType Directory -Force "$esp\mOS\usr\local\opt\zlib\lib" | Out-Null
    Copy-Item $zl.FullName "$esp\mOS\usr\local\opt\zlib\lib\libz.1.dylib" -Force
}
# WebKit.framework do mOS (userland\frameworks\WebKit): WKWebView sobre o motor web do sistema (SYS_WEB, kernel/web/webview.c)
$wkdir = "$esp\mOS\System\Library\Frameworks\WebKit.framework\Versions\A"
New-Item -ItemType Directory -Force $wkdir | Out-Null
& $zig cc -target x86_64-macos -O2 -shared -x objective-c "-I$root\userland\sdk\include" -Wno-objc-root-class -Wno-objc-method-access `
    "$root\userland\frameworks\WebKit\WebKit.m" -o "$wkdir\WebKit" '-Wl,-install_name,/System/Library/Frameworks/WebKit.framework/Versions/A/WebKit' `
    '-Wl,-undefined,dynamic_lookup' 2>&1 | Select-String ' error' | ForEach-Object { "  $($_.Line)" }
if (-not (Test-Path "$wkdir\WebKit")) { Write-Host 'WebKit.framework build FAILED' -ForegroundColor Red; exit 1 }
# tradutor arm64 (docs/tradutor-arm64.md): o runtime a64rt.mos (tools\build-a64rt.ps1) e o libSystem arm64
# (tools\build-libsystem-arm64.ps1) em /Sistema/Biblioteca/a64; sem eles os apps só arm64 dizem o que falta
$a64 = "$esp\mOS\Sistema\Biblioteca\a64"
& "$root\tools\build-a64rt.ps1" -Parcial
if ($LASTEXITCODE -ne 0) { Write-Host 'aviso: a64rt.mos não compilou; apps arm64 ficam sem o tradutor' -ForegroundColor Yellow }
if ((Test-Path "$root\build\a64rt.mos") -and (Test-Path "$root\build\libSystem-arm64.mos")) {
    New-Item -ItemType Directory -Force $a64 | Out-Null
    Copy-Item "$root\build\a64rt.mos" "$a64\a64rt.mos" -Force
    Copy-Item "$root\build\libSystem-arm64.mos" "$a64\libSystem.mos" -Force
}
# bibliotecas Mach-O universais (x86_64 + arm64) por cima das só x86_64: o carregador escolhe o slice do processo
# (tools\build-libs-arm64.ps1: libc++, sqlite, ICU, WebKit.framework)
& "$root\tools\build-libs-arm64.ps1"
if ($LASTEXITCODE -ne 0) { Write-Host 'aviso: slices arm64 das bibliotecas incompletos; apps arm64 ficam sem elas' -ForegroundColor Yellow }
if (Test-Path "$root\build\universal") { Copy-Item "$root\build\universal\*" "$esp\mOS\" -Recurse -Force }
# ferramentas de linha de comando do macOS em Mach-O (userland\cli-macos\<nome>.c -> /usr/bin/<nome>): open(1)
New-Item -ItemType Directory -Force "$esp\mOS\usr\bin" | Out-Null
foreach ($c in Get-ChildItem "$root\userland\cli-macos" -Filter *.c -ErrorAction SilentlyContinue) {
    & $zig cc -target x86_64-macos -O2 $c.FullName -o "$esp\mOS\usr\bin\$($c.BaseName)" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host "/usr/bin/$($c.BaseName) FAILED" -ForegroundColor Red; exit 1 }
}

# user-mode programs (ring 3): userland\*.c -> \mOS\Sistema\Programas\<name> (static ELF64 at 0x400000000000)
$progs = Get-ChildItem "$root\userland" -Filter *.c -ErrorAction SilentlyContinue
if ($progs) {
    Write-Host "==> Building $($progs.Count) user program(s)" -ForegroundColor Cyan
    $pdir = "$esp\mOS\Sistema\Programas"
    New-Item -ItemType Directory -Force $pdir | Out-Null
    foreach ($p in $progs) {
        & $zig cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -nostdlib -fno-stack-protector -fno-pic -mcmodel=large -static `
            '-Wl,--image-base=0x400000000000' '-Wl,-e,_start' -Wno-unused-function -Iuserland -Ikernel/include $p.FullName -o "$pdir\$($p.BaseName)" 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) { Write-Host "user program $($p.Name) FAILED" -ForegroundColor Red; exit 1 }
    }
    # apps Cocoa de exemplo (userland\samples) -> \mOS\Aplicativos\<Nome>.app (Mach-O, rodam pelo AppKit do mOS)
Write-Host "==> Building Cocoa sample apps" -ForegroundColor Cyan
& "$root\tools\build-samples.ps1" -Dest "$esp\mOS\Aplicativos"
if ($LASTEXITCODE -ne 0) { Write-Host "sample apps FAILED" -ForegroundColor Red; exit 1 }
Write-Host "==> Building Swift / SwiftUI apps" -ForegroundColor Cyan
& "$root\tools\build-swift-apps.ps1" -Dest "$esp\mOS\Aplicativos"
if ($LASTEXITCODE -ne 0) { Write-Host "Swift apps FAILED" -ForegroundColor Red; exit 1 }
# -Tests: the kernel runs /Sistema/Programas/teste at boot (output on the serial log)
    if ($Tests) {
        Set-Content -Path "$pdir\.autoteste" -Value 'teste' -Encoding ascii
        # teste POSIX em Mach-O (fork/execvp/waitpid, dup2, links, #!): o "teste" nativo o chama no fim
        & $zig cc -target x86_64-macos -O1 "$root\userland\tests-posix\main.c" -o "$pdir\posixteste" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) { Write-Host "posixteste FAILED" -ForegroundColor Red; exit 1 }
        # libdispatch em C (filas, grupos, fontes, semáforos, isa OS_dispatch_*): o "teste" também o chama
        & $zig cc -target x86_64-macos -O1 "$root\userland\tests-dispatch\main.c" -o "$pdir\dispatchteste" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) { Write-Host "dispatchteste FAILED" -ForegroundColor Red; exit 1 }
        # sinais POSIX (tratadores, máscaras, sigsuspend, sigaltstack, EINTR/SA_RESTART, SIGCHLD): o "teste" também o chama
        & $zig cc -target x86_64-macos -O1 "$root\userland\tests-signals\main.c" -o "$pdir\sinaisteste" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) { Write-Host "sinaisteste FAILED" -ForegroundColor Red; exit 1 }
        # simulated Wi-Fi radio + access point (kernel/wifi/sim.c); boot with run.ps1 -NoNet to test it end to end
        Set-Content -Path "$esp\mOS\Sistema\Biblioteca\wifi-simulado" -Value 'mOS-Teste' -Encoding ascii
    }
    # -MonitorExtra 1280x800: força a 2ª saída do virtio-gpu (testes sem janela, run.ps1 -Vga virtio -Monitores 2)
    if ($MonitorExtra) {
        Set-Content -Path "$esp\mOS\Sistema\Biblioteca\monitor-extra" -Value $MonitorExtra -Encoding ascii
    }
}

Write-Host "==> Creating GPT/FAT32 disk image" -ForegroundColor Cyan
# mkimg REJEITA staging contaminado (contas, logs, preferências, pastas pessoais, estado) e gera o manifesto SHA-256 do payload;
# os exemplos de fábrica (root-live\mOS\Users) entram só como overlay Live, fora do payload e do manifesto
Remove-Item $OutDir/mOS.img -ErrorAction SilentlyContinue
# Atualização sem reinstalar (kernel/fs/update.c): o manifesto é assinado com Ed25519 pela chave do repositório,
# tools\chave-atualizacao.semente (MOS_CHAVE_ATUALIZACAO aponta outra). Sistemas instalados com esta imagem só aceitam pacotes
# desta chave. $OutDir\manifesto.txt + $OutDir\esp formam o pacote que tools\servir-atualizacao.ps1 entrega por HTTP.
$key = if ($env:MOS_CHAVE_ATUALIZACAO) { $env:MOS_CHAVE_ATUALIZACAO } else { Join-Path $root 'tools\chave-atualizacao.semente' }
New-Item -ItemType Directory -Force (Split-Path $key) | Out-Null
$rev = (& git -C $root rev-parse --short HEAD 2>$null)
if (-not $rev) { $rev = 'local' }
$versao = "0.1-$(Get-Date -Format 'yyyyMMdd.HHmm')-$rev"
& $OutDir/mkimg.exe $OutDir/mOS.img $esp 128 --live-overlay "$root\root-live" --key $key --manifest-out "$OutDir/manifesto.txt" --versao $versao
if ($LASTEXITCODE -ne 0) { Write-Host "image creation FAILED" -ForegroundColor Red; exit 1 }
$k = (Get-Item $OutDir/BOOTX64.EFI).Length / 1KB
Write-Host ("OK: $OutDir\mOS.img (kernel {0:N0} KiB). Flash it to a USB stick with Rufus (DD mode) or balenaEtcher." -f $k) -ForegroundColor Green

if ($Run) { & "$root\run.ps1" -Image "$OutDir/mOS.img" }
