# mOS - põe o Zig (build oficial x86_64-macos) na imagem e roda a "escada" da auto-hospedagem no boot
# (docs/auto-hospedagem.md, fase 1). Saída no log serial, linhas "zig:".
#   .\build.ps1 -OutDir build-zig
#   .\tools\stage-zig.ps1 [-OutDir build-zig] [-Degrau 5]
#   .\run.ps1 -Headless -Image build-zig/mOS.img -Serial build-zig/serial.log -Memory 3072 [-Accel whpx]
# O Zig vem de https://ziglang.org/download/0.17.1/zig-x86_64-macos-0.17.1.tar.xz (SHA-256 9af07b20...dbb7),
# extraído em .tools\macos-zig-dl. Vai para /usr/local/zig sem as libcs de outros sistemas (só o Darwin fica).
# O código do mOS que a escada compila (kernel, libSystem) vai para /mos-src.
param([string]$OutDir = 'build-zig', [string]$Zig = '.tools\macos-zig-dl\zig-x86_64-macos-0.17.1',
      [int]$Degrau = 8, [int]$DegrauMin = 1, [switch]$RastrearZig, [int]$SizeMiB = 1024, [string]$Bottles = 'build\brew\shell')
$root = Split-Path $PSScriptRoot -Parent
$esp = Join-Path $root "$OutDir\esp"
$z = Join-Path $root $Zig
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
if (-not (Test-Path "$z\zig")) { Write-Host "falta o zig em $z (ver o cabeçalho deste script)" -ForegroundColor Red; exit 1 }

$dst = "$esp\mOS\usr\local\zig"
if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
New-Item -ItemType Directory -Force $dst | Out-Null
Copy-Item -Force "$z\zig", "$z\LICENSE" $dst
$fora = @('docs', 'build-web', 'lldb', 'libtsan', 'libcxx', 'libcxxabi', 'glibc', 'musl', 'mingw', 'wasi', 'freebsd', 'netbsd', 'openbsd')
robocopy "$z\lib" "$dst\lib" /E /XD @fora /NFL /NDL /NJH /NJS /NP | Out-Null
# headers de libc: só os do Darwin (alvo x86_64-macos); os alvos freestanding/uefi do mOS não usam libc
Get-ChildItem "$dst\lib\libc\include" -Directory | Where-Object { $_.Name -notlike '*darwin*' } | Remove-Item -Recurse -Force

