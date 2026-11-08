# mOS - fase 6 de docs/swift-objc-runtime.md: SwiftUI.framework para binários Swift da Apple.
# O OpenSwiftUI (github.com/OpenSwiftUIProject, MIT) e as dependências dele compilados sobre a stdlib COM interop ObjC
# (build\swift\objc), com library evolution e -module-abi-name SwiftUI, e ligados num dylib só:
#   /System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI   (em build\swift\objc\dylib)
# Os fontes são preparados como na variante nativa (tools\build-openswiftui-deps.ps1: caminho não-Darwin, sem atores),
# com três trocas: Combine em vez de OpenCombine (o Combine.framework do mOS), Observation em vez de OpenObservation
# (o overlay da variante) e o host do AppKit do mOS (NSHostingView/NSHostingController, userland\sdk\swift-objc\SwiftUI).
# Os objetos C/C++ (Compute, OpenRenderBox, OpenSwiftUI_SPI, ctfont) são os da variante nativa; o CFRuntime é outro
# (userland\sdk\swift-objc\SwiftUI\c\cf_objc_runtime.c: tipos CF como objetos ObjC do libSystem).
# Antes: tools\build-openswiftui-deps.ps1 (objetos C/C++ e fontes baixados), build-swift-foundation-objc.ps1,
# build-swift-combine-objc.ps1 e build-swift-overlays-objc.ps1.
#   .\tools\build-swiftui-objc.ps1                 # tudo
#   .\tools\build-swiftui-objc.ps1 -From Core       # a partir de uma etapa (Deps, Core, UI, Link)
#   .\tools\build-swiftui-objc.ps1 -Only UI -TypeCheck   # só checa tipos (rápido, sem .o)
param([string]$From = 'Deps', [string[]]$Only, [switch]$TypeCheck, [string]$Arch = 'x86_64')
$root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\swift-arch.ps1" $Arch
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Where-Object { Test-Path "$($_.FullName)\zig.exe" })[0].FullName + '\zig.exe'
$objc = "$SwiftBuild\objc"
$ov = "$objc\overlays"
$inc = "$objc\sdk\include"
$sui = "$objc\swiftui"
$nat = "$SwiftBuild\openswiftui"          # variante nativa (objetos C/C++; no arm64: build-openswiftui-deps.ps1 -Arch arm64)
$deps = "$root\.tools\openswiftui\deps"
$osui = "$root\.tools\openswiftui\main\OpenSwiftUI-main\Sources"
$nsdk = "$root\userland\sdk\openswiftui"         # SDK da variante nativa (fontes Swift do mOS)
$mine = "$root\userland\sdk\swift-objc\SwiftUI"  # o que é só desta variante
$utf8 = New-Object Text.UTF8Encoding $false
$stages = 'Deps', 'Core', 'UI', 'Link'
$run = if ($Only) { @($Only | ForEach-Object { $_ -split ',' }) } else { $stages[([array]::IndexOf($stages, $From))..($stages.Count - 1)] }
foreach ($m in 'Foundation', 'Combine', 'Observation', 'Dispatch', 'CoreGraphics') {
    if (-not (Test-Path "$ov\$m.swiftmodule")) { Write-Host "falta o overlay $m em $ov" -ForegroundColor Red; exit 1 }
}
New-Item -ItemType Directory -Force $sui | Out-Null
# Cabeçalhos desta variante: cópia do SDK da variante ObjC (build\swift\objc\sdk\include) com o AppKit do mOS (para o
# NSHostingView) e o -[NSObject init] não-nulo, como no SDK da Apple (lá vem das API notes do ObjectiveC; sem isso
# `Classe()` de uma subclasse de NSObject seria opcional). Cópia própria, com cache de módulos próprio, para não
# mudar os módulos clang que os outros overlays já usam.
$sinc = "$sui\sdk-include"
if (Test-Path $sinc) { Remove-Item $sinc -Recurse -Force }
Copy-Item $inc $sinc -Recurse
Copy-Item "$mine\sdk\AppKit" $sinc -Recurse -Force
$mm = [IO.File]::ReadAllText("$sinc\module.modulemap")
$mm += "`n// AppKit do mOS (userland\libsystem\appkit.c): o subconjunto que o SwiftUI.framework usa (NSHostingView)`nmodule AppKit [system] [extern_c] {`n    header `"AppKit/AppKit.h`"`n    export *`n}`n"
[IO.File]::WriteAllText("$sinc\module.modulemap", $mm)
$nso = [IO.File]::ReadAllText("$sinc\objc\NSObject.h")
[IO.File]::WriteAllText("$sinc\objc\NSObject.h", $nso.Replace("- (instancetype)init`n", "- (instancetype _Nonnull)init /* mOS: não-nulo, como no SDK da Apple */`n"))
# ponte automática NSString -> String, NSArray -> Array, NSData -> Data...: no SDK da Apple vem das API notes do Foundation
# (SwiftBridge); sem o atributo o importador entrega os tipos ObjC. O nome tem de ser qualificado (Swift.String).
$fh = [IO.File]::ReadAllText("$sinc\Foundation\Foundation.h")
foreach ($b in @('NSString=Swift.String', 'NSArray=Swift.Array', 'NSDictionary=Swift.Dictionary', 'NSSet=Swift.Set', 'NSData=Foundation.Data',
                 'NSURL=Foundation.URL', 'NSDate=Foundation.Date', 'NSUUID=Foundation.UUID', 'NSLocale=Foundation.Locale',
                 'NSTimeZone=Foundation.TimeZone', 'NSDateComponents=Foundation.DateComponents', 'NSIndexSet=Foundation.IndexSet',
                 'NSNotification=Foundation.Notification', 'NSURLRequest=Foundation.URLRequest', 'NSIndexPath=Foundation.IndexPath')) {
    $k, $v = $b -split '='
    $fh = [regex]::Replace($fh, "(?m)^@interface $k([ <])", "__attribute__((swift_bridge(`"$v`"))) @interface $k`$1")
}
[IO.File]::WriteAllText("$sinc\Foundation\Foundation.h", $fh)
# classes do Foundation do libSystem que o SwiftUI usa (Bundle, Formatter, RunLoop...): módulo clang _MOSFoundationSwiftUI,
# reexportado pelo MOSFoundationExtras (dentro do módulo Foundation, os decls ficariam no submódulo errado)
Copy-Item "$mine\sdk\Foundation\NSFoundationSwiftUI.h" "$sinc\Foundation" -Force
[IO.File]::AppendAllText("$sinc\module.modulemap", "`n// classes do Foundation do libSystem que o SwiftUI.framework usa (tools\build-swiftui-objc.ps1)`nmodule _MOSFoundationSwiftUI [system] [extern_c] {`n    header `"Foundation/NSFoundationSwiftUI.h`"`n    export *`n}`n")
# tipos CG/IOSurface que o CoreGraphics.h do mOS não tem (CGBlendMode...) e o CommonCrypto (SHA1 do StrongHash)
Copy-Item "$mine\sdk\CoreGraphics\CGSwiftUI.h" "$sinc\CoreGraphics" -Force
$mm2 = "`n// SwiftUI.framework (tools\build-swiftui-objc.ps1)`nmodule _MOSCoreGraphicsSwiftUI [system] [extern_c] {`n    header `"CoreGraphics/CGSwiftUI.h`"`n    export *`n}`n"
if (-not [IO.File]::ReadAllText("$sinc\module.modulemap").Contains('module CommonCrypto')) { $mm2 += "module CommonCrypto [system] [extern_c] {`n    header `"CommonCrypto/CommonDigest.h`"`n    export *`n}`n" }
[IO.File]::AppendAllText("$sinc\module.modulemap", $mm2)
# como na variante nativa: o destrutor do pthread_key_create recebe ponteiro anulável (o OpenSwiftUI faz guard let)
$pth = [IO.File]::ReadAllText("$sinc\pthread.h")
[IO.File]::WriteAllText("$sinc\pthread.h", $pth.Replace('int pthread_key_create(pthread_key_t *, void (* _Nullable)(void *));', 'int pthread_key_create(pthread_key_t *, void (* _Nullable)(void * _Nullable));'))
# os módulos de sistema não são revalidados pelo clang: o cache começa vazio a cada execução
$mcache = "$sui\modcache"
if (Test-Path $mcache) { Remove-Item $mcache -Recurse -Force }
$fails = 0
$script:extras = $false   # Convert-Swift só junta o import MOSFoundationExtras nos fontes do OpenSwiftUI

# ---- módulo Swift sobre a stdlib com ObjC
function Build-Swift($name, $files, $extra, $lang = '5', [bool]$evolution = $true) {
    $out = "$sui\$name"; New-Item -ItemType Directory -Force $out | Out-Null
    $a = @('-target', $SwiftTriple, '-module-name', $name, '-parse-as-library', '-swift-version', $lang, '-O',
           '-I', $objc, '-I', $ov, '-module-cache-path', $mcache,
           '-I', "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims",
           '-disable-implicit-concurrency-module-import',
           '-enable-experimental-feature', 'Extern',
           '-Xcc', "-isystem$sinc", '-Xcc', '-DMOS_SWIFT_OBJC=1', '-Xcc', '-Wno-nullability-completeness',
           '-emit-module-path', "$out\$name.swiftmodule")
    if ($TypeCheck) { $a = @('-typecheck') + $a | Where-Object { $_ -ne '-emit-module-path' -and $_ -ne "$out\$name.swiftmodule" } }
    else { $a = @('-c') + $a + @('-o', "$out\$name.o") }
    foreach ($d in Get-ChildItem $sui -Directory) { if ($d.Name -ne $name -and (Test-Path "$($d.FullName)\$($d.Name).swiftmodule")) { $a += '-I', $d.FullName } }
    if ($evolution) { $a += '-enable-library-evolution' }
    $a += $extra; $a += $files
    $rsp = "$out\$name.rsp"
    ($a | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join "`n" | Set-Content -Encoding ascii $rsp
    $t0 = Get-Date
    $log = & "$($tc.FullName)\usr\bin\swift-frontend.exe" -frontend "@$rsp" 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $log | Set-Content -Encoding utf8 "$out\build.log"
    $err = @($log | Select-String -Pattern ':\d+:\d+: error: |<unknown>:0: error: |^error: ')
    $dt = [int]((Get-Date) - $t0).TotalSeconds
    if ($code -ne 0 -or $err) { $err | Select-Object -First 30 | ForEach-Object { Write-Host "  $($_.Line)" }; Write-Host "${name}: FALHOU ($($err.Count) erros, ${dt}s; log em $out\build.log)" -ForegroundColor Red; $script:fails++; return $false }
    Write-Host "${name}: ok ($($files.Count) fontes, ${dt}s)" -ForegroundColor Green
    return $true
}
function Build-C($name, $src, [string[]]$flags = @()) {
    $out = "$sui\$name"; New-Item -ItemType Directory -Force $out | Out-Null
    $o = "$out\" + [IO.Path]::GetFileNameWithoutExtension($src) + '.o'
    & $zig cc -target $ZigTarget -O2 -c -w @flags $src -o $o 2>&1 | ForEach-Object { "  $_" }
    if ($LASTEXITCODE) { Write-Host "${name}: FALHOU" -ForegroundColor Red; $script:fails++ }
}

# ---- fontes preparados: o mesmo caminho da variante nativa. Nas linhas #if/#elseif, os(macOS) e canImport de módulos
# que só existem nesta variante (Darwin, ObjectiveC, Combine, AppKit, UIKit, QuartzCore...) viram false; atributos de
# ator saem (sem executor de atores no grafo de views). Depois as trocas desta variante.
$noImport = 'Darwin|ObjectiveC|Combine|AppKit|UIKit|QuartzCore|CoreGraphics|os|OSLog|Observation|UniformTypeIdentifiers|Metal|simd|XPC|IOKit|CoreImage|CoreAudio|CoreMIDI|Spatial|QuickLookUI|CryptoKit|SwiftUI'
# modos: up = fontes do upstream (os(macOS) e os módulos acima viram false); ov = substituições do mOS e arquivos de
# plataforma, escritos para a variante nativa, em que os(macOS) e canImport(CoreGraphics) eram verdadeiros
function Convert-Swift([string]$txt, [string]$mode = 'up') {
    $lines = $txt -split "`r?`n", -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $l = $lines[$i]
        if ($l -match '^\s*#\s*(if|elseif)\b') {
            if ($mode -eq 'up') {
                $l = [regex]::Replace($l, '\bos\(\s*macOS\s*\)', 'false')
                $l = [regex]::Replace($l, "\bcanImport\(\s*($noImport)\s*\)", 'false')
            } else {
                $l = [regex]::Replace($l, "\bcanImport\(\s*($($noImport -replace 'CoreGraphics\|', ''))\s*\)", 'false')
            }
            # sem _Concurrency (como na variante nativa): no upstream a condição inteira sai; nas substituições só o canImport
            if ($mode -eq 'up' -and $l -match 'canImport\(\s*_Concurrency\s*\)') { $l = ($l -replace '^(\s*#\s*(?:if|elseif)).*$', '$1 false // mOS: sem _Concurrency') }
            elseif ($mode -ne 'up') { $l = [regex]::Replace($l, '\bcanImport\(\s*_Concurrency\s*\)', 'false') }
        } elseif ($l -notmatch '^\s*#') {
            $l = $l -replace '@MainActor(\s*\(unsafe\))?\s?', '' -replace '@isolated\(any\)\s?', '' -replace '\{\s+in\b', '{'
            # Combine e Observation da variante (Combine.framework e o overlay Observation)
            $l = $l -replace '^(\s*)(@_spi\(OpenSwiftUI\)\s+)?((?:public|package|internal)\s+)?import OpenObservation\b', '$1@_spi(OpenSwiftUI) $3import Observation'
            $l = $l -replace '\bOpenObservation\.', 'Observation.'
            # classes que aqui moram em outros módulos (NSAttributedString em Swift; Thread, RunLoop... no _MOSFoundationSwiftUI)
            $l = $l -replace '\bimport class Foundation\.NSAttributedString\b', 'import class MOSFoundationExtras.NSAttributedString'
            $l = $l -replace '\bimport class Foundation\.(Thread|RunLoop|Bundle|Timer|UserDefaults|NSLock|Formatter|NumberFormatter|DateFormatter)\b', 'import class _MOSFoundationSwiftUI.$1'
            $l = $l -replace '\bFoundation\.(Progress|Thread|RunLoop|Bundle|Timer|UserDefaults)\b', '$1'
            # os nomes C que a variante nativa dava ao CoreGraphics do libSystem (MOSCGPathRef...) são os da Apple aqui
            $l = $l -replace '\bMOSCGMutablePathRef\b', 'CGMutablePath' -replace '\bMOSCGPathRef\b', 'CGPath' -replace '\bMOSCGPathElement\b', 'CGPathElement'
            # o que a variante nativa tinha no Foundation do mOS e o Foundation da variante não tem (MOSFoundationExtras)
            if ($script:extras -and $l -match '^(\s*)((?:@_spi\([^)]*\)\s*)*)((?:public|package|internal|@_exported)\s+)?import Foundation\s*$') {
                $l = $l + "`n" + $Matches[1] + $Matches[3] + 'import MOSFoundationExtras'
            }
        }
        $lines[$i] = $l
    }
    return ($lines -join "`n")
}
function Prepare-Src($name, $srcDir, [string[]]$overlayDirs, [string]$ovMode = 'ov') {
    $dst = "$sui\src\$name"
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
    New-Item -ItemType Directory -Force $dst | Out-Null
    foreach ($f in $(if ($srcDir) { Get-ChildItem $srcDir -Recurse -Filter *.swift })) {
        $rel = $f.FullName.Substring((Resolve-Path $srcDir).Path.Length + 1)
        $out = Join-Path $dst $rel
        New-Item -ItemType Directory -Force (Split-Path $out -Parent) | Out-Null
        [IO.File]::WriteAllText($out, (Convert-Swift ([IO.File]::ReadAllText($f.FullName, $utf8))), $utf8)
    }
    # substituições (mesmos caminhos relativos): escritas para as condições #if originais; só os atores saem
    foreach ($od in $overlayDirs) {
        if (-not $od -or -not (Test-Path $od)) { continue }
        foreach ($f in Get-ChildItem $od -Recurse -File) {
            $rel = $f.FullName.Substring((Resolve-Path $od).Path.Length + 1)
            $out = Join-Path $dst $rel
            New-Item -ItemType Directory -Force (Split-Path $out -Parent) | Out-Null
            if ($f.Extension -eq '.swift') { [IO.File]::WriteAllText($out, (Convert-Swift ([IO.File]::ReadAllText($f.FullName, $utf8)) $ovMode), $utf8) }
            elseif ($f.Extension -eq '.remove') { Remove-Item ($out.Substring(0, $out.Length - 7)) -ErrorAction SilentlyContinue }
            else { Copy-Item $f.FullName $out -Force }
        }
    }
    return $dst
}
# remendos pontuais (arquivo relativo à cópia preparada, texto antigo, texto novo); um remendo que não casa é avisado
function Apply-Patches($dir, $list) {
    foreach ($pt in $list) {
        $f = Join-Path $dir $pt[0]
        if (-not (Test-Path $f)) { Write-Host "  remendo: falta $($pt[0])" -ForegroundColor Yellow; continue }
        $x = [IO.File]::ReadAllText($f, $utf8)
        if (-not $x.Contains($pt[1])) { Write-Host "  remendo não casou em $($pt[0]): $($pt[1])" -ForegroundColor Yellow; continue }
        [IO.File]::WriteAllText($f, $x.Replace($pt[1], $pt[2]), $utf8)
    }
}
$corePatches = @(
    # CFRuntimeClass.equal devolve Boolean (DarwinBoolean no Swift com o SDK do macOS)
    @('Shim\CoreFoundationNonDarwinShims.swift', 'equal: { lhs, rhs in (lhs as? NSObject)?.isEqual(rhs) ?? false },', 'equal: { lhs, rhs in DarwinBoolean((lhs as? NSObject)?.isEqual(rhs) ?? false) },'),
    @('View\Text\Typesetting\TypesettingLanguage.swift', ').map(String.init) {', ').map({ $0 as String }) {'),
    # o NSAttributedString do texto é uma classe Swift (MOSFoundationExtras), sem NSCoding
    @('Util\AccessibilityShims.swift', 'coder.decodeObject(of: NSString.self, forKey: "label") as String?', 'coder.decodeObject(of: NSString.self, forKey: "label") as? String'),
    @('Util\AccessibilityShims.swift', 'coder.decodeObject(of: NSString.self, forKey: "value") as String?', 'coder.decodeObject(of: NSString.self, forKey: "value") as? String'),
    @('Util\AccessibilityShims.swift', 'coder.decodeObject(of: NSAttributedString.self, forKey: "attributedLabel")?.copy() as? NSAttributedString', 'nil'),
    @('Util\AccessibilityShims.swift', 'coder.decodeObject(of: NSAttributedString.self, forKey: "attributedValue")?.copy() as? NSAttributedString', 'nil'),
    @('Util\AccessibilityShims.swift', 'coder.encode(attributedLabel, forKey: "attributedLabel")', '_ = attributedLabel'),
    @('Util\AccessibilityShims.swift', 'coder.encode(attributedValue, forKey: "attributedValue")', '_ = attributedValue'),
    # Decimal -> NSNumber: sem NSDecimalNumber no mOS
    @('Accessibility\AccessibilityValue.swift', ').map { $0 as NSNumber }', ').map { NSNumber(value: Double("\($0)") ?? 0) }'),
    # IOSurfaceRef (tipo CF) é a classe IOSurface no Swift
    @('View\Image\GraphicsImage.swift', 'case ioSurface(IOSurfaceRef)', 'case ioSurface(IOSurface)'),
    # SwiftUI.Log é público na Apple (o código inlinável dos apps usa Log.runtimeIssues: Core\MOSAppKitCore.swift)
    @('Log\Logging.swift', "@usableFromInline`npackage enum Log {", 'public enum Log {'),
    # na Apple, Log.runtimeIssuesLog é o OS_os_log; o Logger do swift-log do OpenSwiftUI muda de nome
    @('Log\Logging.swift', 'runtimeIssuesLog', '_mosRuntimeIssuesLogger'),
    @('Data\Environment\Environment.swift', 'Log.runtimeIssuesLog', 'Log._mosRuntimeIssuesLogger')
)
function Swift-Files($dir) { @(Get-ChildItem $dir -Recurse -Filter *.swift | ForEach-Object FullName) }

$cmp = "$deps\Compute-main\Sources"
$orb = "$deps\OpenRenderBox-main\Sources"
$orbInc = "$sui\orb-include"   # include do OpenRenderBoxCxx sem o submódulo ObjC (exige Metal e o ObjC do OpenRenderBox)
$spiInc = "$nat\OpenSwiftUI_SPI\include"
$sdkSwift = "$nsdk\swift"
$osuiDefs = @('OPENSWIFTUI', 'OPENSWIFTUI_RELEASE_2024', 'OPENSWIFTUI_SUPPORT_2021_API', 'OPENSWIFTUI_SUPPORT_2022_API', 'OPENSWIFTUI_SUPPORT_2023_API',
              'OPENSWIFTUI_SUPPORT_2024_API', 'OPENSWIFTUI_CF_CGTYPES', 'OPENRENDERBOX_CF_CGTYPES', 'OPENSWIFTUI_SWIFT_LOG',
              'OPENSWIFTUI_COMPUTE', 'OPENSWIFTUI_MOS', 'OPENSWIFTUI_MOS_OBJC', 'OPENSWIFTUI_XCFRAMEWORK_BUILD')
function OSUI-Flags {
    $f = @('-I', "$sdkSwift\COpenSwiftUI", '-I', $spiInc, '-I', "$cmp\ComputeCxx\include", '-I', "$orbInc",
           '-I', "$sdkSwift\CoreText\include", '-I', "$root\userland\sdk\swift\MOSBridge")
    foreach ($a in @("-I$spiInc", "-I$orbInc", "-I$cmp\Platform\include", "-I$cmp\ComputeCxx\include", "-I$sdkSwift\CoreText\include",
                     '-DOPENSWIFTUI', '-DOPENSWIFTUI_MOS', '-DOPENSWIFTUI_CF_CGTYPES', '-DOPENRENDERBOX_CF_CGTYPES', '-DOPENSWIFTUI_LINK_COREUI=0')) { $f += '-Xcc', $a }
    foreach ($d in $osuiDefs) { $f += '-D', $d }
    foreach ($ft in 'BareSlashRegexLiterals', 'InternalImportsByDefault', 'InferSendableFromCaptures') { $f += '-enable-upcoming-feature', $ft }
    $f += '-package-name', 'OpenSwiftUI', '-module-abi-name', 'SwiftUI', '-experimental-spi-only-imports', '-unavailable-decl-optimization=stub'
    $minv = 'macOS 10.15, iOS 13.0, tvOS 13.0, watchOS 6.0, visionOS 1.0'
    foreach ($v in 'v1_0', 'v1_4', 'v2_0', 'macOS_v2_0', 'v2_1', 'v2_3', 'v3_0', 'iOS_v3_0', 'macOS_v3_0', 'v3_2', 'v3_4', 'v4_0', 'iOS_v4_0', 'watchOS_v4_0',
                   'v4_1', 'v4_4', 'v5_0', 'v5_1', 'v5_2', 'v5_4', 'v5_5', 'v6_0', 'v7_0', 'v8_0') { $f += '-define-availability', "OpenSwiftUI_${v}:$minv" }
    $f += '-define-availability', '_distantFuture:macOS 99.0, iOS 99.0, tvOS 99.0, watchOS 99.0, visionOS 99.0'
    return $f
}

if ($run -contains 'Deps') {
    # C: CFRuntime ObjC e os descritores de protocolo que o grafo percorre (nomes da Apple: Combine.ObservableObject)
    Build-C 'c' "$mine\c\cf_objc_runtime.c"
    Build-C 'c' "$mine\c\ProtocolDescriptor.c"
    # Glibc: o OpenSwiftUI fora do Darwin importa Glibc; aqui ele reexporta o Darwin e o CoreGraphics da variante
    Build-Swift 'Glibc' @("$mine\Glibc\Glibc.swift") @() | Out-Null
    Build-Swift 'Logging' (Swift-Files "$sdkSwift\Logging") @() | Out-Null
    # MOSFoundationExtras: o que o OpenSwiftUI usa do Foundation do mOS e o Foundation da variante não tem (NSAttributedString
    # com trechos, NSRange, ReferenceConvertible, FormatStyles sem ICU). Nome ABI "Foundation": os tipos saem com os nomes
    # mangled da Apple ($s10Foundation20ReferenceConvertibleP...)
    Build-Swift 'MOSFoundationExtras' (Swift-Files "$mine\Extras") @('-module-abi-name', 'Foundation') | Out-Null
    # CoreText do mOS (ctfont.c: tipos CF pelo CFRuntime acima) e CoreText_Private
    $ctFlags = @('-I', "$sdkSwift\CoreText\include", '-Xcc', "-I$sdkSwift\CoreText\include")
    $ctSrc = Prepare-Src 'CoreText' "$sdkSwift\CoreText" @("$mine\Replace\CoreText")
    Build-Swift 'CoreText' (Swift-Files $ctSrc) $ctFlags | Out-Null
    Build-Swift 'CoreText_Private' (Swift-Files "$sdkSwift\CoreText_Private") $ctFlags | Out-Null
    # Compute (camada Swift) e os Shims do OpenAttributeGraph sobre ele
    $ov2 = "$nsdk\overlay"
    $sup = Prepare-Src 'ComputeCxxSwiftSupport' "$cmp\ComputeCxxSwiftSupport" @("$ov2\Compute\ComputeCxxSwiftSupport")
    Build-Swift 'ComputeCxxSwiftSupport' (Swift-Files $sup) @() | Out-Null
    $xcc = @(); foreach ($a in @("-I$cmp\Platform\include", "-I$cmp\ComputeCxx\include")) { $xcc += '-Xcc', $a }
    $cFlags = @('-I', "$cmp\ComputeCxx\include") + $xcc
    $cmpSrc = Prepare-Src 'Compute' "$cmp\Compute" @("$ov2\Compute\Compute", "$mine\Replace\Compute")
    Build-Swift 'Compute' (Swift-Files $cmpSrc) $cFlags '6' | Out-Null
    $shSrc = Prepare-Src 'OpenAttributeGraphShims' "$deps\OpenAttributeGraph-main\Sources\OpenAttributeGraphShims" @("$ov2\OpenAttributeGraph\OpenAttributeGraphShims")
    Build-Swift 'OpenAttributeGraphShims' (Swift-Files $shSrc) ($cFlags + @('-D', 'OPENATTRIBUTEGRAPH_COMPUTE')) | Out-Null
    # OpenCoreGraphics: aqui os tipos CG são os do CoreGraphics de verdade (structs C e tipos CF do libSystem, os nomes
    # mangled da Apple: So6CGRectV, So10CGLineCapV...). OpenCoreGraphicsShims reexporta o CoreGraphics
    # (OPENCOREGRAPHICS_COREGRAPHICS); o OpenQuartzCore (CALayer/CATransform3D em Swift) vai sobre ele.
    $ocg = "$deps\OpenCoreGraphics-main\Sources"
    Build-Swift 'OpenCoreGraphicsShims' (Swift-Files "$mine\OpenCoreGraphicsShims") @() '6' | Out-Null
    $src = Prepare-Src 'OpenQuartzCore' "$ocg\OpenQuartzCore" @()
    foreach ($pf in Get-ChildItem $src -Filter *.swift) {
        $t = [IO.File]::ReadAllText($pf.FullName, $utf8)
        [IO.File]::WriteAllText($pf.FullName, ($t -replace 'import OpenCoreGraphics\b', 'import CoreGraphics' -replace '\bOpenCoreGraphics\.', 'CoreGraphics.'), $utf8)
    }
    Build-Swift 'OpenQuartzCore' (Swift-Files $src) @('-enable-upcoming-feature', 'InternalImportsByDefault') '6' | Out-Null
    $src = Prepare-Src 'OpenQuartzCoreShims' "$ocg\OpenQuartzCoreShims" @()
    Build-Swift 'OpenQuartzCoreShims' (Swift-Files $src) @() '6' | Out-Null
    # include do OpenRenderBoxCxx sem o submódulo ObjC (com interop o clang entraria nos cabeçalhos ObjC/Metal dele)
    if (Test-Path $orbInc) { Remove-Item $orbInc -Recurse -Force }
    Copy-Item "$orb\OpenRenderBoxCxx\include" $orbInc -Recurse
    $mmo = [IO.File]::ReadAllText("$orbInc\module.modulemap") -replace '(?s)\r?\nmodule OpenRenderBoxCxx\.ObjC \{.*?\}\r?\n', "`n"
    [IO.File]::WriteAllText("$orbInc\module.modulemap", $mmo)
    # OpenRenderBox (camada Swift sobre o C++ já compilado) e OpenRenderBoxShims
    $xcc = @(); foreach ($a in @("-I$orbInc", '-DOPENRENDERBOX', '-DOPENRENDERBOX_CF_CGTYPES', '-DOPENRENDERBOX_MOS')) { $xcc += '-Xcc', $a }
    $orbFlags = @('-I', "$orbInc", '-D', 'OPENRENDERBOX', '-D', 'OPENRENDERBOX_CF_CGTYPES',
                  '-enable-upcoming-feature', 'InternalImportsByDefault', '-package-name', 'OpenRenderBox') + $xcc
    $orbSrc = Prepare-Src 'OpenRenderBox' "$orb\OpenRenderBox" @("$ov2\OpenRenderBox\OpenRenderBox")
    Build-Swift 'OpenRenderBox' (Swift-Files $orbSrc) $orbFlags | Out-Null
    Build-Swift 'OpenRenderBoxShims' (Swift-Files "$orb\OpenRenderBoxShims") $orbFlags | Out-Null
}

$script:extras = $true
if ($run -contains 'Core') {
    $mos = "$sdkSwift\OpenSwiftUIMOS"
    $coreSrc = Prepare-Src 'OpenSwiftUICore' "$osui\OpenSwiftUICore" @("$mos\Replace\Sources\OpenSwiftUICore", "$nsdk\overlay\OpenSwiftUICore", "$mine\Replace\OpenSwiftUICore")
    Apply-Patches $coreSrc $corePatches
    foreach ($pf in Get-ChildItem $coreSrc -Recurse -Filter *.swift) {
        $t = [IO.File]::ReadAllText($pf.FullName, $utf8)
        if ($t.Contains('#isolation')) { [IO.File]::WriteAllText($pf.FullName, $t.Replace('#isolation', 'nil'), $utf8) }
    }
    $rt = Join-Path $coreSrc 'View\Text\Resolve\ResolvedText.swift'
    if (Test-Path $rt) {
        $t = [IO.File]::ReadAllText($rt, $utf8)
        $n = $t.Replace("#if false`n        var adaptiveImageGlyph", "#if canImport(CoreText)`n        var adaptiveImageGlyph")
        if ($n -ne $t) { [IO.File]::WriteAllText($rt, $n, $utf8) }
    }
    # arquivos de plataforma do mOS (render, imagens, foco: OpenSwiftUIMOS\Core) e os desta variante, no mesmo módulo
    $files = (Swift-Files $coreSrc) + (Swift-Files (Prepare-Src 'OpenSwiftUICore-mos' $null @("$mos\Core", "$mine\Core")))
    Write-Host "OpenSwiftUICore: $($files.Count) fontes..."
    Build-Swift 'OpenSwiftUICore' $files (OSUI-Flags) '5' $true | Out-Null
}

if ($run -contains 'UI') {
    $mos = "$sdkSwift\OpenSwiftUIMOS"
    $uiSrc = Prepare-Src 'OpenSwiftUI' "$osui\OpenSwiftUI" @("$mos\Replace\Sources\OpenSwiftUI", "$mine\Replace\OpenSwiftUI") 'up'
    foreach ($pf in Get-ChildItem $uiSrc -Recurse -Filter *.swift) {
        $txt = [IO.File]::ReadAllText($pf.FullName, $utf8)
        $o = $txt.Replace('#isolation', 'nil')
        # sem atores no grafo de views: as funções async do OpenSwiftUI viram síncronas (como na variante nativa)
        $lines = $o -split "`n", -1
        for ($i = 0; $i -lt $lines.Length; $i++) {
            if ($lines[$i] -match '^\s*#') { continue }
            $n = [regex]::Replace($lines[$i], '\)\s+async\b', ')')
            $n = [regex]::Replace($n, '\basync\s+throws\b', 'throws')
            $n = [regex]::Replace($n, '\basync\s+->', '->')
            $lines[$i] = [regex]::Replace($n, '(?<!\.)\bawait\b', '')
        }
        $o = $lines -join "`n"
        if ($o -ne $txt) { [IO.File]::WriteAllText($pf.FullName, $o, $utf8) }
    }
    $files = (Swift-Files $uiSrc) + (Swift-Files (Prepare-Src 'OpenSwiftUI-mos' $null @("$mos\App", "$mine\UI")))
    Write-Host "OpenSwiftUI: $($files.Count) fontes..."
    Build-Swift 'OpenSwiftUI' $files (OSUI-Flags) '5' $true | Out-Null
}

if ($run -contains 'Link' -and -not $TypeCheck) {
    # um dylib só, como o SwiftUI.framework (+ SwiftUICore reexportado) da Apple
    $objs = @(Get-ChildItem $sui -Recurse -Filter *.o | Where-Object { $_.DirectoryName -notlike "$sui\src*" } | ForEach-Object FullName)
    foreach ($d in 'Compute', 'OpenRenderBox', 'OpenSwiftUI_SPI') {
        $objs += @(Get-ChildItem "$nat\$d\*.o" | Where-Object { $_.Name -notlike '*_swift.o' -and $_.Length -gt 0 } | ForEach-Object FullName)
    }
    $objs += "$nat\CoreText\ctfont.o"
    $dir = "$objc\dylib\System\Library\Frameworks\SwiftUI.framework\Versions\A"
    New-Item -ItemType Directory -Force $dir | Out-Null
    # as dependências Swift entram como dylibs (LC_LOAD_DYLIB): o carregador do mOS só carrega do disco o que alguém pede,
    # e o Combine.framework não é pedido pelos apps; o resto (libSystem, AppKit, libobjc) fica para a busca plana
    $dy = "$objc\dylib"
    # o ld do zig só aceita dylib com extensão: uma cópia com nome .dylib (o LC_LOAD_DYLIB usa o install_name de dentro)
    Copy-Item "$dy\System\Library\Frameworks\Combine.framework\Versions\A\Combine" "$sui\c\Combine.dylib" -Force
    $libs = @("$sui\c\Combine.dylib")
    foreach ($m in 'Core', 'Foundation', 'Dispatch', 'ObjectiveC', 'Darwin', 'CoreFoundation', 'CoreGraphics', 'os', 'Observation',
                   '_Concurrency', '_StringProcessing') { $libs += "$dy\usr\lib\swift\libswift$m.dylib" }
    $libs = @($libs | Where-Object { Test-Path $_ })
    $log = & $zig c++ -target $ZigTarget -shared -o "$dir\SwiftUI" '-Wl,-install_name,/System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI' '-Wl,-undefined,dynamic_lookup' @objs @libs 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { $log | Select-Object -First 15 | ForEach-Object { "  $_" }; Write-Host 'SwiftUI.framework: link FALHOU' -ForegroundColor Red; exit 1 }
    Write-Host ("SwiftUI.framework: ok ({0:N0} KB, {1} objetos)" -f ((Get-Item "$dir\SwiftUI").Length / 1KB), $objs.Count) -ForegroundColor Green
}
exit $fails
