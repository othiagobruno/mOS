# mOS - teste dos pseudoterminais (userland\tests-pty) como PtyTest.app; com -OutDir, põe na imagem junto com o bash do
# Homebrew (e ls/cat/stty/tty/sleep do coreutils) e o abre sozinho no boot, com a saída no log serial.
#   .\build.ps1 -OutDir build-pty
#   .\tools\build-ptytest.ps1 -OutDir build-pty
#   .\run.ps1 -Headless -Image build-pty/mOS.img -Serial build-pty/serial.log -Memory 1024     (procure "ptytest:" no log)
# O mesmo caminho do node-pty do VS Code: posix_openpt + TIOCPTYGNAME + posix_spawn(POSIX_SPAWN_SETSID) de um
# "spawn-helper" que adota o escravo como terminal de controle e executa o filho; depois o bash interativo.
param([string]$OutDir = '', [string]$Dest = "$PSScriptRoot\..\build\ptytest", [string]$Bottles = 'build\brew\x', [int]$SizeMiB = 160,
      [int]$AtrasoMs = 4000)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'PtyTest.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 "$root\userland\tests-pty\main.c" -o "$out\PtyTest" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'ptytest: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>PtyTest</string>
	<key>CFBundleIdentifier</key><string>com.mos.ptytest</string>
	<key>CFBundleExecutable</key><string>PtyTest</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'PtyTest.app\Contents\Info.plist')
Write-Host "ptytest: $Dest\PtyTest.app" -ForegroundColor Green
if (-not $OutDir) { exit 0 }
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
Copy-Item -Recurse -Force (Join-Path $Dest 'PtyTest.app') "$esp\mOS\Aplicativos\"
# bash + readline + ncurses (terminfo do xterm-256color) + gettext, como o -ComBash do stage-vscode.ps1
$b = Join-Path $root $Bottles
$usr = "$esp\mOS\usr\local"
$falta = @('bash', 'readline', 'ncurses', 'gettext') | Where-Object { -not (Get-ChildItem "$b\$_" -Directory -ErrorAction SilentlyContinue) }
if ($falta) { Write-Host "sem os bottles $($falta -join ', ') em ${b}: só a parte sem bash vai rodar" -ForegroundColor Yellow }
else {
    $rl = (Get-ChildItem "$b\readline" -Directory | Select-Object -First 1).FullName
    $nc = (Get-ChildItem "$b\ncurses" -Directory | Select-Object -First 1).FullName
    $gt = (Get-ChildItem "$b\gettext" -Directory | Select-Object -First 1).FullName
    $bs = (Get-ChildItem "$b\bash" -Directory | Select-Object -First 1).FullName
    foreach ($f in 'readline', 'ncurses', 'gettext') { New-Item -ItemType Directory -Force "$usr\opt\$f\lib" | Out-Null }
    New-Item -ItemType Directory -Force "$usr\bin" | Out-Null
    Copy-Item -Force "$rl\lib\libreadline.8.dylib", "$rl\lib\libhistory.8.dylib" "$usr\opt\readline\lib\"
    Copy-Item -Force "$nc\lib\libncursesw.6.dylib" "$usr\opt\ncurses\lib\"
    Copy-Item -Force "$gt\lib\libintl.8.dylib" "$usr\opt\gettext\lib\"
    foreach ($t in 'x\xterm-256color', 'x\xterm', 'd\dumb', 'v\vt100') {
        if (Test-Path "$nc\share\terminfo\$t") {
            $dir = Split-Path "$usr\opt\ncurses\share\terminfo\$t" -Parent
            New-Item -ItemType Directory -Force $dir | Out-Null
            Copy-Item -Force "$nc\share\terminfo\$t" $dir
        }
    }
    Copy-Item -Force "$bs\bin\bash" "$usr\bin\bash"
    # coreutils: só ls/cat/stty/tty/sleep (gX em /usr/local/bin e o link X -> gX do mOS, como o gnubin do Homebrew)
    $cu = Get-ChildItem "$b\coreutils" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cu) {
        foreach ($n in 'ls', 'cat', 'stty', 'tty', 'sleep', 'uname') {
            Copy-Item -Force -LiteralPath "$($cu.FullName)\bin\g$n" -Destination "$usr\bin\g$n"
            [IO.File]::WriteAllText("$usr\bin\$n", "#mOS-link:g$n", [Text.Encoding]::ASCII)
        }
    }
    Write-Host "bash do Homebrew em /usr/local/bin/bash$(if ($cu) { ' (+ ls, cat, stty, tty, sleep)' })" -ForegroundColor Cyan
}
"Programa=/Aplicativos/PtyTest.app/Contents/MacOS/PtyTest`nAtrasoMs=$AtrasoMs" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.ptytest.servico"
Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
# imagem de teste: overlay de teste com o marcador que pula o assistente do Live (tools\overlay-teste.ps1)
. "$PSScriptRoot\overlay-teste.ps1"
$ovt = New-OverlayTeste $root $OutDir
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB --test-overlay $ovt
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img com PtyTest no boot" -ForegroundColor Green
