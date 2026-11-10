# mOS - libcurl 8.23.0 (licença curl, COPYING) como /usr/lib/libcurl.4.dylib.
# Fonte: https://curl.se/download/curl-8.23.0.tar.gz em .tools\curl-dl.
# tools\libcurl\curl_config.h liga HTTP, arquivo local e zlib, e deixa de fora TLS, LDAP,
# HTTP/2, HTTP/3, SSH, brotli, zstd, libidn e libpsl. A libz é a de tools\build-zlib.ps1.
#   .\tools\build-libcurl.ps1 -Arch x86_64 [-Force]
param([switch]$Force, [ValidateSet('x86_64', 'arm64')][string]$Arch = 'x86_64')
$root = Split-Path $PSScriptRoot -Parent
$zigDir = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
if (-not $zigDir) { Write-Host 'zig nao encontrado em .tools (zig.exe)' -ForegroundColor Red; exit 1 }
$zig = Join-Path $zigDir.FullName 'zig.exe'
$tgt = if ($Arch -eq 'arm64') { 'aarch64-macos.11.0' } else { 'x86_64-macos.11.0' }
$out = "$root\build\curl-$Arch"
$dylib = "$out\libcurl.4.dylib"
if ((Test-Path $dylib) -and -not $Force) { exit 0 }
$ver = '8.23.0'
$sha = 'e3de77cfa6367083c776a26647fc9f633b538c06c3baa73d931a252ac8f72b49'
$dl = "$root\.tools\curl-dl"
$tgz = "$dl\curl-$ver.tar.gz"
$src = "$dl\curl-$ver"
New-Item -ItemType Directory -Force $dl | Out-Null
$have = $false
if (Test-Path $tgz) {
    $got = (Get-FileHash -Algorithm SHA256 $tgz).Hash.ToLower()
    if ($got -eq $sha) { $have = $true } else { Remove-Item $tgz -Force }
}
if (-not $have) {
    Invoke-WebRequest -Uri "https://curl.se/download/curl-$ver.tar.gz" -OutFile $tgz -UseBasicParsing
    $got = (Get-FileHash -Algorithm SHA256 $tgz).Hash.ToLower()
    if ($got -ne $sha) { Write-Host "curl: hash $got != $sha" -ForegroundColor Red; exit 1 }
}
if (-not (Test-Path "$src\lib\easy.c")) {
    & tar -xzf $tgz -C $dl
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$src\lib\easy.c")) { Write-Host 'curl: extrair o fonte falhou' -ForegroundColor Red; exit 1 }
}
& "$PSScriptRoot\build-zlib.ps1" -Arch $Arch
if ($LASTEXITCODE -ne 0) { exit 1 }
$zlib = "$root\build\zlib-$Arch\libz.1.dylib"
$zsrc = Get-ChildItem "$root\.tools\zlib-dl" -Directory -Filter 'zlib-*' | Sort-Object Name | Select-Object -Last 1
if (-not (Test-Path $zlib) -or -not $zsrc) { Write-Host 'libcurl: falta a libz deste arch' -ForegroundColor Red; exit 1 }
$obj = "$out\obj"
New-Item -ItemType Directory -Force $obj | Out-Null
$cflags = @('-target', $tgt, '-O2', '-g0', '-fPIC', '-fvisibility=hidden',
    '-DBUILDING_LIBCURL', '-DHAVE_CONFIG_H', '-DCURL_HIDDEN_SYMBOLS',
    '-I', "$PSScriptRoot\libcurl", '-I', "$src\lib", '-I', "$src\include", '-I', $zsrc.FullName)
$objs = @()
foreach ($f in Get-ChildItem "$src\lib" -Recurse -Filter *.c) {
    $tag = $f.FullName.Substring(("$src\lib\").Length) -replace '[\\/]', '_'
    $o = "$obj\$tag.o"
    $objs += $o
    & $zig cc @cflags -c $f.FullName -o $o 2>&1 | Select-String 'error:' | Select-Object -First 8 | ForEach-Object { "  $($_.Line)" }
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $o)) { Write-Host "libcurl: $($f.Name) FALHOU" -ForegroundColor Red; exit 1 }
}
$utf8 = New-Object System.Text.UTF8Encoding $false
$rsp = "$out\link.rsp"
$lines = $objs | ForEach-Object { '"' + ($_ -replace '\\', '/') + '"' }
[IO.File]::WriteAllLines($rsp, [string[]]$lines, $utf8)
& $zig cc -target $tgt -shared -fvisibility=hidden -o $dylib "@$rsp" $zlib `
    '-Wl,-install_name,/usr/lib/libcurl.4.dylib' '-Wl,-compatibility_version,7.0.0' '-Wl,-current_version,8.23.0' `
    '-Wl,-undefined,dynamic_lookup' 2>&1 | Select-String ' error' | ForEach-Object { "  $_" }
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dylib)) { Write-Host 'libcurl.4.dylib FALHOU' -ForegroundColor Red; exit 1 }
Copy-Item "$src\COPYING" "$out\COPYING" -Force
Write-Host ("OK: $dylib ({0:N0} KiB, curl {1}, sem TLS)" -f ((Get-Item $dylib).Length / 1KB), $ver)
