# mOS - compila a biblioteca padrão Swift "Embedded" (só .swiftmodule, como no build oficial) para
# x86_64-apple-macos, a partir do fonte oficial em .tools\swift-src (tag swift-6.4.2-RELEASE).
# Saída: build\swift\embedded\Swift.swiftmodule  (os apps são compilados em modo Embedded contra ela)
$root = Split-Path $PSScriptRoot -Parent
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$swiftc = "$($tc.FullName)\usr\bin\swiftc.exe"
$rt = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Runtimes" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
if ($rt) { $env:Path = "$($rt.FullName)\usr\bin;" + $env:Path }
$sdk = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Platforms" -Directory | Select-Object -First 1
$shims = "$($sdk.FullName)\Windows.platform\Developer\SDKs\Windows.sdk\usr\lib\swift\shims"
$src = "$root\.tools\swift-src"
$core = "$src\stdlib\public\core"
$out = "$root\build\swift\embedded"
New-Item -ItemType Directory -Force $out | Out-Null
$files = @()
foreach ($f in Get-Content "$src\embedded-sources.txt") { if (Test-Path "$core\$f") { $files += "$core\$f" } else { $files += "$src\gen\$f" } }
$files += Get-ChildItem "$src\gen\*.swift" | Where-Object { $files -notcontains $_.FullName } | ForEach-Object FullName
$avail = @()
foreach ($l in Get-Content "$src\utils\availability-macros.def") {
    if (!$l -or $l.StartsWith('#')) { continue }
    $avail += '-Xfrontend', '-define-availability', '-Xfrontend', (($l -replace ':.*', '') + ':*')
}
$feat = 'Embedded', 'Macros', 'FreestandingMacros', 'Extern', 'BitwiseCopyable', 'ValueGenerics', 'AddressableParameters', 'AddressableTypes',
        'AllowUnsafeAttribute', 'SuppressedAssociatedTypesWithDefaults', 'Reparenting', 'Lifetimes', 'BorrowAndMutateAccessors', 'BorrowInout', 'LifetimeDependence'
$fl = @()
foreach ($x in $feat) { $fl += '-enable-experimental-feature', $x }
$args = @('-frontend', '-emit-module', '-target', 'x86_64-apple-macos14', '-module-name', 'Swift', '-parse-stdlib', '-parse-as-library',
          '-nostdimport', '-swift-version', '5', '-O', '-disable-objc-interop', '-enforce-exclusivity=unchecked',
          '-Xcc', '-ffreestanding', '-I', $shims, '-strict-memory-safety', '-warn-implicit-overrides',
          '-emit-module-path', "$out\Swift.swiftmodule\x86_64-apple-macos.swiftmodule") + $fl + ($avail | Where-Object { $_ -ne '-Xfrontend' }) + $files
New-Item -ItemType Directory -Force "$out\Swift.swiftmodule" | Out-Null
$log = & $swiftc @args -Xcc -Wno-nullability-completeness 2>&1 | ForEach-Object { "$_" }
$log | Set-Content "$out\build.log"
$log | Select-String -Pattern ' error: ' | Select-Object -First 30 | ForEach-Object { $_.Line }
if ($LASTEXITCODE -ne 0) { Write-Host "stdlib Embedded FALHOU" -ForegroundColor Red; exit 1 }
Write-Host "stdlib Embedded ok: $out\Swift.swiftmodule"
