# mOS - zlib (licença zlib) como /usr/lib/libz.1.dylib na imagem.
# Fonte oficial: zlib-1.3.2.tar.gz de github.com/madler/zlib/releases, em .tools\zlib-dl.
# Os dois slices (x86_64 e arm64) saem daqui. O build.ps1 copia o x86_64 para a imagem;
# tools\build-libs-arm64.ps1 junta o universal em usr\lib\libz.1.dylib. Sem bottle do Homebrew.
#   .\tools\build-zlib.ps1 -Arch x86_64 [-Force]
param([switch]$Force, [ValidateSet('x86_64', 'arm64')][string]$Arch = 'x86_64')
$root = Split-Path $PSScriptRoot -Parent
$zigDir = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
if (-not $zigDir) { Write-Host 'zig nao encontrado em .tools (zig.exe)' -ForegroundColor Red; exit 1 }
$zig = Join-Path $zigDir.FullName 'zig.exe'
$ver = '1.3.2'
$sha = 'bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16'
$dl = "$root\.tools\zlib-dl"
$tgz = "$dl\zlib-$ver.tar.gz"
$marker = "$dl\zlib-$ver\zlib.h"
New-Item -ItemType Directory -Force $dl | Out-Null
$have = $false
if (Test-Path $tgz) {
    $got = (Get-FileHash -Algorithm SHA256 $tgz).Hash.ToLower()
    if ($got -eq $sha) { $have = $true } else { Remove-Item $tgz -Force }
}
if (-not $have) {
    Invoke-WebRequest -Uri "https://github.com/madler/zlib/releases/download/v$ver/zlib-$ver.tar.gz" -OutFile $tgz -UseBasicParsing
    $got = (Get-FileHash -Algorithm SHA256 $tgz).Hash.ToLower()
    if ($got -ne $sha) { Write-Host "zlib: hash $got != $sha" -ForegroundColor Red; exit 1 }
}
if (-not (Test-Path $marker)) {
    & tar -xzf $tgz -C $dl
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $marker)) { Write-Host 'zlib: extrair o fonte falhou' -ForegroundColor Red; exit 1 }
}
$src = Get-Item "$dl\zlib-$ver"
$tgt = if ($Arch -eq 'arm64') { 'aarch64-macos.11.0' } else { 'x86_64-macos.11.0' }
$out = "$root\build\zlib-$Arch"
$dylib = "$out\libz.1.dylib"
if ((Test-Path $dylib) -and -not $Force) { exit 0 }
New-Item -ItemType Directory -Force $out | Out-Null
$c = 'adler32', 'compress', 'crc32', 'deflate', 'gzclose', 'gzlib', 'gzread', 'gzwrite', 'infback', 'inffast', 'inflate', 'inftrees', 'trees', 'uncompr', 'zutil' |
    ForEach-Object { "$($src.FullName)\$_.c" }
& $zig cc -target $tgt -O2 -g0 -fPIC -shared -DHAVE_UNISTD_H -DHAVE_STDARG_H -D_LARGEFILE64_SOURCE=1 "-I$($src.FullName)" @c -o $dylib `
    '-Wl,-install_name,/usr/lib/libz.1.dylib' '-Wl,-compatibility_version,1.0.0' '-Wl,-current_version,1.3.2' 2>&1 |
    Select-String ' error' | ForEach-Object { "  $_" }
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dylib)) { Write-Host 'libz.1.dylib FALHOU' -ForegroundColor Red; exit 1 }
Copy-Item "$($src.FullName)\LICENSE" "$out\LICENSE" -Force
Write-Host ("OK: $dylib ({0:N0} KiB, zlib {1})" -f ((Get-Item $dylib).Length / 1KB), $ver)
