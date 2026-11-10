# mOS - /usr/lib/libcompression.dylib
# ZLIB (deflate cru), LZ4, LZ4_RAW e LZFSE. LZMA fica de fora.
#   .\tools\build-libcompression.ps1 -Arch x86_64 [-Force]
param([switch]$Force, [ValidateSet('x86_64', 'arm64')][string]$Arch = 'x86_64')
$root = Split-Path $PSScriptRoot -Parent
$zigDir = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
if (-not $zigDir) { Write-Host 'zig nao encontrado em .tools (zig.exe)' -ForegroundColor Red; exit 1 }
$zig = Join-Path $zigDir.FullName 'zig.exe'
$src = "$root\userland\frameworks\libcompression\compression.c"
$lz4 = "$root\userland\third_party\lz4"
$lzfse = "$root\userland\third_party\lzfse"
if (-not (Test-Path $src)) { Write-Host 'falta compression.c' -ForegroundColor Red; exit 1 }
$tgt = if ($Arch -eq 'arm64') { 'aarch64-macos.11.0' } else { 'x86_64-macos.11.0' }
$out = "$root\build\libcompression-$Arch"
$dylib = "$out\libcompression.dylib"
if ((Test-Path $dylib) -and -not $Force) { exit 0 }
New-Item -ItemType Directory -Force $out | Out-Null
$files = @(
    $src,
    "$lz4\lz4.c", "$lz4\lz4hc.c", "$lz4\lz4frame.c", "$lz4\xxhash.c",
    "$lzfse\lzfse_decode.c", "$lzfse\lzfse_decode_base.c", "$lzfse\lzfse_encode.c",
    "$lzfse\lzfse_encode_base.c", "$lzfse\lzfse_fse.c",
    "$lzfse\lzvn_decode_base.c", "$lzfse\lzvn_encode_base.c"
)
& $zig cc -target $tgt -O2 -g0 -fPIC -shared @files -o $dylib `
    "-I$lz4" "-I$lzfse" `
    '-Wl,-install_name,/usr/lib/libcompression.dylib' '-Wl,-undefined,dynamic_lookup' 2>&1 |
    Select-String ' error' | ForEach-Object { "  $_" }
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dylib)) { Write-Host 'libcompression.dylib FALHOU' -ForegroundColor Red; exit 1 }
Write-Host ("OK: $dylib ({0:N0} KiB)" -f ((Get-Item $dylib).Length / 1KB))
