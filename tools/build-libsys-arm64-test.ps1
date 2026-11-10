# mOS - teste da fronteira app <-> libSystem do tradutor arm64 (frente LIBSYS, docs/tradutor-arm64.md):
# userland\tests-a64libsys\abi.mm (Objective-C++) compilado como app do macOS (zig c++ -target aarch64-macos, a ABI da Apple) e também
# x86_64 para comparar, e rodado no machotest (o x86_64 direto, o arm64 pelo runtime do tradutor).
#   .\tools\build-libsys-arm64-test.ps1            # compila os dois e roda
#   .\tools\build-libsys-arm64-test.ps1 -NoRun
param([switch]$NoRun, [int]$Timeout = 120)
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
$out = "$root\build\a64libsys-test"
New-Item -ItemType Directory -Force $out | Out-Null
$src = "$root\userland\tests-a64libsys\abi.mm"
$fail = 0
foreach ($t in @(@('aarch64-macos.12.0', 'libsys-abi-arm64'), @('x86_64-macos.12.0', 'libsys-abi-x86'))) {
    & $zig c++ -target $t[0] -O1 -fblocks -fobjc-exceptions -std=c++17 -x objective-c++ $src -o "$out\$($t[1])" '-Wl,-undefined,dynamic_lookup' 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host "$($t[1]) FALHOU" -ForegroundColor Red; $fail++ }
}
# dylibs.cpp: ligado nas bibliotecas do disco (slices arm64 de tools\build-libs-arm64.ps1), carregadas dos universais
$dl = @("$root\build\libcxx-arm64\libc++.1.dylib", "$root\build\sqlite-arm64\libsqlite3.dylib", "$root\build\icu-arm64\libicucore.A.dylib", "$root\build\zlib-arm64\libz.1.dylib")
$haveDylibs = -not ($dl | Where-Object { -not (Test-Path $_) }) -and (Test-Path "$root\build\universal\usr\lib")
if ($haveDylibs) {
    & $zig c++ -target aarch64-macos.12.0 -O1 -std=c++17 -c "$root\userland\tests-a64libsys\dylibs.cpp" -o "$out\dylibs.o" 2>&1 | ForEach-Object { "$_" }
    & $zig c++ -target aarch64-macos.12.0 -nostdlib++ "$out\dylibs.o" @dl -o "$out\libsys-dylibs-arm64" 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host 'libsys-dylibs-arm64 FALHOU' -ForegroundColor Red; $fail++ }
} else { Write-Host '  (sem os slices arm64/universais: tools\build-libs-arm64.ps1; o teste das dylibs fica de fora)' -ForegroundColor DarkGray }
if ($fail) { exit 1 }
if ($NoRun) { exit 0 }

# harness: machotest com o VFS do runtime (o libSystem arm64 e o a64rt.mos em /Sistema/Biblioteca/a64)
$vfs = "$root\build\a64rt-test\vfs"
$a64 = "$vfs\Sistema\Biblioteca\a64"
New-Item -ItemType Directory -Force $a64 | Out-Null
foreach ($f in @('a64rt.mos', 'libSystem-arm64.mos')) { if (-not (Test-Path "$root\build\$f")) { Write-Host "falta build\$f" -ForegroundColor Red; exit 1 } }
Copy-Item "$root\build\a64rt.mos" "$a64\a64rt.mos" -Force
Copy-Item "$root\build\libSystem-arm64.mos" "$a64\libSystem.mos" -Force
if (-not (Test-Path "$root\build\machotest.exe")) { Write-Host 'falta build\machotest.exe (build\a64rt-test\run.sh compila)' -ForegroundColor Red; exit 1 }
if ($haveDylibs) {
    New-Item -ItemType Directory -Force "$vfs\usr\lib", "$vfs\usr\share\icu" | Out-Null
    Copy-Item "$root\build\universal\usr" "$vfs\" -Recurse -Force             # /usr/lib (libz, libcurl, libcompression quando o universal existe)
    Copy-Item "$root\build\icu\icudt74l.dat" "$vfs\usr\share\icu\" -Force -ErrorAction SilentlyContinue
}
$rc = 0
$runs = @('libsys-abi-x86', 'libsys-abi-arm64') + $(if ($haveDylibs) { @('libsys-dylibs-arm64') } else { @() })
foreach ($b in $runs) {
    $p = Start-Process -FilePath "$root\build\machotest.exe" -ArgumentList @("`"$root\build\libSystem.mos`"", "`"$vfs`"", "`"$out\$b`"") -NoNewWindow -PassThru `
        -RedirectStandardOutput "$out\$b.log" -RedirectStandardError "$out\$b.err"
    $null = $p.Handle
    if (-not $p.WaitForExit($Timeout * 1000)) { $p.Kill(); Write-Host "${b}: passou de $Timeout s" -ForegroundColor Red; $rc++ }
    Get-Content "$out\$b.log", "$out\$b.err" -ErrorAction SilentlyContinue | Where-Object { $_ -match 'ok |FALHA|falhas|macho|erro|panic|sinal' }
    # o x86_64 é referência no mesmo libSystem: falha aqui é do userland, não do tradutor
    if ($p.ExitCode -ne 0) { if ($b -like "*arm64") { $rc++ } else { Write-Host "  (x86_64 de referência com $($p.ExitCode) falha(s): pendências do libSystem x86)" -ForegroundColor Yellow } }
}
exit $rc
