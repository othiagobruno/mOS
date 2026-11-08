# mOS - compara o tradutor arm64 com o oráculo QEMU num ELF de teste: a64test (interpretador e,
# com -Jit, também o JIT) × tools\a64oracle.ps1. Compara stdout, stderr e código de saída.
#   powershell -File tools\a64cmp.ps1 build\a64tests\hello.elf [-Jit] [-Timeout 60] [-Quiet]
# Sai com 0 se tudo bate, 1 se diverge, 2 em erro.
param([Parameter(Mandatory = $true)][string]$Elf, [switch]$Jit, [int]$Timeout = 60, [switch]$Quiet)
. "$PSScriptRoot\a64oracle\common.ps1"

function Show-Diff([string]$name, [string]$a, [string]$b, [string]$la, [string]$lb) {
    $x = $a -split "`r?`n"; $y = $b -split "`r?`n"
    $n = [Math]::Max($x.Count, $y.Count); $shown = 0
    Write-Host "  $name difere:" -ForegroundColor Yellow
    for ($i = 0; $i -lt $n -and $shown -lt 12; $i++) {
        $p = if ($i -lt $x.Count) { $x[$i] } else { '<fim>' }
        $q = if ($i -lt $y.Count) { $y[$i] } else { '<fim>' }
        if ($p -ne $q) { Write-Host ("    {0,-6} {1}" -f $la, $p); Write-Host ("    {0,-6} {1}" -f $lb, $q); $shown++ }
    }
}

$elfPath = (Resolve-Path $Elf).Path
$oracle = Invoke-A64Oracle $elfPath $Timeout
if ($oracle.TimedOut) { Write-Host "a64cmp: oraculo estourou o tempo em $Elf" -ForegroundColor Red; exit 2 }
$engines = @(@{ Name = 'interp'; Args = @('--argv0', 'prog', $elfPath); Exe = (Get-A64Test) })
if ($Jit) { $engines += @{ Name = 'jit'; Args = @('--jit', '--argv0', 'prog', $elfPath); Exe = (Get-A64Test -Jit) } }
$fail = $false
foreach ($e in $engines) {
    $r = Invoke-A64Capture $e.Exe $e.Args $Timeout
    $ok = (-not $r.TimedOut) -and $r.Out -eq $oracle.Out -and $r.Err -eq $oracle.Err -and $r.Code -eq $oracle.Code
    if ($ok) { if (-not $Quiet) { Write-Host "IGUAL  $($e.Name)  $(Split-Path $Elf -Leaf)  (saida $($r.Code))" -ForegroundColor Green }; continue }
    $fail = $true
    Write-Host "DIFERE $($e.Name)  $(Split-Path $Elf -Leaf)" -ForegroundColor Red
    if ($r.TimedOut) { Write-Host "  $($e.Name) estourou o tempo ($Timeout s)" }
    if ($r.Code -ne $oracle.Code) { Write-Host "  codigo de saida: $($e.Name)=$($r.Code) qemu=$($oracle.Code)" }
    if ($r.Err -ne $oracle.Err) { Show-Diff 'stderr' $r.Err $oracle.Err $e.Name 'qemu' }
    if ($r.Out -ne $oracle.Out) { Show-Diff 'stdout' $r.Out $oracle.Out $e.Name 'qemu' }
}
if ($fail) { exit 1 } else { exit 0 }
