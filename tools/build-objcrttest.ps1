# mOS - compila o teste do runtime ObjC (userland\tests-objcrt) como ObjcRtTest.app
#   .\tools\build-objcrttest.ps1 [-Dest build\objcrttest]
#   $env:MACHOTEST_PATH = '/ObjcRtTest.app/Contents/MacOS/ObjcRtTest'
#   .\build\machotest.exe build\libSystem.mos build\objcrttest build\objcrttest\ObjcRtTest.app\Contents\MacOS\ObjcRtTest
param([string]$Dest = "$PSScriptRoot\..\build\objcrttest")
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'ObjcRtTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 -fobjc-arc -I"$root\userland\sdk\include" -x objective-c "$root\userland\tests-objcrt\main.m" -o "$out\ObjcRtTest" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'objcrttest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>ObjcRtTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.objcrttest</string>
	<key>CFBundleExecutable</key><string>ObjcRtTest</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'ObjcRtTest.app\Contents\Info.plist')
Write-Host "objcrttest: $Dest\ObjcRtTest.app" -ForegroundColor Green
