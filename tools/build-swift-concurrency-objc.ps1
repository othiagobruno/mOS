# mOS - _Concurrency (async/await, Task, actors) da variante com interop ObjC (docs/swift-objc-runtime.md, fase 3),
# do fonte oficial (.tools\swift-src\stdlib\public\Concurrency, swift-6.4.2) com o executor global do Darwin
# (DispatchGlobalExecutor + CFExecutor), sobre o libdispatch do libSystem do mOS. Como no CMakeLists.txt do Concurrency.
# Precisa de tools\build-swift-full-objc.ps1, tools\build-swift-runtime-objc.ps1 e tools\build-swift-overlays-objc.ps1
# (Darwin e Dispatch). Saída em build\swift\objc\overlays: _Concurrency.swiftmodule, _Concurrency.o e _Concurrency_*.o (C++).
#   .\tools\build-swift-concurrency-objc.ps1 [-SoSwift] [-SoCxx]
param([switch]$SoSwift, [switch]$SoCxx)
$root = Split-Path $PSScriptRoot -Parent
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*')[0].FullName + '\zig.exe'
$src = "$root\.tools\swift-src"
$cc = "$src\stdlib\public\Concurrency"
$objc = "$root\build\swift\objc"
$out = "$objc\overlays"
$inc = "$objc\sdk\include"
if (-not (Test-Path "$out\Dispatch.swiftmodule")) { Write-Host 'falta o overlay Dispatch (tools\build-swift-overlays-objc.ps1)' -ForegroundColor Red; exit 1 }
$fails = 0

# ---- C++ (o runtime de tarefas, atores, grupos, executor)
if (-not $SoSwift) {
    $files = @('..\CompatibilityOverride\CompatibilityOverride.cpp', 'Actor.cpp', 'AsyncLet.cpp', 'Clock.cpp', 'ConcurrencyHooks.cpp',
               'GlobalExecutor.cpp', 'EmbeddedSupport.cpp', 'Error.cpp', 'ExecutorBridge.cpp', 'ExecutorChecks.cpp', 'Task.cpp',
               'TaskAlloc.cpp', 'TaskStatus.cpp', 'TaskGroup.cpp', 'TaskLocal.cpp', 'ThreadingError.cpp', 'TracingSignpost.cpp',
               'AsyncStream.cpp', 'ExecutorImpl.cpp', 'DispatchGlobalExecutor.cpp', 'CFExecutor.cpp') | ForEach-Object { "$cc\$_" }
    # executor global do mOS (threads de trabalho + prazos) pelos ganchos do Concurrency: o dispatch do mOS ainda não tem threads
    $files += "$root\userland\sdk\swift-objc\Concurrency\mos_executor.cpp"
    $defs = @('-Dswift_Concurrency_EXPORTS', '-DSWIFT_RUNTIME', '-DSWIFT_OBJC_INTEROP=1', '-DSWIFT_THREADING_PTHREADS', '-DSWIFT_LIBRARY_EVOLUTION=1',
              '-DSWIFT_TARGET_LIBRARY_NAME=swift_Concurrency', '-DSWIFT_STDLIB_HAS_TYPE_PRINTING=1', '-DSWIFT_ENABLE_REFLECTION=1',
              '-DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1', '-DSWIFT_STDLIB_HAS_DLADDR', '-DSWIFT_STDLIB_HAS_STDIN',
              '-DSWIFT_STDLIB_HAS_ENVIRON', '-DSWIFT_STDLIB_HAS_LOCALE', '-DSWIFT_INLINE_NAMESPACE=__runtime',
              '-DSWIFT_RUNTIME_OS_VERSIONING=0', '-DSWIFT_CONCURRENCY_USES_DISPATCH=1', '-D__STDC_WANT_LIB_EXT1__=1', '-DNDEBUG')
    $cinc = @("-I$src\include", "-I$src\stdlib\include", "-I$src\stdlib\public\SwiftShims", "-I$src\stdlib\public", "-I$cc",
              "-I$root\userland\sdk\objc\include")
    $fl = @('-target', 'x86_64-macos', '-std=c++20', '-O2', '-fno-exceptions', '-fno-rtti', '-ffunction-sections', '-fno-sanitize=all',
            '-fswift-async-fp=always', '-w', '-c')
    $ok = 0; $bad = @()
    # ThreadingError.cpp (swift::threading::fatal) existe também no runtime: na Apple cada dylib tem a sua cópia; na
    # ligação estática (tools\link-swift-objc.ps1) fica só a do runtime, então esta vai para dylib-only\ (fase 4)
    New-Item -ItemType Directory -Force "$out\dylib-only" | Out-Null
    Remove-Item "$out\_Concurrency_ThreadingError.o" -ErrorAction SilentlyContinue
    foreach ($f in $files) {
        $dir = if ([IO.Path]::GetFileNameWithoutExtension($f) -eq 'ThreadingError') { "$out\dylib-only" } else { $out }
        $o = "$dir\_Concurrency_" + [IO.Path]::GetFileNameWithoutExtension($f) + '.o'
        $log = & $zig c++ @fl @defs @cinc $f -o $o 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) { $bad += [IO.Path]::GetFileName($f); $log | Select-String 'error:' | Select-Object -First 4 | ForEach-Object { '  ' + $_.Line.Substring(0, [Math]::Min(240, $_.Line.Length)) } } else { $ok++ }
    }
    Write-Host "_Concurrency (C++): $ok ok, $($bad.Count) com erro: $($bad -join ', ')"
    if ($bad.Count) { $fails++ }
}

