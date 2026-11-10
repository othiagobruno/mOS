# mOS - teste dos servicos do sistema (launchd, XPC, notifyd, logd, configd) de ponta a ponta no QEMU.
#   .\build.ps1 -OutDir build-svc
#   .\tools\build-launchdtest.ps1 -OutDir build-svc
#   .\run.ps1 -Headless -Image build-svc/mOS.img -Serial build-svc/serial.log    (procure "launchdteste:" no log)
# Monta:
#   /Aplicativos/LaunchdTeste.app                    cliente (userland\tests-launchd\cliente.c), iniciado no boot (RunAtLoad)
#     Contents/XPCServices/com.mos.teste.bundle.xpc  o servidor como XPC service do bundle
#   /Library/mOS-teste/servidor                      o mesmo servidor como daemon sob demanda
#   /Library/LaunchDaemons/com.mos.teste.xpc.plist   MachServices com.mos.teste.xpc (sob demanda)
#   /Library/LaunchDaemons/com.mos.teste.cliente.plist
param([string]$OutDir = '', [string]$Dest = "$PSScriptRoot\..\build\launchdtest", [int]$SizeMiB = 128)
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }

function Plist([string]$path, [string]$body) {
    $xml = "<?xml version=`"1.0`" encoding=`"UTF-8`"?>`n<!DOCTYPE plist PUBLIC `"-//Apple//DTD PLIST 1.0//EN`" `"http://www.apple.com/DTDs/PropertyList-1.0.dtd`">`n<plist version=`"1.0`">`n<dict>`n$body</dict>`n</plist>`n"
    [IO.File]::WriteAllText($path, $xml, [Text.Encoding]::ASCII)
}

$app = Join-Path $Dest 'LaunchdTeste.app\Contents'
$xpc = Join-Path $app 'XPCServices\com.mos.teste.bundle.xpc\Contents'
New-Item -ItemType Directory -Force "$app\MacOS", "$xpc\MacOS", "$Dest\lib" | Out-Null
& $zig cc -target x86_64-macos -O1 -fblocks "$root\userland\tests-launchd\cliente.c" -o "$app\MacOS\LaunchdTeste" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'launchdtest: cliente FAILED' -ForegroundColor Red; exit 1 }
& $zig cc -target x86_64-macos -O1 -fblocks "$root\userland\tests-launchd\servidor.c" -o "$Dest\lib\servidor" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'launchdtest: servidor FAILED' -ForegroundColor Red; exit 1 }
Copy-Item -Force "$Dest\lib\servidor" "$xpc\MacOS\BundleXpc"
Plist "$app\Info.plist" "`t<key>CFBundleName</key><string>LaunchdTeste</string>`n`t<key>CFBundleIdentifier</key><string>com.mos.teste.cliente</string>`n`t<key>CFBundleExecutable</key><string>LaunchdTeste</string>`n`t<key>LSUIElement</key><true/>`n"
Plist "$xpc\Info.plist" "`t<key>CFBundleName</key><string>BundleXpc</string>`n`t<key>CFBundleIdentifier</key><string>com.mos.teste.bundle</string>`n`t<key>CFBundleExecutable</key><string>BundleXpc</string>`n`t<key>CFBundlePackageType</key><string>XPC!</string>`n"
Write-Host "launchdtest: $Dest" -ForegroundColor Green

if ($OutDir) {
    $esp = Join-Path $root "$OutDir\esp"
    if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
    New-Item -ItemType Directory -Force "$esp\mOS\Aplicativos", "$esp\mOS\Library\LaunchDaemons", "$esp\mOS\Library\mOS-teste" | Out-Null
    Copy-Item -Recurse -Force (Join-Path $Dest 'LaunchdTeste.app') "$esp\mOS\Aplicativos\"
    Copy-Item -Force "$Dest\lib\servidor" "$esp\mOS\Library\mOS-teste\servidor"
    Plist "$esp\mOS\Library\LaunchDaemons\com.mos.teste.xpc.plist" "`t<key>Label</key><string>com.mos.teste.xpc</string>`n`t<key>ProgramArguments</key><array><string>/Library/mOS-teste/servidor</string></array>`n`t<key>MachServices</key><dict><key>com.mos.teste.xpc</key><true/></dict>`n"
    Plist "$esp\mOS\Library\LaunchDaemons\com.mos.teste.cliente.plist" "`t<key>Label</key><string>com.mos.teste.cliente</string>`n`t<key>Program</key><string>/Aplicativos/LaunchdTeste.app/Contents/MacOS/LaunchdTeste</string>`n`t<key>RunAtLoad</key><true/>`n"
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
    # imagem de teste: overlay de teste com o marcador que pula o assistente do Live (tools\overlay-teste.ps1)
    . "$PSScriptRoot\overlay-teste.ps1"
    $ovt = New-OverlayTeste $root $OutDir
    & (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
    if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
    Write-Host "OK: $OutDir\mOS.img com o teste do launchd no boot" -ForegroundColor Green
}
