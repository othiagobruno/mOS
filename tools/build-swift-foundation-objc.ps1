# mOS - fase 5, marco B de docs/swift-objc-runtime.md: o swift-foundation (github.com/swiftlang/swift-foundation, Apache
# 2.0; a base do Foundation do macOS 14+) na variante com interop ObjC, como pacote no macOS (Darwin/os de verdade, sem
# as reescritas MOSDarwin/MOSos do caminho sem ObjC). O FoundationEssentials sai com -module-abi-name Foundation, para
# os tipos terem os nomes mangled da Apple ($s10Foundation4DataV...), e com library evolution (dispatch thunks como
# JSONDecoder.decode). Também: Synchronization, swift-collections 1.7.3, _FoundationCShims (C) e _FoundationRefs.
# Saída: build\swift\objc\overlays (junto dos outros overlays). Depois: tools\build-swift-overlays-objc.ps1 -Only Foundation.
#   .\tools\build-swift-foundation-objc.ps1 [-Only FoundationEssentials]
param([string[]]$Only)
$root = Split-Path $PSScriptRoot -Parent
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*')[0].FullName + '\zig.exe'
$objc = "$root\build\swift\objc"
$out = "$objc\overlays"
$inc = "$objc\sdk\include"
$deps = "$root\.tools\openswiftui\deps"
if ($Only) { $Only = @($Only | ForEach-Object { $_ -split ',' }) }
if (-not (Test-Path "$out\Darwin.swiftmodule")) { Write-Host 'faltam os overlays (tools\build-swift-overlays-objc.ps1)' -ForegroundColor Red; exit 1 }
$fails = 0

