# mOS - compila o teste do WebKit.framework (userland\tests-webkit) como WebKitTest.app e, com -OutDir, põe na imagem
# para abrir sozinho no boot com a saída no log serial (procure "webkit:").
#   .\build.ps1 -OutDir build-wk                      (já compila o WebKit.framework para a imagem)
#   .\tools\build-webkittest.ps1 -OutDir build-wk
#   .\run.ps1 -Headless -Image build-wk\mOS.img -Serial build-wk\serial.log
param([string]$OutDir = '', [string]$Dest = "$PSScriptRoot\..\build\webkittest", [int]$SizeMiB = 128)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force $Dest | Out-Null
# o framework: o da imagem (build.ps1) ou compilado aqui; o teste liga a ele para os símbolos terem o WebKit como dono
# (com só -undefined dynamic_lookup a busca seria plana e acharia primeiro a fachada do libSystem)
$fw = "$Dest\WebKit-link.dylib"
& $zig cc -target x86_64-macos -O2 -shared -x objective-c "-I$root\userland\sdk\include" -Wno-objc-root-class -Wno-objc-method-access `
    "$root\userland\frameworks\WebKit\WebKit.m" -o $fw '-Wl,-install_name,/System/Library/Frameworks/WebKit.framework/Versions/A/WebKit' `
    '-Wl,-undefined,dynamic_lookup' 2>&1 | Select-String ' error' | ForEach-Object { "  $($_.Line)" }
if (-not (Test-Path $fw)) { Write-Host 'WebKit.framework: FAILED' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'WebKitTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 -fobjc-arc -x objective-c "-I$root\userland\sdk\include" "$root\userland\tests-webkit\main.m" -x none $fw `
    -o "$out\WebKitTest" "-Wl,-undefined,dynamic_lookup" 2>&1 | Select-String ' error' | ForEach-Object { "  $($_.Line)" }
if (-not (Test-Path "$out\WebKitTest")) { Write-Host 'webkittest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>WebKitTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.webkittest</string>
	<key>CFBundleExecutable</key><string>WebKitTest</string>
	<key>NSPrincipalClass</key><string>NSApplication</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'WebKitTest.app\Contents\Info.plist')
Write-Host "webkittest: $Dest\WebKitTest.app" -ForegroundColor Green
if ($OutDir) {
    $esp = Join-Path $root "$OutDir\esp"
    if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
    $wkdir = "$esp\mOS\System\Library\Frameworks\WebKit.framework\Versions\A"
    New-Item -ItemType Directory -Force $wkdir | Out-Null
    Copy-Item $fw "$wkdir\WebKit" -Force
    New-Item -ItemType Directory -Force "$esp\mOS\Aplicativos" | Out-Null
    Copy-Item -Recurse -Force (Join-Path $Dest 'WebKitTest.app') "$esp\mOS\Aplicativos\"
    New-Item -ItemType Directory -Force "$esp\mOS\Sistema\Biblioteca\LaunchDaemons" | Out-Null
    "Programa=/Aplicativos/WebKitTest.app/Contents/MacOS/WebKitTest`nArgumentos=`nAtrasoMs=6000" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.webkittest.servico"
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
    & (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
    if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
    Write-Host "OK: $OutDir\mOS.img com WebKitTest no boot" -ForegroundColor Green
}