# ---- Swift
if (-not $SoCxx) {
    $gen = "$objc\concurrency-src"
    New-Item -ItemType Directory -Force $gen | Out-Null
    $py = Get-ChildItem "$root\.tools\swift-py\LocalApp" -Recurse -Filter python.exe | Select-Object -First 1
    foreach ($g in 'Task+init.swift.gyb', 'TaskGroup+addTask.swift.gyb', 'Task+immediate.swift.gyb') {
        $dst = "$gen\" + $g.Replace('.gyb', '')
        if (-not (Test-Path $dst)) { & $py.FullName "$src\utils\gyb.py" -o $dst "$cc\$g" }
    }
    $cm = Get-Content "$cc\CMakeLists.txt" -Raw
    $m = [regex]::Match($cm, 'set\(SWIFT_RUNTIME_CONCURRENCY_SWIFT_SOURCES(.*?)\)', 'Singleline')
    $files = @($m.Groups[1].Value -split '\s+' | Where-Object { $_ -like '*.swift' } | ForEach-Object { "$cc\$($_.Replace('/', '\'))" })
    $files += @('DispatchExecutor.swift', 'CFExecutor.swift', 'ExecutorImpl.swift') | ForEach-Object { "$cc\$_" }
    $files += Get-ChildItem $gen -Filter *.swift | ForEach-Object FullName
    $avail = @()
    # versões reais (os @backDeployed do Concurrency precisam delas; achatadas em 10.15 viram erro)
    foreach ($l in Get-Content "$src\utils\availability-macros.def") { if ($l -and -not $l.StartsWith('#')) { $avail += '-define-availability', $l.Trim() } }
    $feat = 'IsolatedAny', 'AllowUnsafeAttribute', 'Extern', 'Macros', 'FreestandingMacros', 'BitwiseCopyable', 'Lifetimes', 'LifetimeDependence',
            'SuppressedAssociatedTypesWithDefaults', 'AddressableParameters', 'AddressableTypes', 'ValueGenerics'
    $a = @('-c', '-target', 'x86_64-apple-macos26', '-module-name', '_Concurrency', '-D', 'SWIFT_CONCURRENCY_USES_DISPATCH', '-parse-stdlib', '-parse-as-library', '-swift-version', '5', '-O',
           '-enable-library-evolution', '-strict-memory-safety', '-enforce-exclusivity=unchecked', '-swift-async-frame-pointer=always',
           '-disable-implicit-concurrency-module-import', '-disable-implicit-string-processing-module-import',
           '-I', $objc, '-I', $out, '-I', "$src\stdlib\public\SwiftShims\swift\shims", '-I', "$cc\InternalShims",
           '-module-cache-path', "$objc\modcache", '-Xcc', "-isystem$inc", '-Xcc', '-DMOS_SWIFT_OBJC=1', '-Xcc', '-Wno-nullability-completeness',
           '-module-link-name', 'swift_Concurrency', '-autolink-force-load',
           '-emit-module-path', "$out\_Concurrency.swiftmodule", '-o', "$out\_Concurrency.o")
    foreach ($x in $feat) { $a += '-enable-experimental-feature', $x }
    $a += $avail + $files
    $rsp = "$out\_Concurrency.rsp"
    ($a | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join "`n" | Set-Content -Encoding ascii $rsp
    $log = & "$($tc.FullName)\usr\bin\swift-frontend.exe" -frontend "@$rsp" 2>&1 | ForEach-Object { "$_" }
    $log | Set-Content -Encoding utf8 "$out\_Concurrency.log"
    $err = $log | Select-String ' error: '
    if ($LASTEXITCODE -ne 0 -or $err) { $err | Select-Object -First 20 | ForEach-Object { "  $($_.Line)" }; Write-Host '_Concurrency (Swift): FALHOU' -ForegroundColor Red; $fails++ }
    else { Write-Host ("_Concurrency (Swift): ok ({0:N0} KB, {1} fontes)" -f ((Get-Item "$out\_Concurrency.o").Length / 1KB), $files.Count) }
}
exit $fails
