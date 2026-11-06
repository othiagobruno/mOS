# mOS - automated test: boots headless QEMU, drives it over QMP and takes PNG screenshots.
# Script is a list of steps, separated by ';':
#   wait:<ms>  shot:<name>  shothead:<head>,<name>  move:<x>,<y>  click  rclick  key:<qcode>[+<qcode>]  type:<text>  scroll:<n>
# Example: .\tools\qemu-test.ps1 -Steps "wait:6000;shot:desk;move:20,10;click;wait:500;shot:menu"
#   -Port     : porta QMP (padrão: uma porta livre qualquer, para várias sessões poderem testar ao mesmo tempo)
#   -AhciTest : attaches a scratch SATA disk marked "mOS-AHCI-TEST"; the kernel runs its AHCI write self-test on it
param([string]$Steps = 'wait:6000;shot:screen', [int]$Memory = 1024, [switch]$KeepRunning, [string]$OutDir = 'build', [int]$Port = 0, [switch]$AhciTest, [switch]$Persist, [string]$NvmeDisk = '', [switch]$BootNvme, [switch]$UsbInput, [switch]$BootUsb, [switch]$NoNet, [string]$Vga = 'std', [int]$Monitores = 1)
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
New-Item -ItemType Directory -Force $OutDir\shots | Out-Null
# Porta QMP: várias sessões testam ao mesmo tempo, então cada execução usa a sua.
#   sem -Port: o Windows escolhe uma porta livre;
#   com -Port: encerra só um QEMU que tenha sobrado DESTA imagem nessa porta; se ela estiver com o QEMU de
#   outra pasta/sessão, não mexe nele e troca para uma porta livre.
function Get-FreePort { $l = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0); $l.Start(); $p = $l.LocalEndpoint.Port; $l.Stop(); $p }
function Test-PortBusy([int]$p) {
    try { $l = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $p); $l.Start(); $l.Stop(); $false } catch { $true }
}
if ($Port -gt 0) {
    Get-CimInstance Win32_Process -Filter "Name='qemu-system-x86_64.exe'" |
        Where-Object { $_.CommandLine -match "tcp:127.0.0.1:$Port," -and $_.CommandLine -like "*file=$OutDir/mOS.img*" } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue; Start-Sleep -Milliseconds 500 }
    if (Test-PortBusy $Port) { $old = $Port; $Port = Get-FreePort; Write-Host "porta $old em uso por outra sessão; usando $Port" -ForegroundColor Yellow }
} else {
    $Port = Get-FreePort
}
Remove-Item $OutDir\serial-test.log -ErrorAction SilentlyContinue

