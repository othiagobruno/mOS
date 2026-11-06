# mOS - compila e liga um programa em Swift COMPLETO (não Embedded) para o mOS:
# objeto do app + stdlib (build\swift\full\swiftCore.o) + runtime C++ (build\swift\full\runtime\*.o) + libc++ do zig.
# Uso: tools\link-swift-full.ps1 -Out <executável> -Module <Nome> <fontes.swift...>
param([Parameter(Mandatory)][string]$Out, [string]$Module = 'App', [string[]]$Libs = @(), [string[]]$Inc = @(), [Parameter(ValueFromRemainingArguments)][string[]]$Sources)
$root = Split-Path $PSScriptRoot -Parent
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$swiftc = "$($tc.FullName)\usr\bin\swiftc.exe"
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*')[0].FullName + '\zig.exe'
$full = "$root\build\swift\full"
$obj = [IO.Path]::ChangeExtension($Out, '.o')
$extra = @(if (($Sources | ForEach-Object { Get-Content $_ -Raw }) -join "`n" -match '@main') { '-parse-as-library' })
$log = & $swiftc -frontend -c -target x86_64-apple-macos26 -module-name $Module -O -disable-objc-interop @extra `
    -I $full -I "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims" -I "$root\userland\sdk\swift\MOSBridge" `
    -Xcc -Wno-nullability-completeness ($Inc | ForEach-Object { '-I', $_ }) @Sources -o $obj 2>&1 | ForEach-Object { "$_" }
$err = $log | Select-String ' error: '
if ($err -or -not (Test-Path $obj)) { $err | Select-Object -First 20 | ForEach-Object { $_.Line }; Write-Host 'compilação FALHOU' -ForegroundColor Red; exit 1 }
$rt = Get-ChildItem "$full\runtime\*.o" | ForEach-Object FullName
& $zig c++ -target x86_64-macos $obj @Libs "$full\swiftCore.o" @rt -o $Out "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" } | Select-Object -First 30
if ($LASTEXITCODE) { Write-Host 'link FALHOU' -ForegroundColor Red; exit 1 }
Write-Host ("ok: {0} ({1:N0} KB)" -f $Out, ((Get-Item $Out).Length / 1KB))
