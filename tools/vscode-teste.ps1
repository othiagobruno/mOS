# mOS - ciclo rápido de teste do porte do VS Code (docs/porte-vscode.md)
#   .\tools\vscode-teste.ps1 -App "<pasta>\Visual Studio Code.app" [-SemBuild] [-Ocioso 40] [-Limite 600]
# Compila (uma vez, sem novas tentativas), monta a imagem com o VS Code abrindo sozinho no boot, sobe o QEMU
# sem interface e acompanha o log serial: termina assim que o processo do VS Code cai, ou quando o log
# fica -Ocioso segundos sem novidades depois que ele começa, ou no -Limite total. Mostra o que importa do
# log e simboliza a falha (libSystem ou Electron Framework).
param([Parameter(Mandatory = $true)][string]$App, [switch]$SemBuild, [int]$Ocioso = 40, [int]$Limite = 600,
      [int]$Memory = 2700, [int]$Port = 5917, [string]$OutDir = 'build-vscode',
      [string]$Argumentos = '--single-process --in-process-gpu --disable-gpu --no-sandbox --verbose --enable-logging')
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$t0 = Get-Date
if (-not $SemBuild) {
    $b = .\build.ps1 -OutDir $OutDir 6>&1 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { 'build falhou:'; $b | Where-Object { $_ -match 'error' } | Select-Object -First 8; exit 1 }
    .\tools\stage-vscode.ps1 -App $App -OutDir $OutDir -Diagnostico -ComApp -Autostart -SizeMiB 768 -Args0 $Argumentos | Out-Null
}
"build+imagem: {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds
Get-CimInstance Win32_Process -Filter "Name='qemu-system-x86_64.exe'" | Where-Object { $_.CommandLine -match "tcp:127.0.0.1:$Port," } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
$serial = "$OutDir/serial-rapido.log"
Remove-Item $serial -ErrorAction SilentlyContinue
Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$root\run.ps1`" -Headless -Memory $Memory -Image $OutDir/mOS.img -Port $Port -Serial $serial" -WindowStyle Hidden | Out-Null
$t1 = Get-Date; $started = $null; $lastLen = 0; $lastChange = Get-Date; $fim = 'limite de tempo'
while (((Get-Date) - $t1).TotalSeconds -lt $Limite) {
    Start-Sleep -Milliseconds 1500
    if (-not (Test-Path $serial)) { continue }
    $txt = Get-Content $serial -Raw -ErrorAction SilentlyContinue
    if (-not $txt) { continue }
    if (-not $started -and $txt -match 'proc: iniciado Code') { $started = Get-Date }
    $sig = ($txt -replace '(?m)^persist:.*\r?\n', '').Length   # gravações de arquivo não contam como atividade
    if ($sig -ne $lastLen) { $lastLen = $sig; $lastChange = Get-Date }
    if ($txt -match 'proc: Code \(pid \d+\): [^\r\n]*encerrado|init: com\.microsoft\.vscode \(pid \d+\) terminou') { $fim = 'o processo terminou'; break }
    if ($started -and ((Get-Date) - $lastChange).TotalSeconds -ge $Ocioso) { $fim = "log parado por $Ocioso s (processo vivo)"; break }
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
$s = ($log | Select-String 'proc: iniciado Code' | Select-Object -First 1).LineNumber
if (-not $s) { $log | Select-Object -Last 15; exit 0 }
$ruido = '^persist|^\s*$|^HDA|accessibilityDisplay|defaultQueue|effectiveAppearance|SCNetwork|network_change|network_config_watcher|SCDynamicStore|dns_config'
$log[$s..($log.Count - 1)] | Where-Object { $_ -notmatch $ruido } | Select-Object -Unique | Select-Object -Last 30
$m = $log | Select-String -Pattern 'proc: Code.* em (0x[0-9a-f]+)' | Select-Object -Last 1
if ($m) {
    $a = $m.Matches[0].Groups[1].Value
    $v = [Convert]::ToUInt64($a.Substring(2), 16)
    if ($v -lt 0x400001000000) { 'falha em libSystem: ' + (& "$root\build\machocheck.exe" --sym "$root\build\libSystem.mos" $a.Substring(2)) }
    else { 'falha em Electron Framework+0x{0:X}' -f ($v - 0x400001010000) }
}
