# mOS - teste de execução das dependências do OpenSwiftUI (Foundation/CoreFoundation do mOS, OpenCoreGraphics,
# OpenQuartzCore, OpenObservation, Compute + OpenAttributeGraphShims, OpenRenderBox) com o Swift completo, sem QEMU:
# liga userland\sdk\openswiftui\tests\{main.swift,cside.cpp} e roda no build\machotest.exe sobre um libSystem
# montado com os fontes atuais (em build\swift\openswiftui\test, sem tocar em build\libSystem.mos).
# Pré-requisitos: tools\build-openswiftui-deps.ps1, build\swift\full (stdlib + runtime) e build\machotest.exe.
param([string]$Main,   # outro programa de teste no lugar de tests\main.swift (para isolar falhas)
      [switch]$Essentials)   # tests\essentials.swift: FoundationEssentials (swift-foundation) + _StringProcessing
$root = Split-Path $PSScriptRoot -Parent
$tc = (Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory)[0].FullName
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*')[0].FullName + '\zig.exe'
$o = "$root\build\swift\openswiftui"
$full = "$root\build\swift\full"
$deps = "$root\.tools\openswiftui\deps"
$tests = "$root\userland\sdk\openswiftui\tests"
$out = "$o\test"
New-Item -ItemType Directory -Force "$out\vfs" | Out-Null

$ls = "$out\libSystem.mos"
$src = Get-ChildItem "$root\userland\libsystem\*" -Include *.c, *.S | ForEach-Object FullName
& $zig cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie -fpie -mno-red-zone -w `
    "-Wl,--image-base=0x400000100000" "-Wl,-e,cx_start" "-Wl,--no-gc-sections" $src -o $ls 2>&1 | Select-Object -First 10
if ($LASTEXITCODE) {
    # outra frente pode estar no meio de uma edição do libSystem: tenta de novo sem os arquivos que não compilam
    $bad = @(& $zig cc -target x86_64-freestanding-none -fsyntax-only -ffreestanding -fno-builtin -w $src 2>&1 | ForEach-Object { "$_" } |
             Select-String '([\w\-]+\.c)(:\d+:\d+: error|:\d+:$)' | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique)
    $bad += @($src | Where-Object { $f = $_; $null -ne (& $zig cc -target x86_64-freestanding-none -fsyntax-only -ffreestanding -fno-builtin -w $f 2>&1 | Select-String 'fatal error') } | ForEach-Object { Split-Path $_ -Leaf })
    $bad = $bad | Sort-Object -Unique
    if ($bad) {
        Write-Host "libSystem: sem $($bad -join ', ') (não compilam agora)" -ForegroundColor Yellow
        $src2 = $src | Where-Object { $bad -notcontains (Split-Path $_ -Leaf) }
        & $zig cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie -fpie -mno-red-zone -w `
            "-Wl,--image-base=0x400000100000" "-Wl,-e,cx_start" "-Wl,--no-gc-sections" $src2 -o $ls 2>&1 | Select-Object -First 10
    }
}
if ($LASTEXITCODE) {
    # ainda não liga (símbolo que só o arquivo removido tinha): usa o último build\libSystem.mos bom
    if (-not (Test-Path "$root\build\libSystem.mos")) { Write-Host 'libSystem FALHOU' -ForegroundColor Red; exit 1 }
    Write-Host 'libSystem dos fontes atuais não compila; usando build\libSystem.mos' -ForegroundColor Yellow
    Copy-Item "$root\build\libSystem.mos" $ls -Force
}

if ($Essentials) {
    # FoundationEssentials sozinho: fontes Swift + os módulos que ele usa, sem o C++ das deps
    $ess = @('_StringProcessing', '_RegexParser', 'FoundationEssentials', '_FoundationRefs', 'BasicContainers', 'DequeModule', 'OrderedCollections', '_RopeModule',
             'SpanPreview', 'InternalCollectionsUtilities', 'MOSDarwin', 'MOSos', 'Synchronization')
    $mods = @(); foreach ($m in $ess) { $mods += @('-I', "$o\$m") }
    $mods += @('-I', "$deps\swift-foundation-main\Sources\_FoundationCShims\include", '-Xcc', "-isystem$o\darwin-libc")
    $log = & "$tc\usr\bin\swift-frontend.exe" -frontend -c -target x86_64-apple-macos26 -module-name EssTest -O -disable-objc-interop -I $full `
        -I "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims" -disable-implicit-concurrency-module-import @mods -Xcc -Wno-nullability-completeness `
        $(if ($Main) { $Main } else { "$tests\essentials.swift" }) -o "$out\ess.o" 2>&1 | ForEach-Object { "$_" }
    $err = $log | Select-String ' error: '
    if ($err) { $err | Select-Object -First 20 | ForEach-Object Line; Write-Host 'Swift FALHOU' -ForegroundColor Red; exit 1 }
    $libs = @($ess + @('_FoundationCShims')) | ForEach-Object { "$o\$_\lib$_.a" }
    $rt = Get-ChildItem "$full\runtime\*.o" | ForEach-Object FullName
    & $zig c++ -target x86_64-macos "$out\ess.o" @libs "$full\swiftCore.o" @rt -o "$out\esstest" "-Wl,-undefined,dynamic_lookup" 2>&1 | Select-Object -First 30
    if ($LASTEXITCODE) { Write-Host 'link FALHOU' -ForegroundColor Red; exit 1 }
    $run = & "$root\build\machotest.exe" $ls "$out\vfs" "$out\esstest" 2>&1 | ForEach-Object { "$_" }
    $run
    if (-not ($run -match '^TUDO OK')) { Write-Host 'teste FALHOU' -ForegroundColor Red; exit 1 }
    exit 0
}

