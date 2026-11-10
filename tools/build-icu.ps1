# mOS - ICU4C (Unicode License, permissiva: .tools\icu-dl\icu, o icu4c-74_2-src.tgz do github.com/unicode-org/icu) como a
# libicucore do macOS: build\icu\libicucore.A.dylib (Mach-O x86_64, install name /usr/lib/libicucore.A.dylib) e os dados
# em build\icu\icudt74l.dat, que o build.ps1 põe na imagem em /usr/lib e /usr/share/icu.
#
# Por quê: o Claude Code (o binário único do Bun com o JavaScriptCore, docs/compat-apps.md) importa 184 funções da
# libicucore (Intl, normalização, quebra de texto, fusos: uloc_*, ucol_*, ubrk_*, udat_*, unumf_*...). Até aqui o
# libSystem do mOS só tinha o uidna_* (IDN do curl), e o "claude --version" terminava em "símbolo não implementado:
# _uloc_setDefault". A libicucore da Apple exporta a API C do ICU com os nomes sem versão (uloc_open, não uloc_open_74):
# U_DISABLE_RENAMING faz o mesmo. Uma biblioteca só com common + i18n (U_COMBINED_IMPLEMENTATION) e o stubdata: os
# dados vêm do icudt74l.dat em /usr/share/icu (ICU_DATA_DIR), mapeado com mmap. A libc++ é a do disco
# (build\libcxx\libc++.1.dylib, tools\build-libcxx.ps1), como nos módulos nativos dos apps.
#
#   .\tools\build-icu.ps1             (só recompila se faltar o dylib; sem o código-fonte em .tools\icu-dl, só avisa)
#   .\tools\build-icu.ps1 -Force
#   .\tools\build-icu.ps1 -Arch arm64 (build\icu-arm64, contra build\libcxx-arm64: slice arm64 do tradutor, tools\build-libs-arm64.ps1)
param([switch]$Force, [int]$Jobs = 8, [ValidateSet('x86_64', 'arm64')][string]$Arch = 'x86_64')
$tgt = if ($Arch -eq 'arm64') { 'aarch64-macos.12.0' } else { 'x86_64-macos.12.0' }
$sfx = if ($Arch -eq 'arm64') { '-arm64' } else { '' }
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
$src = "$root\.tools\icu-dl\icu\source"
if (-not (Test-Path "$src\common\uloc.cpp")) { Write-Host 'falta o ICU4C em .tools\icu-dl\icu (icu4c-74_2-src.tgz)' -ForegroundColor Yellow; exit 0 }
$cxx = "$root\build\libcxx$sfx\libc++.1.dylib"
if (-not (Test-Path $cxx)) { Write-Host "falta $cxx (tools\build-libcxx.ps1 -Arch $Arch)" -ForegroundColor Red; exit 1 }
$out = "$root\build\icu$sfx"
$dylib = "$out\libicucore.A.dylib"
$dat = "$out\icudt74l.dat"
if ((Test-Path $dylib) -and (Test-Path $dat) -and -not $Force) { exit 0 }
New-Item -ItemType Directory -Force "$out\obj" | Out-Null
Write-Host '==> Building libicucore.A.dylib (ICU4C 74.2, Mach-O)' -ForegroundColor Cyan

# definições num cabeçalho incluído em todo arquivo (sem aspas na linha de comando)
$cfg = "$out\icu_mos_config.h"
@'
/* mOS: o ICU como a libicucore do macOS (tools/build-icu.ps1) */
#define U_COMBINED_IMPLEMENTATION 1      /* common + i18n numa biblioteca, tudo exportado */
#define U_DISABLE_RENAMING 1             /* uloc_open, não uloc_open_74 (como a libicucore da Apple) */
#define U_USING_ICU_NAMESPACE 0
#define U_HAVE_STRTOD_L 0                /* sem strtod_l/newlocale do libSystem */
#define U_ENABLE_DYLOAD 0
#define ICU_DATA_DIR "/usr/share/icu"    /* icudt74l.dat, mapeado com mmap */
'@ | Set-Content -Encoding ascii $cfg
# o putil.cpp inclui <tzfile.h> no macOS (TZDIR e TZDEFAULT); o zig não traz esse cabeçalho
New-Item -ItemType Directory -Force "$out\inc" | Out-Null
"#define TZDIR `"/usr/share/zoneinfo`"`n#define TZDEFAULT `"/etc/localtime`"`n" | Set-Content -Encoding ascii "$out\inc\tzfile.h"

