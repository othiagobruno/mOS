# mOS - compila o teste do Metal por software (userland\tests-metal) como MetalTest.app
#   .\tools\build-metaltest.ps1 [-Dest build\metaltest]
#   .\build\machotest.exe build\libSystem.mos build\metaltest build\metaltest\MetalTest.app\Contents\MacOS\MetalTest
param([string]$Dest = "$PSScriptRoot\..\build\metaltest")
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'MetalTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 -fblocks "$root\userland\tests-metal\main.c" -o "$out\MetalTest" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'metaltest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>MetalTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.metaltest</string>
	<key>CFBundleExecutable</key><string>MetalTest</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'MetalTest.app\Contents\Info.plist')
Write-Host "metaltest: $Dest\MetalTest.app" -ForegroundColor Green
