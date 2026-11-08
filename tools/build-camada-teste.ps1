# mOS - compila o app de teste do QuartzCore/IOSurface (userland\testes\CamadaIOSurface) como bundle .app
#   .\tools\build-camada-teste.ps1 [-Dest <pasta>]   (padrão: build\testes)
#   .\tools\build-camada-teste.ps1 -Imagem build-ca  depois de .\build.ps1 -OutDir build-ca: põe o app na imagem,
#       abrindo sozinho 3 s depois do boot (com a saída no log serial), e regrava build-ca\mOS.img. Em seguida:
#       .\tools\qemu-test.ps1 -OutDir build-ca -Port 5923 -Memory 768 -Steps "wait:40000;shot:camada"
# Não entra na imagem pelo build.ps1 (não é um sample).
param([string]$Dest = "$PSScriptRoot\..\build\testes", [string]$Imagem = '')
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
$name = 'CamadaIOSurface'
$app = Join-Path $Dest "$name.app\Contents"
New-Item -ItemType Directory -Force "$app\MacOS" | Out-Null
& $zig cc -target x86_64-macos -O2 -fobjc-arc -I"$root\userland\sdk\include" -x objective-c "$root\userland\testes\$name\main.m" `
    -o "$app\MacOS\$name" "-Wl,-undefined,dynamic_lookup" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { Write-Host "teste $name FAILED" -ForegroundColor Red; exit 1 }
@"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>$name</string>
	<key>CFBundleIdentifier</key><string>com.mos.camadaiosurface</string>
	<key>CFBundleShortVersionString</key><string>1.0</string>
	<key>CFBundleExecutable</key><string>$name</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
"@ | Set-Content -Encoding ascii "$app\Info.plist"
Write-Host "==> $Dest\$name.app"
if ($Imagem) {
    $esp = Join-Path $root "$Imagem\esp"
    if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes .\build.ps1 -OutDir $Imagem" -ForegroundColor Red; exit 1 }
    Copy-Item -Recurse -Force (Join-Path $Dest "$name.app") "$esp\mOS\Aplicativos\"
    New-Item -ItemType Directory -Force "$esp\mOS\Sistema\Biblioteca\LaunchDaemons" | Out-Null
    "Programa=/Aplicativos/$name.app/Contents/MacOS/$name`nAtrasoMs=3000" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.mos.camadaiosurface.servico"
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
    # imagem de teste: overlay de teste com o marcador que pula o assistente do Live (tools\overlay-teste.ps1)
    . "$PSScriptRoot\overlay-teste.ps1"
    $ovt = New-OverlayTeste $root $Imagem
    & (Join-Path $root "$Imagem\mkimg.exe") (Join-Path $root "$Imagem\mOS.img") $esp 128 --test-overlay $ovt
    if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
}
