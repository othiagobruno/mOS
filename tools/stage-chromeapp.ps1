# mOS - põe um app com o layout do Google Chrome (framework "<Nome> Framework.framework/Versions/<versão>" com os
# helpers e as bibliotecas dentro dele) numa imagem de teste. Feito para o Codex desktop da OpenAI, que é o runtime
# "OWL" (Chromium com marca própria e um app.asar no estilo do Electron; Resources/owl-electron-app.json), e não o
# Electron Framework do tools\stage-electron.ps1. O Chrome do Google continua no tools\stage-chrome.ps1.
#   .\build.ps1 -OutDir build-codex
#   .\tools\stage-chromeapp.ps1 -App "build\apps\x\codex\Codex.app" -OutDir build-codex -Autostart -Diagnostico
#   .\run.ps1 -Image build-codex/mOS.img -Memory 3072
# O que vai para /Aplicativos/<Nome>.app (o sistema de arquivos da VM mora na RAM, então só o necessário):
#   - Info.plist, PkgInfo, ícone, o executável, a fatia x86_64 do framework, os helpers (*.app com Info.plist e
#     executável, e os executáveis soltos de Helpers), as bibliotecas de Libraries (GL/Vulkan por software) e os
#     recursos do framework (ICU, .pak, snapshot do V8; só en e pt_BR das traduções);
#   - do Resources do app: app.asar, app.asar.unpacked (até -MaxModuloMiB por arquivo), os .json/.ini do OWL,
#     ícones, native\ e plugins\; -ComCLI copia também o binário "codex" (270 MiB, o agente) e o rg;
#   - fica de fora: cua_node (365 MiB, o "computer use"), Sparkle (atualizações), as outras traduções.
# O FAT32 não tem links simbólicos: tudo fica em Versions/<versão>, onde o carregador do mOS procura.
param([Parameter(Mandatory = $true)][string]$App, [string]$OutDir = 'build-codex', [int]$SizeMiB = 1536,
      [switch]$Autostart, [switch]$Diagnostico, [switch]$ComCLI, [int]$MaxModuloMiB = 16,
      [string]$Argumentos = '--no-sandbox --in-process-gpu --disable-gpu --enable-unsafe-swiftshader --enable-logging')
