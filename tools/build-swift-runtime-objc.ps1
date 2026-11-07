# mOS - runtime Swift completo (C++) COM interoperabilidade ObjC, para binários Swift da Apple
# (docs/swift-objc-runtime.md, fase 2). Mesmo fonte de tools\build-swift-runtime.ps1 (.tools\swift-src), mas com
# SWIFT_OBJC_INTEROP=1 e os .mm compilados como Objective-C++ (SwiftObject, SwiftValue, ErrorObject...), sobre a
# libobjc do mOS (userland\libsystem\objc.c). Os cabeçalhos objc/*.h vêm do zig (any-darwin-any).
# Saída separada: build\swift\objc\runtime\*.o (a variante sem ObjC em build\swift\full continua igual).
#   .\tools\build-swift-runtime-objc.ps1 [-Only SwiftObject,Metadata]
param([string[]]$Only)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Select-Object -First 1
$zig = Join-Path $zig.FullName 'zig.exe'
$src = "$root\.tools\swift-src"
$out = "$root\build\swift\objc\runtime"
New-Item -ItemType Directory -Force $out | Out-Null
$cfg = "$src\include\swift\Runtime\CMakeConfig.h"
if (-not (Test-Path $cfg)) {
    "#ifndef SWIFT_RUNTIME_CMAKECONFIG_H`n#define SWIFT_RUNTIME_CMAKECONFIG_H`n#define SWIFT_VERSION_MAJOR `"6`"`n#define SWIFT_VERSION_MINOR `"4`"`n#endif" | Set-Content -Encoding ascii $cfg
}
$rt = "$src\stdlib\public\runtime"
$skip = 'ImageInspectionELF', 'ImageInspectionCOFF', 'ImageInspectionStatic', 'ImageInspectionWasm', 'WindowsUnicode', 'Backtrace', 'BacktraceUtils',
        'CrashHandlerMacOS', 'CrashHandlerLinux', 'CrashHandlerWindows', 'SwiftRT-COFF', 'SwiftRT-ELF-WASM', 'Leaks'
$files = @(Get-ChildItem "$rt\*.cpp" | Where-Object { $skip -notcontains $_.BaseName } | ForEach-Object FullName)
$files += Get-ChildItem "$rt\*.mm" | Where-Object { $_.BaseName -ne 'Leaks' } | ForEach-Object FullName
$files += "$src\stdlib\public\CompatibilityOverride\CompatibilityOverride.cpp"
$files += Get-ChildItem "$src\lib\Demangling\*.cpp" | ForEach-Object FullName
$files += Get-ChildItem "$src\stdlib\public\LLVMSupport\*.cpp" | ForEach-Object FullName
$files += Get-ChildItem "$src\lib\Threading\*.cpp" | Where-Object { $_.BaseName -in 'Pthreads', 'ThreadSanitizer' } | ForEach-Object FullName
# stubs: com interop entram também os .mm (ponte de NSString/coleções: SwiftNativeNSXXXBase, FoundationHelpers)
$files += Get-ChildItem "$src\stdlib\public\stubs\*.cpp" | Where-Object { $_.BaseName -notmatch 'OptionalParsing|Android|Windows' } | ForEach-Object FullName
$files += Get-ChildItem "$src\stdlib\public\stubs\*.mm" -ErrorAction SilentlyContinue | ForEach-Object FullName
$files += Get-ChildItem "$src\stdlib\public\stubs\Unicode\*.cpp" -ErrorAction SilentlyContinue | ForEach-Object FullName
if ($Only) { $files = $files | Where-Object { $n = [IO.Path]::GetFileNameWithoutExtension($_); $Only -contains $n } }
$defs = @('-DswiftCore_EXPORTS', '-DSWIFT_RUNTIME', '-DSWIFT_OBJC_INTEROP=1', '-DSWIFT_THREADING_PTHREADS', '-DSWIFT_LIBRARY_EVOLUTION=1',
          '-DSWIFT_TARGET_LIBRARY_NAME=swiftRuntimeCore', '-DSWIFT_STDLIB_HAS_TYPE_PRINTING=1', '-DSWIFT_ENABLE_REFLECTION=1',
          '-DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1', '-DSWIFT_RUNTIME_ENABLE_LEAK_CHECKER=0', '-DSWIFT_STDLIB_HAS_DLADDR',
          '-DSWIFT_STDLIB_OVERRIDABLE_RETAIN_RELEASE=1', '-DSWIFT_STDLIB_HAS_STDIN',
          '-DSWIFT_STDLIB_HAS_ENVIRON', '-DSWIFT_STDLIB_HAS_LOCALE', '-DSWIFT_STDLIB_ENABLE_UNICODE_DATA=1',
          '-DSWIFT_INLINE_NAMESPACE=__runtime', '-DSWIFT_RUNTIME_OS_VERSIONING=0', '-DNDEBUG')
$inc = @("-I$src\include", "-I$src\stdlib\include", "-I$src\stdlib\public\SwiftShims", "-I$src\stdlib\public", "-I$rt",
         "-I$root\userland\sdk\objc\include")   # o que falta dos cabeçalhos privados da Apple (objc-internal.h...)
$fl = @('-target', 'x86_64-macos', '-std=c++20', '-O2', '-fno-exceptions', '-fno-rtti', '-ffunction-sections', '-fno-sanitize=all', '-w', '-c')
$ok = 0; $bad = @()
foreach ($f in $files) {
    $o = "$out\" + (Split-Path (Split-Path $f) -Leaf) + '_' + [IO.Path]::GetFileNameWithoutExtension($f) + '.o'
    $lang = if ($f -like '*.mm') { @('-x', 'objective-c++', '-fobjc-runtime=macosx-14.0') } else { @() }
    $log = & $zig c++ @lang @fl @defs @inc $f -o $o 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { $bad += [IO.Path]::GetFileName($f); $log | Select-String 'error:' | Select-Object -First 3 | ForEach-Object { '  ' + $_.Line.Substring(0, [Math]::Min(240, $_.Line.Length)) } } else { $ok++ }
}
Write-Host "runtime (ObjC): $ok ok, $($bad.Count) com erro: $($bad -join ', ')"
if ($bad.Count) { exit 1 }
