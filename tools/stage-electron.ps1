# mOS - põe um app Electron qualquer (Slack, Cursor, ...) numa imagem do mOS, para testar o caminho da janela
# (CoreAnimation/IOSurface, docs/porte-vscode.md). Genérico a partir do stage-vscode.ps1 (que é só do VS Code).
#   .\build.ps1 -OutDir build-electron
#   .\tools\stage-electron.ps1 -App "build\apps\x\slack-x64\Slack\Slack.app" -OutDir build-electron -Autostart -Diagnostico -CaLog
#   .\run.ps1 -Image build-electron/mOS.img -Memory 2700      (ou tools\qemu-test.ps1 -OutDir build-electron ...)
# Copia: Info.plist, o executável, os binários dos frameworks (Versions/A), os apps auxiliares "<Nome> Helper*.app",
# os recursos do Electron Framework (ICU, snapshot do V8, .pak; traduções en e pt_BR), libffmpeg, e do Resources do app
# só app.asar (+ app.asar.unpacked até -MaxModuloMiB por arquivo) ou a pasta app (apps sem asar, como o Cursor).
# -CaLog liga /Sistema/Biblioteca/ca-log: o QuartzCore registra no log serial (com -Diagnostico) cada IOSurface posto num
# layer, cada CAContext publicado e cada quadro composto na janela, com a contagem de pixels visíveis.
param([Parameter(Mandatory = $true)][string]$App, [string]$OutDir = 'build-electron', [int]$SizeMiB = 768,
      [switch]$Autostart, [switch]$Diagnostico, [switch]$CaLog, [int]$MaxModuloMiB = 16,
      [string]$Args0 = '--in-process-gpu --disable-gpu --no-sandbox --enable-logging --v=0', [string]$Tracepoints = '')
