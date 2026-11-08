# mOS - prepara o Chromium (codigo aberto, licenca BSD) como motor do Horizonte, o navegador do sistema
# (docs/navegador-chromium.md). O Horizonte continua sendo o unico app de navegador; o Chromium fica fora de /Aplicativos.
#
#   .\tools\stage-chromium.ps1
#       baixa (se faltar) o snapshot oficial do Chromium para Mac x64 e monta uma copia enxuta em
#       build\chromium\Chromium.app, pronta para -MotorHorizonte (tools\stage-electron.ps1, tools\vscode-teste.ps1)
#   .\tools\stage-chromium.ps1 -OutDir build-cr [-SizeMiB 1024] [-AutoAbrir] [-Argumentos '...'] [-Diagnostico]
#       instala tambem em <OutDir>\esp\mOS\Sistema\Biblioteca\Horizonte\Chromium.app (o motor do Horizonte) e gera
#       <OutDir>\mOS.img (rode antes .\build.ps1 -OutDir <OutDir>)
#
# De onde vem: gs://chromium-browser-snapshots/Mac/<revisao>/chrome-mac.zip (o bucket publico dos builds do Chromium).
# A revisao padrao 1717346 fica 3 commits antes do ponto de ramificacao do Chrome 157 (1717349), o mesmo codigo do
# Google Chrome 157.0.8103 que ja roda no mOS (docs/porte-chrome.md). O Chrome do Google nao pode ir na imagem; o
# Chromium pode (BSD + licencas de terceiros em chrome://credits).
#
# O que vai para o motor (o sistema de arquivos da VM mora na RAM, entao so o necessario):
#   - o executavel, o Chromium Framework (305 MiB, ja so x86_64 no snapshot), os helpers (executavel + Info.plist),
#     chrome_crashpad_handler, libaperitif/libvulkan/libvk_swiftshader e os dados de componentes de Libraries;
#   - recursos que o Chromium le ao iniciar: icudtl.dat, resources.pak, chrome_100/200_percent.pak, o snapshot do V8,
#     e as traducoes en e pt_BR (as outras 220 ficam de fora);
#   - fica de fora: Assets.car (o mOS le o app.icns), o manifesto de politicas, scripting.sdef, install.sh.
# O FAT32 nao tem links simbolicos: tudo fica em Versions/<versao>, onde o carregador do mOS procura.
#   -AbrirURL <url> [-AbrirAposMs N]: N ms depois do boot o init roda /usr/bin/open <url>, o caminho de um app pedindo
#   uma pagina: Launch Services -> Horizonte -> motor Chromium (com os argumentos que o kernel acrescenta)
param([string]$App = '', [string]$Revisao = '1717346', [string]$Destino = '', [string]$OutDir = '', [int]$SizeMiB = 1024,
      [switch]$AutoAbrir, [string]$Argumentos = '', [switch]$Diagnostico, [string]$AbrirURL = '', [int]$AbrirAposMs = 20000)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
# SHA-256 dos zips conferidos (outra revisao: o script mostra o hash e segue)
$conhecidos = @{ '1717346' = '455ffe0d1520c07fd971154df70d65607444f513b1bf3654d5b7c6f011f81f59' }

