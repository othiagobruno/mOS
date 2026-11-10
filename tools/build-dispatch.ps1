# mOS - módulo Swift `Dispatch` (API da Apple) sobre o libdispatch em C do libSystem, para o Swift completo.
# Fonte: userland\sdk\openswiftui\swift\Dispatch  Saída: build\swift\openswiftui\Dispatch\{Dispatch.swiftmodule, libDispatch.a}
$root = Split-Path $PSScriptRoot -Parent
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$swiftc = "$($tc.FullName)\usr\bin\swiftc.exe"
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
$out = "$root\build\swift\openswiftui\Dispatch"
New-Item -ItemType Directory -Force $out | Out-Null
$src = @(Get-ChildItem "$root\userland\sdk\openswiftui\swift\Dispatch\*.swift" | ForEach-Object FullName)
$log = & $swiftc -frontend -c -target x86_64-apple-macos26 -module-name Dispatch -parse-as-library -swift-version 5 -O -disable-objc-interop `
    -enable-library-evolution -I "$root\build\swift\full" -I "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims" `
    -Xcc -Wno-nullability-completeness -emit-module-path "$out\Dispatch.swiftmodule" -o "$out\Dispatch.o" @src 2>&1 | ForEach-Object { "$_" }
$err = $log | Select-String ' error: '
if ($err -or -not (Test-Path "$out\Dispatch.o")) { $err | Select-Object -First 20 | ForEach-Object { $_.Line }; Write-Host 'Dispatch FALHOU' -ForegroundColor Red; exit 1 }
if (Test-Path "$out\libDispatch.a") { Remove-Item "$out\libDispatch.a" }
& $zig ar rcs "$out\libDispatch.a" "$out\Dispatch.o"
Write-Host "Dispatch ok: $out\libDispatch.a"
