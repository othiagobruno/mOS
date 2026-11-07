# mOS - compila o teste de IPv6 dos apps (userland\tests-net6) como Net6Test.app e, com -OutDir, põe na imagem
# para abrir sozinho no boot com a saída no log serial. O user-net do QEMU já vem com IPv6 (ipv6=on é o padrão):
# prefixo fec0::/64 por RA, gateway e DNS em fec0::2/fec0::3; uma conexão para [fec0::2]:porta chega ao ::1 do host.
#   .\build.ps1 -OutDir build-net6
#   .\tools\build-nettest6.ps1 -OutDir build-net6 [-Nome example.com] [-Alvo fec0::2] [-Porta 8086]
#   (no host: um servidor HTTP em [::1]:8086, p.ex. tools\testdata\http6-servidor.ps1)
#   .\run.ps1 -Headless -Image build-net6/mOS.img -Serial build-net6/serial.log      (procure "net6test:" no log)
param([string]$OutDir = '', [string]$Dest = "$PSScriptRoot\..\build\net6test", [string]$Nome = 'example.com', [string]$Alvo = 'fec0::2', [int]$Porta = 8086, [int]$SizeMiB = 128)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
$out = Join-Path $Dest 'Net6Test.app\Contents\MacOS'
New-Item -ItemType Directory -Force $out | Out-Null
& $zig cc -target x86_64-macos -O1 "$root\userland\tests-net6\main.c" -o "$out\Net6Test" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host 'net6test: FAILED' -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>Net6Test</string>
	<key>CFBundleIdentifier</key><string>com.mos.net6test</string>
	<key>CFBundleExecutable</key><string>Net6Test</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii (Join-Path $Dest 'Net6Test.app\Contents\Info.plist')
Write-Host "net6test: $Dest\Net6Test.app" -ForegroundColor Green
if ($OutDir) {
    $esp = Join-Path $root "$OutDir\esp"
    if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
    Copy-Item -Recurse -Force (Join-Path $Dest 'Net6Test.app') "$esp\mOS\Aplicativos\"
    # o init abre o teste 8 s depois do boot (tempo do DHCP); a saída dos apps vai ao log serial
    "Programa=/Aplicativos/Net6Test.app/Contents/MacOS/Net6Test`nArgumentos=$Nome $Alvo $Porta`nAtrasoMs=8000" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.net6test.servico"
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
    & (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
    if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
    Write-Host "OK: $OutDir\mOS.img com Net6Test no boot" -ForegroundColor Green
}
