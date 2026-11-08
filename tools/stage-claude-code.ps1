# mOS - põe o Claude Code (binário único da Anthropic: Bun + JavaScriptCore) na imagem e o abre no boot com a saída no log serial.
#   .\build.ps1 -OutDir build-net
#   .\tools\stage-claude-code.ps1 -Binario "build\apps\x\claude-code\claude" [-OutDir build-net] [-Argumentos "--version"] [-Entrada /dev/null]
#   .\run.ps1 -Headless -Memory 1536 -Image build-net/mOS.img -Serial build-net/serial.log
# -Tui: em vez de rodar o claude direto, o PtyRun (tools\build-ptyrun.ps1) o abre num pseudoterminal com o roteiro
#       tools\testdata\claude-tui.txt (modo interativo, sem login) e imprime a tela no log ("tela|").
# O binário exige AVX: sob WHPX (run.ps1 -Accel whpx) cai com #UD; teste no TCG (run.ps1 sem -Accel).
# Baixe de https://downloads.claude.ai/claude-code-releases/<versão>/darwin-x64/claude. Ver docs/compat-apps.md.
param([Parameter(Mandatory = $true)][string]$Binario, [string]$OutDir = 'build-net', [string]$Argumentos = '--version',
      [int]$SizeMiB = 512, [int]$AtrasoMs = 8000, [string]$Entrada = '/dev/null', [switch]$Tui,
      [string]$Roteiro = "$PSScriptRoot\testdata\claude-tui.txt", [switch]$DespejarThreads, [switch]$RastrearMunmap)
# -DespejarThreads: o kernel imprime as threads de cada processo (estado, rip, CPU) a cada 30 s no log serial
# -RastrearMunmap: o kernel registra cada mmap/munmap ("mm: ...") para casar com uma "falta em" na área de mmap
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
$servico = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.anthropic.claudecode.servico"
$ptyrun = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.ptyrun.servico"
if ($Tui) {
    Remove-Item -Force -ErrorAction SilentlyContinue $servico
    & (Join-Path $PSScriptRoot 'build-ptyrun.ps1') -OutDir $OutDir -Roteiro $Roteiro -AtrasoMs $AtrasoMs -SemImagem
    if ($LASTEXITCODE -ne 0) { exit 1 }
    $Argumentos = "modo interativo pelo PtyRun"
} else {
    Remove-Item -Force -ErrorAction SilentlyContinue $ptyrun
    "Programa=/Aplicativos/ClaudeCode.app/Contents/MacOS/claude`nArgumentos=$Argumentos`nAtrasoMs=$AtrasoMs`nEntrada=$Entrada" | Set-Content -Encoding ascii $servico
}
Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
$despejo = "$esp\mOS\Sistema\Biblioteca\despejar-threads"
if ($DespejarThreads) { Set-Content -Path $despejo -Value '1' -Encoding ascii } else { Remove-Item -Force -ErrorAction SilentlyContinue $despejo }
$rastro = "$esp\mOS\Sistema\Biblioteca\rastrear-munmap"
if ($RastrearMunmap) { Set-Content -Path $rastro -Value '1' -Encoding ascii } else { Remove-Item -Force -ErrorAction SilentlyContinue $rastro }
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img com o Claude Code ($Argumentos) no boot" -ForegroundColor Green
