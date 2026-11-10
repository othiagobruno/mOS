# mOS - slices arm64 das bibliotecas Mach-O que o mOS instala para os apps (tradutor arm64, docs/tradutor-arm64.md, frente
# LIBSYS) e os binários universais (x86_64 + arm64) nos mesmos caminhos da Apple: o carregador (kernel/compat/macho.c)
# escolhe o slice da arquitetura do processo, então um processo arm64 liga na libc++ arm64 sem caminho novo.
#   build\universal\usr\lib\libc++.1.dylib       (tools\build-libcxx.ps1 -Arch arm64)
#   build\universal\usr\lib\libsqlite3.dylib     (tools\build-sqlite.ps1 -Arch arm64)
#   build\universal\usr\lib\libicucore.A.dylib   (tools\build-icu.ps1 -Arch arm64; os dados icudt74l.dat são os mesmos)
#   build\universal\usr\lib\libz.1.dylib             (tools\build-zlib.ps1, os dois slices)
#   build\universal\usr\lib\libcurl.4.dylib          (tools\build-libcurl.ps1)
#   build\universal\usr\lib\libcompression.dylib     (tools\build-libcompression.ps1)
#   build\universal\System\Library\Frameworks\WebKit.framework\Versions\A\WebKit
# O build.ps1 copia o que existir aqui por cima das versões só x86_64.
#   .\tools\build-libs-arm64.ps1 [-Force]
param([switch]$Force)
$root = Split-Path $PSScriptRoot -Parent
$zig = Join-Path (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1).FullName 'zig.exe'
$u = "$root\build\universal"
$fail = 0
function Step($name, [scriptblock]$b) { & $b; if ($LASTEXITCODE -ne 0) { Write-Host "$name FALHOU" -ForegroundColor Red; $script:fail++ } }
function Lipo($dst, $x86, $arm) {
    if ((Test-Path $x86) -and (Test-Path $arm)) { & powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\lipo-mos.ps1" -Out $dst $x86 $arm }
    else { Write-Host "  (sem $(if (Test-Path $x86) { $arm } else { $x86 }): $dst fica de fora)" -ForegroundColor DarkGray }
}
$f = if ($Force) { @('-Force') } else { @() }

Step 'libc++ x86_64' { & "$PSScriptRoot\build-libcxx.ps1" -Arch x86_64 @f }
Step 'libc++ arm64' { & "$PSScriptRoot\build-libcxx.ps1" -Arch arm64 @f }
Lipo "$u\usr\lib\libc++.1.dylib" "$root\build\libcxx\libc++.1.dylib" "$root\build\libcxx-arm64\libc++.1.dylib"

Step 'sqlite x86_64' { & "$PSScriptRoot\build-sqlite.ps1" -Arch x86_64 @f }
Step 'sqlite arm64' { & "$PSScriptRoot\build-sqlite.ps1" -Arch arm64 @f }
Lipo "$u\usr\lib\libsqlite3.dylib" "$root\build\sqlite\libsqlite3.dylib" "$root\build\sqlite-arm64\libsqlite3.dylib"

Step 'ICU x86_64' { & "$PSScriptRoot\build-icu.ps1" -Arch x86_64 @f }
Step 'ICU arm64' { & "$PSScriptRoot\build-icu.ps1" -Arch arm64 @f }
Lipo "$u\usr\lib\libicucore.A.dylib" "$root\build\icu\libicucore.A.dylib" "$root\build\icu-arm64\libicucore.A.dylib"

# libz, libcurl e libcompression: os dois slices saem do fonte (sem bottle). O universal fica em /usr/lib.
Step 'zlib x86_64' { & "$PSScriptRoot\build-zlib.ps1" -Arch x86_64 @f }
Step 'zlib arm64' { & "$PSScriptRoot\build-zlib.ps1" -Arch arm64 @f }
Lipo "$u\usr\lib\libz.1.dylib" "$root\build\zlib-x86_64\libz.1.dylib" "$root\build\zlib-arm64\libz.1.dylib"
Step 'libcurl x86_64' { & "$PSScriptRoot\build-libcurl.ps1" -Arch x86_64 @f }
Step 'libcurl arm64' { & "$PSScriptRoot\build-libcurl.ps1" -Arch arm64 @f }
Lipo "$u\usr\lib\libcurl.4.dylib" "$root\build\curl-x86_64\libcurl.4.dylib" "$root\build\curl-arm64\libcurl.4.dylib"
Step 'libcompression x86_64' { & "$PSScriptRoot\build-libcompression.ps1" -Arch x86_64 @f }
Step 'libcompression arm64' { & "$PSScriptRoot\build-libcompression.ps1" -Arch arm64 @f }
Lipo "$u\usr\lib\libcompression.dylib" "$root\build\libcompression-x86_64\libcompression.dylib" "$root\build\libcompression-arm64\libcompression.dylib"

# WebKit.framework do mOS (o build.ps1 compila o x86_64 direto na imagem; aqui os dois slices)
$wk = "$root\userland\frameworks\WebKit\WebKit.m"
if (Test-Path $wk) {
    New-Item -ItemType Directory -Force "$root\build\webkit-arm64", "$root\build\webkit-x86" | Out-Null
    foreach ($a in @(@('x86_64-macos', "$root\build\webkit-x86\WebKit"), @('aarch64-macos', "$root\build\webkit-arm64\WebKit"))) {
        & $zig cc -target $a[0] -O2 -shared -x objective-c "-I$root\userland\sdk\include" -Wno-objc-root-class -Wno-objc-method-access `
            $wk -o $a[1] '-Wl,-install_name,/System/Library/Frameworks/WebKit.framework/Versions/A/WebKit' '-Wl,-undefined,dynamic_lookup' 2>&1 |
            Select-String ' error' | ForEach-Object { "  $($_.Line)" }
        if ($LASTEXITCODE -ne 0) { Write-Host "WebKit $($a[0]) FALHOU" -ForegroundColor Red; $fail++ }
    }
    Lipo "$u\System\Library\Frameworks\WebKit.framework\Versions\A\WebKit" "$root\build\webkit-x86\WebKit" "$root\build\webkit-arm64\WebKit"
}
if ($fail) { exit 1 }
