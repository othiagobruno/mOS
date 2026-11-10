# mOS - compila o libSystem para processos arm64 (tradutor arm64 -> x86_64, docs/tradutor-arm64.md) em
# build\libSystem-arm64.mos: o mesmo formato do build\libSystem.mos (ELF "static-pie" ligado em 0, so relocacoes RELATIVE,
# .symtab com os mesmos nomes, entrada cx_start), mas EM_AARCH64 com R_AARCH64_RELATIVE.
#
# ABI: os apps arm64 do macOS sao compilados com o Darwin PCS da Apple (variadicos na pilha, argumentos pequenos
# estendidos pelo chamador, empacotamento na pilha, x18 reservado, char com sinal, long double = double, va_list =
# char*). O ELF puro com "-target-abi darwinpcs" NAO serve (o va_list e o va_start continuam os do AAPCS: os variadicos
# saem errados). O triplo arm64-apple-macos11-elf e o Darwin inteiro (o clang e o LLVM decidem a ABI pelo SO do triplo)
# com o conteiner ELF (decidido pelo ambiente "-elf"): o codigo gerado e identico ao de -target aarch64-macos.
# Conferido em tools\a64tests\libsys_abi*.c (tools\build-libsystem-arm64.ps1 -AbiCheck compara o assembly dos dois).
#
#   .\tools\build-libsystem-arm64.ps1            # compila (paralelo) e liga
#   .\tools\build-libsystem-arm64.ps1 -AbiCheck  # tambem confere a ABI contra -target aarch64-macos
param([switch]$AbiCheck, [int]$Jobs = 0)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zigDir = Get-Item -LiteralPath $mosZigPick.Dir
$zig = $mosZigPick.Path
$inc = Join-Path $zigDir.FullName 'lib\include'
$ls = "$root\userland\libsystem"
$obj = "$root\build\libsystem-arm64"
New-Item -ItemType Directory -Force $obj | Out-Null
if ($Jobs -le 0) { $Jobs = [Environment]::ProcessorCount }

$triple = 'arm64-apple-macos11-elf'
$cflags = @("--target=$triple", '-mcpu=apple-m1', '-O2', '-g0', '-ffreestanding', '-fno-stack-protector', '-fno-builtin',
    '-fPIC', '-ffixed-x18', '-fsigned-char', '-fno-common', '-funwind-tables',
    '-Wall', '-Wno-unused-parameter', '-Wno-incompatible-library-redeclaration', '-Wno-builtin-requires-header',
    '-Wno-unknown-warning-option', '-Wno-unused-function', "-isystem", $inc, "-I$ls\kshim", "-I$root\userland\third_party\h264bsd",
    "-I$ls\kshim\xz-config",
    "-I$root\userland\third_party\zlib", "-I$root\userland\third_party\lz4", "-I$root\userland\third_party\lzfse",
    "-I$root\userland\third_party\xz", "-I$root\userland\third_party\xz\src\liblzma\api",
    "-I$root\userland\third_party\xz\src\liblzma\common", "-I$root\userland\third_party\xz\src\liblzma\check",
    "-I$root\userland\third_party\xz\src\liblzma\lz", "-I$root\userland\third_party\xz\src\liblzma\rangecoder",
    "-I$root\userland\third_party\xz\src\liblzma\lzma", "-I$root\userland\third_party\xz\src\liblzma\delta",
    "-I$root\userland\third_party\xz\src\liblzma\simple", "-I$root\userland\third_party\xz\src\common",
    "-I$root\userland\third_party\brotli\c\include",
    '-include', "$ls\kshim\math.h",
    '-DHAVE_CONFIG_H', '-DTUKLIB_SYMBOL_PREFIX=lzma_', '-DMOS_LIBSYSTEM')

