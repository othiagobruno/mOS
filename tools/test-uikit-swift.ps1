# mOS - teste do overlay Swift do UIKit do Catalyst (userland\tests-swiftobjc\uikit) no harness: liga o teste (estático,
# como os outros swiftobjc) ao UIKit.framework do Catalyst e monta uma raiz com o framework.
#   .\tools\test-uikit-swift.ps1   (antes: tools\build-frameworks-objc.ps1 -Only UIKit e tools\build-swift-uikit-objc.ps1)
$root = Split-Path $PSScriptRoot -Parent
$fwrel = 'System\iOSSupport\System\Library\Frameworks\UIKit.framework\Versions\A\UIKit'
$fw = "$root\build\swift\objc\dylib\$fwrel"
$t = "$root\build\swiftobjctest\uikit"
New-Item -ItemType Directory -Force (Split-Path "$t\$fwrel") | Out-Null
Copy-Item $fw "$t\$fwrel" -Force
Copy-Item $fw "$t\UIKit-link.dylib" -Force     # cópia com extensão para o ligador
& "$PSScriptRoot\link-swift-objc.ps1" -Out "$t\UIKitTest" -Module UIKitTest -Libs "$t\UIKit-link.dylib","$root\build\swift\objc\overlays\uikit\UIKit.o" -Sources "$root\userland\tests-swiftobjc\uikit\main.swift" 2>&1 |
    Select-String 'error|ok:' | Select-Object -First 10 | ForEach-Object { $_.Line }
if (-not (Test-Path "$t\UIKitTest")) { Write-Host 'link FALHOU' -ForegroundColor Red; exit 1 }
& "$root\build\machotest.exe" "$root\build\libSystem.mos" $t "$t\UIKitTest" 2>&1 | ForEach-Object { "$_" } | Select-String 'uikit|ok   |FALHA|exce|nÃ£o implementado|não implementado' | ForEach-Object { $_.Line }