# Argumentos: sem --disable-gpu o ANGLE tenta Metal/OpenGL e o Codex cai; só com ele, o Chromium 149 nega o "acesso à GPU"
# sem o --enable-unsafe-swiftshader (SwiftShader por software). Memória: o Codex com o agente pede -Memory 3400.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path (Resolve-Path $App).Path 'Contents'
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
function Get-LongPath([string]$p) { if ($p.StartsWith('\\?\')) { $p } else { '\\?\' + $p } }
# fatia x86_64 de um Mach-O universal (copia como está se já for thin)
function Copy-X64([string]$from, [string]$to) {
    $from = Get-LongPath $from; $to = Get-LongPath $to
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($to)) | Out-Null
    $fs = [IO.File]::OpenRead($from); $h = New-Object byte[] 4096; $k = $fs.Read($h, 0, 4096); $fs.Close()
    $be = { param($o) ([uint32]$h[$o] -shl 24) -bor ([uint32]$h[$o + 1] -shl 16) -bor ([uint32]$h[$o + 2] -shl 8) -bor [uint32]$h[$o + 3] }
    $magic = if ($k -ge 8) { & $be 0 } else { 0 }
    if ($magic -ne [uint32]3405691582 -and $magic -ne [uint32]3405691583) { [IO.File]::Copy($from, $to, $true); return }
    $wide = $magic -eq [uint32]3405691583
    $n = & $be 4
    for ($i = 0; $i -lt $n; $i++) {
        $e = 8 + $i * ($(if ($wide) { 32 } else { 20 }))
        if ((& $be $e) -ne [uint32]16777223) { continue }   # CPU_TYPE_X86_64
        if ($wide) { $off = [uint64](& $be ($e + 8)) * 4294967296 + (& $be ($e + 12)); $len = [uint64](& $be ($e + 16)) * 4294967296 + (& $be ($e + 20)) }
        else { $off = [uint64](& $be ($e + 8)); $len = [uint64](& $be ($e + 12)) }
        $in = [IO.File]::OpenRead($from); $out = [IO.File]::Create($to)
        try {
            [void]$in.Seek([int64]$off, 'Begin'); $buf = New-Object byte[] (4MB); $left = [int64]$len
            while ($left -gt 0) { $r = $in.Read($buf, 0, [int][Math]::Min($buf.Length, $left)); if ($r -le 0) { break }; $out.Write($buf, 0, $r); $left -= $r }
        } finally { $in.Close(); $out.Close() }
        return
    }
    throw "sem fatia x86_64: $from"
}
$plText = [IO.File]::ReadAllText("$src\Info.plist")
$exe = if ($plText -match '<key>CFBundleExecutable</key>\s*<string>([^<]+)</string>') { $Matches[1] } else { (Get-ChildItem -LiteralPath "$src\MacOS" -File | Select-Object -First 1).Name }
$name = [IO.Path]::GetFileNameWithoutExtension((Resolve-Path $App).Path)
$fwDir = Get-ChildItem -LiteralPath "$src\Frameworks" -Directory -Filter '*Framework.framework' | Where-Object Name -ne 'Sparkle.framework' | Select-Object -First 1
if (-not $fwDir) { Write-Host 'não achei o framework do Chromium em Contents\Frameworks' -ForegroundColor Red; exit 1 }
$fwName = $fwDir.Name -replace '\.framework$', ''
$ver = (Get-ChildItem -LiteralPath "$($fwDir.FullName)\Versions" -Directory | Where-Object { $_.Name -match '^\d' } | Select-Object -First 1).Name
if (-not $ver) { Write-Host "não achei a versão de $fwName" -ForegroundColor Red; exit 1 }
$app = "$esp\mOS\Aplicativos\$name.app"
$dst = "$app\Contents"
if (Test-Path -LiteralPath $app) { [IO.Directory]::Delete((Get-LongPath $app), $true) }
New-Item -ItemType Directory -Force "$dst\MacOS", "$dst\Resources" | Out-Null
Copy-Item -LiteralPath "$src\Info.plist" "$dst\Info.plist"
Copy-Item -LiteralPath "$src\PkgInfo" "$dst\PkgInfo" -ErrorAction SilentlyContinue
Copy-X64 "$src\MacOS\$exe" "$dst\MacOS\$exe"

$fs = "Frameworks\$fwName.framework\Versions\$ver"
Write-Host "$fwName $ver (fatia x86_64)..." -ForegroundColor Cyan
Copy-X64 "$src\$fs\$fwName" "$dst\$fs\$fwName"
if (Test-Path -LiteralPath "$src\$fs\Libraries") {
    foreach ($l in Get-ChildItem -LiteralPath "$src\$fs\Libraries" -File) {
        if ($l.Extension -eq '.dylib') { Copy-X64 $l.FullName "$dst\$fs\Libraries\$($l.Name)" }
        elseif ($l.Extension -eq '.json') { Copy-Item -LiteralPath $l.FullName "$dst\$fs\Libraries\" }
    }
}
foreach ($h in Get-ChildItem -LiteralPath "$src\$fs\Helpers" -Directory -Filter '*.app') {
    $t = "$dst\$fs\Helpers\$($h.Name)\Contents"
    New-Item -ItemType Directory -Force "$t\MacOS" | Out-Null
    Copy-Item -LiteralPath "$($h.FullName)\Contents\Info.plist" "$t\Info.plist"
    foreach ($x in @(Get-ChildItem -LiteralPath (Get-LongPath "$($h.FullName)\Contents\MacOS") -File)) { Copy-X64 "$($h.FullName)\Contents\MacOS\$($x.Name)" "$t\MacOS\$($x.Name)" }
}
foreach ($x in Get-ChildItem -LiteralPath "$src\$fs\Helpers" -File) { Copy-X64 $x.FullName "$dst\$fs\Helpers\$($x.Name)" }
New-Item -ItemType Directory -Force "$dst\$fs\Resources" | Out-Null
Get-ChildItem -LiteralPath "$src\$fs\Resources" -File | Where-Object { $_.Name -notmatch 'arm64' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName "$dst\$fs\Resources\" }
foreach ($l in 'en.lproj', 'pt_BR.lproj') { if (Test-Path -LiteralPath "$src\$fs\Resources\$l") { Copy-Item -LiteralPath "$src\$fs\Resources\$l" "$dst\$fs\Resources\" -Recurse } }

# Resources do app
foreach ($f in Get-ChildItem -LiteralPath "$src\Resources" -File) {
    if ($f.Name -in 'Assets.car', 'THIRD_PARTY_NOTICES.txt', 'scripting.sdef') { continue }
    if ($f.Name -in 'codex', 'rg' -and -not $ComCLI) { continue }
    if ($f.Name -in 'codex', 'rg') { Copy-X64 $f.FullName "$dst\Resources\$($f.Name)" } else { Copy-Item -LiteralPath $f.FullName "$dst\Resources\" }
}
foreach ($l in 'en.lproj', 'pt_BR.lproj', 'Base.lproj') { if (Test-Path -LiteralPath "$src\Resources\$l") { Copy-Item -Recurse -LiteralPath "$src\Resources\$l" "$dst\Resources\" } }
foreach ($d in 'app.asar.unpacked', 'native', 'plugins', 'default_app', 'com.openai.codex.manifest') {
    if (-not (Test-Path -LiteralPath "$src\Resources\$d")) { continue }
    $u = (Get-Item -LiteralPath "$src\Resources\$d").FullName
    foreach ($f in Get-ChildItem -LiteralPath $u -Recurse -File -ErrorAction SilentlyContinue) {
        if ($f.Length -gt $MaxModuloMiB * 1MB) { continue }
        $t = "$dst\Resources\$d" + $f.FullName.Substring($u.Length)
        try { Copy-X64 $f.FullName $t } catch { }
    }
}
# Sparkle.framework do próprio app (o sparkle.node o carrega; o FAT não tem links, então Versions/B inteiro)
$sp = "$src\Frameworks\Sparkle.framework\Versions\B"
if (Test-Path -LiteralPath $sp) {
    foreach ($f in Get-ChildItem -LiteralPath $sp -Recurse -File) {
        $t = "$dst\Frameworks\Sparkle.framework\Versions\B" + $f.FullName.Substring($sp.Length)
        try { Copy-X64 $f.FullName $t } catch { }
    }
}
# runtime Swift do mOS (docs/swift-objc-runtime.md): /usr/lib/swift/libswift*.dylib e os frameworks Mach-O (Combine,
# WebKit, ImageIO...), para os helpers e módulos .node em Swift (native/*.node, bare-modifier-monitor...)
$swd = "$root\build\swift\objc\dylib"
if (Test-Path "$swd\usr\lib\swift") {
    New-Item -ItemType Directory -Force "$esp\mOS\usr\lib\swift" | Out-Null
    Copy-Item "$swd\usr\lib\swift\*.dylib" "$esp\mOS\usr\lib\swift\" -Force
    Copy-Item "$swd\usr\lib\*.dylib" "$esp\mOS\usr\lib\" -Force   # libcompression (o sparkle.node a liga)
    if (Test-Path "$swd\System\Library\Frameworks") {
        New-Item -ItemType Directory -Force "$esp\mOS\System\Library\Frameworks" | Out-Null
        Copy-Item "$swd\System\Library\Frameworks\*" "$esp\mOS\System\Library\Frameworks\" -Recurse -Force
    }
    # os universais (x86_64 + arm64, tools\build-swift-arm64.ps1) por cima dos só x86_64
    if (Test-Path "$root\build\universal-swift\usr\lib\swift") { Copy-Item "$root\build\universal-swift\*" "$esp\mOS\" -Recurse -Force }
}
Remove-Item -LiteralPath "$esp\mOS\Sistema\Biblioteca\dyld-experimental" -ErrorAction SilentlyContinue
if ($Diagnostico) {
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\dyld-registrar" -Value '1' -Encoding ascii
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\despejar-threads" -Value '1' -Encoding ascii   # threads de cada processo a cada 30 s
}
New-Item -ItemType Directory -Force "$esp\mOS\Sistema\Biblioteca\LaunchDaemons" | Out-Null
$svc = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.chromeapp-teste.servico"
if ($Autostart) {
    if ($Argumentos.Length -gt 127) { Write-Host 'Argumentos: o init aceita até 127 caracteres' -ForegroundColor Red; exit 1 }
    [IO.File]::WriteAllText($svc, "Programa=/Aplicativos/$name.app/Contents/MacOS/$exe`nArgumentos=$Argumentos`nAtrasoMs=3000`n", [Text.Encoding]::ASCII)
} elseif (Test-Path -LiteralPath $svc) { Remove-Item -LiteralPath $svc }
$mb = (Get-ChildItem -LiteralPath (Get-LongPath $app) -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("$name.app: {0:N1} MiB" -f $mb) -ForegroundColor Cyan
# imagem de teste: overlay de teste com o marcador que pula o assistente do Live (tools\overlay-teste.ps1)
. "$PSScriptRoot\overlay-teste.ps1"
$ovt = New-OverlayTeste $root $OutDir
. (Join-Path $PSScriptRoot 'stage-xpc.ps1')
Copy-MosBundleServices $src $dst
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img (boote com -Memory 3072 ou mais: os arquivos do app ficam na RAM)" -ForegroundColor Green
