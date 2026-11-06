# mOS - biblioteca padrão Swift COMPLETA (não Embedded: metadados, genéricos em runtime, `any P`, casts)
# para x86_64-apple-macos, do fonte oficial em .tools\swift-src (swift-6.4.2-RELEASE), sem interoperabilidade
# ObjC (como no Linux). Saída: build\swift\full\Swift.swiftmodule + swiftCore.o (+ o runtime C++ em
# tools\build-swift-runtime.ps1). É o passo 1 para o SwiftUI completo (OpenSwiftUI) no mOS.
param([switch]$NoEvolution)
$root = Split-Path $PSScriptRoot -Parent
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$swiftc = "$($tc.FullName)\usr\bin\swiftc.exe"
$src = "$root\.tools\swift-src"
$core = "$src\stdlib\public\core"
$shims = "$src\stdlib\public\SwiftShims\swift\shims"
$out = "$root\build\swift\full"
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
$args = @('-frontend', '-c', '-target', 'x86_64-apple-macos26', '-module-name', 'Swift', '-parse-stdlib', '-parse-as-library',
          '-nostdimport', '-swift-version', '5', '-O', '-disable-objc-interop', '-enforce-exclusivity=unchecked', '-I', $shims,
          '-strict-memory-safety', '-Xcc', '-Wno-nullability-completeness', '-Xcc', '-DSWIFT_OBJC_INTEROP=0', '-D', 'SWIFT_ENABLE_REFLECTION', '-D', 'SWIFT_STDLIB_ENABLE_VECTOR_TYPES', '-D', 'SWIFT_STDLIB_HAS_COMMANDLINE', '-D', 'SWIFT_STDLIB_HAS_STDIN',
          '-emit-module-path', "$out\Swift.swiftmodule\x86_64-apple-macos.swiftmodule", '-o', "$out\swiftCore.o") + $evo + $fl + $avail + $files
$t0 = Get-Date
$log = & $swiftc @args 2>&1 | ForEach-Object { "$_" }
$log | Set-Content "$out\build.log"
$log | Select-String -Pattern ' error: ' | Select-Object -First 30 | ForEach-Object { $_.Line }
if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$out\swiftCore.o")) { Write-Host "stdlib completa FALHOU ($([int]((Get-Date) - $t0).TotalSeconds)s)" -ForegroundColor Red; exit 1 }
Write-Host ("stdlib completa ok: {0:N0} KB em {1}s ({2} fontes)" -f ((Get-Item "$out\swiftCore.o").Length / 1KB), [int]((Get-Date) - $t0).TotalSeconds, $files.Count)
