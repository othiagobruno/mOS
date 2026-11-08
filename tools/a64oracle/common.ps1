# mOS - oráculo do tradutor arm64: funções comuns dos scripts (a64oracle.ps1, a64cmp.ps1, a64placar.ps1)
$script:A64Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $script:A64Root) { $script:A64Root = (Resolve-Path "$PSScriptRoot\..\..").Path }

function Get-A64Qemu {
    if ($env:MOS_QEMU_A64) { return $env:MOS_QEMU_A64 }
    $p = 'C:\Program Files\qemu\qemu-system-aarch64.exe'
    if (Test-Path $p) { return $p }
    $c = Get-Command qemu-system-aarch64 -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    throw 'qemu-system-aarch64 nao encontrado (defina MOS_QEMU_A64)'
}

function Get-A64Monitor {
    $m = Join-Path $script:A64Root 'build\a64oracle\mon.elf'
    $src = Get-ChildItem (Join-Path $script:A64Root 'tools\a64oracle') -Include mon.c, mon.S, mon.ld, fz.h -Recurse
    $stale = -not (Test-Path $m)
    if (-not $stale) { $t = (Get-Item $m).LastWriteTime; $stale = [bool]($src | Where-Object { $_.LastWriteTime -gt $t }) }
    if ($stale) {
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $script:A64Root 'tools\a64oracle\build.ps1') | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'build do monitor falhou' }
    }
    return $m
}

function ConvertTo-A64Arg([string]$a) {
    if ($a -eq '') { return '""' }
    if ($a -notmatch '[\s"]') { return $a }
    return '"' + ($a -replace '(\*)"', '$1$1\"' -replace '(\+)$', '$1$1') + '"'
}

# roda um executável capturando stdout/stderr (UTF-8) com tempo limite
function Invoke-A64Capture([string]$Exe, [string[]]$ArgList, [int]$TimeoutSec = 60, [string]$Cwd = '') {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = ($ArgList | ForEach-Object { ConvertTo-A64Arg $_ }) -join ' '
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $psi.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    $psi.CreateNoWindow = $true
    if ($Cwd) { $psi.WorkingDirectory = $Cwd }
    $p = [System.Diagnostics.Process]::Start($psi)
    $o = $p.StandardOutput.ReadToEndAsync()
    $e = $p.StandardError.ReadToEndAsync()
    $timedOut = $false
    if (-not $p.WaitForExit($TimeoutSec * 1000)) { $timedOut = $true; try { $p.Kill() } catch {} ; $p.WaitForExit() }
    $p.WaitForExit()
    [pscustomobject]@{ Out = $o.Result; Err = $e.Result; Code = $(if ($timedOut) { -1 } else { $p.ExitCode }); TimedOut = $timedOut }
}

# QEMU com o monitor e um blob (ELF de teste ou lote do fuzzer)
function Invoke-A64Oracle([string]$Blob, [int]$TimeoutSec = 60, [string]$Cwd = '') {
    $q = Get-A64Qemu
    $m = Get-A64Monitor
    $blob = (Resolve-Path $Blob).Path
    if ($blob -match ',') { throw "caminho com virgula nao e aceito pelo QEMU: $blob" }
    $a = @('-M', 'virt', '-cpu', 'max', '-m', '2048', '-display', 'none', '-serial', 'none', '-monitor', 'none',
           '-nodefaults', '-semihosting-config', 'enable=on,target=native', '-kernel', $m,
           '-device', "loader,file=$blob,addr=0x44000000,force-raw=on")
    $r = Invoke-A64Capture $q $a $TimeoutSec $Cwd
    # o console de semihosting do QEMU no Windows troca cada LF por CRLF; desfazer é exato
    # (um CRLF do próprio teste vira CR CR LF e volta a ser CRLF)
    $r.Out = $r.Out.Replace("`r`n", "`n"); $r.Err = $r.Err.Replace("`r`n", "`n")
    $r
}

function Get-A64Test([switch]$Jit) {
    if ($env:MOS_A64TEST) { return $env:MOS_A64TEST }   # o placar fixa uma cópia própria
    $exe = Join-Path $script:A64Root 'build\a64test.exe'
    if ($Jit) { $j = Join-Path $script:A64Root 'build\a64test-jit.exe'; if (Test-Path $j) { return $j } }
    if (-not (Test-Path $exe)) { throw 'build\a64test.exe nao existe (rode tools\build-a64test.ps1)' }
    return $exe
}
