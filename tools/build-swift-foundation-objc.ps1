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
#
# Cópia preparada do fonte (build\swift\objc\foundation-src): no Foundation da Apple, Data.ReadingOptions,
# String.CompareOptions etc. são as opções NS_OPTIONS do ObjC (FOUNDATION_FRAMEWORK), e os nomes mangled levam o nome C
# delas (So20NSDataReadingOptionsV); no pacote são structs próprias. Aqui só esses apelidos mudam: o #if de cada um
# passa a valer com MOS_NS_OPTIONS, apontando para os tipos do módulo clang _MOSNSOptions
# (userland\sdk\swift-objc\sdk\Foundation\NSFoundationOptions.h). Leitura/gravação em UTF-8 pelo .NET (PowerShell 5.1).
$aliases = [ordered]@{ 'NSData.ReadingOptions' = 'NSDataReadingOptions'; 'NSData.WritingOptions' = 'NSDataWritingOptions'
                       'NSData.Base64EncodingOptions' = 'NSDataBase64EncodingOptions'; 'NSData.Base64DecodingOptions' = 'NSDataBase64DecodingOptions'
                       'NSString.CompareOptions' = 'NSStringCompareOptions'
                       'NSURL.BookmarkResolutionOptions' = 'NSURLBookmarkResolutionOptions'; 'NSURL.BookmarkCreationOptions' = 'NSURLBookmarkCreationOptions' }
