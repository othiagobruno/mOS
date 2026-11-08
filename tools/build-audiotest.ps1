# mOS - compila o teste de áudio dos apps (userland\tests-audio: CoreAudio, AudioUnit, AudioQueue, AVFoundation) como
# AudioTest.app e, com -OutDir, põe na imagem para abrir sozinho no boot com a saída no log serial (procure "audiotest:").
#   .\build.ps1 -OutDir build-aud
#   .\tools\build-audiotest.ps1 -OutDir build-aud
#   .\run.ps1 -Headless -Image build-aud/mOS.img -Serial build-aud/serial.log -WavOut build-aud/saida.wav
param([string]$OutDir = '', [string]$Dest = "$PSScriptRoot\..\build\audiotest", [int]$SizeMiB = 128)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'AudioTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 -fobjc-arc -fblocks -x objective-c "-I$root\userland\sdk\include" "$root\userland\tests-audio\main.m" `
    -o "$out\AudioTest" "-Wl,-undefined,dynamic_lookup" 2>&1 | Select-String ' error' | ForEach-Object { "  $($_.Line)" }
if (-not (Test-Path "$out\AudioTest")) { Write-Host 'audiotest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>AudioTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.audiotest</string>
	<key>CFBundleExecutable</key><string>AudioTest</string>
	<key>NSPrincipalClass</key><string>NSApplication</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'AudioTest.app\Contents\Info.plist')
Write-Host "audiotest: $Dest\AudioTest.app" -ForegroundColor Green
if ($OutDir) {
    $esp = Join-Path $root "$OutDir\esp"
    if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
    New-Item -ItemType Directory -Force "$esp\mOS\Aplicativos" | Out-Null
    Copy-Item -Recurse -Force (Join-Path $Dest 'AudioTest.app') "$esp\mOS\Aplicativos\"
    New-Item -ItemType Directory -Force "$esp\mOS\Sistema\Biblioteca\LaunchDaemons" | Out-Null
    "Programa=/Aplicativos/AudioTest.app/Contents/MacOS/AudioTest`nArgumentos=`nAtrasoMs=6000" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.audiotest.servico"
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
    & (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
    if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
    Write-Host "OK: $OutDir\mOS.img com AudioTest no boot" -ForegroundColor Green
}
