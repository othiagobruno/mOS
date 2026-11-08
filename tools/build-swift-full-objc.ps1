# mOS - biblioteca padrão Swift completa COM interoperabilidade ObjC (docs/swift-objc-runtime.md, fase 3), para
# binários Swift da Apple. Mesmo fonte e lista de tools\build-swift-full.ps1, sem -disable-objc-interop e com
# SWIFT_OBJC_INTEROP=1 (ponte de String/coleções com NSString/NSArray, AnyObject ObjC). Runtime C++ correspondente:
# tools\build-swift-runtime-objc.ps1. Saída separada: build\swift\objc\Swift.swiftmodule + swiftCore.o.
# -Arch arm64: slice arm64 em build\swift-arm64\objc (tools\swift-arch.ps1); o padrão x86_64 não muda.
param([switch]$NoEvolution, [string]$Arch = 'x86_64')
$root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\swift-arch.ps1" $Arch
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$swiftc = "$($tc.FullName)\usr\bin\swiftc.exe"
$src = "$root\.tools\swift-src"
$core = "$src\stdlib\public\core"
$shims = "$src\stdlib\public\SwiftShims\swift\shims"
$out = "$SwiftBuild\objc"
New-Item -ItemType Directory -Force "$out\Swift.swiftmodule" | Out-Null
$cm = Get-Content "$core\CMakeLists.txt"
$files = @()
foreach ($raw in $cm) {
    $l = $raw.Trim()
    if ($l -match '^(EMBEDDED|NORMAL)\s+(\S+)') {
        $f = $Matches[2]
        if ($f -match '\$\{|linker-support') { continue }
        $f = $f.Replace('/', '\')
        if ($f -like '*.gyb') { $files += "$src\gen\" + $f.Replace('.gyb', '') } else { $files += "$core\$f" }
    }
}
$files += "$core\SIMDVector.swift"
$files = $files | Select-Object -Unique
$avail = @()
foreach ($l in Get-Content "$src\utils\availability-macros.def") { if ($l -and -not $l.StartsWith('#')) { $avail += '-define-availability', (($l -replace ':.*', '') + ':macOS 10.15') } }
$feat = 'Macros', 'FreestandingMacros', 'Extern', 'BitwiseCopyable', 'ValueGenerics', 'AddressableParameters', 'AddressableTypes',
        'AllowUnsafeAttribute', 'SuppressedAssociatedTypesWithDefaults', 'Reparenting', 'Lifetimes', 'BorrowAndMutateAccessors', 'BorrowInout', 'LifetimeDependence'
$fl = @(); foreach ($x in $feat) { $fl += '-enable-experimental-feature', $x }
$evo = if ($NoEvolution) { @() } else { @('-enable-library-evolution') }
$args = @('-frontend', '-c', '-target', $SwiftTriple, '-module-name', 'Swift', '-parse-stdlib', '-parse-as-library',
          '-nostdimport', '-swift-version', '5', '-O', '-enforce-exclusivity=unchecked', '-I', $shims,
          '-disable-objc-attr-requires-foundation-module', '-Xcc', "-I$root\userland\sdk\objc\include",   # como a stdlib da Apple
          '-strict-memory-safety', '-Xcc', '-Wno-nullability-completeness', '-Xcc', '-DSWIFT_OBJC_INTEROP=1', '-D', 'SWIFT_ENABLE_REFLECTION', '-D', 'SWIFT_RUNTIME_OS_VERSIONING', '-D', 'SWIFT_STDLIB_ENABLE_VECTOR_TYPES', '-D', 'SWIFT_STDLIB_HAS_COMMANDLINE', '-D', 'SWIFT_STDLIB_HAS_STDIN',
          '-emit-module-path', "$out\Swift.swiftmodule\$SwiftModuleTriple.swiftmodule", '-o', "$out\swiftCore.o") + $evo + $fl + $avail + $files
$t0 = Get-Date
$log = & $swiftc @args 2>&1 | ForEach-Object { "$_" }
$log | Set-Content "$out\build.log"
$log | Select-String -Pattern ' error: ' | Select-Object -First 30 | ForEach-Object { $_.Line }
if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$out\swiftCore.o")) { Write-Host "stdlib com ObjC FALHOU ($([int]((Get-Date) - $t0).TotalSeconds)s)" -ForegroundColor Red; exit 1 }
Write-Host ("stdlib com ObjC ok: {0:N0} KB em {1}s ({2} fontes)" -f ((Get-Item "$out\swiftCore.o").Length / 1KB), [int]((Get-Date) - $t0).TotalSeconds, $files.Count)
