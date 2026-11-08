# mOS - slice arm64 da cadeia Swift (tradutor arm64, docs/tradutor-arm64.md, frente LIBSYS) e os binários universais
# (x86_64 + arm64) nos mesmos caminhos da Apple: /usr/lib/swift/libswift*.dylib, /System/Library/Frameworks/<X>.framework
# (Combine, SwiftUI, os de userland\frameworks) e /System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib.
# O carregador (kernel/compat/macho.c) escolhe o slice do processo, então um processo arm64 liga na libswiftCore arm64 sem
# caminho novo. Cada script da cadeia aceita -Arch arm64 (tools\swift-arch.ps1): mesmo fonte, alvo arm64-apple-macos26 /
# aarch64-macos, saída em build\swift-arm64\... (a saída x86_64 em build\swift\... não muda).
#   .\tools\build-swift-arm64.ps1               # compila o arm64 (o que faltar) e junta
#   .\tools\build-swift-arm64.ps1 -Force         # recompila tudo do arm64
#   .\tools\build-swift-arm64.ps1 -SoJuntar      # só junta (o build.ps1 usa este: não compila nada)
#   .\tools\build-swift-arm64.ps1 -De Foundation # a partir de uma etapa
# Etapas: Stdlib Runtime Overlays Concurrency Foundation Combine Frameworks UIKit SwiftUI Link
# Saída universal: build\universal-swift\usr\lib\swift, build\universal-swift\System\... (o build.ps1 -ComSwift copia na imagem;
# sem -ComSwift a imagem base fica como antes, sem dylibs Swift: o sistema de arquivos da VM mora na RAM).
param([switch]$Force, [switch]$SoJuntar, [string]$De, [string[]]$Pular = @())
$root = Split-Path $PSScriptRoot -Parent
$x86 = "$root\build\swift\objc\dylib"
$arm = "$root\build\swift-arm64\objc\dylib"
$u = "$root\build\universal-swift"   # à parte: ~110 MB, só entra na imagem com build.ps1 -ComSwift
$fail = 0

if (-not $SoJuntar) {
    $a = "$root\build\swift-arm64\objc"
    $ov = "$a\overlays"
    # [nome, já feito?, comando]
    $steps = @(
        @('Stdlib', (Test-Path "$a\swiftCore.o"), { & "$PSScriptRoot\build-swift-full-objc.ps1" -Arch arm64 }),
        @('Runtime', ((Get-ChildItem "$a\runtime\*.o" -ErrorAction SilentlyContinue).Count -gt 80), { & "$PSScriptRoot\build-swift-runtime-objc.ps1" -Arch arm64 }),
        @('Overlays', (Test-Path "$ov\Dispatch.o"), { & "$PSScriptRoot\build-swift-overlays-objc.ps1" -Arch arm64 }),
        @('Concurrency', (Test-Path "$ov\_Concurrency.o"), { & "$PSScriptRoot\build-swift-concurrency-objc.ps1" -Arch arm64 }),
        # o Foundation e o UniformTypeIdentifiers (que importa o Foundation) depois dele
        @('Foundation', (Test-Path "$ov\Foundation.o"), { & "$PSScriptRoot\build-swift-foundation-objc.ps1" -Arch arm64; if (-not $LASTEXITCODE) { & "$PSScriptRoot\build-swift-overlays-objc.ps1" -Arch arm64 -Only UniformTypeIdentifiers } }),
        # o Combine religa as dylibs (Dispatch ganha o Scheduler)
        @('Combine', (Test-Path "$ov\Combine.o"), { & "$PSScriptRoot\build-swift-combine-objc.ps1" -Arch arm64 }),
        @('Frameworks', (Test-Path "$a\frameworks-obj"), { & "$PSScriptRoot\build-frameworks-objc.ps1" -Arch arm64 }),
        @('UIKit', (Test-Path "$ov\uikit\UIKit.o"), { & "$PSScriptRoot\build-swift-uikit-objc.ps1" -Arch arm64 }),
        @('SwiftUI', (Test-Path "$arm\System\Library\Frameworks\SwiftUI.framework\Versions\A\SwiftUI"), {
            # os objetos C/C++ da variante nativa (Compute, OpenRenderBox, OpenSwiftUI_SPI, ctfont) em arm64, depois o framework
            & "$PSScriptRoot\build-openswiftui-deps.ps1" -Arch arm64 -Repo Compute, OpenRenderBox, CoreText
            if (-not $LASTEXITCODE) { & "$PSScriptRoot\build-swiftui-objc.ps1" -Arch arm64 } }),
        @('Link', $false, { & "$PSScriptRoot\link-swift-dylibs-objc.ps1" -Arch arm64 })
    )
    $on = -not $De
    foreach ($s in $steps) {
        if ($De -and $s[0] -eq $De) { $on = $true }
        if (-not $on -or $Pular -contains $s[0]) { continue }
        if ($s[1] -and -not $Force -and -not $De) { Write-Host "$($s[0]) arm64: já feito" -ForegroundColor DarkGray; continue }
        Write-Host "==> $($s[0]) arm64" -ForegroundColor Cyan
        $t0 = Get-Date
        $global:LASTEXITCODE = 0
        & $s[2]
        if ($LASTEXITCODE) { Write-Host "$($s[0]) arm64 FALHOU ($LASTEXITCODE)" -ForegroundColor Red; $fail++ }
        else { Write-Host ("$($s[0]) arm64: ok ({0}s)" -f [int]((Get-Date) - $t0).TotalSeconds) }
    }
}

# ---- universais: todo Mach-O fino que existe nas duas árvores (mesmo caminho relativo)
if (-not (Test-Path $x86) -or -not (Test-Path $arm)) { Write-Host "falta $(if (Test-Path $x86) { $arm } else { $x86 }): Swift universal fica de fora" -ForegroundColor Yellow; exit $(if ($SoJuntar) { 0 } else { 1 }) }
function Is-MachO64($p) {
    $fs = [IO.File]::OpenRead($p)
    try { $b = New-Object byte[] 4; $n = $fs.Read($b, 0, 4) } finally { $fs.Close() }
    return $n -eq 4 -and [BitConverter]::ToUInt32($b, 0) -eq 0xFEEDFACFL
}
$n = 0; $only = @()
foreach ($f in Get-ChildItem $x86 -Recurse -File) {
    $rel = $f.FullName.Substring($x86.Length)
    $a = "$arm$rel"
    if (-not (Is-MachO64 $f.FullName)) { continue }
    if (-not (Test-Path $a)) { $only += $rel; continue }
    $dst = "$u$rel"
    # só refaz quando um dos slices mudou
    if ((Test-Path $dst) -and (Get-Item $dst).LastWriteTime -ge (Get-Item $f.FullName).LastWriteTime -and (Get-Item $dst).LastWriteTime -ge (Get-Item $a).LastWriteTime) { $n++; continue }
    & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\lipo-mos.ps1" -Out $dst $f.FullName $a | Out-Null
    if ($LASTEXITCODE) { Write-Host "lipo FALHOU: $rel" -ForegroundColor Red; $fail++ } else { $n++ }
}
Write-Host "Swift universal: $n binários em build\universal-swift$(if ($only) { "; só x86_64 (sem slice arm64): $($only -join ', ')" })"
if ($fail) { exit 1 }