# os .S x86 tem versao *_arm64.S; os *_arm64.S ficam fora do build x86 por #if defined(__aarch64__)
$x86only = @('objc_msg.S', 'objc_tramp.S', 'sigjmp.S', 'start.S', 'tlv.S')
$src = @(Get-ChildItem "$ls\*" -Include *.c, *.S | Where-Object { $x86only -notcontains $_.Name } | ForEach-Object FullName) + @(Get-ChildItem "$root\userland\third_party\h264bsd\*.c" | ForEach-Object FullName)
$src += "$root\userland\frameworks\libcompression\compression.c"
$src += @('lz4.c','lz4hc.c','lz4frame.c','xxhash.c') | ForEach-Object { "$root\userland\third_party\lz4\$_" }
$src += @('lzfse_decode.c','lzfse_decode_base.c','lzfse_encode.c','lzfse_encode_base.c','lzfse_fse.c','lzvn_decode_base.c','lzvn_encode_base.c') | ForEach-Object { "$root\userland\third_party\lzfse\$_" }
$skip = @('crc_clmul_consts_gen.c','crc32_small.c','crc64_small.c','tuklib_exit.c','tuklib_mbstr_fw.c','tuklib_mbstr_nonprint.c','tuklib_mbstr_width.c','tuklib_mbstr_wrap.c','tuklib_open_stdxxx.c','tuklib_progname.c','stream_decoder_mt.c','stream_encoder_mt.c')
$src += Get-ChildItem "$root\userland\third_party\xz\src\liblzma","$root\userland\third_party\xz\src\common" -Recurse -Filter *.c | Where-Object { $skip -notcontains $_.Name } | ForEach-Object FullName
$src += Get-ChildItem "$root\userland\third_party\brotli\c\common","$root\userland\third_party\brotli\c\dec","$root\userland\third_party\brotli\c\enc" -Filter *.c | ForEach-Object FullName

function Run-Pool($items, $mkargs) {
    $running = @(); $fail = 0
    foreach ($it in $items) {
        while ($running.Count -ge $Jobs) {
            $done = $running | Where-Object { $_.p.HasExited }
            foreach ($d in $done) { if ($d.p.ExitCode -ne 0) { $fail++ }; Show-Log $d }
            $running = @($running | Where-Object { -not $_.p.HasExited })
            if ($running.Count -ge $Jobs) { Start-Sleep -Milliseconds 40 }
        }
        $a = & $mkargs $it
        $log = "$obj\" + [IO.Path]::GetFileName($it) + '.log'
        $p = Start-Process -FilePath $zig -ArgumentList $a -NoNewWindow -PassThru -RedirectStandardError $log -RedirectStandardOutput "$log.out"
        $null = $p.Handle   # sem isto o ExitCode volta vazio no PowerShell 5.1
        $running += [pscustomobject]@{ p = $p; log = $log; name = [IO.Path]::GetFileName($it) }
    }
    foreach ($d in $running) { $d.p.WaitForExit(); if ($d.p.ExitCode -ne 0) { $fail++ }; Show-Log $d }
    return $fail
}
function Show-Log($d) {
    $t = if (Test-Path $d.log) { Get-Content $d.log -Raw } else { '' }
    if ($t -and $t.Trim()) { Write-Host "--- $($d.name)" -ForegroundColor Yellow; Write-Host $t.TrimEnd() }
}
function Q($s) { '"' + $s + '"' }
# aplica $fn(bytes, offset do Elf64_Sym) a cada símbolo da .symtab de um ELF64; devolve quantos $fn mudou
function Edit-Symtab($file, $fn) {
    $b = [IO.File]::ReadAllBytes($file)
    $shoff = [BitConverter]::ToUInt64($b, 40); $shentsize = [BitConverter]::ToUInt16($b, 58); $shnum = [BitConverter]::ToUInt16($b, 60)
    $n = 0
    for ($i = 0; $i -lt $shnum; $i++) {
        $sh = [int]($shoff + $i * $shentsize)
        if ([BitConverter]::ToUInt32($b, $sh + 4) -ne 2) { continue }      # SHT_SYMTAB
        $off = [BitConverter]::ToUInt64($b, $sh + 24); $size = [BitConverter]::ToUInt64($b, $sh + 32); $ent = [BitConverter]::ToUInt64($b, $sh + 56)
        for ($k = 1; $k -lt $size / $ent; $k++) { $n += (& $fn $b ([int]($off + $k * $ent))) }
    }
    [IO.File]::WriteAllBytes($file, $b)
    return $n
}