function Build-Mod($name, [string[]]$files, [string[]]$extra = @(), [string]$lang = '5', [bool]$evo = $true) {
    if ($Only -and $Only -notcontains $name) { return }
    $a = @('-c', '-target', 'x86_64-apple-macos26', '-module-name', $name, '-parse-as-library', '-swift-version', $lang, '-O',
           '-I', $objc, '-I', $out, '-I', "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims",
           '-module-cache-path', "$objc\modcache", '-Xcc', "-isystem$inc", '-Xcc', '-DMOS_SWIFT_OBJC=1', '-Xcc', '-Wno-nullability-completeness',
           '-module-link-name', "swift$name", '-emit-module-path', "$out\$name.swiftmodule", '-o', "$out\$name.o") + $extra + $files
    if ($evo) { $a = @('-enable-library-evolution') + $a }
    $rsp = "$out\$name.rsp"
    ($a | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join "`n" | Set-Content -Encoding ascii $rsp
    $log = & "$($tc.FullName)\usr\bin\swift-frontend.exe" -frontend "@$rsp" 2>&1 | ForEach-Object { "$_" }
    $log | Set-Content -Encoding utf8 "$out\$name.log"
    $err = $log | Select-String -Pattern ':\d+:\d+: error: |<unknown>:0: error: '
    if ($LASTEXITCODE -ne 0 -or $err) { $err | Select-Object -First 20 | ForEach-Object { "  $($_.Line)" }; Write-Host "${name}: FALHOU ($($err.Count) erros)" -ForegroundColor Red; $script:fails++; return }
    Write-Host ("{0}: ok ({1:N0} KB, {2} fontes)" -f $name, ((Get-Item "$out\$name.o").Length / 1KB), $files.Count)
}

# ---- Synchronization (Atomic/Mutex): o do mOS (userland\sdk\openswiftui\swift\Synchronization), como no caminho sem ObjC
Build-Mod 'Synchronization' @(Get-ChildItem "$root\userland\sdk\openswiftui\swift\Synchronization" -Filter *.swift | ForEach-Object FullName) @('-parse-stdlib', '-enable-experimental-feature', 'RawLayout')

# ---- swift-collections 1.7.3 (Apache 2.0), a versão que o swift-foundation fixa
$sc = "$deps\swift-collections-1.7.3\Sources"
$scAvail = @{ '5.0' = '10.14.4'; '5.1' = '10.15'; '5.6' = '12.3'; '5.7' = '13.0'; '5.8' = '13.3'; '5.9' = '14.0'; '5.10' = '14.4'
              '6.0' = '15.0'; '6.1' = '15.4'; '6.2' = '26.0'; '6.3' = '26.4'; '6.4' = '27.0' }
$scFlags = @('-package-name', 'SwiftCollections', '-enable-upcoming-feature', 'MemberImportVisibility')
foreach ($ft in 'BuiltinModule', 'Lifetimes', 'InoutLifetimeDependence', 'AddressableParameters', 'AddressableTypes', 'SuppressedAssociatedTypesWithDefaults') { $scFlags += @('-enable-experimental-feature', $ft) }
foreach ($k in $scAvail.Keys) { $scFlags += @('-enable-experimental-feature', "AvailabilityMacro=SwiftStdlib ${k}: macOS $($scAvail[$k]), iOS 99, watchOS 99, tvOS 99, visionOS 99") }
foreach ($m in @(@('InternalCollectionsUtilities', 'InternalCollectionsUtilities', '6'), @('SpanPreview', 'SpanPreview', '6'), @('BasicContainers', 'BasicContainers', '6'),
                 @('DequeModule', 'DequeModule', '6'), @('OrderedCollections', 'OrderedCollections', '6'), @('_RopeModule', 'RopeModule', '5'))) {
    Build-Mod $m[0] @(Get-ChildItem "$sc\$($m[1])" -Recurse -Filter *.swift | ForEach-Object FullName) $scFlags $m[2]
}

# ---- swift-foundation
$sfs = "$deps\swift-foundation-main\Sources"
$csh = "$sfs\_FoundationCShims"
if (-not $Only -or $Only -contains '_FoundationCShims') {
    & $zig cc -target x86_64-macos -O2 -c "-I$csh\include" -std=gnu11 "$csh\FoundationCShims.c" -o "$out\_FoundationCShims_c.o" 2>&1 | ForEach-Object { "  $_" }
    if ($LASTEXITCODE) { $fails++ } else { Write-Host '_FoundationCShims (C): ok' }
}
$sfAvail = @()
foreach ($v in '6.0.2', '6.1', '6.2', '6.3', '6.4', '6.4.2', '6.5') { $sfAvail += @('-enable-experimental-feature', "AvailabilityMacro=FoundationPreview ${v}:macOS 26, iOS 26, tvOS 26, watchOS 26, visionOS 26") }
$sfFlags = @('-package-name', 'SwiftFoundation', '-I', "$csh\include", '-enable-upcoming-feature', 'InferSendableFromCaptures',
             '-enable-upcoming-feature', 'MemberImportVisibility') + $sfAvail
foreach ($ft in 'VariadicGenerics', 'Lifetimes', 'AddressableTypes', 'AllowUnsafeAttribute', 'BuiltinModule', 'AccessLevelOnImport', 'ImportMacroAliases') { $sfFlags += @('-enable-experimental-feature', $ft) }
# _FoundationRefs: interno, sem library evolution (o código dele não a aceita; no SwiftPM ninguém o compila assim)
Build-Mod '_FoundationRefs' @(Get-ChildItem "$sfs\_FoundationRefs" -Recurse -Filter *.swift | ForEach-Object FullName) $sfFlags '5' $false
# FoundationEssentials: nomes mangled da Apple (módulo Foundation); fora o ProgressManager e as mensagens do
# NotificationCenter, como no caminho sem ObjC
$ess = @(Get-ChildItem "$sfs\FoundationEssentials" -Recurse -Filter *.swift -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notmatch '\\(ProgressManager|NotificationCenter)\\' } | ForEach-Object FullName)
Build-Mod 'FoundationEssentials' $ess ($sfFlags + @('-module-abi-name', 'Foundation'))
exit $fails
