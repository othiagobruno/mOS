# mOS - compila e liga um programa Swift COM interoperabilidade ObjC (docs/swift-objc-runtime.md, fases 2-3), estático:
# objeto do app + stdlib com ObjC (build\swift\objc\swiftCore.o) + runtime C++ com ObjC (build\swift\objc\runtime\*.o)
# + libc++ do zig. A libobjc é a do libSystem do mOS (resolvida no carregamento).
# Uso: tools\link-swift-objc.ps1 -Out <executável> -Module <Nome> <fontes.swift...>
# -Dylib: em vez dos objetos, liga contra as dylibs de build\swift\objc\dylib\usr\lib\swift (tools\link-swift-dylibs-objc.ps1),
# como um binário da Apple; rodar com build\swift\objc\dylib como raiz do VFS do harness.
param([Parameter(Mandatory)][string]$Out, [string]$Module = 'App', [string[]]$Libs = @(), [string[]]$Inc = @(), [switch]$Dylib, [Parameter(ValueFromRemainingArguments)][string[]]$Sources, [string]$Arch = 'x86_64')
$root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\swift-arch.ps1" $Arch
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$swiftc = "$($tc.FullName)\usr\bin\swiftc.exe"
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Where-Object { Test-Path "$($_.FullName)\zig.exe" })[0].FullName + '\zig.exe'
$objc = "$SwiftBuild\objc"
$obj = [IO.Path]::ChangeExtension($Out, '.o')
$extra = @(if (($Sources | ForEach-Object { Get-Content $_ -Raw }) -join "`n" -match '@main') { '-parse-as-library' })
$log = & $swiftc -frontend -c -target $SwiftTriple -module-name $Module -O @extra -disable-objc-attr-requires-foundation-module `
    -I $objc -I "$objc\overlays" -I "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims" -module-cache-path "$objc\modcache" `
    -Xcc "-isystem$objc\sdk\include" -Xcc -DMOS_SWIFT_OBJC=1 -Xcc -Wno-nullability-completeness ($Inc | ForEach-Object { '-I', $_ }) @Sources -o $obj 2>&1 | ForEach-Object { "$_" }
$err = $log | Select-String ' error: '
if ($err -or -not (Test-Path $obj)) { $err | Select-Object -First 20 | ForEach-Object { $_.Line }; Write-Host 'compilação FALHOU' -ForegroundColor Red; exit 1 }
if ($Dylib) {
    $dl = @(Get-ChildItem "$objc\dylib\usr\lib\swift\*.dylib" | ForEach-Object FullName)
    & $zig cc -target $ZigTarget $obj @Libs @dl -o $Out "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" } | Select-Object -First 30
} else {
    $rt = @(Get-ChildItem "$objc\runtime\*.o" | ForEach-Object FullName) + @(Get-ChildItem "$objc\overlays\*.o" -ErrorAction SilentlyContinue | ForEach-Object FullName)
    & $zig c++ -target $ZigTarget $obj @Libs "$objc\swiftCore.o" @rt -o $Out "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" } | Select-Object -First 30
}
if ($LASTEXITCODE) { Write-Host 'link FALHOU' -ForegroundColor Red; exit 1 }
Write-Host ("ok: {0} ({1:N0} KB)" -f $Out, ((Get-Item $Out).Length / 1KB))
