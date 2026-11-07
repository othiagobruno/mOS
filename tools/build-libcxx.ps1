# mOS - compila a libc++ e a libc++abi do LLVM (o código-fonte que vem com o zig, Apache-2.0 com exceção LLVM)
# como um dylib Mach-O x86_64 para macOS: build\libcxx\libc++.1.dylib, instalado na imagem em /usr/lib/libc++.1.dylib.
#
# Por que a libc++ de verdade: os módulos nativos (.node) dos apps Electron foram compilados com os cabeçalhos da
# libc++ da Apple, e o código inline deles mexe direto nos objetos da biblioteca (ios_base, basic_ios, vtables e VTT
# de basic_stringstream, facetas de locale, regex). Só a própria libc++ (ABI v1, configuração Apple) tem o mesmo
# layout. O carregador do mOS (kernel/compat/macho.c) carrega dylibs de caminho absoluto do disco, então a imagem
# só precisa do arquivo; as importações dele (164 funções do libSystem) existem todas no libSystem do mOS.
#
# Exceções: a libc++abi lança pelo _Unwind_RaiseException do libSystem, que ainda não desenrola a pilha
# (devolve _URC_END_OF_STACK): uma exceção C++ não tratada vira std::terminate, como antes.
#
#   .\tools\build-libcxx.ps1            (só recompila se faltar o dylib)
#   .\tools\build-libcxx.ps1 -Force
param([switch]$Force, [int]$Jobs = 8)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zigdir = $zig.FullName; $zig = Join-Path $zigdir 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG; $zigdir = Split-Path $zig } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source; $zigdir = Split-Path $zig }
if (-not $zig) { Write-Host 'zig nao encontrado (.tools\, MOS_ZIG ou PATH)' -ForegroundColor Red; exit 1 }
$out = "$root\build\libcxx"
$dylib = "$out\libc++.1.dylib"
if ((Test-Path $dylib) -and -not $Force) { exit 0 }
New-Item -ItemType Directory -Force $out | Out-Null
Write-Host '==> Building libc++.1.dylib (libc++/libc++abi do LLVM, Mach-O)' -ForegroundColor Cyan

$L = "$zigdir\lib\libcxx"
$A = "$zigdir\lib\libcxxabi"
# o zig não instala __config_site: estas são as definições que ele passaria (configuração padrão da libc++)
$defs = @(
    '-D_LIBCPP_ABI_VERSION=1', '-D_LIBCPP_ABI_NAMESPACE=__1', '-D_LIBCPP_HAS_THREADS=1', '-D_LIBCPP_HAS_MONOTONIC_CLOCK=1',
    '-D_LIBCPP_HAS_TERMINAL=1', '-D_LIBCPP_HAS_MUSL_LIBC=0', '-D_LIBCPP_HAS_THREAD_API_PTHREAD=1', '-D_LIBCPP_HAS_THREAD_API_EXTERNAL=0',
    '-D_LIBCPP_HAS_THREAD_API_WIN32=0', '-D_LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS=0', '-D_LIBCPP_HAS_FILESYSTEM=1',
    '-D_LIBCPP_HAS_RANDOM_DEVICE=1', '-D_LIBCPP_HAS_LOCALIZATION=1', '-D_LIBCPP_HAS_UNICODE=1', '-D_LIBCPP_HAS_WIDE_CHARACTERS=1',
    '-D_LIBCPP_HAS_TIME_ZONE_DATABASE=0', '-D_LIBCPP_INSTRUMENTED_WITH_ASAN=0', '-D_LIBCPP_PSTL_BACKEND_SERIAL=1',
    '-D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE', '-D_LIBCPP_ASSERTION_SEMANTIC_DEFAULT=_LIBCPP_ASSERTION_SEMANTIC_IGNORE',
    '-D_LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS', '-D_LIBCPP_BUILDING_LIBRARY', '-DLIBCXX_BUILDING_LIBCXXABI',
    '-D_LIBCXXABI_BUILDING_LIBRARY', '-D_LIBCPP_DISABLE_NEW_DELETE_DEFINITIONS')
$common = @('-target', 'x86_64-macos.12.0', '-std=c++23', '-O2', '-fPIC', '-fvisibility=default', '-nostdinc++',
            "-I$L\include", "-I$A\include", "-I$L\src", "-I$L\libc") + $defs

# new/delete vêm da libc++abi (stdlib_new_delete.cpp é a cópia da libc++abi, cxa_noexception é a variante sem exceções)
$srcs = @(Get-ChildItem "$L\src\*.cpp", "$L\src\filesystem\*.cpp", "$L\src\ryu\*.cpp", "$A\src\*.cpp") +
        @(Get-Item "$L\src\experimental\keep.cpp", "$L\src\experimental\log_hardening_failure.cpp")
$srcs = $srcs | Where-Object { $_.Name -notin @('cxa_noexception.cpp', 'stdlib_new_delete.cpp', 'int128_builtins.cpp') }

$procs = @()
$objs = @()
foreach ($s in $srcs) {
    $tag = ($s.FullName.Substring($zigdir.Length + 5) -replace '[\\/]', '_')
    $o = "$out\$tag.o"
    $objs += $o
    while (($procs | Where-Object { -not $_.HasExited }).Count -ge $Jobs) { Start-Sleep -Milliseconds 100 }
    $cargs = $common + @('-c', $s.FullName, '-o', $o) | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }
    $procs += Start-Process -FilePath $zig -ArgumentList (@('c++') + $cargs) -NoNewWindow -PassThru -RedirectStandardError "$o.err"
}
$procs | ForEach-Object { $_.WaitForExit() }
$bad = $objs | Where-Object { -not (Test-Path $_) }
if ($bad) {
    foreach ($b in $bad) { Write-Host "falhou: $b" -ForegroundColor Red; Get-Content "$b.err" -TotalCount 5 }
    exit 1
}
& $zig c++ -target x86_64-macos.12.0 -shared -nostdlib++ -o $dylib @objs '-Wl,-install_name,/usr/lib/libc++.1.dylib' 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dylib) -or (Get-Item $dylib).Length -eq 0) { Write-Host 'libc++.1.dylib link FAILED' -ForegroundColor Red; exit 1 }
Remove-Item "$out\*.err" -ErrorAction SilentlyContinue
Write-Host ("OK: build\libcxx\libc++.1.dylib ({0:N0} KiB)" -f ((Get-Item $dylib).Length / 1KB))
