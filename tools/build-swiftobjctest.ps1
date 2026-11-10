# mOS - compila o teste do runtime ObjC (userland\tests-swiftobjc) como SwiftObjcTest.app
#   .\tools\build-swiftobjctest.ps1 [-Dest build\swiftobjctest]
#   $env:MACHOTEST_PATH = '/SwiftObjcTest.app/Contents/MacOS/SwiftObjcTest'
#   .\build\machotest.exe build\libSystem.mos build\swiftobjctest build\swiftobjctest\SwiftObjcTest.app\Contents\MacOS\SwiftObjcTest
param([string]$Dest = "$PSScriptRoot\..\build\swiftobjctest")
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'SwiftObjcTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 -I"$root\userland\sdk\include" -x objective-c "$root\userland\tests-swiftobjc\main.m" -o "$out\SwiftObjcTest" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'swiftobjctest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>SwiftObjcTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.swiftobjctest</string>
	<key>CFBundleExecutable</key><string>SwiftObjcTest</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'SwiftObjcTest.app\Contents\Info.plist')
Write-Host "swiftobjctest: $Dest\SwiftObjcTest.app" -ForegroundColor Green
