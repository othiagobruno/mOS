# mOS - compila o teste de rede dos apps (userland\tests-net) como NetTest.app e, com -OutDir, põe na imagem
# para abrir sozinho no boot com a saída no log serial.
#   .\build.ps1 -OutDir build-net
#   .\tools\build-nettest.ps1 -OutDir build-net [-Host example.com]
#   .\run.ps1 -Headless -Image build-net/mOS.img -Serial build-net/serial.log      (procure "nettest:" no log)
param([string]$OutDir = '', [string]$Dest = "$PSScriptRoot\..\build\nettest", [string]$Host_ = 'example.com', [int]$SizeMiB = 128)
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'NetTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 "$root\userland\tests-net\main.c" -o "$out\NetTest" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'nettest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>NetTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.nettest</string>
	<key>CFBundleExecutable</key><string>NetTest</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'NetTest.app\Contents\Info.plist')
Write-Host "nettest: $Dest\NetTest.app" -ForegroundColor Green
if ($OutDir) {
    $esp = Join-Path $root "$OutDir\esp"
    if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
    Copy-Item -Recurse -Force (Join-Path $Dest 'NetTest.app') "$esp\mOS\Aplicativos\"
    # o init abre o teste 8 s depois do boot (tempo do DHCP); a saída dos apps vai ao log serial
    "Programa=/Aplicativos/NetTest.app/Contents/MacOS/NetTest`nArgumentos=$Host_`nAtrasoMs=8000" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.nettest.servico"
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
    # imagem de teste: overlay de teste com o marcador que pula o assistente do Live (tools\overlay-teste.ps1)
    . "$PSScriptRoot\overlay-teste.ps1"
    $ovt = New-OverlayTeste $root $OutDir
    & (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
    if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
    Write-Host "OK: $OutDir\mOS.img com NetTest no boot" -ForegroundColor Green
}
