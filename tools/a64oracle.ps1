# mOS - oráculo do tradutor arm64: roda um ELF de teste (o mesmo do a64test) no qemu-system-aarch64,
# em EL0 sob o monitor bare-metal (tools\a64oracle\mon.c), e imprime stdout/stderr/código de saída
# exatamente como o build\a64test.exe imprimiria. argv = {"prog"} (use a64test --argv0 prog).
#   powershell -File tools\a64oracle.ps1 build\a64tests\hello.elf [-Timeout 60]
param([Parameter(Mandatory = $true)][string]$Elf, [int]$Timeout = 60)
. "$PSScriptRoot\a64oracle\common.ps1"
$r = Invoke-A64Oracle $Elf $Timeout
[Console]::Out.Write($r.Out)
[Console]::Error.Write($r.Err)
if ($r.TimedOut) { [Console]::Error.WriteLine("a64oracle: tempo esgotado ($Timeout s)") }
exit $r.Code