$cmp = "$deps\Compute-main\Sources"
# o Foundation do mOS reexporta o FoundationEssentials: a pilha dele entra no -I e no link
$essMods = @('_StringProcessing', '_RegexParser', 'FoundationEssentials', '_FoundationRefs', 'BasicContainers', 'DequeModule', 'OrderedCollections', '_RopeModule',
             'SpanPreview', 'InternalCollectionsUtilities', 'MOSDarwin', 'MOSos', 'Synchronization')
$mods = @('-I', "$root\userland\sdk\openswiftui\swift\MOSLibm", '-I', "$o\OpenObservation\include", '-I', "$cmp\ComputeCxx\include") +
        (@('Dispatch', 'CoreGraphics', 'Foundation', 'OpenCoreGraphics', 'OpenQuartzCore', 'OpenObservation', 'Compute', 'OpenAttributeGraphShims') | ForEach-Object { '-I', "$o\$_" }) +
        @('-Xcc', "-I$root\userland\sdk\openswiftui\importer", '-Xcc', "-isystem$o\darwin-libc", '-Xcc', "-I$cmp\Platform\include", '-Xcc', "-I$cmp\ComputeCxx\include") +
        ($essMods | ForEach-Object { '-I', "$o\$_" }) + @('-I', "$deps\swift-foundation-main\Sources\_FoundationCShims\include")
$log = & "$tc\usr\bin\swiftc.exe" -frontend -c -target x86_64-apple-macos26 -module-name DepsTest -O -disable-objc-interop -I $full `
    -I "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims" @mods -Xcc -Wno-nullability-completeness $(if ($Main) { $Main } else { "$tests\main.swift" }) -o "$out\main.o" 2>&1 | ForEach-Object { "$_" }
$err = $log | Select-String ' error: '
if ($err) { $err | Select-Object -First 20 | ForEach-Object Line; Write-Host 'Swift FALHOU' -ForegroundColor Red; exit 1 }

$s1 = "$deps\OpenAttributeGraph-main\Sources"; $s2 = "$deps\OpenRenderBox-main\Sources"; $shim = "$root\userland\sdk\openswiftui\include"
& $zig c++ -target x86_64-macos -O1 -w -c -std=c++20 -fno-exceptions -fno-rtti -fblocks -I $shim -include "$shim\mos_openswiftui_prelude.h" `
    -I "$s1\OpenAttributeGraphCxx\include" -I "$s1\Platform\include" -I $s1 -isystem "$s1\SwiftCorelibs\include" -I "$s2\OpenRenderBoxCxx\include" `
    -isystem "$root\userland\sdk\include" -isystem "$root\.tools\swift-src\include" -isystem "$root\.tools\swift-src\stdlib\public\SwiftShims" `
    -DOPENATTRIBUTEGRAPH -DOPENRENDERBOX -DOPENRENDERBOX_CF_CGTYPES "$tests\cside.cpp" -o "$out\cside.o" 2>&1 | Select-Object -First 15
if ($LASTEXITCODE) { Write-Host 'C++ FALHOU' -ForegroundColor Red; exit 1 }

$libs = @(@('OpenAttributeGraphShims', 'Compute', 'OpenCoreGraphics', 'OpenQuartzCore', 'OpenObservation', 'OpenRenderBox', 'Foundation', 'CoreGraphics', 'MOSCFRuntime') + $essMods + @('_FoundationCShims')) | ForEach-Object { "$o\$_\lib$_.a" }
$rt = Get-ChildItem "$full\runtime\*.o" | ForEach-Object FullName
& $zig c++ -target x86_64-macos "$out\main.o" "$out\cside.o" @libs "$full\swiftCore.o" @rt -o "$out\depstest" "-Wl,-undefined,dynamic_lookup" 2>&1 | Select-Object -First 30
if ($LASTEXITCODE) { Write-Host 'link FALHOU' -ForegroundColor Red; exit 1 }

$run = & "$root\build\machotest.exe" $ls "$out\vfs" "$out\depstest" 2>&1 | ForEach-Object { "$_" }
$run
if (-not ($run -match '^TUDO OK')) { Write-Host 'teste FALHOU' -ForegroundColor Red; exit 1 }
