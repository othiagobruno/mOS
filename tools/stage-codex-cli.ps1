# mOS - põe o Codex CLI (binário Rust da OpenAI) na imagem e o abre no boot com a saída no log serial.
#   .\build.ps1 -OutDir build-net
#   .\tools\stage-codex-cli.ps1 -Binario "build\apps\x\codex-cli\codex-x86_64-apple-darwin" [-OutDir build-net] [-Argumentos "--version"]
#   .\run.ps1 -Headless -Memory 1536 -Image build-net/mOS.img -Serial build-net/serial.log
# Baixe de https://github.com/openai/codex/releases (codex-x86_64-apple-darwin.tar.gz). Ver docs/compat-apps.md.
param([Parameter(Mandatory = $true)][string]$Binario, [string]$OutDir = 'build-net', [string]$Argumentos = '--version',
      [int]$SizeMiB = 512, [int]$AtrasoMs = 8000)
$root = Split-Path $PSScriptRoot -Parent
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
$app = "$esp\mOS\Aplicativos\CodexCLI.app\Contents"
New-Item -ItemType Directory -Force "$app\MacOS" | Out-Null
Copy-Item -Force $Binario "$app\MacOS\codex"
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>Codex CLI</string>
	<key>CFBundleIdentifier</key><string>com.openai.codex.cli</string>
	<key>CFBundleExecutable</key><string>codex</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii "$app\Info.plist"
"Programa=/Aplicativos/CodexCLI.app/Contents/MacOS/codex`nArgumentos=$Argumentos`nAtrasoMs=$AtrasoMs" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.openai.codex.servico"
Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img com o Codex CLI ($Argumentos) no boot" -ForegroundColor Green
