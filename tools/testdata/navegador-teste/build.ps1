# mOS - compila o "navegador" de teste (main.m) como build\navegador-teste\Navegador de Teste.app, para o
# vscode-teste.ps1 -ComApp (testar o openURL: de http/https de um app sem o Chrome na imagem)
$root = Resolve-Path "$PSScriptRoot\..\..\.."
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
$app = "$root\build\navegador-teste\Navegador de Teste.app\Contents"
New-Item -ItemType Directory -Force "$app\MacOS" | Out-Null
& $zig cc -target x86_64-macos -O2 -fobjc-arc -I"$root\userland\sdk\include" -x objective-c "$PSScriptRoot\main.m" -o "$app\MacOS\NavegadorTeste" "-Wl,-undefined,dynamic_lookup"
if ($LASTEXITCODE -ne 0) { Write-Host 'navegador-teste FAILED' -ForegroundColor Red; exit 1 }
Copy-Item "$PSScriptRoot\Info.plist" "$app\Info.plist" -Force
Write-Host "ok: $app" -ForegroundColor Green
