# mOS - teste de um arquivo C só, compilado para macOS x86_64 com o zig, que abre sozinho no boot e escreve no log serial.
#   .\build.ps1 -OutDir build-cc
#   .\tools\stage-teste-macho.ps1 -Fonte userland\tests-signals\avx.c -Nome SinalAVX -OutDir build-cc -Flags '-mavx2'
#   .\run.ps1 -Headless -Memory 1024 -Image build-cc/mOS.img -Serial build-cc/serial.log -Port <livre>
# Tira da imagem os serviços de teste do Claude Code e do PtyRun (para não rodarem juntos). Com AVX (-mavx2), use TCG.
param([Parameter(Mandatory = $true)][string]$Fonte, [Parameter(Mandatory = $true)][string]$Nome, [string]$OutDir = 'build-cc',
      [string]$Flags = '', [int]$SizeMiB = 0, [int]$AtrasoMs = 4000, [string]$Argumentos = '')
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
$app = "$esp\mOS\Aplicativos\$Nome.app\Contents"
New-Item -ItemType Directory -Force "$app\MacOS" | Out-Null
$zf = @('cc', '-target', 'x86_64-macos', '-O1') + ($Flags -split ' ' | Where-Object { $_ }) + @((Join-Path $root $Fonte), '-o', "$app\MacOS\$Nome")
& $zig @zf 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host "${Nome}: compilacao FAILED" -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>$Nome</string>
	<key>CFBundleIdentifier</key><string>com.mos.teste.$($Nome.ToLower())</string>
	<key>CFBundleExecutable</key><string>$Nome</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii "$app\Info.plist"
$ld = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons"
foreach ($s in 'com.anthropic.claudecode.servico', 'com.mos.ptyrun.servico') { Remove-Item -Force -ErrorAction SilentlyContinue "$ld\$s" }
"Programa=/Aplicativos/$Nome.app/Contents/MacOS/$Nome`nArgumentos=$Argumentos`nAtrasoMs=$AtrasoMs`nEntrada=/dev/null" |
    Set-Content -Encoding ascii "$ld\com.mos.teste.$($Nome.ToLower()).servico"
Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
. "$PSScriptRoot\overlay-teste.ps1"
$ovt = New-OverlayTeste $root $OutDir
$sz = if ($SizeMiB) { $SizeMiB } else { 160 }
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $sz --test-overlay $ovt
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img com $Nome no boot" -ForegroundColor Green
