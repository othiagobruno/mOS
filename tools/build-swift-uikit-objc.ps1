# mOS - overlay Swift do UIKit do Mac Catalyst (docs/uikit-catalyst.md): userland\sdk\swift-objc\UIKit\*.swift sobre o
# módulo clang UIKit (userland\sdk\swift-objc\sdk\UIKit), com interop ObjC e library evolution, como o módulo "UIKit".
# Sai em build\swift\objc\overlays (UIKit.swiftmodule; o objeto em overlays\uikit, fora do link estático dos outros testes) e como
# /System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib na raiz das dylibs (build\swift\objc\dylib).
# Antes: tools\build-swift-foundation-objc.ps1 (Foundation) e tools\build-frameworks-objc.ps1 -Only UIKit.
#   .\tools\build-swift-uikit-objc.ps1
$root = Split-Path $PSScriptRoot -Parent
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*')[0].FullName + '\zig.exe'
$objc = "$root\build\swift\objc"
$out = "$objc\overlays"
$inc = "$objc\sdk\include"
$sdkSrc = "$root\userland\sdk\swift-objc"
if (-not (Test-Path "$out\Foundation.swiftmodule")) { Write-Host 'falta o Foundation (tools\build-swift-foundation-objc.ps1)' -ForegroundColor Red; exit 1 }
# o módulo clang UIKit vai junto dos outros cabeçalhos da variante
Copy-Item "$sdkSrc\sdk\module.modulemap" "$inc\module.modulemap" -Force
Copy-Item "$sdkSrc\sdk\UIKit" $inc -Recurse -Force
Copy-Item "$sdkSrc\sdk\Foundation" $inc -Recurse -Force

$files = @(Get-ChildItem "$sdkSrc\UIKit" -Filter *.swift | ForEach-Object FullName)
$a = @('-c', '-target', 'x86_64-apple-macos26', '-module-name', 'UIKit', '-parse-as-library', '-swift-version', '5', '-O',
       '-enable-library-evolution', '-I', $objc, '-I', $out, '-module-cache-path', "$objc\modcache",
       '-I', "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims",
       '-Xcc', "-isystem$inc", '-Xcc', '-Wno-nullability-completeness', '-Xcc', '-DMOS_SWIFT_OBJC=1',
       '-import-underlying-module', '-module-link-name', 'swiftUIKit', '-autolink-force-load',
       '-emit-module-path', "$out\UIKit.swiftmodule", '-o', "$out\uikit\UIKit.o") + $files
New-Item -ItemType Directory -Force "$out\uikit" | Out-Null
$rsp = "$out\UIKit.rsp"
($a | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join "`n" | Set-Content -Encoding ascii $rsp
$log = & "$($tc.FullName)\usr\bin\swift-frontend.exe" -frontend "@$rsp" 2>&1 | ForEach-Object { "$_" }
$log | Set-Content -Encoding utf8 "$out\UIKit.log"
$err = $log | Select-String -Pattern ':\d+:\d+: error: |<unknown>:0: error: '
if ($LASTEXITCODE -ne 0 -or $err) { $err | Select-Object -First 20 | ForEach-Object { "  $($_.Line)" }; Write-Host 'UIKit (Swift): FALHOU' -ForegroundColor Red; exit 1 }
Write-Host ("UIKit (Swift): ok ({0:N0} KB, {1} fontes)" -f ((Get-Item "$out\uikit\UIKit.o").Length / 1KB), $files.Count)

$dir = "$objc\dylib\System\iOSSupport\usr\lib\swift"
New-Item -ItemType Directory -Force $dir | Out-Null
$log = & $zig c++ -target x86_64-macos -shared -o "$dir\libswiftUIKit.dylib" '-Wl,-install_name,/System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib' '-Wl,-undefined,dynamic_lookup' "$out\uikit\UIKit.o" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { $log | Select-Object -First 8 | ForEach-Object { "  $_" }; Write-Host 'libswiftUIKit: link FALHOU' -ForegroundColor Red; exit 1 }
Write-Host ("libswiftUIKit: ok ({0:N0} KB) -> /System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib" -f ((Get-Item "$dir\libswiftUIKit.dylib").Length / 1KB))
