# mOS - overlays Swift da variante com interop ObjC (docs/swift-objc-runtime.md, fase 3): os módulos que binários
# Swift da Apple importam de /usr/lib/swift (libswiftObjectiveC, libswiftDarwin, ...), escritos para o mOS com os
# nomes e layouts do SDK da Apple. Fontes em userland\sdk\swift-objc\<Módulo>; stdlib com ObjC em build\swift\objc
# (tools\build-swift-full-objc.ps1). Saída: build\swift\objc\overlays\<Módulo>.swiftmodule e <Módulo>.o
#   .\tools\build-swift-overlays-objc.ps1 [-Only ObjectiveC]
param([string[]]$Only)
$root = Split-Path $PSScriptRoot -Parent
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*')[0].FullName
$objc = "$root\build\swift\objc"
$out = "$objc\overlays"
$sdkSrc = "$root\userland\sdk\swift-objc"
if (-not (Test-Path "$objc\swiftCore.o")) { Write-Host 'falta a stdlib com ObjC (tools\build-swift-full-objc.ps1)' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force $out | Out-Null
if ($Only) { $Only = @($Only | ForEach-Object { $_ -split ',' }) }

# ---- "SDK": cabeçalhos Darwin do zig (o ABI que o libSystem do mOS implementa) + módulos clang Darwin e ObjectiveC.
# Como em tools\build-openswiftui-deps.ps1, `__has_feature(modules)` vira 0 na cópia (os cabeçalhos esperam o
# stddef modular do SDK da Apple).
$inc = "$objc\sdk\include"
if (-not (Test-Path "$inc\mos_darwin.h")) {
    if (Test-Path $inc) { Remove-Item $inc -Recurse -Force }
    New-Item -ItemType Directory -Force (Split-Path $inc) | Out-Null
    Copy-Item "$zig\lib\libc\include\any-darwin-any" $inc -Recurse
    foreach ($h in Get-ChildItem $inc -Recurse -File) {
        $b = [IO.File]::ReadAllText($h.FullName)
        if ($b.Contains('__has_feature(modules)')) { [IO.File]::WriteAllText($h.FullName, $b.Replace('__has_feature(modules)', '0')) }
    }
    Get-ChildItem "$root\userland\sdk\openswiftui\darwin" | Where-Object Name -ne 'module.modulemap' | Copy-Item -Destination $inc -Recurse -Force
}
# dispatch/workloop.h: no importador Swift o os_workgroup_t (os/workgroup_object.h) não chega a essa declaração em
# modo de módulos (no clang puro chega). Ela só vale com o workgroup visível; o resto do Dispatch não usa.
$wl = "$inc\dispatch\workloop.h"
$wt = [IO.File]::ReadAllText($wl)
if (-not $wt.Contains('mOS: workgroup')) {
    $decl = 'dispatch_workloop_set_os_workgroup(dispatch_workloop_t workloop,'
    $i = $wt.IndexOf($decl); $s = $wt.LastIndexOf('API_AVAILABLE', $i); $e = $wt.IndexOf(';', $i) + 1
    $wt = $wt.Substring(0, $s) + "#if defined(__OS_WORKGROUP_OBJECT__) /* mOS: workgroup */`n" + $wt.Substring($s, $e - $s) + "`n#endif" + $wt.Substring($e)
    [IO.File]::WriteAllText($wl, $wt)
}
Copy-Item "$sdkSrc\sdk\module.modulemap" "$inc\module.modulemap" -Force
Copy-Item "$sdkSrc\sdk\os\log.h" "$inc\os\log.h" -Force
Copy-Item "$sdkSrc\sdk\Foundation" $inc -Recurse -Force   # Foundation em ObjC para o overlay Foundation (fase 5)
# CoreFoundation e CoreGraphics em C: os mesmos cabeçalhos que o importador do OpenSwiftUI usa
foreach ($fw in 'CoreFoundation') { Copy-Item "$root\userland\sdk\openswiftui\importer\$fw" $inc -Recurse -Force }
Copy-Item "$root\userland\sdk\include\CoreGraphics" $inc -Recurse -Force   # o do SDK (com MOS_SWIFT_OBJC: nomes da Apple)

$fails = 0
function Build-Overlay($name, [string[]]$files = @(), [bool]$underlying = $true, [string[]]$extra = @()) {
    if ($Only -and $Only -notcontains $name) { return }
    if (-not $files) { $files = @(Get-ChildItem "$sdkSrc\$name" -Filter *.swift | ForEach-Object FullName) }
    $a = @('-c', '-target', 'x86_64-apple-macos26', '-module-name', $name, '-parse-as-library', '-swift-version', '5', '-O',
           '-enable-library-evolution', '-I', $objc, '-I', $out, '-module-cache-path', "$objc\modcache",
           '-I', "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims",
           '-disable-implicit-concurrency-module-import', '-disable-implicit-string-processing-module-import',
           '-Xcc', "-isystem$inc", '-Xcc', '-Wno-nullability-completeness', '-Xcc', '-DMOS_SWIFT_OBJC=1',
           '-emit-module-path', "$out\$name.swiftmodule", '-o', "$out\$name.o")
    if ($underlying) { $a += '-import-underlying-module' }
    # _swift_FORCE_LOAD_$_swift<Módulo>: os binários da Apple importam este símbolo de cada libswift<Módulo>.dylib
    $a += '-module-link-name', "swift$name", '-autolink-force-load'
    $a += $extra + $files
    $rsp = "$out\$name.rsp"
    ($a | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join "`n" | Set-Content -Encoding ascii $rsp
    $log = & "$($tc.FullName)\usr\bin\swift-frontend.exe" -frontend "@$rsp" 2>&1 | ForEach-Object { "$_" }
    $log | Set-Content -Encoding utf8 "$out\$name.log"
    $err = $log | Select-String ' error: '
    if ($LASTEXITCODE -ne 0 -or $err) { $err | Select-Object -First 15 | ForEach-Object { "  $($_.Line)" }; Write-Host "${name}: FALHOU" -ForegroundColor Red; $script:fails++; return }
    Write-Host ("{0}: ok ({1:N0} KB)" -f $name, ((Get-Item "$out\$name.o").Length / 1KB))
}

$gen = "$objc\overlays-src"
New-Item -ItemType Directory -Force $gen | Out-Null
$utf8 = New-Object Text.UTF8Encoding $false

# Darwin: o mesmo overlay da libc que o OpenSwiftUI usa (userland\sdk\openswiftui\swift\MOSDarwin, errno,
# POSIXErrorCode, open/fcntl/ioctl...), aqui sobre o módulo clang com o nome da Apple
$md = [IO.File]::ReadAllText("$root\userland\sdk\openswiftui\swift\MOSDarwin\MOSDarwin.swift", $utf8)
[IO.File]::WriteAllText("$gen\Darwin.swift", "// GERADO por tools\build-swift-overlays-objc.ps1 a partir de MOSDarwin.swift`n" + $md.Replace('@_exported import MOSDarwin', '@_exported import Darwin').Replace('MOSDarwin.', 'Darwin.'), $utf8)
Build-Overlay 'Darwin' @("$gen\Darwin.swift")
Build-Overlay 'ObjectiveC'
Build-Overlay 'CoreFoundation'
# CoreGraphics: CGFloat do módulo CoreGraphics do OpenSwiftUI (userland\sdk\openswiftui\swift\CoreGraphics) + CGGeometry.swift
$cg = [IO.File]::ReadAllText("$root\userland\sdk\openswiftui\swift\CoreGraphics\CGFloat.swift", $utf8)
[IO.File]::WriteAllText("$gen\CGFloat.swift", "// GERADO por tools\build-swift-overlays-objc.ps1 a partir do CGFloat.swift do OpenSwiftUI`n" + $cg.Replace('@_exported import MOSLibm', "@_exported import CoreGraphics`nimport Darwin"), $utf8)
Build-Overlay 'CoreGraphics' @("$gen\CGFloat.swift", "$sdkSrc\CoreGraphics\CGGeometry.swift")
Build-Overlay 'os'
Build-Overlay 'Dispatch'
# Foundation (fase 5): pontes ObjC e API sobre as classes do Foundation do libSystem do mOS
Build-Overlay 'Foundation'
# Observation: o OpenObservation (github.com/OpenSwiftUIProject, MIT; porte do Observation da Apple) com o nome do
# módulo da Apple, no caminho Darwin em Swift puro (sem a parte C++ do toolchain)
$obs = "$root\.tools\openswiftui\deps\OpenObservation-main\Sources\OpenObservation"
if (Test-Path $obs) { Build-Overlay 'Observation' @(Get-ChildItem $obs -Filter *.swift | ForEach-Object FullName) $false }

# _StringProcessing (Regex): swift-experimental-string-processing 6.4.2 (Apache 2.0, a versão do toolchain), como no
# tools\build-openswiftui-deps.ps1 mas com interop: _RegexParser + _CUnicode (C) + _StringProcessing
$spr = "$root\.tools\openswiftui\deps\swift-experimental-string-processing-release-6.4.2\Sources"
if (Test-Path $spr) {
    $scAvail = @{ '5.7' = '13.0'; '5.8' = '13.3'; '5.9' = '14.0'; '5.10' = '14.4'; '6.0' = '15.0'; '6.1' = '15.4'; '6.2' = '26.0'; '6.3' = '26.4'; '6.4' = '27.0' }
    $spAvail = @(); foreach ($k in $scAvail.Keys) { $spAvail += @('-define-availability', "SwiftStdlib ${k}:macOS $($scAvail[$k]), iOS 99, watchOS 99, tvOS 99") }
    Build-Overlay '_RegexParser' @(Get-ChildItem "$spr\_RegexParser" -Recurse -Filter *.swift | ForEach-Object FullName) $false $spAvail
    if (-not $Only -or $Only -contains '_StringProcessing') {
        foreach ($c in Get-ChildItem "$spr\_CUnicode" -Filter *.c) {
            & "$zig\zig.exe" cc -target x86_64-macos -O2 -c "-I$spr\_CUnicode\include" -std=gnu11 $c.FullName -o "$out\_CUnicode_$($c.BaseName).o" 2>&1 | ForEach-Object { "  $_" }
        }
    }
    Build-Overlay '_StringProcessing' @(Get-ChildItem "$spr\_StringProcessing" -Recurse -Filter *.swift | ForEach-Object FullName) $false ($spAvail + @('-Xcc', "-I$spr\_CUnicode\include"))
}

# módulos de que o Postico só importa o _swift_FORCE_LOAD_$_swift<Módulo> (a API vem dos frameworks em C/ObjC)
foreach ($m in '_Builtin_float', 'simd', 'XPC', 'QuartzCore', 'Metal', 'IOKit', 'OSLog', 'Spatial', 'CoreAudio', 'CoreImage', 'CoreMIDI', 'QuickLookUI') {
    $f = "$gen\$m.swift"
    [IO.File]::WriteAllText($f, "// GERADO: módulo vazio; só carrega o símbolo _swift_FORCE_LOAD_`$_swift$m`n", $utf8)
    Build-Overlay $m @($f) $false
}
exit $fails
