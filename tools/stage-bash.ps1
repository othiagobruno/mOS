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
Copy-Item -Force (Join-Path $root $Script) "$usr\bash-teste.sh"
"Programa=/usr/local/bin/bash`nArgumentos=/usr/local/bash-teste.sh`nAtrasoMs=$AtrasoMs" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\sh.brew.bash.servico"
Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img com o bash do Homebrew rodando $Script no boot" -ForegroundColor Green
