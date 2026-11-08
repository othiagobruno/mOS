# mOS - põe um app Electron qualquer (Slack, Cursor, ...) numa imagem do mOS, para testar o caminho da janela
# (CoreAnimation/IOSurface, docs/porte-vscode.md). Genérico a partir do stage-vscode.ps1 (que é só do VS Code).
#   .\build.ps1 -OutDir build-electron
#   .\tools\stage-electron.ps1 -App "build\apps\x\slack-x64\Slack\Slack.app" -OutDir build-electron -Autostart -Diagnostico -CaLog
#   .\run.ps1 -Image build-electron/mOS.img -Memory 2700      (ou tools\qemu-test.ps1 -OutDir build-electron ...)
# Copia: Info.plist, o executável, os binários dos frameworks (Versions/A), os apps auxiliares "<Nome> Helper*.app",
# os recursos do Electron Framework (ICU, snapshot do V8, .pak; traduções en e pt_BR), libffmpeg, e do Resources do app
# só app.asar (+ app.asar.unpacked até -MaxModuloMiB por arquivo) ou a pasta app (apps sem asar, como o Cursor).
# -CaLog liga /Sistema/Biblioteca/ca-log: o QuartzCore registra no log serial (com -Diagnostico) cada IOSurface posto num
# layer, cada CAContext publicado e cada quadro composto na janela, com a contagem de pixels visíveis.
# Módulos .node em Swift (ligam libswiftCore) ficam de fora, porque o mOS ainda não tem esse runtime; -ComSwift os copia.
# -SubstituirApp <pasta> troca o código do app (Resources/app) por um teste, ex.: tools\testdata\electron-wcv.
param([Parameter(Mandatory = $true)][string]$App, [string]$OutDir = 'build-electron', [int]$SizeMiB = 768,
      [switch]$Autostart, [switch]$Diagnostico, [switch]$CaLog, [int]$MaxModuloMiB = 16,
      [string]$Args0 = '--in-process-gpu --disable-gpu --no-sandbox --enable-logging --v=0', [string]$Tracepoints = '',
      [string]$SubstituirApp = '', [switch]$ComSwift, [string]$AbrirURL = '', [int]$AbrirAposMs = 120000, [string]$ComApp = '',
      [string]$NavegadorPadrao = '', [string[]]$ArgumentosApp = @(), [string]$MotorHorizonte = '',
      [string]$AbrirURL2 = '', [int]$AbrirApos2Ms = 180000, [string]$Fatia = 'x86_64')
