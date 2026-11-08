# mOS - compila o teste do UDP (SOCK_DGRAM) (userland\tests-udp) como UdpTest.app e, com -OutDir, põe na imagem
# para abrir sozinho no boot com a saída no log serial.
#   .\build.ps1 -OutDir build-udp
#   .\tools\build-udptest.ps1 -OutDir build-udp
#   .\run.ps1 -Headless -Image build-udp/mOS.img -Serial build-udp/serial.log      (procure "udptest:" no log)
param([string]$OutDir = '', [string]$Dest = "$PSScriptRoot\..\build\udptest", [int]$SizeMiB = 128)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'UdpTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 "$root\userland\tests-udp\main.c" -o "$out\UdpTest" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'udptest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>UdpTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.udptest</string>
	<key>CFBundleExecutable</key><string>UdpTest</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'UdpTest.app\Contents\Info.plist')
Write-Host "udptest: $Dest\UdpTest.app" -ForegroundColor Green
if ($OutDir) {
    $esp = Join-Path $root "$OutDir\esp"
    if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
    New-Item -ItemType Directory -Force "$esp\mOS\Aplicativos" | Out-Null
    Copy-Item -Recurse -Force (Join-Path $Dest 'UdpTest.app') "$esp\mOS\Aplicativos\"
    # o init abre o teste 8 s depois do boot (tempo do DHCP); a saída dos apps vai ao log serial
    New-Item -ItemType Directory -Force "$esp\mOS\Sistema\Biblioteca\LaunchDaemons" | Out-Null
    "Programa=/Aplicativos/UdpTest.app/Contents/MacOS/UdpTest`nArgumentos=`nAtrasoMs=8000" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.udptest.servico"
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
    & (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
    if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
    Write-Host "OK: $OutDir\mOS.img com UdpTest no boot" -ForegroundColor Green
}
