# mOS - roda passos de entrada (mouse/teclado) e capturas num QEMU que já está rodando, pelo QMP.
#   .\tools\qmp-passos.ps1 -Port 5917 -Passos "move:1070,719;click;wait:3000;shot:depois" [-OutDir build-vscode]
# Passos (separados por ';'), a mesma linguagem do tools\qemu-test.ps1:
#   wait:<ms>  shot:<nome>  move:<x>,<y>  click  dclick  rclick  down  up  key:<qcode>[+<qcode>]  type:<texto>
#   scroll:<n>  rel:<dx>,<dy>  kdown:<qcode>  kup:<qcode>
# As capturas vão para <OutDir>\shots\<nome>.png. O mouse do QEMU é relativo (PS/2): "move" leva o cursor
# ao canto e anda compensando a aceleração do mOS (|d| > 6 vale 1,5x), como o qemu-test.ps1.
# O QMP aceita um cliente por vez: não rode junto com outra captura no mesmo instante.
param([Parameter(Mandatory = $true)][int]$Port, [Parameter(Mandatory = $true)][string]$Passos, [string]$OutDir = 'build-vscode')
$root = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force "$root\$OutDir\shots" | Out-Null
$client = $null
for ($i = 0; $i -lt 20 -and -not $client; $i++) {
    try { $client = New-Object System.Net.Sockets.TcpClient('127.0.0.1', $Port) } catch { Start-Sleep -Milliseconds 250 }
}
if (-not $client) { "qmp-passos: sem conexão com o QMP na porta $Port"; exit 1 }
$stream = $client.GetStream()
$reader = New-Object System.IO.StreamReader($stream)
$writer = New-Object System.IO.StreamWriter($stream)
$writer.AutoFlush = $true
function Qmp($obj) {
    $writer.WriteLine(($obj | ConvertTo-Json -Depth 10 -Compress))
    while ($true) {
        $line = $reader.ReadLine()
        if ($null -eq $line) { return $null }
        if ($line -match '"return"' -or $line -match '"error"') { return $line }
    }
}
$null = $reader.ReadLine()
$null = Qmp @{ execute = 'qmp_capabilities' }
function Btn($b, $down) { $null = Qmp @{ execute = 'input-send-event'; arguments = @{ events = @(@{ type = 'btn'; data = @{ down = $down; button = $b } }) } } }
function RelMove([int]$dx, [int]$dy) {
    $null = Qmp @{ execute = 'input-send-event'; arguments = @{ events = @(
        @{ type = 'rel'; data = @{ axis = 'x'; value = $dx } }, @{ type = 'rel'; data = @{ axis = 'y'; value = $dy } }) } }
}
function Key($combo) {
    $keys = $combo.Split('+') | ForEach-Object { @{ type = 'qcode'; data = $_ } }
    $null = Qmp @{ execute = 'send-key'; arguments = @{ keys = @($keys) } }
}
# teclado ABNT2 (o layout padrão do mOS)
$qmap = @{ ' ' = 'spc'; '.' = 'dot'; ',' = 'comma'; '/' = 'ro'; '?' = 'shift+ro'; '-' = 'minus'; '_' = 'shift+minus'; '=' = 'equal'; '+' = 'shift+equal';
    ':' = 'shift+slash'; ';' = 'slash'; '>' = 'shift+dot'; '<' = 'shift+comma'; '\' = 'less'; '|' = 'shift+less';
    '!' = 'shift+1'; '@' = 'shift+2'; '#' = 'shift+3'; '$' = 'shift+4'; '%' = 'shift+5'; '&' = 'shift+7'; '*' = 'shift+8'; '(' = 'shift+9'; ')' = 'shift+0';
    '[' = 'bracket_right'; ']' = 'backslash'; '{' = 'shift+bracket_right'; '}' = 'shift+backslash'; "'" = 'grave_accent'; '"' = 'shift+grave_accent' }

