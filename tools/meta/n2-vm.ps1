# mOS - N2 "Abre" da meta (docs/meta-compat.md): a janela principal do app fica de pé por 10 min na VM.
# Roda onde houver QEMU (o PC Windows do Thiago, com -Accel whpx). Não roda nesta máquina Linux.
#   .\tools\meta\n2-vm.ps1 -Id slack -App "build\apps\x\slack\Slack.app" [-Minutos 10] [-Accel whpx] [-SemBuild]
#   depois, no Linux ou no Windows: python3 tools/meta/placar.py --build <build> --n2 build-n2
# Usa o tools\vscode-teste.ps1 -Electron (stage-electron + QEMU + capturas QMP + tools\analisa-tela.ps1).
# Cobre os apps Electron: chatgpt, cursor, antigravity, vscode, slack, grok, claude, openbot.
# WhatsApp, Postico, Setapp (AppKit/SwiftUI) e o Codex CLI (terminal) ainda não têm partida automática
# numa imagem de teste: o script recusa esses ids em vez de dar um N2 falso.
# Saída: build-n2\<Id>\n2.json {ok, janela_min, capturas, fim, inicio_app} + n2-saida.txt + shots\ + serial.
# ok = o processo principal não caiu antes do limite, o QEMU não fechou, e todas as capturas desde a
# primeira com janela, cobrindo -Minutos, acharam a janela (janela=True) com conteúdo desenhado (conteudo>0).
param([Parameter(Mandatory = $true)][string]$Id, [Parameter(Mandatory = $true)][string]$App,
      [int]$Minutos = 10, [string]$Accel = '', [switch]$SemBuild, [int]$Port = 5931, [string]$OutBase = 'build-n2')
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $root
$electron = @('chatgpt', 'cursor', 'antigravity', 'vscode', 'slack', 'grok', 'claude', 'openbot')
if ($electron -notcontains $Id) {
    Write-Error "n2-vm: '$Id' não é Electron; ainda não há partida automática para ele (docs/meta-compat.md, N2)."
    exit 2
}
$plist = Join-Path $App 'Contents\Info.plist'
$nome = $null
if (Test-Path $plist) {
    $t = Get-Content -Raw $plist
    if ($t -match '<key>CFBundleExecutable</key>\s*<string>([^<]+)</string>') { $nome = $Matches[1] }
}
if (-not $nome) { Write-Error "n2-vm: sem CFBundleExecutable em $plist (Info.plist binário? converta com plutil)"; exit 2 }
$dir = Join-Path $root "$OutBase\$Id"
New-Item -ItemType Directory -Force $dir | Out-Null
$out = "$OutBase-$Id"
$cap = 60
$limite = $Minutos * 60 + 360   # boot + partida do app antes da primeira janela
$p = @{ App = $App; Electron = $true; Nome = $nome; OutDir = $out; Limite = $limite; Ocioso = 100000; Capturas = $cap; Port = $Port }
if ($Accel) { $p.Accel = $Accel }
if ($SemBuild) { $p.SemBuild = $true }
$saida = & "$root\tools\vscode-teste.ps1" @p 2>&1 | ForEach-Object { "$_" }
$saida | Set-Content -Encoding utf8 "$dir\n2-saida.txt"
Copy-Item "$root\$out\serial-rapido.log" "$dir\serial.log" -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$dir\shots" | Out-Null
Copy-Item "$root\$out\shots\cap-*.png", "$root\$out\shots\ultimo.png" "$dir\shots" -ErrorAction SilentlyContinue

$caps = @()
foreach ($l in $saida) {
    $m = [regex]::Match($l, 'captura (\d+): janela=(True|False) conteudo=(\d+)')
    if ($m.Success) { $caps += [pscustomobject]@{ n = [int]$m.Groups[1].Value; janela = $m.Groups[2].Value -eq 'True'; conteudo = [int]$m.Groups[3].Value } }
}
$fimLinha = ($saida | Where-Object { $_ -match '^QEMU: .* s \((.+)\); total' } | Select-Object -Last 1)
$fim = if ($fimLinha -match '\((.+)\); total') { $Matches[1] } else { 'desconhecido' }
$primeira = ($caps | Where-Object { $_.janela -and $_.conteudo -gt 0 } | Select-Object -First 1)
$janelaMin = 0
$continua = $true
if ($primeira) {
    $depois = $caps | Where-Object { $_.n -ge $primeira.n }
    $continua = -not ($depois | Where-Object { -not $_.janela -or $_.conteudo -le 0 })
    $janelaMin = [math]::Round(((($depois | Measure-Object -Property n -Maximum).Maximum - $primeira.n) * $cap) / 60.0, 1)
}
$ok = [bool]($primeira -and $continua -and $janelaMin -ge $Minutos -and $fim -eq 'limite de tempo')
$res = [ordered]@{
    id = $Id; ok = $ok; janela_min = $janelaMin; minutos_exigidos = $Minutos; fim = $fim
    capturas = $caps.Count; primeira_janela = $(if ($primeira) { $primeira.n } else { $null })
    data = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm') + ' UTC'
    commit = (git -C $root rev-parse --short HEAD); accel = $Accel
}
$res | ConvertTo-Json | Set-Content -Encoding utf8 "$dir\n2.json"
"N2 {0}: {1} (janela {2} min de {3}; fim: {4}; capturas {5})" -f $Id, $(if ($ok) { 'ok' } else { 'não' }), $janelaMin, $Minutos, $fim, $caps.Count
