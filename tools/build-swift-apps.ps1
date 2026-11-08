# mOS - compila apps em Swift / SwiftUI (userland\samples-swift\<Nome>\*.swift) como bundles .app.
# Usa o toolchain Swift (Embedded) de .tools\swift, a stdlib de build\swift\embedded (tools\build-swift-stdlib.ps1)
# e o módulo SwiftUI do mOS (userland\sdk\swift\SwiftUI), que roda sobre o AppKit do libSystem.
param([string]$Dest = "$PSScriptRoot\..\build\samples")
$root = Split-Path $PSScriptRoot -Parent
$tcd = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $tcd) { Write-Host 'toolchain Swift ausente (.tools\swift): apps Swift ignorados' -ForegroundColor Yellow; exit 0 }
$swiftc = "$($tcd.FullName)\usr\bin\swiftc.exe"
$sdk = (Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Platforms" -Directory | Select-Object -First 1).FullName
$shims = "$sdk\Windows.platform\Developer\SDKs\Windows.sdk\usr\lib\swift\shims"
if (-not (Test-Path "$root\build\swift\embedded\Swift.swiftmodule")) { & "$PSScriptRoot\build-swift-stdlib.ps1"; if ($LASTEXITCODE) { exit 1 } }
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
$zig = if ($zig) { Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $env:MOS_ZIG } else { (Get-Command zig).Source }
$obj = "$root\build\swift\obj"
New-Item -ItemType Directory -Force $obj, $Dest | Out-Null
$common = @('-frontend', '-c', '-target', 'x86_64-apple-macos14', '-enable-experimental-feature', 'Embedded', '-Osize', '-swift-version', '5',
            '-I', "$root\build\swift\embedded", '-I', $shims, '-I', "$root\userland\sdk\swift\MOSBridge", '-I', $obj,
            '-Xcc', '-Wno-nullability-completeness')
function Run($a) { $log = & $swiftc @a 2>&1 | ForEach-Object { "$_" }; $err = $log | Select-String ' error: '; if ($err -or $LASTEXITCODE) { $log | Select-String -Pattern 'error|note:' | Select-Object -First 25 | ForEach-Object { $_.Line }; return $false }; return $true }
# módulo SwiftUI
$ui = Get-ChildItem "$root\userland\sdk\swift\SwiftUI\*.swift" | ForEach-Object FullName
if (-not (Run ($common + @('-parse-as-library', '-module-name', 'SwiftUI', '-emit-module-path', "$obj\SwiftUI.swiftmodule", '-o', "$obj\SwiftUI.o") + $ui))) { Write-Host 'SwiftUI do mOS FALHOU' -ForegroundColor Red; exit 1 }
foreach ($dir in Get-ChildItem "$root\userland\samples-swift" -Directory -ErrorAction SilentlyContinue) {
    $name = $dir.Name
    $src = @(Get-ChildItem $dir.FullName -Filter *.swift -Recurse | ForEach-Object FullName)
    $app = Join-Path $Dest "$name.app\Contents"
    New-Item -ItemType Directory -Force "$app\MacOS" | Out-Null
    $code = ($src | ForEach-Object { Get-Content $_ -Raw }) -join "`n"
    $libs = @("$obj\SwiftUI.o")
    $extra = if ($src.Count -eq 1 -and $code -notmatch '@main') { @() } else { @('-parse-as-library') }
    if (-not (Run ($common + $extra + @('-module-name', $name, '-o', "$obj\$name.o") + $src))) { Write-Host "app Swift $name FALHOU" -ForegroundColor Red; exit 1 }
    & $zig cc -target x86_64-macos "$obj\$name.o" @libs -o "$app\MacOS\$name" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE) { Write-Host "link de $name FALHOU" -ForegroundColor Red; exit 1 }
    @"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>$name</string>
	<key>CFBundleIdentifier</key><string>com.mos.$($name.ToLower())</string>
	<key>CFBundleShortVersionString</key><string>1.0</string>
	<key>CFBundleExecutable</key><string>$name</string>
	<key>LSMinimumSystemVersion</key><string>14.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii "$app\Info.plist"
    Write-Host "app Swift $name ok"
}
