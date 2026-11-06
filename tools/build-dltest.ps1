# mOS - compila o teste do dlopen em tempo de execucao (userland\tests-dlopen) como DLTest.app:
#   Contents/MacOS/DLTest (executavel), Plugin.bundle (MH_BUNDLE, depende de @rpath/libdep.dylib), libdep.dylib
#   .\tools\build-dltest.ps1 -Dest <pasta>      (padrao: build\dltest)
# Rodar no host: build\machotest.exe build\libSystem.mos <Dest> <Dest>\DLTest.app\Contents\MacOS\DLTest
#   com $env:MACHOTEST_PATH = '/DLTest.app/Contents/MacOS/DLTest'
param([string]$Dest = "$PSScriptRoot\..\build\dltest")
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$src = "$root\userland\tests-dlopen"
$out = Join-Path $Dest 'DLTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
$inc = "-I$root\userland\sdk\include"
function Run($what) { if ($LASTEXITCODE -ne 0) { Write-Host "dltest: $what FAILED" -ForegroundColor Red; exit 1 } }
& $zig cc -target x86_64-macos -O1 -dynamiclib "$src\libdep.c" -o "$out\libdep.dylib" "-Wl,-install_name,@rpath/libdep.dylib" 2>&1 | ForEach-Object { "$_" }
Run 'libdep'
# o linker do zig nao tem -bundle: liga como dylib e troca o filetype do cabecalho para MH_BUNDLE (8),
# que e o que o carregador olha (os .node reais do VS Code sao MH_BUNDLE do ld64)
& $zig cc -target x86_64-macos -O1 -shared $inc -x objective-c "$src\plugin.m" -x none "$out\libdep.dylib" -o "$out\Plugin.bundle" "-Wl,-rpath,@loader_path" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
Run 'plugin'
$b = [IO.File]::ReadAllBytes("$out\Plugin.bundle"); $b[12] = 8; [IO.File]::WriteAllBytes("$out\Plugin.bundle", $b)
& $zig cc -target x86_64-macos -O1 $inc -x objective-c "$src\main.m" -o "$out\DLTest" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
Run 'app'
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>DLTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.dltest</string>
	<key>CFBundleExecutable</key><string>DLTest</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'DLTest.app\Contents\Info.plist')
Write-Host "dltest: $Dest\DLTest.app" -ForegroundColor Green