$extra = ''
if ($AhciTest) {
    $scratch = "$root\$OutDir\ahci-test.img"
    $bytes = New-Object byte[] (8MB)
    $m = [Text.Encoding]::ASCII.GetBytes('mOS-AHCI-TEST')
    [Array]::Copy($m, $bytes, $m.Length)
    [IO.File]::WriteAllBytes($scratch, $bytes)
    $extra = " -ExtraDisk `"$scratch`""
}
if ($Persist) { $extra += ' -Persist' }
if ($NvmeDisk) { $extra += " -NvmeDisk `"$NvmeDisk`"" }
if ($BootNvme) { $extra += ' -BootNvme' }
if ($UsbInput) { $extra += ' -UsbInput' }
if ($BootUsb) { $extra += ' -BootUsb' }
if ($NoNet) { $extra += ' -NoNet' }
$extra += " -Vga $Vga -Monitores $Monitores"
$proc = Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$root\run.ps1`" -Headless -Memory $Memory -Image $OutDir/mOS.img -Port $Port -Serial $OutDir/serial-test.log$extra" -WindowStyle Hidden -PassThru
$client = $null
for ($i = 0; $i -lt 50; $i++) {
    Start-Sleep -Milliseconds 200
    try { $client = New-Object System.Net.Sockets.TcpClient('127.0.0.1', $Port); break } catch {}
}
if (-not $client) { Write-Error 'could not connect to QMP'; exit 1 }
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

$script:mx = 0; $script:my = 0
function Btn($b, $down) { $null = Qmp @{ execute = 'input-send-event'; arguments = @{ events = @(@{ type = 'btn'; data = @{ down = $down; button = $b } }) } } }
function RelMove([int]$dx, [int]$dy) {
    $null = Qmp @{ execute = 'input-send-event'; arguments = @{ events = @(
        @{ type = 'rel'; data = @{ axis = 'x'; value = $dx } }, @{ type = 'rel'; data = @{ axis = 'y'; value = $dy } }) } }
}
function Key($combo) {
    $keys = $combo.Split('+') | ForEach-Object { @{ type = 'qcode'; data = $_ } }
    $null = Qmp @{ execute = 'send-key'; arguments = @{ keys = @($keys) } }
}
# mapping for the mOS default keyboard layout (ABNT2)
$qmap = @{ ' ' = 'spc'; '.' = 'dot'; ',' = 'comma'; '/' = 'ro'; '?' = 'shift+ro'; '-' = 'minus'; '_' = 'shift+minus'; '=' = 'equal'; '+' = 'shift+equal';
    ':' = 'shift+slash'; ';' = 'slash'; '>' = 'shift+dot'; '<' = 'shift+comma'; '\' = 'less'; '|' = 'shift+less' }

foreach ($s in $Steps.Split(';')) {
    $s = $s.Trim(); if (-not $s) { continue }
    $cmd, $arg = $s.Split(':', 2)
    switch ($cmd) {
        'wait' { Start-Sleep -Milliseconds ([int]$arg) }
        'shot' {
            $f = "$root\$OutDir\shots\$arg.png"
            $r = Qmp @{ execute = 'screendump'; arguments = @{ filename = $f; format = 'png' } }
            Write-Host "shot $f $r"
        }
        'shothead' {   # shothead:<saída>,<nome> — captura de um monitor específico (-Vga virtio -Monitores N)
            $hd, $nm = $arg.Split(',', 2)
            $f = "$root\$OutDir\shots\$nm.png"
            $r = Qmp @{ execute = 'screendump'; arguments = @{ filename = $f; format = 'png'; device = 'vga0'; head = [int]$hd } }
            Write-Host "shot $f $r"
        }
        'home' { RelMove -3000 -3000; Start-Sleep -Milliseconds 80; $script:mx = 0; $script:my = 0 }
        'move' {
            $x, $y = $arg.Split(',') | ForEach-Object { [int]$_ }
            # relative PS/2 mouse: home to the corner in small steps, then move each axis
            # compensating mOS pointer acceleration (|d| > 6 is multiplied by 1.5)
            for ($i = 0; $i -lt 12; $i++) { RelMove -200 0; Start-Sleep -Milliseconds 10 }
            for ($i = 0; $i -lt 8; $i++) { RelMove 0 -200; Start-Sleep -Milliseconds 10 }
            Start-Sleep -Milliseconds 60
            foreach ($axis in 'x', 'y') {
                $rem = if ($axis -eq 'x') { $x } else { $y }
                while ($rem -gt 0) {
                    if ($rem -ge 99) { $s = 66; $eff = 99 }
                    elseif ($rem -gt 9) { $s = [Math]::Floor($rem * 2 / 3); $eff = [Math]::Floor($s * 3 / 2) }
                    else { $s = [Math]::Min($rem, 6); $eff = $s }
                    if ($axis -eq 'x') { RelMove $s 0 } else { RelMove 0 $s }
                    $rem -= $eff; Start-Sleep -Milliseconds 25
                }
            }
            Start-Sleep -Milliseconds 80
        }
        'click' { Btn 'left' $true; Start-Sleep -Milliseconds 60; Btn 'left' $false; Start-Sleep -Milliseconds 60 }
        # kdown:<qcode> / kup:<qcode> — hold and release a key (e.g. meta_l for Cmd+Tab)
        'kdown' { $null = Qmp @{ execute = 'input-send-event'; arguments = @{ events = @(@{ type = 'key'; data = @{ down = $true; key = @{ type = 'qcode'; data = $arg } } }) } }; Start-Sleep -Milliseconds 40 }
        'kup' { $null = Qmp @{ execute = 'input-send-event'; arguments = @{ events = @(@{ type = 'key'; data = @{ down = $false; key = @{ type = 'qcode'; data = $arg } } }) } }; Start-Sleep -Milliseconds 40 }
        'down' { Btn 'left' $true; Start-Sleep -Milliseconds 60 }
        'up' { Btn 'left' $false; Start-Sleep -Milliseconds 60 }
        'drag' {
            $dx, $dy = $arg.Split(',') | ForEach-Object { [int]$_ }
            Btn 'left' $true; Start-Sleep -Milliseconds 80
            for ($i = 1; $i -le 10; $i++) { RelMove ([int]($dx / 10)) ([int]($dy / 10)); Start-Sleep -Milliseconds 30 }
            Btn 'left' $false; Start-Sleep -Milliseconds 80
        }
        'rel' {
            # relative move without homing (keeps hover state), in steps with |dx|+|dy| <= 6 (no acceleration)
            $dx, $dy = $arg.Split(',') | ForEach-Object { [int]$_ }
            while ($dx -ne 0 -or $dy -ne 0) {
                $sx = [Math]::Max(-3, [Math]::Min(3, $dx)); $sy = [Math]::Max(-3, [Math]::Min(3, $dy))
                if ($sy -eq 0) { $sx = [Math]::Max(-6, [Math]::Min(6, $dx)) }
                if ($sx -eq 0) { $sy = [Math]::Max(-6, [Math]::Min(6, $dy)) }
                RelMove $sx $sy; $dx -= $sx; $dy -= $sy; Start-Sleep -Milliseconds 15
            }
        }
        'rclick' { Btn 'right' $true; Start-Sleep -Milliseconds 60; Btn 'right' $false }
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
    }
}
if (-not $KeepRunning) { $null = Qmp @{ execute = 'quit' } }
$client.Close()
if (Test-Path $OutDir\serial-test.log) { Get-Content $OutDir\serial-test.log | Select-Object -Last 60 }
