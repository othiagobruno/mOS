# mOS - teste da amostra Catalyst (userland\tests-catalyst) contra o UIKit.framework do mOS (userland\frameworks\UIKit):
# compila o app ligado ao UIKit do Catalyst (/System/iOSSupport/...), monta uma raiz para o harness com o framework e roda.
# O NSApplication fica no laço; o watchdog do harness encerra depois de alguns segundos. A tela vai para tela.bmp.
#   .\tools\test-catalyst.ps1   (antes: .\tools\build-frameworks-objc.ps1 -Only UIKit)
$root = Split-Path $PSScriptRoot -Parent
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*')[0].FullName + '\zig.exe'
$fwrel = 'System\iOSSupport\System\Library\Frameworks\UIKit.framework\Versions\A\UIKit'
$fw = "$root\build\swift\objc\dylib\$fwrel"
if (-not (Test-Path $fw)) { Write-Host 'falta o UIKit.framework (tools\build-frameworks-objc.ps1 -Only UIKit)' -ForegroundColor Red; exit 1 }
$t = "$root\build\catalysttest"
New-Item -ItemType Directory -Force (Split-Path "$t\$fwrel") | Out-Null
Copy-Item $fw "$t\$fwrel" -Force
# o binário do framework não tem extensão: uma cópia .dylib para o ligador (o install name dentro dele continua o do Catalyst)
Copy-Item $fw "$t\UIKit-link.dylib" -Force
& $zig cc -target x86_64-macos -O1 -fobjc-arc -x objective-c "-I$root\userland\sdk\include" "$root\userland\tests-catalyst\main.m" -x none "$t\UIKit-link.dylib" -o "$t\Catalyst" '-Wl,-undefined,dynamic_lookup' 2>&1 | Select-String ' error' | ForEach-Object { "  $($_.Line)" }
if (-not (Test-Path "$t\Catalyst")) { Write-Host 'compilação FALHOU' -ForegroundColor Red; exit 1 }
$env:MACHOTEST_WATCHDOG = '6000'
$env:MACHOTEST_DUMP = "$t\tela.bmp"
& "$root\build\machotest.exe" "$root\build\libSystem.mos" $t "$t\Catalyst" 2>&1 | ForEach-Object { "$_" } | Select-String 'catalyst|ok   |FALHA|UIKit|exce' | ForEach-Object { $_.Line }
Remove-Item Env:\MACHOTEST_WATCHDOG, Env:\MACHOTEST_DUMP
if (Test-Path "$t\tela.bmp") { Write-Host "tela: $t\tela.bmp" }
