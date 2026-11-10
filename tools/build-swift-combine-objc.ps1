# mOS - Combine.framework para binários Swift da Apple (docs/swift-objc-runtime.md): o OpenCombine
# (github.com/OpenCombine/OpenCombine, MIT; já baixado em .tools\openswiftui\deps pelo build-openswiftui-deps.ps1)
# compilado como o módulo "Combine", com interop ObjC e library evolution, para os nomes mangled serem os da Apple
# ($s7Combine...) e haver dispatch thunks. Sai como /System/Library/Frameworks/Combine.framework/Versions/A/Combine na
# raiz das dylibs Swift (build\swift\objc\dylib), que o carregador lê do VFS.
#
# Cópia preparada do fonte (build\swift\objc\combine-src): "OpenCombine." -> "Combine."
# e os atributos de layout fixo do Combine da Apple (library evolution).
#   .\tools\build-swift-combine-objc.ps1
param([string]$Arch = 'x86_64')   # -Arch arm64: build\swift-arm64 (tools\swift-arch.ps1)
$root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\swift-arch.ps1" $Arch
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
$objc = "$SwiftBuild\objc"
$ov = "$objc\overlays"
$inc = "$objc\sdk\include"
$src = "$root\.tools\openswiftui\deps\OpenCombine-master\Sources"
if (-not (Test-Path "$src\OpenCombine")) { Write-Host "falta o OpenCombine (tools\build-openswiftui-deps.ps1 -Repo OpenCombine)" -ForegroundColor Red; exit 1 }
$prep = "$objc\combine-src"
$tmp = "$objc\combine-obj"
if (Test-Path $prep) { Remove-Item $prep -Recurse -Force }
New-Item -ItemType Directory -Force $prep, $tmp | Out-Null
$utf8 = New-Object Text.UTF8Encoding $false

# ---- C++: CombineIdentifier e trava (o os_unfair_lock do libSystem do mOS é no-op: usa pthread). Vai para overlays,
# onde o link estático (tools\link-swift-objc.ps1) pega todos os .o
$cpp = [IO.File]::ReadAllText("$src\COpenCombineHelpers\COpenCombineHelpers.cpp").Replace('#ifdef __APPLE__', '#if 0 /* mOS: os_unfair_lock é no-op no libSystem; usa pthread */')
[IO.File]::WriteAllText("$tmp\COpenCombineHelpers.cpp", $cpp, $utf8)
& $zig c++ -target $ZigTarget -O2 -c -std=c++17 -fno-rtti "-I$src\COpenCombineHelpers\include" "$tmp\COpenCombineHelpers.cpp" -o "$ov\Combine_COpenCombineHelpers.o" 2>&1 | ForEach-Object { "  $_" }
if ($LASTEXITCODE) { Write-Host 'COpenCombineHelpers: FALHOU' -ForegroundColor Red; exit 1 }

