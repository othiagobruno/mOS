# mOS - compila o módulo Swift CryptoKit (userland\sdk\openswiftui\swift\CryptoKit) contra a stdlib completa e o
# Foundation do mOS (build\swift\openswiftui\Foundation), com as mesmas flags do Foundation. Saída em
# build\swift\openswiftui\CryptoKit (CryptoKit.swiftmodule + libCryptoKit.a): o tools\build-swiftui-app.ps1 já liga todo
# lib*.a dessa pasta, então `import CryptoKit` funciona nos apps SwiftUI do mOS. As primitivas são as do libSystem
# (userland\libsystem\cryptokit.c), resolvidas no carregamento.
# Pré-requisito: tools\build-openswiftui-deps.ps1 (Foundation).
$root = Split-Path $PSScriptRoot -Parent
$o = "$root\build\swift\openswiftui"
if (-not (Test-Path "$o\Foundation\Foundation.rsp")) { Write-Host 'Foundation do mOS ausente (rode tools\build-openswiftui-deps.ps1)' -ForegroundColor Yellow; exit 1 }
$tc = (Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1).FullName
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
$out = "$o\CryptoKit"
New-Item -ItemType Directory -Force $out | Out-Null
$base = Get-Content "$o\Foundation\Foundation.rsp"
$skipNext = $false
$a = New-Object System.Collections.Generic.List[string]
for ($i = 0; $i -lt $base.Count; $i++) {
    $line = $base[$i].Trim().Trim('"')
    if ($skipNext) { $skipNext = $false; continue }
    if ($line -ceq '-emit-module-path' -or $line -ceq '-o' -or $line -ceq '-module-name') { $skipNext = $true; continue }
    if ($line -like '*.swift') { continue }
    [void]$a.Add($line)
}
[void]$a.Add('-I'); [void]$a.Add("$o\Foundation")
[void]$a.Add('-module-name'); [void]$a.Add('CryptoKit')
$a.AddRange([string[]]@(Get-ChildItem "$root\userland\sdk\openswiftui\swift\CryptoKit" -Filter *.swift | ForEach-Object FullName))
[void]$a.Add('-emit-module-path'); [void]$a.Add("$out\CryptoKit.swiftmodule")
[void]$a.Add('-o'); [void]$a.Add("$out\CryptoKit_swift.o")
$rsp = "$out\CryptoKit.rsp"
($a | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join "`n" | Set-Content -Encoding ascii $rsp
$log = & "$tc\usr\bin\swift-frontend.exe" -frontend "@$rsp" 2>&1 | ForEach-Object { "$_" }
$code = $LASTEXITCODE
$log | Set-Content -Encoding utf8 "$out\build.log"
$err = $log | Select-String ' error: '
if ($code -ne 0 -or $err) { $err | Select-Object -First 25 | ForEach-Object { $_.Line }; Write-Host 'CryptoKit (Swift): FALHOU' -ForegroundColor Red; exit 1 }
Remove-Item "$out\libCryptoKit.a" -ErrorAction SilentlyContinue
& $zig ar rcs "$out\libCryptoKit.a" "$out\CryptoKit_swift.o" | Out-Null
Write-Host ("CryptoKit (Swift): ok -> libCryptoKit.a ({0:N0} KB)" -f ((Get-Item "$out\libCryptoKit.a").Length / 1KB)) -ForegroundColor Green
