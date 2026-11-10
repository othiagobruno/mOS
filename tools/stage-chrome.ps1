# mOS - coloca o Google Chrome para macOS numa imagem de teste do mOS, para o porte (docs/porte-chrome.md)
#   .\build.ps1 -OutDir build-chrome
#   .\tools\stage-chrome.ps1 -App "<pasta>\Google Chrome.app" [-OutDir build-chrome] [-SizeMiB 1024] [-KernelTeste] [-Diagnostico] [-ComDmg <googlechrome.dmg>]
#   .\run.ps1 -Image build-chrome/mOS.img -Memory 3072      (no mOS: Launchpad > Google Chrome, ou no Terminal: run /Aplicativos/Chrome.app)
#
# Extrair o .app do .dmg no Windows, com o leitor de DMG do proprio mOS (tools/dmgtest.c, ver docs/porte-chrome.md).
# O que vai para /Aplicativos/Google Chrome.app:
#   - so a fatia x86_64 de cada binario universal (o framework cai de 507 para 264 MiB);
#   - Info.plist, icone, o framework, os helpers (o Chromium confere que existem), libvk_swiftshader/libvulkan
#     (GPU por software) e os recursos que o Chromium le ao iniciar (icudtl.dat, .pak, snapshot do V8),
#     com as traducoes en e pt_BR;
#   - fica de fora: GoogleUpdater, Widevine, liboptimization_guide, as outras 218 traducoes.
# O FAT32 nao tem links simbolicos: tudo fica em Versions/<versao>, onde o carregador do mOS procura.
# -KernelTeste: recompila o kernel com MAX_LOAD de 512 MiB (o limite padrao de fs/persist.c e 256 MiB por arquivo,
#   e o framework x86_64 tem 264 MiB).
# -Diagnostico: funcoes ausentes seguem com 0 (e sao registradas) e a saida dos apps vai ao log serial.
param([Parameter(Mandatory = $true)][string]$App, [string]$OutDir = 'build-chrome', [int]$SizeMiB = 1024,
      [switch]$KernelTeste, [switch]$Diagnostico, [string]$ComDmg = '', [switch]$AutoAbrir, [string]$Argumentos = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path (Resolve-Path $App).Path 'Contents'
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path -LiteralPath "$src\MacOS\Google Chrome")) { Write-Host "nao achei $src\MacOS\Google Chrome" -ForegroundColor Red; exit 1 }
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }

