# mOS - ciclo rápido de teste do porte do VS Code (docs/porte-vscode.md)
#   .\tools\vscode-teste.ps1 -App "<pasta>\Visual Studio Code.app" [-SemBuild] [-Ocioso 40] [-Limite 600] [-Persist]
# -Persist: sem -snapshot, o que o app grava fica na imagem (ex.: logs do VS Code em ~/Library/Application Support/Code/logs)
# Compila (uma vez, sem novas tentativas), monta a imagem com o VS Code abrindo sozinho no boot, sobe o QEMU
# sem interface e acompanha o log serial: termina assim que o processo do VS Code cai, ou quando o log
# fica -Ocioso segundos sem novidades depois que ele começa, ou no -Limite total. Mostra o que importa do
# log e simboliza a falha (libSystem ou Electron Framework).
# -Electron: outro app Electron (Slack, Cursor, Claude...) pelo tools\stage-electron.ps1, com o ca-log ligado; -Nome é o
# nome do processo principal no log ("Slack") e -Servico o do LaunchDaemon (o do stage-electron é com.mos.electron-teste).
#   .\tools\vscode-teste.ps1 -Electron -App "build\apps\x\slack-x64\Slack\Slack.app" -Nome Slack -OutDir build-electron
param([Parameter(Mandatory = $true)][string]$App, [switch]$SemBuild, [int]$Ocioso = 40, [int]$Limite = 600,
      [int]$Memory = 2700, [int]$Port = 5917, [string]$OutDir = 'build-vscode',
      [switch]$Persist, [string]$Argumentos = '--in-process-gpu --disable-gpu --no-sandbox --verbose --enable-logging',
      [string]$Tracepoints = '', [switch]$Electron, [string]$Nome = 'Code', [string]$Servico = '')
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
if (-not $Servico) { $Servico = if ($Electron) { 'com.mos.electron-teste' } else { 'com.microsoft.vscode' } }
$NomeRx = [regex]::Escape($Nome)
$ServicoRx = [regex]::Escape($Servico)
function Stage-App {
    if ($Electron) { .\tools\stage-electron.ps1 -App $App -OutDir $OutDir -Diagnostico -CaLog -Autostart -SizeMiB 1024 -Args0 $Argumentos 6>&1 2>&1 | ForEach-Object { "$_" } }
    else { .\tools\stage-vscode.ps1 -App $App -OutDir $OutDir -Diagnostico -ComApp -ComModulosNativos -Autostart -SizeMiB 768 -Args0 $Argumentos -Tracepoints $Tracepoints 6>&1 2>&1 | ForEach-Object { "$_" } }
}
$t0 = Get-Date
# um QEMU de rodada anterior ainda segura o mOS.img: sem isto o mkimg falhava e a rodada subia a imagem velha
Get-CimInstance Win32_Process -Filter "Name='qemu-system-x86_64.exe'" | Where-Object { $_.CommandLine -match "tcp:127.0.0.1:$Port," } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
if (-not $SemBuild) {
    $b = .\build.ps1 -OutDir $OutDir 6>&1 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) {   # falhas intermitentes (pasta da esp sumindo no meio, build de outra sessão): mais uma vez
        Start-Sleep -Seconds 5
        $b = .\build.ps1 -OutDir $OutDir 6>&1 2>&1 | ForEach-Object { "$_" }
    }
    if ($LASTEXITCODE -ne 0) { 'build falhou:'; $b | Where-Object { $_ -match 'error|FAILED' } | Select-Object -First 8; exit 1 }
    # o libSystem exato desta rodada (outras sessões recompilam build\ e os endereços deixam de bater)
    Copy-Item "$root\build\libSystem.mos" "$root\$OutDir\libSystem.mos" -Force
    $st = Stage-App
    if ($st -match 'FAILED') {   # uma nova tentativa (arquivo ainda preso por um instante); depois desiste, sem subir imagem velha
        Start-Sleep -Seconds 3
        $st = Stage-App
        if ($st -match 'FAILED') { 'imagem falhou:'; $st | Select-Object -Last 4; exit 1 }
    }
}
"build+imagem: {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds
$serial = "$OutDir/serial-rapido.log"
Remove-Item $serial -ErrorAction SilentlyContinue
# reset da CPU (triple fault com -no-reboot fecha o QEMU em silêncio): registradores em qemu-reset.log
$env:MOS_QEMU_LOG = "$root\$OutDir\qemu-reset.log"
Remove-Item $env:MOS_QEMU_LOG -ErrorAction SilentlyContinue
Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$root\run.ps1`" -Headless -Memory $Memory -Image $OutDir/mOS.img -Port $Port -Serial $serial$(if ($Persist) { ' -Persist' })" -WindowStyle Hidden | Out-Null
$t1 = Get-Date; $started = $null; $lastLen = 0; $lastChange = Get-Date; $fim = 'limite de tempo'
while (((Get-Date) - $t1).TotalSeconds -lt $Limite) {
    Start-Sleep -Milliseconds 1500
    if (-not (Test-Path $serial)) { continue }
    $txt = Get-Content $serial -Raw -ErrorAction SilentlyContinue
    if (-not $txt) { continue }
    if (-not $started -and $txt -match "proc: iniciado $NomeRx") { $started = Get-Date }
    $sig = ($txt -replace '(?m)^(persist|despejo):.*\r?\n', '').Length   # gravações de arquivo não contam como atividade
    if ($sig -ne $lastLen) { $lastLen = $sig; $lastChange = Get-Date }
    if ($txt -match "proc: $NomeRx \(pid \d+\): [^\r\n]*encerrado|init: $ServicoRx \(pid \d+\) terminou") { $fim = 'o processo terminou'; break }
    if ($started -and ((Get-Date) - $lastChange).TotalSeconds -ge $Ocioso) { $fim = "log parado por $Ocioso s (processo vivo)"; break }
    if (((Get-Date) - $t1).TotalSeconds -gt 20 -and -not (Get-CimInstance Win32_Process -Filter "Name='qemu-system-x86_64.exe'" |
            Where-Object { $_.CommandLine -match "tcp:127.0.0.1:$Port," })) { $fim = 'o QEMU fechou (reset da CPU: veja qemu-reset.log)'; break }
}
if ((Test-Path $env:MOS_QEMU_LOG) -and (Get-Item $env:MOS_QEMU_LOG).Length -gt 0) {
    'qemu-reset.log (o último reset é o que importa; os do começo são do boot):'
    Get-Content $env:MOS_QEMU_LOG | Select-String 'CPU Reset|RIP=|EXCP|check_exception|Triple' | Select-Object -Last 8 | ForEach-Object { '  ' + $_.Line }
}
# captura da tela antes de desligar (QMP): $OutDir\shots\ultimo.png
try {
    New-Item -ItemType Directory -Force "$root\$OutDir\shots" | Out-Null
    $c = New-Object System.Net.Sockets.TcpClient('127.0.0.1', $Port)
    $st = $c.GetStream(); $r = New-Object IO.StreamReader($st); $w = New-Object IO.StreamWriter($st); $w.AutoFlush = $true
    $null = $r.ReadLine(); $w.WriteLine('{"execute":"qmp_capabilities"}'); $null = $r.ReadLine()
    $w.WriteLine((@{ execute = 'screendump'; arguments = @{ filename = "$root\$OutDir\shots\ultimo.png"; format = 'png' } } | ConvertTo-Json -Compress))
    $null = $r.ReadLine(); $c.Close()
    "captura: $OutDir\shots\ultimo.png"
} catch { 'captura: não foi possível (QMP)' }
Get-CimInstance Win32_Process -Filter "Name='qemu-system-x86_64.exe'" | Where-Object { $_.CommandLine -match "tcp:127.0.0.1:$Port," } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
"QEMU: {0:N0} s ({1}); total {2:N0} s" -f ((Get-Date) - $t1).TotalSeconds, $fim, ((Get-Date) - $t0).TotalSeconds
$log = Get-Content $serial
$s = ($log | Select-String "proc: iniciado $NomeRx" | Select-Object -First 1).LineNumber
if (-not $s) { $log | Select-Object -Last 15; exit 0 }
$ruido = '^persist|^despejo|^\s*$|^HDA|accessibilityDisplay|defaultQueue|effectiveAppearance|SCNetwork|network_change|network_config_watcher|SCDynamicStore|dns_config'
$log[$s..($log.Count - 1)] | Where-Object { $_ -notmatch $ruido } | Select-Object -Unique | Select-Object -Last 30
$m = $log | Select-String -Pattern "proc: $NomeRx.* em (0x[0-9a-f]+)" | Select-Object -Last 1
if ($m) {
    $a = $m.Matches[0].Groups[1].Value
    $v = [Convert]::ToUInt64($a.Substring(2), 16)
    $ls = if (Test-Path "$root\$OutDir\libSystem.mos") { "$root\$OutDir\libSystem.mos" } else { "$root\build\libSystem.mos" }
    if ($v -lt 0x400001000000) { 'falha em libSystem: ' + (& "$root\build\machocheck.exe" --sym $ls $a.Substring(2)) }
    else { 'falha em Electron Framework+0x{0:X}' -f ($v - 0x400001010000) }
}
