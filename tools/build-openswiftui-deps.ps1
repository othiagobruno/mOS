# mOS - dependências do OpenSwiftUI completo (github.com/OpenSwiftUIProject, licença MIT):
#   OpenAttributeGraph (grafo de atributos, C++)   OpenRenderBox (paths/cores/animação, C++)
#   OpenObservation (@Observable/ObservationRegistrar, Swift + C++)   OpenCoreGraphics (tipos CG em Swift)
# Fontes em .tools\openswiftui\deps\<Repo>-main (baixados de github.com/OpenSwiftUIProject para
# .tools\openswiftui-dl\<Repo>.zip se faltarem). As partes C/C++ são compiladas com o clang do zig para
# x86_64-macos (o ABI Mach-O que o mOS executa), sobre o libSystem do mOS (CoreGraphics em
# userland\libsystem\coregraphics.c, ABI de C++ em cxxabi.c) e os shims de userland\sdk\openswiftui.
# A parte Swift precisa da stdlib COMPLETA (tools\build-swift-full.ps1: build\swift\full) e, para rodar,
# do runtime Swift (tools\build-swift-runtime.ps1). Saída: build\swift\openswiftui\<Repo>\lib<Repo>.a
#
#   .\tools\build-openswiftui-deps.ps1                         # tudo
#   .\tools\build-openswiftui-deps.ps1 -Repo OpenRenderBox     # um repositório
#   .\tools\build-openswiftui-deps.ps1 -Only Graph,Subgraph    # só alguns fontes (sem gerar a .a)
param([string[]]$Repo = @('OpenAttributeGraph', 'Compute', 'OpenRenderBox', 'OpenObservation', 'Foundation', 'OpenCoreGraphics'), [string[]]$Only)
$root = Split-Path $PSScriptRoot -Parent
$zig = Join-Path (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Select-Object -First 1).FullName 'zig.exe'
$deps = "$root\.tools\openswiftui\deps"
$swsrc = "$root\.tools\swift-src"
$outRoot = "$root\build\swift\openswiftui"
$shim = "$root\userland\sdk\openswiftui\include"   # os/log.h e tipos de CF que faltam nos cabeçalhos do zig
$full = "$root\build\swift\full"                   # stdlib Swift completa (sessão do runtime Swift)
$importerInc = "$root\userland\sdk\openswiftui\importer"

# ---- fontes
$dl = "$root\.tools\openswiftui-dl"
New-Item -ItemType Directory -Force $deps, $dl | Out-Null
foreach ($r in @('OpenAttributeGraph', 'OpenRenderBox', 'OpenObservation', 'OpenCoreGraphics')) {
    if (Test-Path "$deps\$r-main\Sources") { continue }
    $zip = "$dl\$r.zip"
    if (-not (Test-Path $zip)) {
        Write-Host "baixando $r..."
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest "https://github.com/OpenSwiftUIProject/$r/archive/refs/heads/main.zip" -OutFile $zip -UseBasicParsing
    }
    Expand-Archive $zip -DestinationPath $deps -Force
}

$common = @('-target', 'x86_64-macos', '-O2', '-fno-sanitize=all', '-ffunction-sections', '-DNDEBUG', '-w', '-c',
            "-isystem$swsrc\include", "-isystem$swsrc\stdlib\include", "-isystem$swsrc\stdlib\public\SwiftShims")
