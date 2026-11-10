# mOS - fase 4 de docs/swift-objc-runtime.md: o Swift com interop ObjC como dylibs Mach-O com os nomes de instalação
# da Apple (/usr/lib/swift/libswift*.dylib), para binários Swift da Apple (ex.: Postico) carregarem do disco como no
# macOS. O carregador do mOS (kernel/compat/macho.c) lê do VFS as dylibs de caminho absoluto que o libSystem não
# substitui. Uma cópia só do runtime por processo: ele vai inteiro na libswiftCore.dylib, e as outras resolvem
# contra ela (-undefined dynamic_lookup: busca plana no carregamento).
# Entrada: build\swift\objc (tools\build-swift-full-objc.ps1, build-swift-runtime-objc.ps1, build-swift-overlays-objc.ps1,
# build-swift-concurrency-objc.ps1). Saída: build\swift\objc\dylib\usr\lib\swift\*.dylib
#   .\tools\link-swift-dylibs-objc.ps1
param([string]$Arch = 'x86_64')   # -Arch arm64: build\swift-arm64 (tools\swift-arch.ps1)
$root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\swift-arch.ps1" $Arch
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
$objc = "$SwiftBuild\objc"
$ov = "$objc\overlays"
$out = "$objc\dylib\usr\lib\swift"
New-Item -ItemType Directory -Force $out | Out-Null
$fails = 0
function Link-Dylib($name, [string[]]$objs) {
    $objs = @($objs | Where-Object { $_ -and (Test-Path $_) })
    if (-not $objs) { Write-Host "${name}: sem objetos" -ForegroundColor Yellow; $script:fails++; return }
    $dst = "$out\$name.dylib"
    $log = & $zig c++ -target $ZigTarget -shared -o $dst "-Wl,-install_name,/usr/lib/swift/$name.dylib" '-Wl,-undefined,dynamic_lookup' @objs 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dst)) { $log | Select-Object -First 8 | ForEach-Object { "  $_" }; Write-Host "${name}: FALHOU" -ForegroundColor Red; $script:fails++; return }
    Write-Host ("{0}: ok ({1:N0} KB)" -f $name, ((Get-Item $dst).Length / 1KB))
}
function Ov($m) { "$ov\$m.o" }

Link-Dylib 'libswiftCore' (@("$objc\swiftCore.o") + @(Get-ChildItem "$objc\runtime\*.o" | ForEach-Object FullName))
Link-Dylib 'libswift_Concurrency' (@(Get-ChildItem "$ov\_Concurrency*.o" | ForEach-Object FullName) + @(Get-ChildItem "$ov\dylib-only\_Concurrency_*.o" | ForEach-Object FullName))
Link-Dylib 'libswiftDispatch' @(Ov 'Dispatch')
Link-Dylib 'libswiftObjectiveC' @(Ov 'ObjectiveC')
# no macOS o _swift_FORCE_LOAD_$_swift_Builtin_float vem da libswiftDarwin
Link-Dylib 'libswiftDarwin' @((Ov 'Darwin'), (Ov '_Builtin_float'))
# o CGFloat (CoreGraphics.CGFloat) fica na libswiftCoreFoundation na Apple; aqui no overlay CoreGraphics (busca plana)
Link-Dylib 'libswiftCoreFoundation' @(Ov 'CoreFoundation')
Link-Dylib 'libswiftCoreGraphics' @(Ov 'CoreGraphics')
Link-Dylib 'libswiftos' @(Ov 'os')
Link-Dylib 'libswiftObservation' @(Ov 'Observation')
# Foundation Swift (fase 5): o módulo Foundation (swift-foundation + mOS) e o que ele leva dentro (swift-collections,
# Synchronization, _FoundationRefs, _FoundationCShims). O WhatsApp a liga direto; o carregador a junta a quem pede o
# Foundation.framework (kernel/compat/macho.c)
if (Test-Path (Ov 'Foundation')) {
    Link-Dylib 'libswiftFoundation' @((Ov 'Foundation'), (Ov '_FoundationRefs'), "$ov\_FoundationCShims_c.o",
        (Ov 'Synchronization'), (Ov 'InternalCollectionsUtilities'), (Ov 'SpanPreview'), (Ov 'BasicContainers'), (Ov 'DequeModule'),
        (Ov 'OrderedCollections'), (Ov '_RopeModule'))
}
if (Test-Path (Ov 'UniformTypeIdentifiers')) { Link-Dylib 'libswiftUniformTypeIdentifiers' @(Ov 'UniformTypeIdentifiers') }
Link-Dylib 'libswift_RegexParser' @(Ov '_RegexParser')
Link-Dylib 'libswift_StringProcessing' (@(Ov '_StringProcessing') + @(Get-ChildItem "$ov\_CUnicode_*.o" | ForEach-Object FullName))
foreach ($m in 'simd', 'XPC', 'QuartzCore', 'Metal', 'IOKit', 'OSLog', 'Spatial', 'CoreAudio', 'CoreImage', 'CoreMIDI', 'QuickLookUI') {
    Link-Dylib "libswift$m" @(Ov $m)
}
exit $fails