$root = Split-Path $PSScriptRoot -Parent
$App = (Get-Item -LiteralPath $App).FullName
$src = Join-Path $App 'Contents'
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
$exe = $null
$plText = [IO.File]::ReadAllText("$src\Info.plist")
if ($plText -match '<key>CFBundleExecutable</key>\s*<string>([^<]+)</string>') { $exe = $Matches[1] }
if (-not $exe) { $exe = (Get-ChildItem -LiteralPath "$src\MacOS" -File | Select-Object -First 1).Name }
$name = [IO.Path]::GetFileNameWithoutExtension($App)
$dstApp = "$esp\mOS\Aplicativos\$name.app"
$dst = "$dstApp\Contents"
Remove-Item -Recurse -Force -LiteralPath $dstApp -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$dst\MacOS" | Out-Null
Copy-Item -LiteralPath "$src\Info.plist" "$dst\Info.plist"
Copy-Item -LiteralPath "$src\MacOS\$exe" "$dst\MacOS\$exe"
foreach ($fwDir in Get-ChildItem -LiteralPath "$src\Frameworks" -Directory -Filter '*.framework') {
    $fw = $fwDir.Name -replace '\.framework$', ''          # BaseName de pasta mantém a extensão no PS 5.1
    $s = "$($fwDir.FullName)\Versions\A"
    if (-not (Test-Path -LiteralPath "$s\$fw")) { continue }
    $t = "$dst\Frameworks\$fw.framework\Versions\A"
    New-Item -ItemType Directory -Force $t | Out-Null
    Copy-Item -LiteralPath "$s\$fw" "$t\$fw"
}
foreach ($h in Get-ChildItem -LiteralPath "$src\Frameworks" -Directory -Filter '* Helper*.app') {
    $t = "$dst\Frameworks\$($h.Name)\Contents"
    New-Item -ItemType Directory -Force "$t\MacOS" | Out-Null
    Copy-Item -LiteralPath "$($h.FullName)\Contents\Info.plist" "$t\Info.plist"
    Get-ChildItem -LiteralPath "$($h.FullName)\Contents\MacOS" -File | Copy-Item -Destination "$t\MacOS\"
}
# recursos do Electron (sem links simbólicos no FAT32: "Electron Framework.framework\Resources" é onde o Chromium procura)
$res = "Frameworks\Electron Framework.framework\Resources"
New-Item -ItemType Directory -Force "$dst\$res" | Out-Null
$resSrc = "$src\Frameworks\Electron Framework.framework\Versions\A\Resources"
if (-not (Test-Path -LiteralPath $resSrc)) { $resSrc = "$src\Frameworks\Electron Framework.framework\Resources" }
Get-ChildItem -LiteralPath $resSrc -File | Copy-Item -Destination "$dst\$res\"
foreach ($l in 'en.lproj', 'pt_BR.lproj') { if (Test-Path -LiteralPath "$resSrc\$l") { Copy-Item -Recurse -LiteralPath "$resSrc\$l" "$dst\$res\" } }
$lib = "Frameworks\Electron Framework.framework\Versions\A\Libraries"
if (Test-Path -LiteralPath "$src\$lib") {
    New-Item -ItemType Directory -Force "$dst\$lib" | Out-Null
    Get-ChildItem -LiteralPath "$src\$lib" -File | Copy-Item -Destination "$dst\$lib\"
}
# Helpers do framework (chrome_crashpad_handler): o crashpad o inicia pelo caminho sem Versions/A, que no macOS é um
# link simbólico; sem ele o Slack (que liga o crashReporter) parava num FATAL de posix_spawnp
$hlp = "$src\Frameworks\Electron Framework.framework\Versions\A\Helpers"
if (Test-Path -LiteralPath $hlp) {
    $th = "$dst\Frameworks\Electron Framework.framework\Helpers"
    New-Item -ItemType Directory -Force $th | Out-Null
    Get-ChildItem -LiteralPath $hlp -File | Copy-Item -Destination "$th\"
}
# código do app
New-Item -ItemType Directory -Force "$dst\Resources" | Out-Null
foreach ($f in 'app.asar', 'electron.icns') { if (Test-Path -LiteralPath "$src\Resources\$f") { Copy-Item -LiteralPath "$src\Resources\$f" "$dst\Resources\" } }
$extra = @()
if (Test-Path -LiteralPath "$src\Resources\app.asar.unpacked") { $extra += 'app.asar.unpacked' }
if (-not (Test-Path -LiteralPath "$src\Resources\app.asar") -and (Test-Path -LiteralPath "$src\Resources\app")) { $extra += 'app' }
foreach ($d in $extra) {
    $u = (Get-Item -LiteralPath "$src\Resources\$d").FullName
    foreach ($f in Get-ChildItem -LiteralPath $u -Recurse -File -ErrorAction SilentlyContinue) {
        if ($f.Length -gt $MaxModuloMiB * 1MB) { continue }
        $t = "$dst\Resources\$d" + $f.FullName.Substring($u.Length)
        New-Item -ItemType Directory -Force (Split-Path $t) -ErrorAction SilentlyContinue | Out-Null
        try { [IO.File]::Copy('\\?\' + $f.FullName, '\\?\' + $t, $true) } catch { }
    }
}
Set-Content -Path "$esp\mOS\Sistema\Biblioteca\dyld-experimental" -Value $name -Encoding ascii
New-Item -ItemType Directory -Force "$esp\mOS\Sistema\Biblioteca\LaunchDaemons" | Out-Null
$svc = "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.electron-teste.servico"
if ($Autostart) { "Programa=/Aplicativos/$name.app/Contents/MacOS/$exe`nArgumentos=$Args0`nAtrasoMs=3000" | Set-Content -Encoding ascii $svc }
else { Remove-Item $svc -ErrorAction SilentlyContinue }
if ($Diagnostico) {
    foreach ($f in 'dyld-registrar', 'stderr-serial', 'despejar-threads') { Set-Content -Path "$esp\mOS\Sistema\Biblioteca\$f" -Value '1' -Encoding ascii }
}
if ($CaLog) { Set-Content -Path "$esp\mOS\Sistema\Biblioteca\ca-log" -Value '1' -Encoding ascii }
else { Remove-Item "$esp\mOS\Sistema\Biblioteca\ca-log" -ErrorAction SilentlyContinue }
# -Tracepoints: como no stage-vscode.ps1, "processo|imagem|0xdesl[*N][@reg],..." (várias linhas separadas por ';')
$tpFile = "$esp\mOS\Sistema\Biblioteca\diag-tracepoints"
if ($Tracepoints) { ($Tracepoints -split ';') -join "`n" | Set-Content -Path $tpFile -Encoding ascii }
else { Remove-Item $tpFile -ErrorAction SilentlyContinue }
$mb = (Get-ChildItem -LiteralPath $dstApp -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("{0}.app ({1}): {2:N1} MiB" -f $name, $exe, $mb) -ForegroundColor Cyan
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img" -ForegroundColor Green