# caminhos absolutos com \\?\: os executaveis dos helpers "Aperitif" passam de 260 caracteres
# (".../Helpers/Google Chrome Helper (Aperitif Renderer).app/Contents/MacOS/...") e sumiam da copia
function Get-LongPath([string]$p) { if ($p.StartsWith('\\?\')) { $p } else { '\\?\' + $p } }

# --- fatia x86_64 de um Mach-O universal (copia como esta se ja for thin)
function Copy-X64([string]$from, [string]$to) {
    $from = Get-LongPath $from; $to = Get-LongPath $to
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($to)) | Out-Null
    $b = [IO.File]::ReadAllBytes($from)
    $be = { param($o) ([uint32]$b[$o] -shl 24) -bor ([uint32]$b[$o + 1] -shl 16) -bor ([uint32]$b[$o + 2] -shl 8) -bor [uint32]$b[$o + 3] }
    $magic = & $be 0
    if ($magic -ne [uint32]3405691582 -and $magic -ne [uint32]3405691583) { [IO.File]::WriteAllBytes($to, $b); return }
    $wide = $magic -eq [uint32]3405691583         # 0xCAFEBABF: fat_arch_64
    $n = & $be 4
    for ($i = 0; $i -lt $n; $i++) {
        $e = 8 + $i * ($(if ($wide) { 32 } else { 20 }))
        if ((& $be $e) -ne [uint32]16777223) { continue }   # CPU_TYPE_X86_64 (0x01000007)
        if ($wide) { $off = [uint64](& $be ($e + 8)) * 4294967296 + (& $be ($e + 12)); $len = [uint64](& $be ($e + 16)) * 4294967296 + (& $be ($e + 20)) }
        else { $off = [uint64](& $be ($e + 8)); $len = [uint64](& $be ($e + 12)) }
        $fs = [IO.File]::Create($to)
        try { $fs.Write($b, [int]$off, [int]$len) } finally { $fs.Close() }
        return
    }
    throw "sem fatia x86_64: $from"
}

$ver = (Get-ChildItem -LiteralPath "$src\Frameworks\Google Chrome Framework.framework\Versions" -Directory | Where-Object { $_.Name -match '^\d' } | Select-Object -First 1).Name
if (-not $ver) { Write-Host 'nao achei a versao do Google Chrome Framework' -ForegroundColor Red; exit 1 }
$app = "$esp\mOS\Aplicativos\Google Chrome.app"
$dst = "$app\Contents"
if (Test-Path -LiteralPath $app) { [IO.Directory]::Delete((Get-LongPath $app), $true) }
New-Item -ItemType Directory -Force "$dst\MacOS", "$dst\Resources" | Out-Null
Copy-Item -LiteralPath "$src\Info.plist" "$dst\Info.plist"
Copy-Item -LiteralPath "$src\PkgInfo" "$dst\PkgInfo" -ErrorAction SilentlyContinue
Copy-Item -LiteralPath "$src\Resources\app.icns" "$dst\Resources\app.icns"
Copy-X64 "$src\MacOS\Google Chrome" "$dst\MacOS\Google Chrome"

$fs = "Frameworks\Google Chrome Framework.framework\Versions\$ver"
Write-Host "Google Chrome Framework $ver (fatia x86_64)..." -ForegroundColor Cyan
Copy-X64 "$src\$fs\Google Chrome Framework" "$dst\$fs\Google Chrome Framework"
foreach ($lib in 'libvk_swiftshader.dylib', 'libvulkan.dylib', 'libaperitif.dylib', 'libchromecompaneros.dylib') {
    if (Test-Path -LiteralPath "$src\$fs\Libraries\$lib") { Copy-X64 "$src\$fs\Libraries\$lib" "$dst\$fs\Libraries\$lib" }
}
Get-ChildItem -LiteralPath "$src\$fs\Libraries" -File -Filter '*.json' | ForEach-Object { Copy-Item -LiteralPath $_.FullName "$dst\$fs\Libraries\" }
# helpers: Info.plist + executavel (o Chromium confere que existem; os processos auxiliares vem depois)
foreach ($h in Get-ChildItem -LiteralPath "$src\$fs\Helpers" -Directory -Filter 'Google Chrome Helper*.app') {
    $t = "$dst\$fs\Helpers\$($h.Name)\Contents"
    New-Item -ItemType Directory -Force "$t\MacOS" | Out-Null
    Copy-Item -LiteralPath "$($h.FullName)\Contents\Info.plist" "$t\Info.plist"
    $exe = @(Get-ChildItem -LiteralPath (Get-LongPath "$($h.FullName)\Contents\MacOS") -File)
    if (-not $exe) { throw "helper sem executavel (extracao com caminho longo?): $($h.Name)" }
    foreach ($x in $exe) { Copy-X64 "$($h.FullName)\Contents\MacOS\$($x.Name)" "$t\MacOS\$($x.Name)" }
}
foreach ($x in 'chrome_crashpad_handler', 'app_mode_loader', 'web_app_shortcut_copier') {
    if (Test-Path -LiteralPath "$src\$fs\Helpers\$x") { Copy-X64 "$src\$fs\Helpers\$x" "$dst\$fs\Helpers\$x" }
}
# recursos: tudo o que e arquivo (ICU, .pak, snapshot do V8; sem o snapshot arm64) + en e pt_BR
New-Item -ItemType Directory -Force "$dst\$fs\Resources" | Out-Null
Get-ChildItem -LiteralPath "$src\$fs\Resources" -File | Where-Object { $_.Name -notmatch 'arm64' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName "$dst\$fs\Resources\" }
foreach ($l in 'en.lproj', 'pt_BR.lproj') { if (Test-Path -LiteralPath "$src\$fs\Resources\$l") { Copy-Item -LiteralPath "$src\$fs\Resources\$l" "$dst\$fs\Resources\" -Recurse } }

# sem o modo experimental (dyld-experimental): o Chrome tem de abrir como uma instalacao normal; os frameworks que ele
# pede estao na lista do carregador (kernel/compat/macho.c, libsystem_dylib_ok)
Remove-Item -LiteralPath "$esp\mOS\Sistema\Biblioteca\dyld-experimental" -ErrorAction SilentlyContinue
if ($Diagnostico) {
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\dyld-registrar" -Value '1' -Encoding ascii
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
}
# -AutoAbrir: o init abre o Chrome sozinho no boot (testes no QEMU sem depender do mouse)
$svc = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.google.chrome.teste.servico"
if ($AutoAbrir) {
    $t = "Programa=/Aplicativos/Google Chrome.app/Contents/MacOS/Google Chrome`n"
    if ($Argumentos) { $t += "Argumentos=$Argumentos`n" }      # ex.: '--disable-gpu --in-process-gpu' (até 127 caracteres)
    [IO.File]::WriteAllText($svc, $t, [Text.Encoding]::ASCII)
}
elseif (Test-Path -LiteralPath $svc) { Remove-Item -LiteralPath $svc }
# imagem de teste: overlay de teste com o marcador que pula o assistente do Live (tools\overlay-teste.ps1); o .dmg do
# -ComDmg vai nele, porque o mkimg recusa /mOS/Users na esp
. "$PSScriptRoot\overlay-teste.ps1"
$ovt = New-OverlayTeste $root $OutDir
if ($ComDmg) {
    New-Item -ItemType Directory -Force "$ovt\mOS\Users\Visitante\Downloads" | Out-Null
    Copy-Item -LiteralPath $ComDmg "$ovt\mOS\Users\Visitante\Downloads\"
}
$mb = (Get-ChildItem -LiteralPath (Get-LongPath $app) -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("Google Chrome.app: {0:N1} MiB" -f $mb) -ForegroundColor Cyan

if ($KernelTeste) {
    Write-Host '==> kernel de teste (MAX_LOAD 512 MiB)' -ForegroundColor Cyan
    if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
    $mosZigPick = Resolve-MosZig $root
    if (-not $mosZigPick) { exit 1 }
    $zig = $mosZigPick.Path
    Push-Location $root
    $sources = Get-ChildItem kernel -Recurse -Include *.c, *.S | ForEach-Object { $_.FullName.Substring($root.Length + 1) }
    $cflags = @('-target', 'x86_64-uefi-msvc', '-O2', '-g0', '-ffreestanding', '-fno-stack-protector', '-fno-stack-check', '-mno-stack-arg-probe',
        '-mno-red-zone', '-fshort-wchar', '-fno-sanitize=all', '-fno-strict-aliasing', '-nostdlib', '-Ikernel/include', '-I.', '-std=gnu11',
        '-Wno-unused-command-line-argument', '-DMAX_LOAD=(512ull<<20)')
    $ErrorActionPreference = 'Continue'
    # cópia do libSystem que vai embutido neste kernel: outras sessões recompilam build\libSystem.mos, e os
    # endereços do log (machocheck --sym) só batem com a cópia exata
    Copy-Item "$root\build\libSystem.mos" "$root\$OutDir\libSystem-embutido.mos" -Force
    & $zig cc @cflags @sources -o "$OutDir/BOOTX64-teste.EFI" 2>&1 | ForEach-Object { "$_" }
    $ok = $LASTEXITCODE -eq 0
    Pop-Location
    if (-not $ok) { Write-Host 'kernel de teste FAILED' -ForegroundColor Red; exit 1 }
    Copy-Item "$root\$OutDir\BOOTX64-teste.EFI" "$esp\EFI\BOOT\BOOTX64.EFI" -Force
}
. (Join-Path $PSScriptRoot 'stage-xpc.ps1')
Copy-MosBundleServices $src $dst
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img (boote com -Memory 3072: os arquivos do app ficam na RAM)" -ForegroundColor Green