$cxx = @('-std=c++20', '-fno-exceptions', '-fno-rtti')
function Build-Files($name, $files, $cflags, $cxxflags) {
    $out = "$outRoot\$name"; New-Item -ItemType Directory -Force $out | Out-Null
    if ($Only) { $files = $files | Where-Object { $Only -contains [IO.Path]::GetFileNameWithoutExtension($_) } }
    $ok = 0; $bad = @(); $objs = @()
    foreach ($f in $files) {
        $o = "$out\" + [IO.Path]::GetFileNameWithoutExtension($f) + '.o'
        $isCxx = $f -match '\.(cpp|mm)$'
        $drv = if ($isCxx) { 'c++' } else { 'cc' }
        $fl = if ($isCxx) { $cxxflags } else { $cflags }
        $log = & $zig $drv @common @fl $f -o $o 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) {
            $bad += [IO.Path]::GetFileName($f)
            $log | Select-String 'error:' | Select-Object -First 4 | ForEach-Object { Write-Host ('  ' + $_.Line.Substring(0, [Math]::Min(240, $_.Line.Length))) }
        } else { $ok++; $objs += $o }
    }
    Write-Host "${name} (C/C++): $ok ok, $($bad.Count) com erro $(if ($bad) { '(' + ($bad -join ', ') + ')' })" -ForegroundColor $(if ($bad) { 'Yellow' } else { 'Green' })
    $script:objs = $objs
    return $bad.Count
}
function Write-Archive($name, $objs) {
    if ($Only -or -not $objs) { return }
    $a = "$outRoot\$name\lib$name.a"
    Remove-Item $a -ErrorAction SilentlyContinue
    & $zig ar rcs $a @objs | Out-Null
    Write-Host ("  -> lib$name.a ({0:N0} KB)" -f ((Get-Item $a).Length / 1KB))
}
# módulo Swift contra a stdlib completa (sem ObjC, com library evolution, como build\swift\full)
function Build-Swift($name, $files, $extra, $lang = '5', [bool]$evolution = $true) {
    if (-not (Test-Path "$full\Swift.swiftmodule")) { Write-Host "${name} (Swift): sem stdlib completa em build\swift\full; pulando" -ForegroundColor Yellow; return $null }
    $tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
    $out = "$outRoot\$name"
    $a = @('-frontend', '-c', '-target', 'x86_64-apple-macos26', '-module-name', $name, '-parse-as-library', '-swift-version', $lang, '-O',
           '-disable-objc-interop', '-I', $full, '-I', "$swsrc\stdlib\public\SwiftShims\swift\shims",
           '-disable-implicit-concurrency-module-import', '-disable-implicit-string-processing-module-import',
           '-enable-experimental-feature', 'Extern', '-Xcc', '-Wno-nullability-completeness',
           '-Xcc', "-I$importerInc",   # CoreFoundation C mínimo para o importador (a libc do zig quebra em modo de módulos)
           '-emit-module-path', "$out\$name.swiftmodule", '-o', "$out\${name}_swift.o") + $(if ($evolution) { @('-enable-library-evolution') } else { @() }) + $extra + $files
    # argumentos num arquivo de resposta: a linha de comando do Windows tem limite (o OpenSwiftUICore tem 600+ fontes)
    $rsp = "$out\$name.rsp"
    $a = @($a | Where-Object { $_ -ne '-frontend' })  # o swift-frontend.exe já é o frontend
    ($a | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join "`n" | Set-Content -Encoding ascii $rsp
    $log = & "$($tc.FullName)\usr\bin\swift-frontend.exe" -frontend "@$rsp" 2>&1 | ForEach-Object { "$_" }
    $log | Set-Content -Encoding utf8 "$out\build.log"   # log completo (erros além dos 15 mostrados)
    $err = $log | Select-String ' error: '
    if ($LASTEXITCODE -ne 0 -or $err) { $err | Select-Object -First 15 | ForEach-Object { Write-Host "  $($_.Line)" }; Write-Host "${name} (Swift): FALHOU" -ForegroundColor Red; return $false }
    Write-Host "${name} (Swift): ok ($($files.Count) fontes)" -ForegroundColor Green
    return "$out\${name}_swift.o"
}

# cópia pré-processada de fontes Swift: com -target x86_64-apple-macos26, `os(macOS)` é verdadeiro e levaria o
# código aos caminhos que exigem o SDK/ObjC da Apple; no mOS ele vira `false` só em linhas #if/#elseif (canImport(Darwin)
# já é falso), forçando os caminhos não-Darwin. Depois os arquivos do overlay (mesmos caminhos relativos) vão por cima.
function Prepare-Src($name, $srcDir, $overlayDir) {
    $dst = "$outRoot\src-mos\$name"
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
    New-Item -ItemType Directory -Force $dst | Out-Null
    $utf8 = New-Object Text.UTF8Encoding $false
    foreach ($f in Get-ChildItem $srcDir -Recurse -Filter *.swift) {
        $rel = $f.FullName.Substring((Resolve-Path $srcDir).Path.Length + 1)
        $out = Join-Path $dst $rel
        New-Item -ItemType Directory -Force (Split-Path $out -Parent) | Out-Null
        $lines = [IO.File]::ReadAllLines($f.FullName, $utf8)
        for ($i = 0; $i -lt $lines.Length; $i++) {
            if ($lines[$i] -match '^\s*#\s*(if|elseif)\b') { $lines[$i] = [regex]::Replace([regex]::Replace($lines[$i], '\bos\(\s*macOS\s*\)', 'false'), '\bcanImport\(\s*CoreGraphics\s*\)', 'false') }
            # sem runtime de concorrência (_Concurrency) no mOS: condições que dependem dele saem inteiras
            if ($lines[$i] -match '^\s*#\s*(if|elseif)\b.*canImport\(\s*_Concurrency\s*\)') { $lines[$i] = ($lines[$i] -replace '^(\s*#\s*(?:if|elseif)).*$', '$1 false // mOS: sem _Concurrency') }
            # ... e os atributos de isolamento de ator somem (tudo roda numa thread de UI; sem executor de atores)
            elseif ($lines[$i] -notmatch '^\s*#') { $lines[$i] = $lines[$i] -replace '@MainActor(\s*\(unsafe\))?\s?', '' -replace '@isolated\(any\)\s?', '' }
        }
        [IO.File]::WriteAllLines($out, $lines, $utf8)
    }
    if ($overlayDir -and (Test-Path $overlayDir)) {
        foreach ($f in Get-ChildItem $overlayDir -Recurse -File) {
            $rel = $f.FullName.Substring((Resolve-Path $overlayDir).Path.Length + 1)
            $out = Join-Path $dst $rel
            New-Item -ItemType Directory -Force (Split-Path $out -Parent) | Out-Null
            if ($f.Extension -eq '.swift') {
                $txt = [IO.File]::ReadAllText($f.FullName, $utf8) -replace '@MainActor(\s*\(unsafe\))?\s?', '' -replace '@isolated\(any\)\s?', ''
                [IO.File]::WriteAllText($out, $txt, $utf8)
            } else { Copy-Item $f.FullName $out -Force }
        }
    }
    return $dst
}
$overlay = "$root\userland\sdk\openswiftui\overlay"   # substituições do mOS em fontes upstream (<Repo>\<Target>\...)

$fails = 0
# ---- Foundation do mOS (userland\sdk\openswiftui\swift\Foundation), antes de tudo: Compute, OAG e OCG o importam.
# CGFloat/geometria, NSObject, Data, String(format:), helpers de CF. O CoreFoundation é o módulo C de
# userland\sdk\openswiftui\importer (objetos de libMOSCFRuntime.a); MOSLibm traz o math.h do libSystem.
$sdkSwift = "$root\userland\sdk\openswiftui\swift"
# Dispatch (userland\sdk\openswiftui\swift\Dispatch, da sessão do runtime Swift) é compilado antes e sai em
# build\swift\openswiftui\Dispatch; o Foundation o enxerga por -I quando passar a importá-lo
$modInc = @('-I', "$sdkSwift\MOSLibm", '-I', "$outRoot\Dispatch", '-I', "$outRoot\CoreGraphics", '-I', "$outRoot\Foundation")
if ($Repo -contains 'Foundation' -or $Repo -contains 'OpenCoreGraphics') {
    # Dispatch (camada Swift sobre o dispatch C do libSystem, da sessão do runtime Swift) vem antes de tudo
    if (Test-Path "$sdkSwift\Dispatch") {
        New-Item -ItemType Directory -Force "$outRoot\Dispatch" | Out-Null
        $sw = Build-Swift 'Dispatch' @(Get-ChildItem "$sdkSwift\Dispatch" -Filter *.swift | ForEach-Object FullName) $modInc
        if ($sw -eq $false) { $fails++ } elseif ($sw) { Write-Archive 'Dispatch' @($sw) }
    }
    # Synchronization (Atomic) do Swift 6, que o stdlib do mOS não traz: precisa de Builtin (-parse-stdlib) e @_rawLayout
    New-Item -ItemType Directory -Force "$outRoot\Synchronization" | Out-Null
    $sw = Build-Swift 'Synchronization' @(Get-ChildItem "$sdkSwift\Synchronization" -Filter *.swift | ForEach-Object FullName) @('-parse-stdlib', '-enable-experimental-feature', 'RawLayout')
    if ($sw -eq $false) { $fails++ } elseif ($sw) { Write-Archive 'Synchronization' @($sw) }
    foreach ($m in @('CoreGraphics', 'Foundation', 'Logging', 'Glibc')) {   # CoreFoundation é o módulo C de userland\sdk\openswiftui\importer
        New-Item -ItemType Directory -Force "$outRoot\$m" | Out-Null
        $sw = Build-Swift $m @(Get-ChildItem "$sdkSwift\$m" -Filter *.swift | ForEach-Object FullName) $modInc
        if ($sw -eq $false) { $fails++ } elseif ($sw) { Write-Archive $m @($sw) }
    }
}
# CFRuntime/CFRetain por isa/vm_remap do mOS (cf_mos.cpp), comum ao OpenAttributeGraph e ao Compute: ligue uma vez no app
if ($Repo -contains 'OpenAttributeGraph' -or $Repo -contains 'Compute') {
    $oagS = "$deps\OpenAttributeGraph-main\Sources"
    $inc = @("-I$shim", '-include', "$shim\mos_openswiftui_prelude.h", "-isystem$oagS\SwiftCorelibs\include")
    $fails += Build-Files 'MOSCFRuntime' @("$root\userland\sdk\openswiftui\src\cf_mos.cpp") ($inc + @('-std=c11')) ($inc + $cxx)
    $cfObjs = $script:objs
    # classes Swift dos objetos CF (_MOSCFObject, _NSCFConstantString), sem library evolution: metadata estática
    New-Item -ItemType Directory -Force "$outRoot\SwiftFoundation" | Out-Null
    $sw = Build-Swift 'SwiftFoundation' @(Get-ChildItem "$root\userland\sdk\openswiftui\swift\SwiftFoundation" -Filter *.swift | ForEach-Object FullName) @() '5' $false
    if ($sw -eq $false) { $fails++ } elseif ($sw) { $cfObjs += $sw }
    Write-Archive 'MOSCFRuntime' $cfObjs
}
if ($Repo -contains 'OpenAttributeGraph') {
    $s = "$deps\OpenAttributeGraph-main\Sources"
    $inc = @("-I$shim", '-include', "$shim\mos_openswiftui_prelude.h", "-I$s", "-I$s\Platform\include", "-I$s\Utilities\include",
             "-I$s\OpenAttributeGraphCxx", "-I$s\OpenAttributeGraphCxx\include", "-isystem$s\SwiftCorelibs\include",
             '-DOPENATTRIBUTEGRAPH', '-DOPENATTRIBUTEGRAPH_MOS')
    # DebugServer/*.mm, Graph.mm e GraphDescription.mm são Objective-C++ (servidor de depuração e descrição
    # em JSON via Foundation); interpose.c é do dyld. Os stubs ficam em oag_mos.cpp.
    $files = @(Get-ChildItem "$s\Platform", "$s\Utilities", "$s\OpenAttributeGraphCxx" -Recurse -Include *.c, *.cpp |
               Where-Object { $_.Name -ne 'interpose.c' } | ForEach-Object FullName)
    $files += "$root\userland\sdk\openswiftui\src\oag_mos.cpp"   # stubs do DebugServer (o CFRuntime está em MOSCFRuntime)
    $fails += Build-Files 'OpenAttributeGraph' $files ($inc + @('-std=c11')) ($inc + $cxx)
    $oagObjs = $script:objs
    if (-not $Only) {
        # camada Swift (Graph, Attribute, Rule...) sobre o módulo clang OpenAttributeGraphCxx
        $xcc = @()
        foreach ($a in @("-I$s\Platform\include", "-I$s\OpenAttributeGraphCxx\include", '-DOPENATTRIBUTEGRAPH', '-DOPENATTRIBUTEGRAPH_MOS')) { $xcc += '-Xcc', $a }
        $sdkSwift = "$root\userland\sdk\openswiftui\swift"
        $flags = @('-I', "$s\OpenAttributeGraphCxx\include", '-I', "$sdkSwift\MOSLibm", '-I', "$outRoot\Dispatch", '-I', "$outRoot\CoreGraphics", '-I', "$outRoot\Foundation",
                   '-D', 'OPENATTRIBUTEGRAPH', '-D', 'OPENATTRIBUTEGRAPH_RELEASE_2024', '-D', 'OPENATTRIBUTEGRAPH_SUPPORT_2021_API', '-D', 'OPENATTRIBUTEGRAPH_SUPPORT_2022_API',
                   '-D', 'OPENATTRIBUTEGRAPH_SUPPORT_2023_API', '-D', 'OPENATTRIBUTEGRAPH_SUPPORT_2024_API', '-enable-upcoming-feature', 'InternalImportsByDefault') + $xcc
        $sw = Build-Swift 'OpenAttributeGraph' @(Get-ChildItem "$s\OpenAttributeGraph" -Recurse -Filter *.swift | ForEach-Object FullName) $flags
        if ($sw -eq $false) { $fails++ } elseif ($sw) { $oagObjs += $sw }
    }
    Write-Archive 'OpenAttributeGraph' $oagObjs
}
if ($Repo -contains 'Compute') {
    # motor de grafo completo (github.com/OpenSwiftUIProject/Compute, MIT): o grafo do OpenAttributeGraph
    # upstream ainda é TODO; o OpenAttributeGraphShims usa o Compute com OPENATTRIBUTEGRAPH_COMPUTE
    $s = "$deps\Compute-main\Sources"
    $oagS = "$deps\OpenAttributeGraph-main\Sources"
    $inc = @("-I$shim", '-include', "$shim\mos_openswiftui_prelude.h", "-I$s\Platform\include", "-I$s\Utilities\include",
             "-I$s\ComputeCxx", "-I$s\ComputeCxx\internalInclude", "-I$s\ComputeCxx\include", "-I$s\SwiftCorelibsCoreFoundation\include",
             "-isystem$oagS\SwiftCorelibs\include", '-DCOMPILED_WITH_SWIFT', '-DPURE_BRIDGING_MODE', '-DCOMPUTE_MOS',
             '-DSWIFT_INLINE_NAMESPACE=__runtime')   # mesmo namespace do demangler do runtime (build-swift-runtime.ps1)
    # sem o servidor de depuração (sockets BSD); *.mm são Objective-C++: stubs em compute_mos.cpp
    $files = @(Get-ChildItem "$s\Platform", "$s\Utilities", "$s\ComputeCxx" -Recurse -Include *.c, *.cpp |
               Where-Object { $_.Name -notin 'DebugServer.cpp', 'Connection.cpp' } | ForEach-Object FullName)
    $files += "$root\userland\sdk\openswiftui\src\compute_mos.cpp"
    $fails += Build-Files 'Compute' $files ($inc + @('-std=c11')) ($inc + $cxx)
    $cmpObjs = $script:objs
    if (-not $Only) {
        $sdkSwift = "$root\userland\sdk\openswiftui\swift"
        # IAGDispatchEquatable (@_silgen_name) e afins chamados pelo C++
        $sup = Prepare-Src 'ComputeCxxSwiftSupport' "$s\ComputeCxxSwiftSupport" "$overlay\Compute\ComputeCxxSwiftSupport"
        $cmpSrc = Prepare-Src 'Compute' "$s\Compute" "$overlay\Compute\Compute"
        $sw = Build-Swift 'ComputeCxxSwiftSupport' @(Get-ChildItem $sup -Filter *.swift | ForEach-Object FullName) @('-I', "$sdkSwift\MOSLibm", '-I', "$outRoot\Dispatch", '-I', "$outRoot\CoreGraphics", '-I', "$outRoot\Foundation")
        if ($sw -eq $false) { $fails++ } elseif ($sw) { $cmpObjs += $sw }
        $xcc = @(); foreach ($a in @("-I$s\Platform\include", "-I$s\ComputeCxx\include")) { $xcc += '-Xcc', $a }
        $flags = @('-I', "$s\ComputeCxx\include", '-I', "$sdkSwift\MOSLibm", '-I', "$outRoot\Dispatch", '-I', "$outRoot\CoreGraphics", '-I', "$outRoot\Foundation",
                   '-enable-experimental-feature', 'Extern') + $xcc
        $sw = Build-Swift 'Compute' @(Get-ChildItem $cmpSrc -Recurse -Filter *.swift | ForEach-Object FullName) $flags '6'
        if ($sw -eq $false) { $fails++ } elseif ($sw) { $cmpObjs += $sw }
    }
    Write-Archive 'Compute' $cmpObjs
    if (-not $Only) {
        # OpenAttributeGraphShims (o que o OpenSwiftUI importa) sobre o Compute: Graph/Attribute/Rule... do Compute
        $shimSrc = Prepare-Src 'OpenAttributeGraphShims' "$deps\OpenAttributeGraph-main\Sources\OpenAttributeGraphShims" "$overlay\OpenAttributeGraph\OpenAttributeGraphShims"
        New-Item -ItemType Directory -Force "$outRoot\OpenAttributeGraphShims" | Out-Null
        $shFlags = $flags + @('-I', "$outRoot\Compute", '-D', 'OPENATTRIBUTEGRAPH_COMPUTE')
        $sw = Build-Swift 'OpenAttributeGraphShims' @(Get-ChildItem $shimSrc -Recurse -Filter *.swift | ForEach-Object FullName) $shFlags
        if ($sw -eq $false) { $fails++ } elseif ($sw) { Write-Archive 'OpenAttributeGraphShims' @($sw) }
    }
}
if ($Repo -contains 'OpenRenderBox') {
    $s = "$deps\OpenRenderBox-main\Sources"
    $oagS = "$deps\OpenAttributeGraph-main\Sources"
    $inc = @("-I$shim", '-include', "$shim\mos_openswiftui_prelude.h", "-I$s\OpenRenderBoxCxx", "-I$s\OpenRenderBoxCxx\include",
             "-I$s\OpenRenderBoxShimsCxx\include", "-I$oagS", "-isystem$oagS\SwiftCorelibs\include",
             "-isystem$root\userland\sdk\include", '-fblocks',   # CoreGraphics do mOS; ORBPathCallbacks usa blocos
             '-DOPENRENDERBOX', '-DOPENRENDERBOX_CF_CGTYPES', '-DOPENRENDERBOX_MOS')
    # *.m / *.mm são Objective-C (Metal, CALayer, NSCoding, display lists): ficam para uma etapa com o ObjC do mOS
    $files = @(Get-ChildItem "$s\OpenRenderBoxCxx", "$s\OpenRenderBoxShimsCxx" -Recurse -Include *.c, *.cpp | ForEach-Object FullName)
    $fails += Build-Files 'OpenRenderBox' $files ($inc + @('-std=c11')) ($inc + $cxx)
    Write-Archive 'OpenRenderBox' $script:objs
}
if ($Repo -contains 'OpenObservation') {
    # locks e TLS pelo Threading do runtime Swift (swift/Threading/*.h, pthreads): sem módulo Darwin
    $s = "$deps\OpenObservation-main\Sources"
    $out = "$outRoot\OpenObservation"
    $inc = @('-DOPENOBSERVATION_SWIFT_TOOLCHAIN_SUPPORTED', '-DSWIFT_THREADING_PTHREADS', '-DSWIFT_OBJC_INTEROP=0',
             "-I$swsrc\stdlib\public", "-I$swsrc\stdlib\public\SwiftShims")
    $files = @(Get-ChildItem "$s\OpenObservationCxx" -Filter *.cpp | ForEach-Object FullName)
    $fails += Build-Files 'OpenObservation' $files ($inc + @('-std=c11')) ($inc + $cxx)
    $objs = $script:objs
    if (-not $Only) {
        # o Swift faz `import OpenObservationCxx` e chama as funções por @_silgen_name: módulo clang vazio
        New-Item -ItemType Directory -Force "$out\include" | Out-Null
        "/* vazio: as funções vêm de Locking.cpp/ThreadLocal.cpp por @_silgen_name */" | Set-Content -Encoding ascii "$out\include\OpenObservationCxx.h"
        "module OpenObservationCxx {`n    header `"OpenObservationCxx.h`"`n    export *`n}" | Set-Content -Encoding ascii "$out\include\module.modulemap"
        # sem o plugin de macros (SwiftSyntax): ObservationRegistrar/withObservationTracking funcionam, @Observable não
        $sw = Build-Swift 'OpenObservation' @(Get-ChildItem "$s\OpenObservation" -Filter *.swift | ForEach-Object FullName) @('-I', "$out\include", '-D', 'OPENOBSERVATION_SWIFT_TOOLCHAIN_SUPPORTED')
        if ($sw -eq $false) { $fails++ } elseif ($sw) { $objs += $sw }
    }
    Write-Archive 'OpenObservation' $objs
}

if (($Repo -contains 'Foundation' -or $Repo -contains 'CoreText') -and (Test-Path "$sdkSwift\CoreText")) {
    # CoreText do mOS (sessão do runtime Swift): ctfont.c (tipos CF CTFont/CTFontDescriptor...) + CoreText.swift sobre o
    # módulo clang _MOSCoreTextC, e CoreText_Private (só reexporta). Fica depois do Foundation.
    $ctInc = @("-I$importerInc", "-I$sdkSwift\CoreText\include")
    $fails += Build-Files 'CoreText' @("$sdkSwift\CoreText\ctfont.c") ($ctInc + @('-std=gnu11')) ($ctInc + $cxx)
    $ctObjs = $script:objs
    $ctFlags = $modInc + @('-I', "$sdkSwift\CoreText\include", '-I', "$outRoot\CoreText")
    $sw = Build-Swift 'CoreText' @(Get-ChildItem "$sdkSwift\CoreText" -Filter *.swift | ForEach-Object FullName) $ctFlags
    if ($sw -eq $false) { $fails++ } elseif ($sw) { $ctObjs += $sw }
    Write-Archive 'CoreText' $ctObjs
    New-Item -ItemType Directory -Force "$outRoot\CoreText_Private" | Out-Null
    $sw = Build-Swift 'CoreText_Private' @(Get-ChildItem "$sdkSwift\CoreText_Private" -Filter *.swift | ForEach-Object FullName) $ctFlags
    if ($sw -eq $false) { $fails++ } elseif ($sw) { Write-Archive 'CoreText_Private' @($sw) }
}if ($Repo -contains 'OpenCombine') {
    # OpenCombine (github.com/OpenCombine/OpenCombine, MIT): Combine em Swift puro, usado pelo OpenSwiftUI fora do Darwin.
    # Os .gyb já vêm gerados (GENERATED-*.swift). O helper C++ usaria os_unfair_lock no __APPLE__, que no libSystem
    # do mOS não trava nada: a cópia do mOS usa pthread_mutex (trava de verdade no mOS).
    $s = "$deps\OpenCombine-master\Sources"
    if (-not (Test-Path $s)) {
        $zip = "$dl\OpenCombine.zip"
        if (-not (Test-Path $zip)) {
            $ProgressPreference = 'SilentlyContinue'
            Invoke-WebRequest 'https://github.com/OpenCombine/OpenCombine/archive/refs/heads/master.zip' -OutFile $zip -UseBasicParsing
        }
        tar -xf $zip -C $deps
    }
    $ocOut = "$outRoot\OpenCombine"; New-Item -ItemType Directory -Force "$ocOut\cpp" | Out-Null
    $cpp = [IO.File]::ReadAllText("$s\COpenCombineHelpers\COpenCombineHelpers.cpp").Replace('#ifdef __APPLE__', '#if 0 /* mOS: os_unfair_lock é no-op no libSystem; usa pthread */')
    [IO.File]::WriteAllText("$ocOut\cpp\COpenCombineHelpers.cpp", $cpp, (New-Object Text.UTF8Encoding $false))
    $hinc = @("-I$s\COpenCombineHelpers\include")
    $fails += Build-Files 'OpenCombine' @("$ocOut\cpp\COpenCombineHelpers.cpp") ($hinc + @('-std=c11')) ($hinc + @('-std=c++17', '-fno-rtti'))
    $ocObjs = $script:objs
    $ocFlags = $modInc + @('-I', "$s\COpenCombineHelpers\include", '-I', $ocOut, '-Xcc', "-I$s\COpenCombineHelpers\include")
    $ocSrc = Prepare-Src 'OpenCombine' "$s\OpenCombine" "$overlay\OpenCombine\OpenCombine"
    $sw = Build-Swift 'OpenCombine' @(Get-ChildItem $ocSrc -Recurse -Filter *.swift | ForEach-Object FullName) $ocFlags '5' $false   # sem library evolution (não é feito para isso)
    if ($sw -eq $false) { $fails++ } elseif ($sw) { $ocObjs += $sw }
    Write-Archive 'OpenCombine' $ocObjs
}
if ($Repo -contains 'OpenCoreGraphics') {
    # sem OPENCOREGRAPHICS_COREGRAPHICS: os tipos CG vêm do próprio OpenCoreGraphics + Foundation do mOS
    $s = "$deps\OpenCoreGraphics-main\Sources"
    $ocgFlags = $modInc + @('-I', "$outRoot\OpenCoreGraphics", '-I', "$outRoot\OpenQuartzCore", '-enable-upcoming-feature', 'InternalImportsByDefault')
    # *Shims: no mOS (sem OPENCOREGRAPHICS_COREGRAPHICS) só reexportam OpenCoreGraphics/OpenQuartzCore
    $ocgFlags += @('-I', "$outRoot\OpenCoreGraphicsShims", '-I', "$outRoot\OpenQuartzCoreShims")
    foreach ($m in 'OpenCoreGraphics', 'OpenQuartzCore', 'OpenCoreGraphicsShims', 'OpenQuartzCoreShims') {
        New-Item -ItemType Directory -Force "$outRoot\$m" | Out-Null
        $sw = Build-Swift $m @(Get-ChildItem "$s\$m" -Filter *.swift | ForEach-Object FullName) $ocgFlags '6'
        if ($sw -eq $false) { $fails++ } elseif ($sw) { Write-Archive $m @($sw) }
    }
}
if ($Repo -contains 'OpenRenderBox') {
    # camada Swift do OpenRenderBox (ORBPath, ORBColor, display list...) sobre o módulo clang OpenRenderBoxCxx; o C++
    # já foi compilado acima em libOpenRenderBox.a, que recebe também os objetos Swift
    $s = "$deps\OpenRenderBox-main\Sources"
    $xcc = @(); foreach ($a in @("-I$s\OpenRenderBoxCxx\include",
                                 '-DOPENRENDERBOX', '-DOPENRENDERBOX_CF_CGTYPES', '-DOPENRENDERBOX_MOS')) { $xcc += '-Xcc', $a }
    $orbFlags = $modInc + @('-I', "$s\OpenRenderBoxCxx\include", '-I', "$outRoot\OpenCoreGraphics", '-I', "$outRoot\OpenQuartzCore",
                            '-I', "$outRoot\OpenCoreGraphicsShims", '-I', "$outRoot\OpenRenderBox",
                            '-D', 'OPENRENDERBOX', '-D', 'OPENRENDERBOX_CF_CGTYPES', '-enable-upcoming-feature', 'InternalImportsByDefault',
                            '-package-name', 'OpenRenderBox') + $xcc   # declarações "package" valem entre os módulos do pacote
    $orbSrc = Prepare-Src 'OpenRenderBox' "$s\OpenRenderBox" "$overlay\OpenRenderBox\OpenRenderBox"
    $sw = Build-Swift 'OpenRenderBox' @(Get-ChildItem $orbSrc -Recurse -Filter *.swift | ForEach-Object FullName) $orbFlags '5'
    if ($sw -eq $false) { $fails++ } elseif ($sw) {
        $orbObjs = @(Get-ChildItem "$outRoot\OpenRenderBox\*.o" | ForEach-Object FullName)
        Write-Archive 'OpenRenderBox' $orbObjs
    }
    # ---- OpenSwiftUI_SPI (C do OpenSwiftUI): include próprio com só o módulo principal (os *_Private do upstream —
    # CoreText_Private, UIFoundation_Private... — ficam de fora: o mOS tem módulos Swift com esses nomes) e o mOS como
    # plataforma não-Darwin (OPENSWIFTUI_MOS no OpenSwiftUITargetConditionals.h), como o Swift do OpenSwiftUI
    $spi = "$root\.tools\openswiftui\main\OpenSwiftUI-main\Sources\OpenSwiftUI_SPI"
    if (Test-Path $spi) {
        $spiInc = "$outRoot\OpenSwiftUI_SPI\include"
        New-Item -ItemType Directory -Force "$spiInc\OpenRenderBoxShims" | Out-Null
        $u = $spi.Replace('\', '/')
        # os módulos Shims do upstream (cabeçalhos privados da Apple; fora do Darwin ficam quase vazios), menos o
        # CoreText_Private, que no mOS é módulo Swift
        $shimMods = [ordered]@{ QuartzCore_Private = 'QuartzCore'; CoreFoundation_Private = 'CoreFoundation'; CoreGraphics_Private = 'CoreGraphics'
                                CoreUI_Private = 'CoreUI'; OpenSwiftUICSymbols = 'CSymbols'; dyldPrivate = 'dyld'; kdebug_Private = 'kdebug'
                                GraphicsServices_Private = 'GraphicsServices'; MobileGestaltPrivate = 'MobileGestalt'
                                libAccessibilityPrivate = 'Accessibility'; UIFoundation_Private = 'UIFoundation' }
        $mm = "// mOS: OpenSwiftUI_SPI + os Shims (gerado por tools\build-openswiftui-deps.ps1)`nmodule OpenSwiftUI_SPI {`n    umbrella `"$u/Overlay`"`n    export *`n`n    module Util {`n        umbrella `"$u/Util`"`n        export *`n    }`n}`n"
        # CoreGraphics_Private.h do upstream põe cg_nullable no float (inválido com o cg_nullable = _Nullable do SDK): cópia corrigida
        $cgp = "$spiInc\Shims\CoreGraphics"; New-Item -ItemType Directory -Force $cgp | Out-Null
        $h = [IO.File]::ReadAllText("$spi\Shims\CoreGraphics\CoreGraphics_Private.h").Replace('float cg_nullable *headroom', 'float * cg_nullable headroom')
        [IO.File]::WriteAllText("$cgp\CoreGraphics_Private.h", $h, (New-Object Text.UTF8Encoding $false))
        foreach ($k in $shimMods.Keys) {
            $dir = if ($k -eq 'CoreGraphics_Private') { $cgp.Replace('\', '/') } else { "$u/Shims/$($shimMods[$k])" }
            $mm += "`nmodule $k {`n    umbrella `"$dir`"`n    export *`n}`n"
        }
        $mm |
            Set-Content -Encoding ascii "$spiInc\module.modulemap"
        Copy-Item "$spi\OpenSwiftUIBase.h" $spiInc -Force
        $tcnd = [IO.File]::ReadAllText("$spi\OpenSwiftUITargetConditionals.h")
        $mosBranch = "#if defined(OPENSWIFTUI_MOS)   /* mOS: Mach-O do macOS, mas sem o SDK/ObjC da Apple -> caminho não-Darwin */`n" +
                     "#define OPENSWIFTUI_TARGET_OS_DARWIN       0`n#define OPENSWIFTUI_TARGET_OS_LINUX        0`n#define OPENSWIFTUI_TARGET_OS_WINDOWS      0`n" +
                     "#define OPENSWIFTUI_TARGET_OS_BSD          0`n#define OPENSWIFTUI_TARGET_OS_ANDROID      0`n#define OPENSWIFTUI_TARGET_OS_CYGWIN       0`n" +
                     "#define OPENSWIFTUI_TARGET_OS_WASI         0`n#elif __APPLE__"
        $tcnd = ([regex]'(?m)^#if __APPLE__\s*$').Replace($tcnd, $mosBranch, 1)
        [IO.File]::WriteAllText("$spiInc\OpenSwiftUITargetConditionals.h", $tcnd, (New-Object Text.UTF8Encoding $false))
        Copy-Item "$s\OpenRenderBoxShimsCxx\include\OpenRenderBox\OpenRenderBoxShims\*.h" "$spiInc\OpenRenderBoxShims\" -Force
        $spiDefs = @('-DOPENSWIFTUI', '-DOPENSWIFTUI_MOS', '-DOPENSWIFTUI_CF_CGTYPES', '-DOPENRENDERBOX_CF_CGTYPES', '-DOPENSWIFTUI_LINK_COREUI=0',
                     '-DOPENSWIFTUI_OPENCOMBINE', '-DOPENSWIFTUI_RELEASE_2024')
        $cinc = @("-I$spiInc", "-I$s\OpenRenderBoxCxx\include", "-I$shim", '-include', "$shim\mos_openswiftui_prelude.h",
                  "-isystem$deps\OpenAttributeGraph-main\Sources\SwiftCorelibs\include", "-isystem$root\userland\sdk\include") + $spiDefs
        # Shims\ são as interfaces privadas da Apple (dyld, CoreFoundation_Private...): só no Darwin
        $cfiles = @(Get-ChildItem "$spi\Overlay", "$spi\Util" -Recurse -Filter *.c | ForEach-Object FullName)
        $fails += Build-Files 'OpenSwiftUI_SPI' $cfiles ($cinc + @('-std=gnu11')) ($cinc + $cxx)
        Write-Archive 'OpenSwiftUI_SPI' $script:objs
    }
    New-Item -ItemType Directory -Force "$outRoot\OpenRenderBoxShims" | Out-Null
    $sw = Build-Swift 'OpenRenderBoxShims' @(Get-ChildItem "$s\OpenRenderBoxShims" -Filter *.swift | ForEach-Object FullName) $orbFlags '5'
    if ($sw -eq $false) { $fails++ } elseif ($sw) { Write-Archive 'OpenRenderBoxShims' @($sw) }
}
if ($Repo -contains 'OpenSwiftUICore') {
    # ---- OpenSwiftUICore (github.com/OpenSwiftUIProject/OpenSwiftUI, MIT), caminho não-Darwin: fontes pré-processados
    # (os(macOS)/canImport(CoreGraphics)/_Concurrency -> false), overlay OpenSwiftUIMOS\Replace por cima e os arquivos
    # de plataforma do mOS (OpenSwiftUIMOS\Core, da sessão do runtime Swift) no mesmo módulo
    $osui = "$root\.tools\openswiftui\main\OpenSwiftUI-main\Sources"
    $mos = "$sdkSwift\OpenSwiftUIMOS"
    $coreSrc = Prepare-Src 'OpenSwiftUICore' "$osui\OpenSwiftUICore" "$mos\Replace\Sources\OpenSwiftUICore"
    # overlay das deps (substitutos de _Concurrency etc.) e remendos pontuais
    Copy-Item "$overlay\OpenSwiftUICore\*" $coreSrc -Recurse -Force
    $utf8 = New-Object Text.UTF8Encoding $false
    # #isolation (macro do _Concurrency) só aparece como valor padrão de parâmetros `(any Actor)?`: sem atores, nil
    foreach ($pf in Get-ChildItem $coreSrc -Recurse -Filter *.swift) {
        $txt = [IO.File]::ReadAllText($pf.FullName, $utf8)
        if ($txt.Contains('#isolation')) { [IO.File]::WriteAllText($pf.FullName, $txt.Replace('#isolation', 'nil'), $utf8) }
    }
    $files = @(Get-ChildItem $coreSrc -Recurse -Filter *.swift | ForEach-Object FullName) + @(Get-ChildItem "$mos\Core" -Filter *.swift -ErrorAction SilentlyContinue | ForEach-Object FullName)
    $cmp = "$deps\Compute-main\Sources"
    $mods = 'Dispatch', 'CoreGraphics', 'Foundation', 'Logging', 'CoreText', 'CoreText_Private', 'Compute', 'OpenAttributeGraphShims', 'OpenCoreGraphics',
            'OpenCoreGraphicsShims', 'OpenQuartzCore', 'OpenQuartzCoreShims', 'OpenRenderBox', 'OpenRenderBoxShims', 'OpenObservation', 'OpenCombine', 'Synchronization', 'Glibc', 'OpenSwiftUICore'
    $flags = @('-I', "$sdkSwift\MOSLibm", '-I', "$outRoot\OpenSwiftUI_SPI\include", '-I', "$outRoot\OpenObservation\include",
               '-I', "$cmp\ComputeCxx\include", '-I', "$deps\OpenRenderBox-main\Sources\OpenRenderBoxCxx\include",
               '-I', "$deps\OpenCombine-master\Sources\COpenCombineHelpers\include", '-I', "$sdkSwift\CoreText\include",
               '-I', "$root\userland\sdk\swift\MOSBridge")
    foreach ($m in $mods) { $flags += '-I', "$outRoot\$m" }
    foreach ($a in @("-I$outRoot\OpenSwiftUI_SPI\include", "-I$deps\OpenRenderBox-main\Sources\OpenRenderBoxCxx\include", "-I$cmp\Platform\include",
                     "-I$cmp\ComputeCxx\include", "-I$deps\OpenCombine-master\Sources\COpenCombineHelpers\include", "-I$sdkSwift\CoreText\include",
                     '-DOPENSWIFTUI', '-DOPENSWIFTUI_MOS', '-DOPENSWIFTUI_CF_CGTYPES', '-DOPENRENDERBOX_CF_CGTYPES', '-DOPENSWIFTUI_LINK_COREUI=0',
                     '-DOPENSWIFTUI_OPENCOMBINE')) { $flags += '-Xcc', $a }
    foreach ($d in 'OPENSWIFTUI', 'OPENSWIFTUI_RELEASE_2024', 'OPENSWIFTUI_SUPPORT_2021_API', 'OPENSWIFTUI_SUPPORT_2022_API', 'OPENSWIFTUI_SUPPORT_2023_API',
                   'OPENSWIFTUI_SUPPORT_2024_API', 'OPENSWIFTUI_CF_CGTYPES', 'OPENRENDERBOX_CF_CGTYPES', 'OPENSWIFTUI_OPENCOMBINE', 'OPENSWIFTUI_SWIFT_LOG',
                   'OPENSWIFTUI_COMPUTE', 'OPENSWIFTUI_MOS') { $flags += '-D', $d }
    foreach ($ft in 'BareSlashRegexLiterals', 'InternalImportsByDefault', 'InferSendableFromCaptures') { $flags += '-enable-upcoming-feature', $ft }
    $flags += '-package-name', 'OpenSwiftUI', '-module-abi-name', 'OpenSwiftUI', '-experimental-spi-only-imports', '-unavailable-decl-optimization=stub'
    # macros de disponibilidade do Package.swift no modo "ignorar disponibilidade" (todas na versão mínima)
    $minv = 'macOS 10.15, iOS 13.0, tvOS 13.0, watchOS 6.0, visionOS 1.0'
    foreach ($v in 'v1_0', 'v1_4', 'v2_0', 'macOS_v2_0', 'v2_1', 'v2_3', 'v3_0', 'iOS_v3_0', 'macOS_v3_0', 'v3_2', 'v3_4', 'v4_0', 'iOS_v4_0', 'watchOS_v4_0',
                   'v4_1', 'v4_4', 'v5_0', 'v5_1', 'v5_2', 'v5_4', 'v5_5', 'v6_0', 'v7_0', 'v8_0') { $flags += '-define-availability', "OpenSwiftUI_${v}:$minv" }
    $flags += '-define-availability', '_distantFuture:macOS 99.0, iOS 99.0, tvOS 99.0, watchOS 99.0, visionOS 99.0'
    New-Item -ItemType Directory -Force "$outRoot\OpenSwiftUICore" | Out-Null
    Write-Host "OpenSwiftUICore: $($files.Count) fontes..."
    $sw = Build-Swift 'OpenSwiftUICore' $files $flags '5' $false
    if ($sw -eq $false) { $fails++ } elseif ($sw) { Write-Archive 'OpenSwiftUICore' @($sw) }
}
exit $fails