foreach ($s in $Passos.Split(';')) {
    $s = $s.Trim(); if (-not $s) { continue }
    $cmd, $arg = $s.Split(':', 2)
    switch ($cmd) {
        'wait' { Start-Sleep -Milliseconds ([int]$arg) }
        'shot' {
            $f = "$root\$OutDir\shots\$arg.png"
            Remove-Item $f -ErrorAction SilentlyContinue
            $null = Qmp @{ execute = 'screendump'; arguments = @{ filename = $f; format = 'png' } }
            for ($i = 0; $i -lt 20 -and -not (Test-Path $f); $i++) { Start-Sleep -Milliseconds 250 }
            "{0:HH:mm:ss} passo shot:{1} -> {2}" -f (Get-Date), $arg, $(if (Test-Path $f) { & "$root\tools\analisa-tela.ps1" $f } else { 'falhou' })
        }
        'move' {
            $x, $y = $arg.Split(',') | ForEach-Object { [int]$_ }
            for ($i = 0; $i -lt 12; $i++) { RelMove -200 0; Start-Sleep -Milliseconds 10 }
            for ($i = 0; $i -lt 8; $i++) { RelMove 0 -200; Start-Sleep -Milliseconds 10 }
            Start-Sleep -Milliseconds 60
            foreach ($axis in 'x', 'y') {
                $rem = if ($axis -eq 'x') { $x } else { $y }
                while ($rem -gt 0) {
                    if ($rem -ge 99) { $st = 66; $eff = 99 }
                    elseif ($rem -gt 9) { $st = [Math]::Floor($rem * 2 / 3); $eff = [Math]::Floor($st * 3 / 2) }
                    else { $st = [Math]::Min($rem, 6); $eff = $st }
                    if ($axis -eq 'x') { RelMove $st 0 } else { RelMove 0 $st }
                    $rem -= $eff; Start-Sleep -Milliseconds 25
                }
            }
            Start-Sleep -Milliseconds 80
        }
        'click' { Btn 'left' $true; Start-Sleep -Milliseconds 60; Btn 'left' $false; Start-Sleep -Milliseconds 60 }
        'dclick' { foreach ($k in 1, 2) { Btn 'left' $true; Start-Sleep -Milliseconds 40; Btn 'left' $false; Start-Sleep -Milliseconds 60 } }
        'rclick' { Btn 'right' $true; Start-Sleep -Milliseconds 60; Btn 'right' $false; Start-Sleep -Milliseconds 60 }
        'down' { Btn 'left' $true; Start-Sleep -Milliseconds 60 }
        'up' { Btn 'left' $false; Start-Sleep -Milliseconds 60 }
        'kdown' { $null = Qmp @{ execute = 'input-send-event'; arguments = @{ events = @(@{ type = 'key'; data = @{ down = $true; key = @{ type = 'qcode'; data = $arg } } }) } }; Start-Sleep -Milliseconds 40 }
        'kup' { $null = Qmp @{ execute = 'input-send-event'; arguments = @{ events = @(@{ type = 'key'; data = @{ down = $false; key = @{ type = 'qcode'; data = $arg } } }) } }; Start-Sleep -Milliseconds 40 }
        'rel' {
            $dx, $dy = $arg.Split(',') | ForEach-Object { [int]$_ }
            while ($dx -ne 0 -or $dy -ne 0) {
                $sx = [Math]::Max(-3, [Math]::Min(3, $dx)); $sy = [Math]::Max(-3, [Math]::Min(3, $dy))
                if ($sy -eq 0) { $sx = [Math]::Max(-6, [Math]::Min(6, $dx)) }
                if ($sx -eq 0) { $sy = [Math]::Max(-6, [Math]::Min(6, $dy)) }
                RelMove $sx $sy; $dx -= $sx; $dy -= $sy; Start-Sleep -Milliseconds 15
            }
        }
        'scroll' {
            $n = [int]$arg; $b = if ($n -lt 0) { 'wheel-up' } else { 'wheel-down' }
            for ($i = 0; $i -lt [Math]::Abs($n); $i++) { Btn $b $true; Btn $b $false; Start-Sleep -Milliseconds 30 }
        }
        'key' { Key $arg; Start-Sleep -Milliseconds 80 }
        'type' {
            foreach ($ch in $arg.ToCharArray()) {
                $c = [string]$ch
                if ($qmap.ContainsKey($c)) { Key $qmap[$c] }
                elseif ($c -cmatch '[A-Z]') { Key ('shift+' + $c.ToLower()) }
                else { Key $c }
                Start-Sleep -Milliseconds 40
            }
        }
        default { "qmp-passos: passo desconhecido '$s'" }
    }
}
$client.Close()