# -MotorHorizonte <Chromium.app preparado pelo tools\stage-chromium.ps1>: o motor do Horizonte, em
#   /Sistema/Biblioteca/Horizonte/Chromium.app (fora de /Aplicativos: um app só, o Horizonte). docs/navegador-chromium.md
# -NavegadorPadrao <CFBundleIdentifier>: a escolha do usuário em Ajustes > Navegador padrão (LSHandlers de http/https em
#   ~/Library/Preferences/com.apple.LaunchServices/com.apple.launchservices.secure.plist, que o launchsvc.c lê), ex.:
#   com.google.Chrome para testar o login pelo navegador com o Chrome do -ComApp no lugar do Horizonte/Chromium.
# -ArgumentosApp '<id>=<args>' (um ou mais): linhas de /Sistema/Biblioteca/Preferencias/com.mos.argumentos.cfg, os
#   argumentos que o sistema acrescenta ao abrir o app (o Chrome no mOS precisa de --no-sandbox --in-process-gpu --disable-gpu).
# Essas duas preferências não vão para a esp: o mkimg recusa /mOS/Users e /mOS/Sistema/Biblioteca/Preferencias no staging
#   (o payload que o instalador copia é só o sistema). Ficam no overlay de teste ($OutDir\overlay-teste, montado pelo
#   tools\overlay-teste.ps1), passado com --test-overlay: entram na imagem de teste, fora do payload e do manifesto. O
#   overlay leva sempre o marcador com.mos.teste.sem-assistente: o live abre direto na sessão do Visitante, sem o
#   assistente de configuração (kernel/ui/setup.c), para os passos clicarem no app.
# -AbrirURL <url> [-AbrirAposMs N]: N ms depois de iniciar o app, o init roda /usr/bin/open <url> (ex.: slack://open),
#   para testar a entrega de URLs ao app (openurl.c no libSystem). Argumentos do init: até 128 caracteres.
# -AbrirURL2 <url> [-AbrirApos2Ms N]: um segundo open(1), ex.: o slack:// no Chrome que o primeiro já abriu.
# -ComApp <pasta .app já preparada>: copia mais um app para /Aplicativos (ex.: o Google Chrome de build-chrome\esp,
#   que o stage-chrome.ps1 já reduziu à fatia x86_64), para o app testado abrir https://... nele.
$root = Split-Path $PSScriptRoot -Parent
# Mach-O universal (x86_64 + arm64): só a fatia x86_64 vai para a imagem, porque o sistema de arquivos da VM mora na RAM
# (o Electron Framework do Claude tem 404 MB universal). Sem o llvm-lipo do toolchain Swift, copia inteiro (o kernel
# escolhe a fatia sozinho). -Fatia arm64: a fatia arm64 (o processo roda pelo tradutor arm64, docs/tradutor-arm64.md).
$lipo = Get-Item "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains\*\usr\bin\llvm-lipo.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
function Copy-Bin([string]$From, [string]$To) {
    if ($lipo) {
        $h = New-Object byte[] 4
        $fs = [IO.File]::OpenRead('\\?\' + $From); $k = $fs.Read($h, 0, 4); $fs.Close()
        if ($k -eq 4 -and $h[0] -eq 0xCA -and $h[1] -eq 0xFE -and $h[2] -eq 0xBA -and $h[3] -eq 0xBE) {
            & $lipo.FullName $From -thin $Fatia -output $To 2>$null
            if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $To)) { return }
        }
    }
    [IO.File]::Copy('\\?\' + $From, '\\?\' + $To, $true)
}
$App = (Get-Item -LiteralPath $App).FullName
$src = Join-Path $App 'Contents'
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
$exe = $null
$plText = [IO.File]::ReadAllText("$src\Info.plist")
if ($plText -match '<key>CFBundleExecutable</key>\s*<string>([^<]+)</string>') { $exe = $Matches[1] }
if (-not $exe) { $exe = (Get-ChildItem -LiteralPath "$src\MacOS" -File | Select-Object -First 1).Name }
$name = [IO.Path]::GetFileNameWithoutExtension($App)
$dstApp = "$esp\mOS\Aplicativos\$name.app"
$dst = "$dstApp\Contents"
Remove-Item -Recurse -Force -LiteralPath $dstApp -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$dst\MacOS" | Out-Null
Copy-Item -LiteralPath "$src\Info.plist" "$dst\Info.plist"
Copy-Bin "$src\MacOS\$exe" "$dst\MacOS\$exe"
foreach ($fwDir in Get-ChildItem -LiteralPath "$src\Frameworks" -Directory -Filter '*.framework') {
    $fw = $fwDir.Name -replace '\.framework$', ''          # BaseName de pasta mantém a extensão no PS 5.1
    $s = "$($fwDir.FullName)\Versions\A"
    if (-not (Test-Path -LiteralPath "$s\$fw")) { continue }
    $t = "$dst\Frameworks\$fw.framework\Versions\A"
    New-Item -ItemType Directory -Force $t | Out-Null
    Copy-Bin "$s\$fw" "$t\$fw"
}
foreach ($h in Get-ChildItem -LiteralPath "$src\Frameworks" -Directory -Filter '* Helper*.app') {
    $t = "$dst\Frameworks\$($h.Name)\Contents"
    New-Item -ItemType Directory -Force "$t\MacOS" | Out-Null
    Copy-Item -LiteralPath "$($h.FullName)\Contents\Info.plist" "$t\Info.plist"
    foreach ($b in Get-ChildItem -LiteralPath "$($h.FullName)\Contents\MacOS" -File) { Copy-Bin $b.FullName "$t\MacOS\$($b.Name)" }
}
# recursos do Electron (sem links simbólicos no FAT32: "Electron Framework.framework\Resources" é onde o Chromium procura)
$res = "Frameworks\Electron Framework.framework\Resources"
New-Item -ItemType Directory -Force "$dst\$res" | Out-Null
$resSrc = "$src\Frameworks\Electron Framework.framework\Versions\A\Resources"
if (-not (Test-Path -LiteralPath $resSrc)) { $resSrc = "$src\Frameworks\Electron Framework.framework\Resources" }
Get-ChildItem -LiteralPath $resSrc -File | Copy-Item -Destination "$dst\$res\"
foreach ($l in 'en.lproj', 'pt_BR.lproj') { if (Test-Path -LiteralPath "$resSrc\$l") { Copy-Item -Recurse -LiteralPath "$resSrc\$l" "$dst\$res\" } }
$lib = "Frameworks\Electron Framework.framework\Versions\A\Libraries"
if (Test-Path -LiteralPath "$src\$lib") {
    New-Item -ItemType Directory -Force "$dst\$lib" | Out-Null
    foreach ($b in Get-ChildItem -LiteralPath "$src\$lib" -File) { Copy-Bin $b.FullName "$dst\$lib\$($b.Name)" }
}
# Helpers do framework (chrome_crashpad_handler): o crashpad o inicia pelo caminho sem Versions/A, que no macOS é um
# link simbólico; sem ele o Slack (que liga o crashReporter) parava num FATAL de posix_spawnp
$hlp = "$src\Frameworks\Electron Framework.framework\Versions\A\Helpers"
if (Test-Path -LiteralPath $hlp) {
    $th = "$dst\Frameworks\Electron Framework.framework\Helpers"
    New-Item -ItemType Directory -Force $th | Out-Null
    foreach ($b in Get-ChildItem -LiteralPath $hlp -File) { Copy-Bin $b.FullName "$th\$($b.Name)" }
}
# código do app
New-Item -ItemType Directory -Force "$dst\Resources" | Out-Null
foreach ($f in 'app.asar', 'electron.icns') { if (Test-Path -LiteralPath "$src\Resources\$f") { Copy-Item -LiteralPath "$src\Resources\$f" "$dst\Resources\" } }
# outros .asar do app (o Claude carrega a interface de ion-dist.asar) e as pastas pequenas que os acompanham
foreach ($a in Get-ChildItem -LiteralPath "$src\Resources" -File -Filter '*.asar' | Where-Object Name -ne 'app.asar') {
    Copy-Item -LiteralPath $a.FullName "$dst\Resources\"
    $base = "$src\Resources\$($a.BaseName)"
    if ((Test-Path -LiteralPath $base) -and (Get-ChildItem -LiteralPath $base -Recurse -File | Measure-Object Length -Sum).Sum -lt 1MB) {
        Copy-Item -Recurse -LiteralPath $base "$dst\Resources\"
    }
}
# fontes que o app carrega por caminho (o Claude traz as Anthropic Sans/Serif em Resources/fonts)
if (Test-Path -LiteralPath "$src\Resources\fonts") { Copy-Item -Recurse -Force -LiteralPath "$src\Resources\fonts" "$dst\Resources\" }
$extra = @()
if (Test-Path -LiteralPath "$src\Resources\app.asar.unpacked") { $extra += 'app.asar.unpacked' }
if (-not (Test-Path -LiteralPath "$src\Resources\app.asar") -and (Test-Path -LiteralPath "$src\Resources\app")) { $extra += 'app' }
if ($SubstituirApp) { $extra = @() }   # o código vai ser trocado: não copia milhares de arquivos à toa
foreach ($d in $extra) {
    $u = (Get-Item -LiteralPath "$src\Resources\$d").FullName
    foreach ($f in Get-ChildItem -LiteralPath $u -Recurse -File -ErrorAction SilentlyContinue) {
        if ($f.Length -gt $MaxModuloMiB * 1MB) { continue }
        # módulo nativo em Swift (liga /usr/lib/swift/libswiftCore.dylib): o mOS ainda não tem o runtime Swift desses
        # addons, e o dyld ligaria centenas de símbolos ausentes a 0 (o computer_use.node do Claude caía na 1ª chamada).
        # Fora da imagem o require falha e o app segue sem o módulo, como já trata.
        if ($f.Extension -eq '.node' -and -not $ComSwift) {
            $head = New-Object byte[] ([Math]::Min(262144, $f.Length))
            $fs = [IO.File]::OpenRead('\\?\' + $f.FullName); [void]$fs.Read($head, 0, $head.Length); $fs.Close()
            if ([Text.Encoding]::ASCII.GetString($head).Contains('/usr/lib/swift/libswiftCore.dylib')) { Write-Host "  sem $($f.Name) (Swift)" -ForegroundColor DarkYellow; continue }
        }
        $t = "$dst\Resources\$d" + $f.FullName.Substring($u.Length)
        New-Item -ItemType Directory -Force (Split-Path $t) -ErrorAction SilentlyContinue | Out-Null
        try { Copy-Bin $f.FullName $t } catch { }
    }
}
# -SubstituirApp <pasta>: troca o código do app por outro (Resources/app), para testes mínimos com o Electron de um app
# que carrega de pasta (o Cursor). Ex.: tools/testdata/electron-wcv (composição de WebContentsView).
if ($SubstituirApp) {
    Remove-Item -Recurse -Force -LiteralPath "$dst\Resources\app", "$dst\Resources\app.asar", "$dst\Resources\app.asar.unpacked" -ErrorAction SilentlyContinue
    Copy-Item -Recurse -LiteralPath (Resolve-Path $SubstituirApp).Path "$dst\Resources\app"
    Write-Host "código do app trocado por $SubstituirApp" -ForegroundColor Yellow
}
Set-Content -Path "$esp\mOS\Sistema\Biblioteca\dyld-experimental" -Value $name -Encoding ascii
New-Item -ItemType Directory -Force "$esp\mOS\Sistema\Biblioteca\LaunchDaemons" | Out-Null
$svc = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.electron-teste.servico"
if ($Autostart) { "Programa=/Aplicativos/$name.app/Contents/MacOS/$exe`nArgumentos=$Args0`nAtrasoMs=3000" | Set-Content -Encoding ascii $svc }
else { Remove-Item $svc -ErrorAction SilentlyContinue }
$svcUrl = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.zz-abrir-url.servico"   # "zz": o init o lê depois do app
if ($AbrirURL) {
    if ($AbrirURL.Length -gt 127) { Write-Host 'AbrirURL: o init aceita até 127 caracteres de argumentos' -ForegroundColor Red; exit 1 }
    "Programa=/usr/bin/open`nArgumentos=$AbrirURL`nAtrasoMs=$AbrirAposMs" | Set-Content -Encoding ascii $svcUrl
} else { Remove-Item $svcUrl -ErrorAction SilentlyContinue }
$svcUrl2 = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.zz-abrir-url2.servico"
if ($AbrirURL2) {
    if ($AbrirURL2.Length -gt 127) { Write-Host 'AbrirURL2: o init aceita até 127 caracteres de argumentos' -ForegroundColor Red; exit 1 }
    "Programa=/usr/bin/open`nArgumentos=$AbrirURL2`nAtrasoMs=$AbrirApos2Ms" | Set-Content -Encoding ascii $svcUrl2
} else { Remove-Item $svcUrl2 -ErrorAction SilentlyContinue }
if ($ComApp) {
    $extra = (Resolve-Path -LiteralPath $ComApp).Path
    $to = "$esp\mOS\Aplicativos\" + (Split-Path $extra -Leaf)
    if (Test-Path -LiteralPath $to) { [IO.Directory]::Delete('\\?\' + $to, $true) }
    # robocopy: caminhos longos (helpers do Chrome) e cópia rápida de milhares de arquivos
    robocopy $extra $to /E /NFL /NDL /NJH /NJS /NP /R:1 /W:1 | Out-Null
    if ($LASTEXITCODE -ge 8) { Write-Host "ComApp: falhou a cópia de $extra" -ForegroundColor Red; exit 1 }
    $global:LASTEXITCODE = 0
    $emb = (Get-ChildItem -LiteralPath $to -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
    Write-Host ("+ {0}: {1:N1} MiB" -f (Split-Path $extra -Leaf), $emb) -ForegroundColor Cyan
}
if ($MotorHorizonte) {
    $motor = (Resolve-Path -LiteralPath $MotorHorizonte).Path
    $to = "$esp\mOS\Sistema\Biblioteca\Horizonte\Chromium.app"
    if (Test-Path -LiteralPath $to) { [IO.Directory]::Delete('\\?\' + $to, $true) }
    robocopy $motor $to /E /NFL /NDL /NJH /NJS /NP /R:1 /W:1 | Out-Null
    if ($LASTEXITCODE -ge 8) { Write-Host "MotorHorizonte: falhou a cópia de $motor" -ForegroundColor Red; exit 1 }
    $global:LASTEXITCODE = 0
    $emb = (Get-ChildItem -LiteralPath $to -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
    Write-Host ("+ motor do Horizonte (Chromium): {0:N1} MiB" -f $emb) -ForegroundColor Cyan
}
# overlay de teste (tools\overlay-teste.ps1): refeito a cada rodada, com o marcador que pula o assistente do Live
. "$PSScriptRoot\overlay-teste.ps1"
$ovt = New-OverlayTeste $root $OutDir
if ($NavegadorPadrao) {
    $pref = "$ovt\mOS\Users\Visitante\Library\Preferences\com.apple.LaunchServices"
    New-Item -ItemType Directory -Force $pref | Out-Null
    $h = foreach ($s in 'http', 'https') {
        "`t`t<dict>`n`t`t`t<key>LSHandlerURLScheme</key>`n`t`t`t<string>$s</string>`n`t`t`t<key>LSHandlerRoleAll</key>`n`t`t`t<string>$NavegadorPadrao</string>`n`t`t</dict>"
    }
    ("<?xml version=`"1.0`" encoding=`"UTF-8`"?>`n<!DOCTYPE plist PUBLIC `"-//Apple//DTD PLIST 1.0//EN`" `"http://www.apple.com/DTDs/PropertyList-1.0.dtd`">`n" +
     "<plist version=`"1.0`">`n<dict>`n`t<key>LSHandlers</key>`n`t<array>`n" + ($h -join "`n") + "`n`t</array>`n</dict>`n</plist>") |
        Set-Content -Encoding ascii "$pref\com.apple.launchservices.secure.plist"
    Write-Host "navegador padrão: $NavegadorPadrao" -ForegroundColor Cyan
}
if ($ArgumentosApp.Count) {
    New-Item -ItemType Directory -Force "$ovt\mOS\Sistema\Biblioteca\Preferencias" | Out-Null
    ($ArgumentosApp -join "`n") | Set-Content -Encoding ascii "$ovt\mOS\Sistema\Biblioteca\Preferencias\com.mos.argumentos.cfg"
    Write-Host "argumentos de app: $($ArgumentosApp -join ' | ')" -ForegroundColor Cyan
}
if ($Diagnostico) {
    foreach ($f in 'dyld-registrar', 'stderr-serial', 'despejar-threads') { Set-Content -Path "$esp\mOS\Sistema\Biblioteca\$f" -Value '1' -Encoding ascii }
}
if ($CaLog) { Set-Content -Path "$esp\mOS\Sistema\Biblioteca\ca-log" -Value '1' -Encoding ascii }
else { Remove-Item "$esp\mOS\Sistema\Biblioteca\ca-log" -ErrorAction SilentlyContinue }
# -Tracepoints: como no stage-vscode.ps1, "processo|imagem|0xdesl[*N][@reg],..." (várias linhas separadas por ';')
$tpFile = "$esp\mOS\Sistema\Biblioteca\diag-tracepoints"
if ($Tracepoints) { ($Tracepoints -split ';') -join "`n" | Set-Content -Path $tpFile -Encoding ascii }
else { Remove-Item $tpFile -ErrorAction SilentlyContinue }
$mb = (Get-ChildItem -LiteralPath $dstApp -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("{0}.app ({1}): {2:N1} MiB" -f $name, $exe, $mb) -ForegroundColor Cyan
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img" -ForegroundColor Green