$prep = "$objc\foundation-src\FoundationEssentials"
if (-not $Only -or $Only -contains 'Foundation') {
    if (Test-Path $prep) { Remove-Item $prep -Recurse -Force }
    $utf8 = New-Object Text.UTF8Encoding $false
    $base = "$sfs\FoundationEssentials"
    $patched = 0
    foreach ($f in Get-ChildItem $base -Recurse -Filter *.swift -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notmatch '\\(ProgressManager|NotificationCenter)\\' }) {
        $dst = Join-Path $prep $f.FullName.Substring($base.Length + 1)
        New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
        $lines = [Collections.Generic.List[string]]([IO.File]::ReadAllLines($f.FullName, $utf8))
        $touched = $false
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $m = [regex]::Match($lines[$i], '^\s*public typealias \w+ = (NS\w+\.\w+)\s*$')
            if (-not $m.Success -or -not $aliases.Contains($m.Groups[1].Value)) { continue }
            $lines[$i] = $lines[$i].Replace($m.Groups[1].Value, $aliases[$m.Groups[1].Value])
            $touched = $true
            # a diretiva que abre o bloco: #if <cond> (vale também com MOS_NS_OPTIONS) ou #else (o #if dele deixa de valer)
            $j = $i - 1
            while ($j -ge 0 -and $lines[$j] -notmatch '^\s*#\s*(if|else|elseif)\b') { $j-- }
            if ($j -lt 0 -or $lines[$j] -match 'MOS_NS_OPTIONS') { continue }
            if ($lines[$j] -match '^\s*#\s*if\s+(.+)$') { $lines[$j] = "#if ($($Matches[1].Trim())) || MOS_NS_OPTIONS"; continue }
            $depth = 0; $k = $j - 1
            for (; $k -ge 0; $k--) {
                if ($lines[$k] -match '^\s*#\s*endif\b') { $depth++ }
                elseif ($lines[$k] -match '^\s*#\s*if\b') { if ($depth -eq 0) { break } else { $depth-- } }
            }
            if ($k -ge 0 -and $lines[$k] -notmatch 'MOS_NS_OPTIONS' -and $lines[$k] -match '^\s*#\s*if\s+(.+)$') { $lines[$k] = "#if ($($Matches[1].Trim())) && !MOS_NS_OPTIONS" }
        }
        if ($touched) { $patched++ }
        # o fonte se refere ao próprio módulo como FoundationEssentials (nos trechos de quando compila a si mesmo); aqui o
        # módulo é o Foundation
        for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -notmatch '^\s*#') { $lines[$i] = $lines[$i].Replace('FoundationEssentials.', 'Foundation.') } }
        # o pacote declara um URLResourceKey vazio (fora da Apple não há o NSURLResourceKey); aqui ele é o do ObjC
        for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match '^\s*public struct URLResourceKey \{\}\s*$') { $lines[$i] = '// mOS: URLResourceKey = NSURLResourceKey (ObjC)'; $touched = $true } }
        # erros como na Apple (marco C): CocoaError e POSIXError são _BridgedStoredNSError guardando um NSError, e valem os
        # códigos e o CustomNSError do ramo FOUNDATION_FRAMEWORK. Só esses blocos (pela ordem do #if no arquivo); os outros
        # (chaves do userInfo como String, acessores) continuam no ramo do pacote. Os protocolos de ponte, NSError: Error e
        # as conversões ficam em userland\sdk\swift-objc\Foundation\NSError.swift.
        $errBlocks = @{ 'CocoaError.swift' = @(1, 9); 'ErrorCodes.swift' = @(1, 2, 3); 'ErrorCodes+POSIX.swift' = @(1) }
        if ($f.Directory.Name -eq 'Error' -and $errBlocks.ContainsKey($f.Name)) {
            $nth = 0
            for ($i = 0; $i -lt $lines.Count; $i++) {
                if ($lines[$i] -match '^\s*#\s*if\s+FOUNDATION_FRAMEWORK\s*$') {
                    $nth++
                    if ($errBlocks[$f.Name] -contains $nth) { $lines[$i] = '#if FOUNDATION_FRAMEWORK || MOS_NSERROR' }
                }
                # dentro do módulo Foundation o importador não faz a ponte NSString -> String
                $lines[$i] = $lines[$i].Replace('return NSCocoaErrorDomain', 'return _mosString(NSCocoaErrorDomain)').Replace('return NSPOSIXErrorDomain', 'return _mosString(NSPOSIXErrorDomain)').Replace('return (self as NSError).localizedDescription', 'return _mosString((self as NSError).localizedDescription)')
            }
        }
        # ComparisonResult: na Apple é o NSComparisonResult do ObjC (módulo _MOSNSOptions); o enum Swift do pacote sai
        if ($f.Name -eq 'ComparisonResult.swift') {
            for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match '^\s*#\s*if\s+!FOUNDATION_FRAMEWORK\s*$') { $lines[$i] = '#if !FOUNDATION_FRAMEWORK && !MOS_NS_OPTIONS'; break } }
        }
        # em todos os arquivos: com MemberImportVisibility, quem usa .atomic/.caseInsensitive... precisa do import
        $lines.Insert(0, 'import _MOSNSOptions   // mOS: opções NS_OPTIONS da Apple (tools\build-swift-foundation-objc.ps1)')
        [IO.File]::WriteAllLines($dst, $lines, $utf8)
    }
    Write-Host "Foundation: fonte do swift-foundation preparado ($patched arquivos com opções NS_OPTIONS)"
}
# ---- o módulo Foundation, como no SDK da Apple: os fontes do FoundationEssentials (os tipos-valor) + os do mOS
# (userland\sdk\swift-objc\Foundation: pontes ObjC, IndexSet, Notification, URLRequest, URLResourceValues, KVO...) num módulo
# só, sobre o módulo clang Foundation (ObjC). Um módulo só porque é assim na Apple: as pontes e os membros que os binários
# importam ($s10Foundation4DataV19_bridgeToObjectiveC...) ficam no mesmo módulo do tipo, sem contexto de extensão no nome, e
# o importador só trata NSArray/NSString/NSString-tipado (URLResourceKey) como do Foundation dentro dele.
# DATA_LEGACY_ABI: a representação do Data do ABI da Apple (enum _Representation, RangeReference), que o código inlinável
# dos binários usa; o padrão do pacote é a representação nova.
$fnd = @(Get-ChildItem $prep -Recurse -Filter *.swift | ForEach-Object FullName) + @(Get-ChildItem "$root\userland\sdk\swift-objc\Foundation" -Filter *.swift | ForEach-Object FullName)
foreach ($x in 'FoundationEssentials.swiftmodule', 'FoundationEssentials.o') { Remove-Item "$out\$x" -ErrorAction SilentlyContinue }   # da estrutura antiga
if (-not $Only -or $Only -contains 'Foundation') {
    Build-Mod 'Foundation' $fnd ($sfFlags + @('-import-underlying-module', '-autolink-force-load', '-D', 'DATA_LEGACY_ABI', '-D', 'MOS_NS_OPTIONS', '-D', 'MOS_NSERROR'))
}
exit $fails