function Get-LongPath([string]$p) { if ($p.StartsWith('\\?\')) { $p } else { '\\?\' + $p } }

if (-not $App) {
    $x = "$root\build\apps\x\chromium-$Revisao"
    $App = "$x\chrome-mac\Chromium.app"
    if (-not (Test-Path -LiteralPath "$App\Contents\MacOS\Chromium")) {
        $dl = "$root\build\apps\dl\chromium-$Revisao"
        $zip = "$dl\chrome-mac.zip"
        if (-not (Test-Path -LiteralPath $zip)) {
            New-Item -ItemType Directory -Force $dl | Out-Null
            $url = "https://commondatastorage.googleapis.com/chromium-browser-snapshots/Mac/$Revisao/chrome-mac.zip"
            Write-Host "baixando $url ..." -ForegroundColor Cyan
            $ProgressPreference = 'SilentlyContinue'
            Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
        }
        $h = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLower()
        if ($conhecidos.ContainsKey($Revisao) -and $conhecidos[$Revisao] -ne $h) {
            Write-Host "SHA-256 inesperado para chrome-mac.zip ($h); apague $zip e baixe de novo" -ForegroundColor Red; exit 1
        }
        Write-Host "chrome-mac.zip sha256 $h" -ForegroundColor DarkGray
        New-Item -ItemType Directory -Force $x | Out-Null
        # tar do Windows (bsdtar): caminhos longos; os 5 links simbolicos do framework falham e nao fazem falta
        & "$env:SystemRoot\System32\tar.exe" -xf $zip -C $x 2>$null
        $global:LASTEXITCODE = 0
    }
}
$src = Join-Path (Resolve-Path -LiteralPath $App).Path 'Contents'
if (-not (Test-Path -LiteralPath "$src\MacOS\Chromium")) { Write-Host "nao achei $src\MacOS\Chromium" -ForegroundColor Red; exit 1 }
$ver = (Get-ChildItem -LiteralPath "$src\Frameworks\Chromium Framework.framework\Versions" -Directory | Where-Object { $_.Name -match '^\d' } | Select-Object -First 1).Name
if (-not $ver) { Write-Host 'nao achei a versao do Chromium Framework' -ForegroundColor Red; exit 1 }
if (-not $Destino) { $Destino = "$root\build\chromium\Chromium.app" }
$Destino = [IO.Path]::GetFullPath($Destino)

function Copy-One([string]$from, [string]$to) {
    $to = Get-LongPath $to
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($to)) | Out-Null
    [IO.File]::Copy((Get-LongPath $from), $to, $true)
}
function Copy-Tree([string]$from, [string]$to) {
    foreach ($f in Get-ChildItem -LiteralPath (Get-LongPath $from) -Recurse -File) {
        Copy-One $f.FullName ($to + $f.FullName.Substring((Get-LongPath $from).Length))
    }
}

Write-Host "Chromium $ver -> $Destino" -ForegroundColor Cyan
if (Test-Path -LiteralPath $Destino) { [IO.Directory]::Delete((Get-LongPath $Destino), $true) }
$dst = "$Destino\Contents"
foreach ($f in 'Info.plist', 'PkgInfo', 'MacOS\Chromium', 'Resources\app.icns') { Copy-One "$src\$f" "$dst\$f" }
foreach ($l in 'en.lproj', 'pt_BR.lproj') { if (Test-Path -LiteralPath "$src\Resources\$l") { Copy-Tree "$src\Resources\$l" "$dst\Resources\$l" } }

$fs = "Frameworks\Chromium Framework.framework\Versions\$ver"
Copy-One "$src\$fs\Chromium Framework" "$dst\$fs\Chromium Framework"
Copy-Tree "$src\$fs\Libraries" "$dst\$fs\Libraries"
foreach ($h in Get-ChildItem -LiteralPath "$src\$fs\Helpers" -Directory -Filter 'Chromium Helper*.app') {
    $t = "$dst\$fs\Helpers\$($h.Name)\Contents"
    Copy-One "$($h.FullName)\Contents\Info.plist" "$t\Info.plist"
    if (Test-Path -LiteralPath "$($h.FullName)\Contents\PkgInfo") { Copy-One "$($h.FullName)\Contents\PkgInfo" "$t\PkgInfo" }
    $exe = @(Get-ChildItem -LiteralPath (Get-LongPath "$($h.FullName)\Contents\MacOS") -File)
    if (-not $exe) { throw "helper sem executavel (extracao com caminho longo?): $($h.Name)" }
    foreach ($e in $exe) { Copy-One $e.FullName "$t\MacOS\$($e.Name)" }
}
foreach ($e in 'chrome_crashpad_handler', 'app_mode_loader', 'web_app_shortcut_copier') {
    if (Test-Path -LiteralPath "$src\$fs\Helpers\$e") { Copy-One "$src\$fs\Helpers\$e" "$dst\$fs\Helpers\$e" }
}
Get-ChildItem -LiteralPath "$src\$fs\Resources" -File | Where-Object { $_.Name -notmatch 'arm64|^install\.sh$' } |
    ForEach-Object { Copy-One $_.FullName "$dst\$fs\Resources\$($_.Name)" }
foreach ($l in 'en.lproj', 'pt_BR.lproj') { if (Test-Path -LiteralPath "$src\$fs\Resources\$l") { Copy-Tree "$src\$fs\Resources\$l" "$dst\$fs\Resources\$l" } }
$mb = (Get-ChildItem -LiteralPath (Get-LongPath $Destino) -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("Chromium.app: {0:N1} MiB" -f $mb) -ForegroundColor Cyan

if (-not $OutDir) { Write-Host "OK: $Destino (use com -MotorHorizonte)" -ForegroundColor Green; exit 0 }

# -OutDir: instala numa imagem de teste. O motor fica fora de /Aplicativos (o Launch Services e o Launchpad veem so o
# Horizonte); o kernel o acha em HORIZONTE_MOTOR (kernel/include/launchsvc.h)
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
$to = "$esp\mOS\Sistema\Biblioteca\Horizonte\Chromium.app"
if (Test-Path -LiteralPath $to) { [IO.Directory]::Delete((Get-LongPath $to), $true) }
Copy-Tree $Destino $to
# sem o modo experimental do carregador (dyld-experimental): o Chromium abre como uma instalacao normal; os frameworks
# que ele pede (os mesmos do Google Chrome) estao na lista de kernel/compat/macho.c (libsystem_dylib_ok)
Remove-Item -LiteralPath "$esp\mOS\Sistema\Biblioteca\dyld-experimental" -ErrorAction SilentlyContinue
# -Diagnostico: a saida dos processos vai ao log serial e, a cada 30 s, as pilhas de todas as threads (despejar-threads)
if ($Diagnostico) {
    foreach ($f in 'dyld-registrar', 'stderr-serial', 'despejar-threads') { Set-Content -Path "$esp\mOS\Sistema\Biblioteca\$f" -Value '1' -Encoding ascii }
}
# -AutoAbrir: o init abre o motor sozinho no boot (testes no QEMU sem depender do mouse). Sem -Argumentos o init
# passa so os argumentos dele; o kernel acrescenta os do motor apenas quando quem abre e a interface ou o Launch
# Services (registry.c), entao aqui vao os mesmos por padrao. Para o caminho de verdade (Horizonte), use -AbrirURL.
$svc = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.chromium-teste.servico"
if ($AutoAbrir) {
    if (-not $Argumentos) { $Argumentos = '--no-sandbox --in-process-gpu --disable-gpu --no-first-run --no-default-browser-check --disable-component-update' }
    if ($Argumentos.Length -gt 127) { Write-Host 'Argumentos: o init aceita ate 127 caracteres' -ForegroundColor Red; exit 1 }
    New-Item -ItemType Directory -Force (Split-Path $svc) | Out-Null
    [IO.File]::WriteAllText($svc, "Programa=/Sistema/Biblioteca/Horizonte/Chromium.app/Contents/MacOS/Chromium`nArgumentos=$Argumentos`nAtrasoMs=3000`n", [Text.Encoding]::ASCII)
}
elseif (Test-Path -LiteralPath $svc) { Remove-Item -LiteralPath $svc }
$svcUrl = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.zz-abrir-url.servico"   # "zz": o init o le por ultimo
if ($AbrirURL) {
    if ($AbrirURL.Length -gt 127) { Write-Host 'AbrirURL: o init aceita ate 127 caracteres' -ForegroundColor Red; exit 1 }
    New-Item -ItemType Directory -Force (Split-Path $svcUrl) | Out-Null
    [IO.File]::WriteAllText($svcUrl, "Programa=/usr/bin/open`nArgumentos=$AbrirURL`nAtrasoMs=$AbrirAposMs`n", [Text.Encoding]::ASCII)
}
elseif (Test-Path -LiteralPath $svcUrl) { Remove-Item -LiteralPath $svcUrl }
# imagem de teste: overlay de teste com o marcador que pula o assistente do Live (tools\overlay-teste.ps1)
. "$PSScriptRoot\overlay-teste.ps1"
$ovt = New-OverlayTeste $root $OutDir
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img" -ForegroundColor Green
