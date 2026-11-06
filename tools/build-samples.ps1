# mOS - compila os apps Cocoa de exemplo (userland\samples\<Nome>\*.m) como bundles .app do macOS
#   .\tools\build-samples.ps1 -Dest <pasta>   (o build.ps1 usa \mOS\Aplicativos da imagem)
# Os executaveis sao Mach-O x86_64 de verdade, compilados contra o SDK do mOS (userland\sdk).
param([string]$Dest = "$PSScriptRoot\..\build\samples")
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force $Dest | Out-Null
foreach ($dir in Get-ChildItem "$root\userland\samples" -Directory -ErrorAction SilentlyContinue) {
    $name = $dir.Name
    $src = Get-ChildItem $dir.FullName -Include *.m, *.c -File -Recurse | ForEach-Object FullName
    if (-not $src) { continue }
    $app = Join-Path $Dest "$name.app\Contents"
    New-Item -ItemType Directory -Force "$app\MacOS" | Out-Null
    & $zig cc -target x86_64-macos -O2 -fobjc-arc -I"$root\userland\sdk\include" -x objective-c $src -o "$app\MacOS\$name" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host "sample $name FAILED" -ForegroundColor Red; exit 1 }
    if (Test-Path "$($dir.FullName)\Resources") { New-Item -ItemType Directory -Force "$app\Resources" | Out-Null; Copy-Item "$($dir.FullName)\Resources\*" "$app\Resources" -Recurse -Force }
    $plist = "$($dir.FullName)\Info.plist"
    if (Test-Path $plist) { Copy-Item $plist "$app\Info.plist" }
    else {
        @"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>$name</string>
	<key>CFBundleIdentifier</key><string>com.mos.$($name.ToLower())</string>
	<key>CFBundleShortVersionString</key><string>1.0</string>
	<key>CFBundleExecutable</key><string>$name</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii "$app\Info.plist"
    }
}
