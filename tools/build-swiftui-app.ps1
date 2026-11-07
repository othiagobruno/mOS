# mOS - liga um app que faz `import SwiftUI` (OpenSwiftUI) contra a stdlib completa.
# Uso: .\tools\build-swiftui-app.ps1 -Name Ola
param([string]$Name = 'Ola')
$root = Split-Path $PSScriptRoot -Parent
$tc = (Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1).FullName
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*')[0].FullName + '\zig.exe'
$full = "$root\build\swift\full"
$o = "$root\build\swift\openswiftui"
$srcDir = "$root\userland\samples-swiftui\$Name"
$src = @(Get-ChildItem $srcDir -Filter *.swift -Recurse | ForEach-Object FullName)
if (-not $src) { Write-Host "sem fontes em $srcDir" -ForegroundColor Red; exit 1 }
$out = "$root\build\swiftui-apps\$Name"
New-Item -ItemType Directory -Force "$out\vfs" | Out-Null
# Mesmos -I, defines e disponibilidades com que o módulo SwiftUI foi compilado.
$base = Get-Content "$o\SwiftUI\SwiftUI.rsp"
$skipNext = $false
$args = New-Object System.Collections.Generic.List[string]
for ($i = 0; $i -lt $base.Count; $i++) {
    $line = $base[$i].Trim().Trim('"')
    if ($skipNext) { $skipNext = $false; continue }
    if ($line -ceq '-emit-module-path' -or $line -ceq '-o' -or $line -ceq '-module-name') { $skipNext = $true; continue }
    if ($line -like '*.swift') { continue }
    [void]$args.Add($line)
}
[void]$args.Add('-I'); [void]$args.Add("$o\SwiftUI")
[void]$args.Add('-module-name'); [void]$args.Add($Name)
$args.AddRange([string[]]$src)
$obj = "$out\$Name.o"
[void]$args.Add('-o'); [void]$args.Add($obj)
$rsp = "$out\$Name.rsp"
($args | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join "`n" | Set-Content -Encoding ascii $rsp
Remove-Item $obj -ErrorAction SilentlyContinue
$log = & "$tc\usr\bin\swift-frontend.exe" -frontend "@$rsp" 2>&1 | ForEach-Object { "$_" }
$code = $LASTEXITCODE
$log | Set-Content -Encoding utf8 "$out\build.log"
$err = $log | Select-String ' error: '
if ($code -ne 0 -or $err -or -not (Test-Path $obj)) { $err | Select-Object -First 25 | ForEach-Object { $_.Line }; Write-Host 'compilação FALHOU' -ForegroundColor Red; exit 1 }
$libs = @(Get-ChildItem $o -Recurse -Filter lib*.a | ForEach-Object FullName)
$rt = @(Get-ChildItem "$full\runtime\*.o" | ForEach-Object FullName)
$pd = "$out\ProtocolDescriptor.o"
& $zig cc -target x86_64-macos -c "$root\userland\sdk\openswiftui\swift\COpenSwiftUI\ProtocolDescriptor.c" -o $pd
if ($LASTEXITCODE) { Write-Host 'descritores FALHOU' -ForegroundColor Red; exit 1 }
$bin = "$out\$Name"
& $zig c++ -target x86_64-macos $obj $pd '-Wl,--start-group' @libs '-Wl,--end-group' "$full\swiftCore.o" @rt -o $bin '-Wl,-undefined,dynamic_lookup' 2>&1 | Select-Object -First 40
if ($LASTEXITCODE) { Write-Host 'link FALHOU' -ForegroundColor Red; exit 1 }
Write-Host ("ok: {0} ({1:N0} KB)" -f $bin, ((Get-Item $bin).Length / 1KB))