# código que a escada compila: os mesmos caminhos relativos do repositório
$src = "$esp\mOS\mos-src"
if (Test-Path $src) { Remove-Item -Recurse -Force $src }
foreach ($d in 'kernel', 'userland\libsystem', 'userland\sdk\include', 'userland\sdk\swift\MOSBridge', 'assets\fonts', 'assets\certs') {
    robocopy "$root\$d" "$src\$d" /E /XF '*.log' /NFL /NDL /NJH /NJS /NP | Out-Null
}
# do tradutor arm64 o libSystem só inclui headers (a64.h, a64rt_abi.h)
foreach ($d in 'userland\a64', 'userland\a64rt') { robocopy "$root\$d" "$src\$d" '*.h' /NFL /NDL /NJH /NJS /NP | Out-Null }
# um arquivo "nul" (nome reservado do Windows, sobra de redirecionamento) passa pelo robocopy e o mkimg não o abre
Get-ChildItem -LiteralPath $src -Recurse -Force -File | Where-Object { $_.Name -eq 'nul' } |
    ForEach-Object { Remove-Item -LiteralPath ('\\?\' + $_.FullName) -Force }
# headers soltos de userland\ (svcproto.h, sys.h...) que o libSystem inclui com "../"
robocopy "$root\userland" "$src\userland" '*.h' /NFL /NDL /NJH /NJS /NP | Out-Null
# o kernel embute build/libSystem.mos (.incbin em kernel/compat/libsystem_blob.S): o do host, para comparar o kernel
New-Item -ItemType Directory -Force "$src\build" | Out-Null
Copy-Item -Force "$root\build\libSystem.mos" "$src\build\libSystem.mos"
Copy-Item -Force "$root\build\libSystem.mos" "$src\libSystem-host.mos"
[IO.File]::WriteAllText("$src\degrau-max", "$Degrau", [Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$src\degrau-min", "$DegrauMin", [Text.Encoding]::ASCII)
# -RastrearZig: o kernel registra as syscalls de escrita em arquivo dos processos "zig" (proc_syscall, process.c)
$mk = "$esp\mOS\Sistema\Biblioteca\rastrear-arquivos-zig"
if ($RastrearZig) { [IO.File]::WriteAllText($mk, '1', [Text.Encoding]::ASCII) } else { Remove-Item $mk -ErrorAction SilentlyContinue }

# listas de fontes na MESMA ordem do build do host (a ordem do link muda o binário; o glob do bash ordena diferente)
$ls = Get-ChildItem "$root\userland\libsystem\*" -Include *.c, *.S | ForEach-Object { $_.FullName.Substring($root.Length + 1).Replace('\', '/') }
$ks = Get-ChildItem "$root\kernel" -Recurse -Include *.c, *.S | ForEach-Object { $_.FullName.Substring($root.Length + 1).Replace('\', '/') }
[IO.File]::WriteAllText("$src\libsystem-fontes.txt", ($ls -join "`n") + "`n", [Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$src\kernel-fontes.txt", ($ks -join "`n") + "`n", [Text.Encoding]::ASCII)

# referência do host: o libSystem ligado como a escada liga (caminhos relativos, sem o patch-ehframe.ps1 do build normal),
# com os objetos do libunwind que o build-libsystem.ps1 deixou em build\libunwind-x86
$hz = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Select-Object -First 1
$uwo = @('libunwind.cpp', 'UnwindLevel1.c', 'UnwindLevel1-gcc-ext.c', 'UnwindRegistersSave.S', 'UnwindRegistersRestore.S') | ForEach-Object { "build/libunwind-x86/$_.o" }
Push-Location $root
& "$($hz.FullName)\zig.exe" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie `
    -fpie -mno-red-zone -w -Iuserland/libsystem/kshim '-Wl,--image-base=0x400000100000' '-Wl,-e,cx_start' '-Wl,--no-gc-sections' `
    @ls @uwo -o "$src\libSystem-ref.mos" 2>&1 | Select-Object -First 5 | ForEach-Object { "  ref: $_" }
Pop-Location
foreach ($o in $uwo) { Copy-Item -Force "$root\$o" "$src\$(Split-Path $o -Leaf).host" }
# hashes de cada objeto do libSystem compilado no host (degrau 6 da escada compara um a um)
$tmpo = Join-Path $env:TEMP "mos-zig-objs"
New-Item -ItemType Directory -Force $tmpo | Out-Null
$linhas = @()
Push-Location $src
foreach ($f in $ls) {
    $o = Join-Path $tmpo ((Split-Path $f -Leaf) + '.o')
    & "$($hz.FullName)\zig.exe" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib `
        -fpie -mno-red-zone -w -Iuserland/libsystem/kshim -c $f -o $o 2>&1 | Out-Null
    $h = if (Test-Path $o) { (Get-FileHash $o -Algorithm SHA256).Hash.ToLower() } else { 'falhou' }
    $linhas += "$f $h"
}
Pop-Location
[IO.File]::WriteAllText("$src\libsystem-objs.txt", ($linhas -join "`n") + "`n", [Text.Encoding]::ASCII)

# referência do kernel: compilada aqui a partir da própria cópia em mos-src, com as flags do build.ps1 e o mesmo
# build/libSystem.mos que a VM vai embutir (outra sessão pode ter recompilado o build\libSystem.mos do repositório)
Write-Host "kernel de referência no host..." -ForegroundColor Cyan
Push-Location $src
& "$($hz.FullName)\zig.exe" cc -target x86_64-uefi-msvc -O2 -g0 -s '-Wl,/Brepro' -ffreestanding -fno-stack-protector -fno-stack-check -mno-stack-arg-probe `
    -mno-red-zone -fshort-wchar -fno-sanitize=all -fno-strict-aliasing -nostdlib -Ikernel/include -I. -std=gnu11 -w `
    @ks -o 'BOOTX64-host.EFI' 2>&1 | Select-Object -First 5 | ForEach-Object { "  ref: $_" }
Remove-Item 'BOOTX64-host.lib', 'BOOTX64-host.pdb' -ErrorAction SilentlyContinue
Pop-Location

$n = (Get-ChildItem $dst -Recurse -File).Count
Write-Host "zig na imagem: $n arquivos em /usr/local/zig" -ForegroundColor Cyan
& "$PSScriptRoot\stage-bash.ps1" -OutDir $OutDir -Bottles $Bottles -Script 'tools\testdata\zig-escada.sh' -SizeMiB $SizeMiB
exit $LASTEXITCODE
