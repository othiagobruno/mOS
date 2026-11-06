# mOS - compila o teste do Security/CFData (userland\tests-security) como SecTest.app
#   .\tools\build-sectest.ps1 [-Dest build\sectest]
# Rodar no host (duas vezes: grava e depois relê o Keychain em outro processo):
#   $env:MACHOTEST_PATH = '/SecTest.app/Contents/MacOS/SecTest'
#   .\build\machotest.exe build\libSystem.mos build\sectest build\sectest\SecTest.app\Contents\MacOS\SecTest
#   .\build\machotest.exe build\libSystem.mos build\sectest build\sectest\SecTest.app\Contents\MacOS\SecTest reler
# O Keychain fica em <Dest>\Users\Visitante\Library\Keychains\mos-keychain.txt (HOME do carregador).
param([string]$Dest = "$PSScriptRoot\..\build\sectest")
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'SecTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 "$root\userland\tests-security\main.c" -o "$out\SecTest" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'sectest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>SecTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.sectest</string>
	<key>CFBundleExecutable</key><string>SecTest</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'SecTest.app\Contents\Info.plist')
Write-Host "sectest: $Dest\SecTest.app" -ForegroundColor Green
