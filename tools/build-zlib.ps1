# mOS - zlib (licença zlib: .tools\zlib-dl\zlib-1.3.2, o zlib-1.3.2.tar.gz oficial de github.com/madler/zlib) como dylib
# Mach-O: build\zlib-<arch>\libz.1.dylib. O slice x86_64 que vai para a imagem continua o bottle do Homebrew (a mesma
# versão); este script existe para o slice arm64 do tradutor (tools\build-libs-arm64.ps1 junta os dois em
# /usr/local/opt/zlib/lib/libz.1.dylib, para onde o carregador manda /usr/lib/libz.1.dylib).
#   .\tools\build-zlib.ps1 -Arch arm64 [-Force]
param([switch]$Force, [ValidateSet('x86_64', 'arm64')][string]$Arch = 'arm64')
$root = Split-Path $PSScriptRoot -Parent
$zig = Join-Path (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1).FullName 'zig.exe'
$src = Get-ChildItem "$root\.tools\zlib-dl" -Directory -Filter 'zlib-*' -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
if (-not $src) { Write-Host 'falta o fonte da zlib em .tools\zlib-dl (zlib-1.3.2.tar.gz de github.com/madler/zlib/releases)' -ForegroundColor Yellow; exit 0 }
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
Copy-Item "$($src.FullName)\LICENSE" "$out\LICENSE" -Force -ErrorAction SilentlyContinue
Write-Host ("OK: $dylib ({0:N0} KiB, {1})" -f ((Get-Item $dylib).Length / 1KB), $src.Name)