$common = @('-target', $tgt, '-std=c++17', '-O2', '-g0', '-fPIC', '-fvisibility=default',
            '-Wno-deprecated-declarations', '-include', $cfg, "-I$out\inc", "-I$src\common", "-I$src\i18n")
$srcs = @(Get-ChildItem "$src\common\*.cpp", "$src\i18n\*.cpp") + @(Get-Item "$src\stubdata\stubdata.cpp")

# cada arquivo exige o *_IMPLEMENTATION da própria pasta (o #error do ICU), além do combinado do cabeçalho
$procs = New-Object System.Collections.ArrayList
$objs = @()
foreach ($s in $srcs) {
    $dir = Split-Path (Split-Path $s.FullName -Parent) -Leaf
    $o = "$out\obj\" + $dir + "_" + $s.BaseName + ".o"
    $objs += $o
    if ((Test-Path $o) -and -not $Force -and (Get-Item $o).LastWriteTime -gt $s.LastWriteTime) { continue }
    $impl = if ($dir -eq 'i18n') { '-DU_I18N_IMPLEMENTATION' } else { '-DU_COMMON_IMPLEMENTATION' }
    while (@($procs | Where-Object { $_ -and -not $_.HasExited }).Count -ge $Jobs) { Start-Sleep -Milliseconds 100 }
    $cargs = $common + @($impl, '-c', $s.FullName, '-o', $o) | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }
    $p = Start-Process -FilePath $zig -ArgumentList (@('c++') + $cargs) -NoNewWindow -PassThru -RedirectStandardError "$o.err"
    if ($p) { [void]$procs.Add($p) }
}
# espera com prazo (WaitForExit sem prazo chegou a ficar parado depois de todos os compiladores terminarem)
$limit = (Get-Date).AddMinutes(30)
while (@($procs | Where-Object { $_ -and -not $_.HasExited }).Count -gt 0 -and (Get-Date) -lt $limit) { Start-Sleep -Milliseconds 250 }
$bad = $objs | Where-Object { -not (Test-Path $_) }
if ($bad) {
    foreach ($b in $bad | Select-Object -First 8) { Write-Host "falhou: $b" -ForegroundColor Red; Get-Content "$b.err" -TotalCount 6 }
    exit 1
}
# a libc++ do disco (o mesmo arquivo que vai para /usr/lib/libc++.1.dylib), não a estática do zig
# objetos por arquivo de resposta: com os caminhos de build\icu-arm64 a linha de comando passava do limite do Windows
[IO.File]::WriteAllLines("$out\link.rsp", ($objs | ForEach-Object { '"' + ($_ -replace '\\', '/') + '"' }))
& $zig c++ -target $tgt -shared -nostdlib++ -o $dylib "@$out\link.rsp" $cxx '-Wl,-install_name,/usr/lib/libicucore.A.dylib' `
    '-Wl,-compatibility_version,1.0.0' '-Wl,-current_version,74.2.0' 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dylib) -or (Get-Item $dylib).Length -eq 0) { Write-Host 'libicucore.A.dylib link FALHOU' -ForegroundColor Red; exit 1 }
Copy-Item "$src\data\in\icudt74l.dat" $dat -Force
Copy-Item "$root\.tools\icu-dl\icu\LICENSE" "$out\LICENSE" -Force -ErrorAction SilentlyContinue
Remove-Item "$out\obj\*.err" -ErrorAction SilentlyContinue
Write-Host ("OK: build\icu\libicucore.A.dylib ({0:N0} KiB) e icudt74l.dat ({1:N0} KiB)" -f ((Get-Item $dylib).Length / 1KB), ((Get-Item $dat).Length / 1KB))
