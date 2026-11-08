# mOS - teste da camada de Path/DisplayList do SwiftUI do mOS, com o Swift completo e sem QEMU:
# monta um libSystem com os fontes atuais (em build\swift\openswiftui\pathtest, sem tocar em build\libSystem.mos),
# compila userland\sdk\openswiftui\tests\path\*.swift com OpenSwiftUIMOS\Core\MOSPathGeometry.swift e roda no
# build\machotest.exe com MOS_PATH_TRACE (registro da ponte C) e MOS_PATH_RASTER (rasterização própria, que
# mede a área coberta de cada preenchimento).
# Pré-requisitos: tools\build-openswiftui-deps.ps1 (Foundation, OpenCoreGraphics), build\swift\full e build\machotest.exe.
$root = Split-Path $PSScriptRoot -Parent
$tc = (Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory)[0].FullName
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Where-Object { Test-Path "$($_.FullName)\zig.exe" })[0].FullName + '\zig.exe'
$o = "$root\build\swift\openswiftui"
$full = "$root\build\swift\full"
$tests = "$root\userland\sdk\openswiftui\tests\path"
$out = "$o\pathtest"
New-Item -ItemType Directory -Force "$out\vfs" | Out-Null

$ls = "$out\libSystem.mos"
$src = Get-ChildItem "$root\userland\libsystem\*" -Include *.c, *.S | ForEach-Object FullName
& $zig cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie -fpie -mno-red-zone -w `
    "-Wl,--image-base=0x400000100000" "-Wl,-e,cx_start" "-Wl,--no-gc-sections" $src -o $ls 2>&1 | Select-Object -First 10
if ($LASTEXITCODE) { Write-Host 'libSystem FALHOU' -ForegroundColor Red; exit 1 }

# a geometria entra como no OpenSwiftUICore, trocando o import do shim pelo OpenCoreGraphics
$geo = [IO.File]::ReadAllText("$root\userland\sdk\openswiftui\swift\OpenSwiftUIMOS\Core\MOSPathGeometry.swift")
$geo = $geo.Replace('package import OpenCoreGraphicsShims', 'import OpenCoreGraphics').Replace('package import Foundation', 'import Foundation')
[IO.File]::WriteAllText("$out\MOSPathGeometry.swift", $geo, (New-Object Text.UTF8Encoding($false)))

$mods = @('-I', "$root\userland\sdk\openswiftui\swift\MOSLibm", '-I', "$root\userland\sdk\swift\MOSBridge") +
        (@('Dispatch', 'CoreFoundation', 'Foundation', 'OpenCoreGraphics') | ForEach-Object { '-I', "$o\$_" }) +
        @('-Xcc', "-I$root\userland\sdk\openswiftui\importer")
$log = & "$tc\usr\bin\swiftc.exe" -frontend -c -target x86_64-apple-macos26 -module-name PathTest -package-name mos -O -disable-objc-interop -I $full `
    -I "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims" @mods -Xcc -Wno-nullability-completeness `
    "$tests\main.swift" "$tests\path_stub.swift" "$out\MOSPathGeometry.swift" -o "$out\main.o" 2>&1 | ForEach-Object { "$_" }
$err = $log | Select-String ' error: '
if ($err -or -not (Test-Path "$out\main.o")) { $err | Select-Object -First 20 | ForEach-Object Line; Write-Host 'Swift FALHOU' -ForegroundColor Red; exit 1 }

$libs = @('OpenCoreGraphics', 'Foundation', 'CoreFoundation', 'MOSCFRuntime') | ForEach-Object { "$o\$_\lib$_.a" } | Where-Object { Test-Path $_ }
$rt = Get-ChildItem "$full\runtime\*.o" | ForEach-Object FullName
& $zig c++ -target x86_64-macos "$out\main.o" @libs "$full\swiftCore.o" @rt -o "$out\pathtest" "-Wl,-undefined,dynamic_lookup" 2>&1 | Select-Object -First 30
if ($LASTEXITCODE) { Write-Host 'link FALHOU' -ForegroundColor Red; exit 1 }

$env:MOS_PATH_TRACE = '1'; $env:MOS_PATH_RASTER = '1'; $env:MACHOTEST_EVENTS = 'f:3;close'
$run = & "$root\build\machotest.exe" $ls "$out\vfs" "$out\pathtest" 2>&1 | ForEach-Object { "$_" }
Remove-Item Env:MOS_PATH_TRACE, Env:MOS_PATH_RASTER, Env:MACHOTEST_EVENTS
$run
if (-not ($run -match 'TUDO OK')) { Write-Host 'teste FALHOU' -ForegroundColor Red; exit 1 }
