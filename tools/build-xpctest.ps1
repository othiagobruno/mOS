# mOS - compila o teste do XPC (userland\tests-xpc) como XpcTest.app
#   .\tools\build-xpctest.ps1 [-Dest build\xpctest]
#   $env:MACHOTEST_PATH = '/XpcTest.app/Contents/MacOS/XpcTest'
#   .\build\machotest.exe build\libSystem.mos build\xpctest build\xpctest\XpcTest.app\Contents\MacOS\XpcTest
param([string]$Dest = "$PSScriptRoot\..\build\xpctest")
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'XpcTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 -fblocks "$root\userland\tests-xpc\main.c" -o "$out\XpcTest" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'xpctest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>XpcTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.xpctest</string>
	<key>CFBundleExecutable</key><string>XpcTest</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'XpcTest.app\Contents\Info.plist')
Write-Host "xpctest: $Dest\XpcTest.app" -ForegroundColor Green
