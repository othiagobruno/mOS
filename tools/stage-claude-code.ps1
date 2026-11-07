# mOS - põe o Claude Code (binário único da Anthropic: Bun + JavaScriptCore) na imagem e o abre no boot com a saída no log serial.
#   .\build.ps1 -OutDir build-net
#   .\tools\stage-claude-code.ps1 -Binario "build\apps\x\claude-code\claude" [-OutDir build-net] [-Argumentos "--version"]
#   .\run.ps1 -Headless -Memory 1536 -Image build-net/mOS.img -Serial build-net/serial.log
# Baixe de https://downloads.claude.ai/claude-code-releases/<versão>/darwin-x64/claude. Ver docs/compat-apps.md.
param([Parameter(Mandatory = $true)][string]$Binario, [string]$OutDir = 'build-net', [string]$Argumentos = '--version',
      [int]$SizeMiB = 512, [int]$AtrasoMs = 8000)
$root = Split-Path $PSScriptRoot -Parent
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
$app = "$esp\mOS\Aplicativos\ClaudeCode.app\Contents"
New-Item -ItemType Directory -Force "$app\MacOS" | Out-Null
Copy-Item -Force $Binario "$app\MacOS\claude"
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>Claude Code</string>
	<key>CFBundleIdentifier</key><string>com.anthropic.claude-code</string>
	<key>CFBundleExecutable</key><string>claude</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii "$app\Info.plist"
"Programa=/Aplicativos/ClaudeCode.app/Contents/MacOS/claude`nArgumentos=$Argumentos`nAtrasoMs=$AtrasoMs" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.anthropic.claudecode.servico"
Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img com o Claude Code ($Argumentos) no boot" -ForegroundColor Green
