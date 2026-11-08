# mOS - compila o teste do SecureTransport (userland\tests-tls) como TlsTest.app e, com -OutDir, põe na imagem
# para abrir sozinho no boot com a saída no log serial.
#   .\build.ps1 -OutDir build-tls
#   .\tools\build-tlstest-app.ps1 -OutDir build-tls [-Host example.com]
#   .\run.ps1 -Headless -Image build-tls/mOS.img -Serial build-tls/serial.log      (procure "tlstest:" no log)
param([string]$OutDir = '', [string]$Dest = "$PSScriptRoot\..\build\tlstest", [string]$Host_ = 'example.com', [int]$SizeMiB = 128)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'TlsTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 "$root\userland\tests-tls\main.c" -o "$out\TlsTest" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'tlstest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>TlsTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.tlstest</string>
	<key>CFBundleExecutable</key><string>TlsTest</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'TlsTest.app\Contents\Info.plist')
Write-Host "tlstest: $Dest\TlsTest.app" -ForegroundColor Green
if ($OutDir) {
    $esp = Join-Path $root "$OutDir\esp"
    if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
    Copy-Item -Recurse -Force (Join-Path $Dest 'TlsTest.app') "$esp\mOS\Aplicativos\"
    # o init abre o teste 8 s depois do boot (tempo do DHCP); a saída dos apps vai ao log serial
    "Programa=/Aplicativos/TlsTest.app/Contents/MacOS/TlsTest`nArgumentos=$Host_`nAtrasoMs=8000" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.tlstest.servico"
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
    # imagem de teste: overlay de teste com o marcador que pula o assistente do Live (tools\overlay-teste.ps1)
    . "$PSScriptRoot\overlay-teste.ps1"
    $ovt = New-OverlayTeste $root $OutDir
    & (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
    if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
    Write-Host "OK: $OutDir\mOS.img com TlsTest no boot" -ForegroundColor Green
}
