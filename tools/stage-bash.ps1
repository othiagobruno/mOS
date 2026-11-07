# mOS - põe o bash do Homebrew (bottle Intel "sonoma") e as dylibs dele na imagem e roda um script de teste no boot.
#   .\build.ps1 -OutDir build-exec
#   .\tools\stage-bash.ps1 [-OutDir build-exec] [-Bottles build\brew\x]
#   .\run.ps1 -Headless -Image build-exec/mOS.img -Serial build-exec/serial.log
# Os bottles vêm de ghcr.io/homebrew/core (bash 5.3.9, readline 8.3.3, ncurses 6.6, gettext 1.0), ver docs/porte-brew.md.
# Ficam em /usr/local/opt/<nome> (o loader troca @@HOMEBREW_PREFIX@@ por /usr/local) e o bash em /usr/local/bin.
param([string]$OutDir = 'build-exec', [string]$Bottles = 'build\brew\x', [string]$Script = 'tools\testdata\bash-teste.sh',
      [int]$SizeMiB = 256, [int]$AtrasoMs = 6000)
$root = Split-Path $PSScriptRoot -Parent
$esp = Join-Path $root "$OutDir\esp"
$b = Join-Path $root $Bottles
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
$usr = "$esp\mOS\usr\local"
foreach ($f in 'readline', 'ncurses', 'gettext') {
    $src = Get-ChildItem "$b\$f" -Directory | Select-Object -First 1
    if (-not $src) { Write-Host "falta o bottle $f em $b" -ForegroundColor Red; exit 1 }
    New-Item -ItemType Directory -Force "$usr\opt\$f\lib" | Out-Null
}
$rl = (Get-ChildItem "$b\readline" -Directory | Select-Object -First 1).FullName
$nc = (Get-ChildItem "$b\ncurses" -Directory | Select-Object -First 1).FullName
$gt = (Get-ChildItem "$b\gettext" -Directory | Select-Object -First 1).FullName
$bs = (Get-ChildItem "$b\bash" -Directory | Select-Object -First 1).FullName
Copy-Item -Force "$rl\lib\libreadline.8.dylib", "$rl\lib\libhistory.8.dylib" "$usr\opt\readline\lib\"
Copy-Item -Force "$nc\lib\libncursesw.6.dylib" "$usr\opt\ncurses\lib\"
Copy-Item -Force "$gt\lib\libintl.8.dylib" "$usr\opt\gettext\lib\"
# terminfo mínimo: o readline lê a descrição do terminal no início
foreach ($t in 'x\xterm-256color', 'x\xterm', 'd\dumb', 'v\vt100') {
    $from = "$nc\share\terminfo\$t"
    if (Test-Path $from) {
        $dir = Split-Path "$usr\opt\ncurses\share\terminfo\$t" -Parent
        New-Item -ItemType Directory -Force $dir | Out-Null
        Copy-Item -Force $from $dir
    }
}
New-Item -ItemType Directory -Force "$usr\bin" | Out-Null
Copy-Item -Force "$bs\bin\bash" "$usr\bin\bash"
# link simbólico do mOS: arquivo "#mOS-link:<alvo>" (o execve do kernel e o stat do libSystem seguem)
function New-MosLink([string]$at, [string]$target) { [IO.File]::WriteAllText($at, "#mOS-link:$target", [Text.Encoding]::ASCII) }
# coreutils 9.11 (+ gmp): gX em /usr/local/bin, e X -> gX (o que o libexec/gnubin do Homebrew faz)
$cu = Get-ChildItem "$b\coreutils" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
if ($cu) {
    $gmp = (Get-ChildItem "$b\gmp" -Directory | Select-Object -First 1).FullName
    New-Item -ItemType Directory -Force "$usr\opt\gmp\lib" | Out-Null
    Copy-Item -Force "$gmp\lib\libgmp.10.dylib" "$usr\opt\gmp\lib\"
    foreach ($f in Get-ChildItem "$($cu.FullName)\bin" -File) {
        Copy-Item -Force -LiteralPath $f.FullName -Destination "$usr\bin\$($f.Name)"
        if ($f.Name.StartsWith('g') -and $f.Name -ne 'g[') { New-MosLink "$usr\bin\$($f.Name.Substring(1))" $f.Name }
    }
    New-MosLink "$usr\bin\[" 'g['
}
# portable-ruby 3.4.5 (+ zlib, libffi do Homebrew no lugar de /usr/lib/libz e /usr/lib/libffi)
$pr = Get-ChildItem "$b\portable-ruby" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
if ($pr) {
    foreach ($f in 'zlib', 'libffi') { New-Item -ItemType Directory -Force "$usr\opt\$f\lib" | Out-Null }
    Copy-Item -Force (Get-ChildItem "$b\zlib\*\lib\libz.1.*.dylib" | Select-Object -First 1).FullName "$usr\opt\zlib\lib\libz.1.dylib"
    Copy-Item -Force (Get-ChildItem "$b\libffi\*\lib\libffi.8.dylib" | Select-Object -First 1).FullName "$usr\opt\libffi\lib\"
    $dst = "$usr\opt\portable-ruby"
    if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
    New-Item -ItemType Directory -Force $dst | Out-Null
    foreach ($d in 'bin', 'lib') {
        robocopy "$($pr.FullName)\$d" "$dst\$d" /E /XD '*.dSYM' /NFL /NDL /NJH /NJS /NP | Out-Null
    }
    New-MosLink "$usr\bin\ruby" '/usr/local/opt/portable-ruby/bin/ruby'
}
# demais fórmulas baixadas (tools/fetch-bottles.sh): bin, lib (só .dylib e módulos) e libexec em /usr/local/opt/<f>,
# com links em /usr/local/bin. Os bottles Intel já usam o prefixo /usr/local; as dylibs ficam no opt de verdade
# porque o loader não segue links ao carregar dependências.
$feitas = @('bash', 'readline', 'ncurses', 'gettext', 'coreutils', 'gmp', 'portable-ruby', 'zlib', 'libffi')
$gnu = @{ 'gsed' = 'sed'; 'ggrep' = 'grep'; 'gegrep' = 'egrep'; 'gfgrep' = 'fgrep'; 'gtar' = 'tar'; 'gfind' = 'find'; 'gxargs' = 'xargs'; 'gawk' = 'awk' }   # o "gnubin" de cada uma
foreach ($fd in Get-ChildItem $b -Directory) {
    if ($feitas -contains $fd.Name) { continue }
    $src = Get-ChildItem $fd.FullName -Directory | Select-Object -First 1
    if (-not $src) { continue }
    $dst = "$usr\opt\$($fd.Name)"
    New-Item -ItemType Directory -Force $dst | Out-Null
    if (Test-Path "$($src.FullName)\lib") {
        robocopy "$($src.FullName)\lib" "$dst\lib" '*.dylib' /S /XD '*.dSYM' 'pkgconfig' 'cmake' /NFL /NDL /NJH /NJS /NP | Out-Null
    }
    if (Test-Path "$($src.FullName)\share\ca-certificates") {   # cert.pem onde o openssl@3 (OPENSSLDIR) procura
        New-Item -ItemType Directory -Force "$usr\etc\openssl@3", "$usr\etc\ca-certificates" | Out-Null
        Copy-Item -Force "$($src.FullName)\share\ca-certificates\cacert.pem" "$usr\etc\openssl@3\cert.pem"
        Copy-Item -Force "$($src.FullName)\share\ca-certificates\cacert.pem" "$usr\etc\ca-certificates\cert.pem"
    }
    if (Test-Path "$($src.FullName)\bin") {
        New-Item -ItemType Directory -Force "$dst\bin" | Out-Null
        foreach ($f in Get-ChildItem "$($src.FullName)\bin" -File) {
            Copy-Item -Force -LiteralPath $f.FullName -Destination "$dst\bin\$($f.Name)"
            New-MosLink "$usr\bin\$($f.Name)" "/usr/local/opt/$($fd.Name)/bin/$($f.Name)"
            if ($gnu.ContainsKey($f.Name)) { New-MosLink "$usr\bin\$($gnu[$f.Name])" "/usr/local/opt/$($fd.Name)/bin/$($f.Name)" }
        }
    }
    if (Test-Path "$($src.FullName)\libexec") {
        # git-core: ~180 comandos são cópias do binário git (hardlinks no bottle); viram links do mOS para ele
        $main = Get-Item -LiteralPath "$($src.FullName)\bin\git" -ErrorAction SilentlyContinue
        foreach ($f in Get-ChildItem "$($src.FullName)\libexec" -Recurse -File) {
            $rel = $f.FullName.Substring("$($src.FullName)\libexec".Length)
            $to = "$dst\libexec$rel"
            New-Item -ItemType Directory -Force (Split-Path $to -Parent) | Out-Null
            if ($main -and $f.Name -ne 'git' -and $f.Length -eq $main.Length) { New-MosLink $to "/usr/local/opt/$($fd.Name)/bin/git" }
            else { Copy-Item -Force -LiteralPath $f.FullName -Destination $to }
        }
    }
    if ($fd.Name -eq 'git' -and (Test-Path "$($src.FullName)\share\git-core")) {
        robocopy "$($src.FullName)\share\git-core" "$dst\share\git-core" /E /NFL /NDL /NJH /NJS /NP | Out-Null
    }
}
# caminhos fixos do macOS que scripts e programas chamam direto: o tar roda o compressor com
# /bin/sh -c, o brew começa com #!/bin/bash, e muitos scripts usam #!/usr/bin/env
New-Item -ItemType Directory -Force "$esp\mOS\bin", "$esp\mOS\usr\bin" | Out-Null
New-MosLink "$esp\mOS\bin\sh" '/usr/local/bin/bash'
New-MosLink "$esp\mOS\bin\bash" '/usr/local/bin/bash'
if (Test-Path "$usr\bin\genv") { New-MosLink "$esp\mOS\usr\bin\env" '/usr/local/bin/genv' }
Copy-Item -Force (Join-Path $root $Script) "$usr\bash-teste.sh"
"Programa=/usr/local/bin/bash`nArgumentos=/usr/local/bash-teste.sh`nAtrasoMs=$AtrasoMs" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\sh.brew.bash.servico"
Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img com o bash do Homebrew rodando $Script no boot" -ForegroundColor Green