$fail = Run-Pool $src { param($f) @('clang') + ($cflags | ForEach-Object { if ($_ -match ' ') { Q $_ } else { $_ } }) + @('-c', (Q $f), '-o', (Q ("$obj\" + [IO.Path]::GetFileName($f) + '.o'))) }
if ($fail) { Write-Host "libSystem-arm64: $fail arquivo(s) com erro" -ForegroundColor Red; exit 1 }

# compiler-rt do zig para arm64 (o mesmo que o zig cc liga no libSystem x86: __udivti3, __extendhfsf2..., e a libm que o
# x86 fazia com o x87: sin/cos/tan/fmod/log2/exp2). Gerado como bitcode para o alvo aarch64-macos (long double = double,
# como no Darwin) e compilado com o mesmo triplo ELF. Os símbolos dele são fracos: as definições do libSystem ganham.
$crt = "$obj\compiler_rt.o"
$crtStamp = "$obj\compiler_rt.zigver"
$zv = (& $zig version)
if (-not (Test-Path $crt) -or -not (Test-Path $crtStamp) -or (Get-Content $crtStamp -Raw).Trim() -ne $zv) {
    & $zig build-obj -target aarch64-macos.11.0 -mcpu apple_m1 -O ReleaseFast -fPIC -fno-stack-check `
        (Join-Path $zigDir.FullName 'lib\compiler_rt.zig') -fno-emit-bin "-femit-llvm-bc=$obj\compiler_rt.bc" 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host 'compiler-rt arm64 FAILED' -ForegroundColor Red; exit 1 }
    & $zig clang "--target=$triple" -mcpu=apple-m1 -O2 -fPIC -Wno-override-module -c "$obj\compiler_rt.bc" -o $crt 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host 'compiler-rt arm64 FAILED' -ForegroundColor Red; exit 1 }
    # o compiler-rt sai com visibilidade hidden, e o lld aplica a mais restrita: o memcpy do libSystem virava local
    $n = Edit-Symtab $crt { param($b, $s) if ($b[$s + 5] -band 3) { $b[$s + 5] = [byte]($b[$s + 5] -band 0xFC); 1 } else { 0 } }
    Write-Host "compiler-rt arm64: $n simbolo(s) hidden -> default"
    Set-Content $crtStamp $zv
}

# libunwind do LLVM (o do zig, Apache-2.0 com exceção LLVM): _Unwind_* / unw_* de verdade para exceções C++/ObjC/Swift.
# C/C++ no modo Apple (__APPLE__: compact unwind + DWARF das imagens Mach-O por _dyld_find_unwind_sections, dlfcn.c);
# os .S sem __APPLE__ (as diretivas de seção do Mach-O não montam em ELF). O .eh_frame do próprio libSystem vem de
# cx_ls_unwind_sections (darwin_arm64.c), com o deslocamento gravado abaixo depois do link.
$uw = Join-Path $zigDir.FullName 'lib\libunwind'
$uwflags = @("--target=$triple", '-mcpu=apple-m1', '-O2', '-g0', '-fPIC', '-ffixed-x18', '-fno-common', '-funwind-tables',
    '-D_LIBUNWIND_IS_NATIVE_ONLY', '-DNDEBUG', '-w', "-I$uw\include", '-isystem', $inc, '-isystem', (Join-Path $zigDir.FullName 'lib\libc\include\any-darwin-any'))
$uwsrc = @('libunwind.cpp', 'UnwindLevel1.c', 'UnwindLevel1-gcc-ext.c', 'UnwindRegistersSave.S', 'UnwindRegistersRestore.S')
$fail = Run-Pool $uwsrc { param($f)
    $x = switch -Wildcard ($f) { '*.cpp' { @('-std=c++17', '-fno-exceptions', '-fno-rtti', '-nostdinc++') } '*.S' { @('-U__APPLE__', '-U__MACH__') } default { @('-std=c11') } }
    @('clang') + ($uwflags | ForEach-Object { if ($_ -match ' ') { Q $_ } else { $_ } }) + $x + @('-c', (Q "$uw\src\$f"), '-o', (Q "$obj\uw_$f.o")) }
if ($fail) { Write-Host 'libunwind arm64 FAILED' -ForegroundColor Red; exit 1 }

$objs = @($src | ForEach-Object { "$obj\" + [IO.Path]::GetFileName($_) + '.o' }) + @($crt) + @($uwsrc | ForEach-Object { "$obj\uw_$_.o" })
$rsp = "$obj\link.rsp"
[IO.File]::WriteAllLines($rsp, ($objs | ForEach-Object { '"' + ($_ -replace '\\', '/') + '"' }))
& $zig ld.lld -m aarch64elf -pie --no-dynamic-linker -z notext -e cx_start --no-gc-sections --hash-style=sysv `
    --pack-dyn-relocs=none -z max-page-size=0x4000 -z separate-loadable-segments "@$rsp" -o "$root\build\libSystem-arm64.mos" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'libSystem-arm64: link FAILED' -ForegroundColor Red; exit 1 }

# Os símbolos fracos que sobraram (os do compiler-rt que o libSystem não define: sin, __udivti3...) viram GLOBAL na .symtab:
# o carregador só indexa GLOBAL (ver a memória "weak não é exportado"), e no blob x86 eles saem fortes.
$out = "$root\build\libSystem-arm64.mos"
$nweak = Edit-Symtab $out { param($b, $s)
    $info = $b[$s + 4]
    if (($info -shr 4) -eq 2 -and [BitConverter]::ToUInt16($b, $s + 6) -ne 0) { $b[$s + 4] = [byte]((1 -shl 4) -bor ($info -band 15)); 1 } else { 0 } }

# .eh_frame do blob -> cx_ls_ehframe[2..3] (deslocamento a partir do cabeçalho ELF, tamanho), achado pelo marcador
$b = [IO.File]::ReadAllBytes($out)
$shoff = [BitConverter]::ToUInt64($b, 40); $shentsize = [BitConverter]::ToUInt16($b, 58); $shnum = [BitConverter]::ToUInt16($b, 60)
$shstr = [BitConverter]::ToUInt16($b, 62)
$strOff = [BitConverter]::ToUInt64($b, [int]($shoff + $shstr * $shentsize + 24))
$ehAddr = 0; $ehSize = 0
for ($i = 0; $i -lt $shnum; $i++) {
    $sh = [int]($shoff + $i * $shentsize)
    $no = [int]($strOff + [BitConverter]::ToUInt32($b, $sh))
    $e = $no; while ($b[$e]) { $e++ }
    if ([Text.Encoding]::ASCII.GetString($b, $no, $e - $no) -eq '.eh_frame') { $ehAddr = [BitConverter]::ToUInt64($b, $sh + 16); $ehSize = [BitConverter]::ToUInt64($b, $sh + 32) }
}
$magic = [BitConverter]::GetBytes([UInt64]0x4d4f5345485f4652) + [BitConverter]::GetBytes([UInt64]0x4152344d34365f41)
$hit = -1
for ($k = 0; $k -le $b.Length - 32; $k += 8) {
    $m = $true; for ($j = 0; $j -lt 16; $j++) { if ($b[$k + $j] -ne $magic[$j]) { $m = $false; break } }
    if ($m) { $hit = $k; break }
}
if ($hit -lt 0 -or -not $ehSize) { Write-Host 'libSystem-arm64: .eh_frame ou marcador cx_ls_ehframe nao encontrado' -ForegroundColor Red; exit 1 }
[BitConverter]::GetBytes([UInt64]$ehAddr).CopyTo($b, $hit + 16)
[BitConverter]::GetBytes([UInt64]$ehSize).CopyTo($b, $hit + 24)
[IO.File]::WriteAllBytes($out, $b)
Write-Host ("eh_frame: +0x{0:X} ({1:N0} bytes)" -f $ehAddr, $ehSize)

# conferências do formato que o carregador espera: só R_AARCH64_RELATIVE, nenhum símbolo indefinido
$llvm = @("$env:ProgramFiles\LLVM\bin") | Where-Object { Test-Path "$_\llvm-readobj.exe" } | Select-Object -First 1
if ($llvm) {
    $rel = & "$llvm\llvm-readobj.exe" -r $out | Select-String 'R_AARCH64_' | ForEach-Object { ($_ -split '\s+')[2] } | Group-Object | ForEach-Object { "$($_.Name)=$($_.Count)" }
    $und = & "$llvm\llvm-nm.exe" -u $out
    Write-Host "relocacoes: $($rel -join ' ')  simbolos fracos promovidos: $nweak  indefinidos: $(@($und).Count)"
    if (@($und).Count) { Write-Host ($und -join "`n") -ForegroundColor Red; exit 1 }
    if ($rel | Where-Object { $_ -notmatch '^R_AARCH64_RELATIVE=' }) { Write-Host 'relocacao fora de R_AARCH64_RELATIVE' -ForegroundColor Red; exit 1 }
}
$sz = (Get-Item "$root\build\libSystem-arm64.mos").Length
Write-Host ("build\libSystem-arm64.mos: {0:N0} KiB" -f ($sz / 1KB)) -ForegroundColor Green

if ($AbiCheck) { & "$PSScriptRoot\check-abi-arm64.ps1"; if ($LASTEXITCODE -ne 0) { exit 1 } }