# ---- fonte Swift preparado
# tira o invólucro "public struct OCombine { ... public struct Publisher ... }" e o typealias condicional, deixando
# Publisher e "var publisher" direto na extensão de Optional/Result
function Remove-OCombine([string]$t, [string]$name) {
    $l = $t -split "`n"
    $a = -1; $b = -1; $oc = -1; $ta = -1; $e = -1
    for ($i = 0; $i -lt $l.Count; $i++) {
        if ($a -lt 0) { if ($l[$i] -match '^\s*public struct OCombine \{') { $a = $i } }
        elseif ($b -lt 0) { if ($l[$i] -match '^\s*public struct Publisher\b') { $b = $i } }
        elseif ($oc -lt 0) { if ($l[$i] -match '^\s*public var ocombine\b') { $oc = $i } }
        elseif ($ta -lt 0) { if ($l[$i] -match 'public typealias Publisher = OCombine\.Publisher') { $ta = $i } }
        elseif ($e -lt 0) { if ($l[$i] -match '^#endif') { $e = $i } }
    }
    if ($e -lt 0) { throw "Remove-OCombine: formato inesperado em $name" }
    while ($a -gt 0 -and $l[$a - 1] -match '^\s*///') { $a-- }
    $c = $oc; while ($l[$c] -notmatch '^\s{4}\}\s*$') { $c-- }     # o "}" do OCombine
    (@($l[0..($a - 1)]) + @($l[$b..($c - 1)]) + @($l[($ta + 1)..($e - 1)]) + @($l[($e + 1)..($l.Count - 1)])) -join "`n"
}
$files = @()
foreach ($f in Get-ChildItem "$src\OpenCombine" -Recurse -Filter *.swift) {
    $t = [IO.File]::ReadAllText($f.FullName)
    $t = $t.Replace('OpenCombine.', 'Combine.')
    # implementação: o módulo C não aparece na interface pública
    $t = $t.Replace("`nimport COpenCombineHelpers", "`n@_implementationOnly import COpenCombineHelpers")
    # library evolution: os tipos com código @inlinable têm layout fixo, como no Combine da Apple (@frozen AnyPublisher,
    # AnySubscriber, Subscribers.Demand e ImmediateScheduler.SchedulerTimeType.Stride; caixas @usableFromInline @_fixed_layout)
    $t = [regex]::Replace($t, '(?m)^(\s*)public struct (AnyPublisher|AnySubscriber|Demand|Stride)\b', '$1@frozen public struct $2')
    $t = [regex]::Replace($t, '(?m)^(\s*)@usableFromInline(\r?\n\s*)internal (final )?class ', '$1@usableFromInline$2@_fixed_layout internal $3class ')
    # Optional.Publisher e Result.Publisher: o OpenCombine os põe em Optional.OCombine / Result.OCombine (para não colidir
    # com o Combine da Apple); aqui, como na Apple, moram direto em Optional/Result (o nome mangled muda)
    $t = $t.Replace('.OCombine.Publisher', '.Publisher').Replace('extension Optional.OCombine {', 'extension Optional {').Replace('extension Result.OCombine {', 'extension Result {')
    if ($f.Name -in 'Optional.Publisher.swift', 'Result.Publisher.swift') { $t = Remove-OCombine $t $f.Name }
    $rel = $f.FullName.Substring("$src\OpenCombine".Length).TrimStart('\')
    $d = "$prep\$rel"
    New-Item -ItemType Directory -Force (Split-Path $d) | Out-Null
    [IO.File]::WriteAllText($d, $t, $utf8)
    $files += $d
}
# o que o OpenCombine não tem (CombineLatest, Merge...): userland\sdk\swift-objc\Combine
$files += @(Get-ChildItem "$root\userland\sdk\swift-objc\Combine" -Filter *.swift | ForEach-Object FullName)

# ---- módulo Combine
$a = @('-c', '-target', $SwiftTriple, '-module-name', 'Combine', '-parse-as-library', '-swift-version', '5', '-O',
       '-enable-library-evolution', '-I', $objc, '-I', $ov, '-I', "$src\COpenCombineHelpers\include",
       '-I', "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims", '-module-cache-path', "$objc\modcache",
       '-Xcc', "-isystem$inc", '-Xcc', '-DMOS_SWIFT_OBJC=1', '-Xcc', '-Wno-nullability-completeness', '-Xcc', "-I$src\COpenCombineHelpers\include",
       '-emit-module-path', "$ov\Combine.swiftmodule", '-o', "$ov\Combine.o") + $files
$rsp = "$tmp\Combine.rsp"
($a | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join "`n" | Set-Content -Encoding ascii $rsp
$log = & "$($tc.FullName)\usr\bin\swift-frontend.exe" -frontend "@$rsp" 2>&1 | ForEach-Object { "$_" }
$log | Set-Content -Encoding utf8 "$tmp\Combine.log"
$err = $log | Select-String -Pattern ':\d+:\d+: error: |<unknown>:0: error: '
if ($LASTEXITCODE -ne 0 -or $err) { $err | Select-Object -First 25 | ForEach-Object { "  $($_.Line)" }; Write-Host "Combine: FALHOU ($($err.Count) erros)" -ForegroundColor Red; exit 1 }
Write-Host ("Combine: ok ({0:N0} KB, {1} fontes)" -f ((Get-Item "$ov\Combine.o").Length / 1KB), $files.Count)

# ---- Combine.framework
$dir = "$objc\dylib\System\Library\Frameworks\Combine.framework\Versions\A"
New-Item -ItemType Directory -Force $dir | Out-Null
$log = & $zig c++ -target $ZigTarget -shared -o "$dir\Combine" '-Wl,-install_name,/System/Library/Frameworks/Combine.framework/Versions/A/Combine' '-Wl,-undefined,dynamic_lookup' "$ov\Combine.o" "$ov\Combine_COpenCombineHelpers.o" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { $log | Select-Object -First 8 | ForEach-Object { "  $_" }; Write-Host 'Combine.framework: link FALHOU' -ForegroundColor Red; exit 1 }
Write-Host ("Combine.framework: ok ({0:N0} KB)" -f ((Get-Item "$dir\Combine").Length / 1KB))

# ---- o Dispatch ganha DispatchQueue: Scheduler (userland\sdk\swift-objc\Dispatch\Combine): recompila e religa
& "$PSScriptRoot\build-swift-overlays-objc.ps1" -Only Dispatch -Arch $Arch
& "$PSScriptRoot\link-swift-dylibs-objc.ps1" -Arch $Arch | Select-String 'libswiftDispatch|FALHOU'
