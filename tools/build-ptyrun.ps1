# mOS - PtyRun (userland\tests-pty\ptyrun.c): roda um programa num pseudoterminal seguindo um roteiro e imprime a tela no
# log serial ("ptyrun:" para eventos, "tela|" para as linhas). Testa programas de tela cheia sem teclado nem monitor.
# Com -OutDir, põe /Aplicativos/PtyRun.app na imagem com o roteiro e o abre sozinho no boot.
#   .\build.ps1 -OutDir build-cc
#   .\tools\build-ptyrun.ps1 -OutDir build-cc -Roteiro tools\testdata\claude-tui.txt -SizeMiB 700
#   .\run.ps1 -Headless -Memory 1536 -Image build-cc/mOS.img -Serial build-cc/serial.log
# -SemImagem: só copia para o esp (quem chamou monta a imagem). O roteiro está descrito no começo do ptyrun.c.
param([string]$OutDir = '', [string]$Roteiro = '', [string]$Dest = "$PSScriptRoot\..\build\ptyrun", [int]$SizeMiB = 160,
      [int]$AtrasoMs = 8000, [switch]$SemImagem)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'PtyRun.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 "$root\userland\tests-pty\ptyrun.c" -o "$out\PtyRun" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'ptyrun: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>PtyRun</string>
	<key>CFBundleIdentifier</key><string>com.mos.ptyrun</string>
	<key>CFBundleExecutable</key><string>PtyRun</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'PtyRun.app\Contents\Info.plist')
Write-Host "ptyrun: $Dest\PtyRun.app" -ForegroundColor Green
if (-not $OutDir) { exit 0 }
if (-not $Roteiro -or -not (Test-Path $Roteiro)) { Write-Host "falta o -Roteiro (ex.: tools\testdata\claude-tui.txt)" -ForegroundColor Red; exit 1 }
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
Copy-Item -Recurse -Force (Join-Path $Dest 'PtyRun.app') "$esp\mOS\Aplicativos\"
New-Item -ItemType Directory -Force "$esp\mOS\Aplicativos\PtyRun.app\Contents\Resources" | Out-Null
Copy-Item -Force $Roteiro "$esp\mOS\Aplicativos\PtyRun.app\Contents\Resources\roteiro.txt"
"Programa=/Aplicativos/PtyRun.app/Contents/MacOS/PtyRun`nArgumentos=/Aplicativos/PtyRun.app/Contents/Resources/roteiro.txt`nAtrasoMs=$AtrasoMs`nEntrada=/dev/null" |
    Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.ptyrun.servico"
if ($SemImagem) { exit 0 }
# imagem de teste: overlay de teste com o marcador que pula o assistente do Live (tools\overlay-teste.ps1)
. "$PSScriptRoot\overlay-teste.ps1"
$ovt = New-OverlayTeste $root $OutDir
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img com o PtyRun ($(Split-Path $Roteiro -Leaf)) no boot" -ForegroundColor Green
