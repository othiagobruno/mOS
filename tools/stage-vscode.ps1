# mOS - coloca o VS Code para macOS (Intel x64) numa imagem do mOS, para o porte (docs/porte-vscode.md)
#   .\build.ps1 -OutDir build-vscode
#   .\tools\stage-vscode.ps1 -App "<pasta>\Visual Studio Code.app" [-OutDir build-vscode] [-SizeMiB 512]
#   .\run.ps1 -Image build-vscode/mOS.img -Memory 2560      (no Terminal do mOS: macinfo/run /Aplicativos/VSCode.app)
# Copia só o que o carregador precisa hoje: Info.plist, o executável e os frameworks/bibliotecas ligados
# (sem Resources, que só farão falta quando o Electron passar da inicialização). O FAT32 não tem links
# simbólicos: os binários ficam em Versions/A e o carregador do mOS procura lá sozinho.
# Também cria /Sistema/Biblioteca/dyld-experimental, que deixa abrir apps que pedem bibliotecas do sistema
# que o mOS ainda não tem (as funções delas viram stubs que encerram o processo dizendo o nome).
param([Parameter(Mandatory = $true)][string]$App, [string]$OutDir = 'build-vscode', [int]$SizeMiB = 512, [switch]$Diagnostico, [switch]$ComApp, [switch]$Autostart,
      [switch]$ComModulosNativos, [int]$MaxModuloMiB = 32,
      [string]$Args0 = '--single-process --in-process-gpu --disable-gpu --no-sandbox --verbose --enable-logging')
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path $App 'Contents'
$esp = Join-Path $root "$OutDir\esp"
if (-not (Test-Path "$src\MacOS\Code")) { Write-Host "não achei $src\MacOS\Code" -ForegroundColor Red; exit 1 }
if (-not (Test-Path "$esp\mOS")) { Write-Host "rode antes: .\build.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
$dst = "$esp\mOS\Aplicativos\VSCode.app\Contents"
Remove-Item -Recurse -Force "$esp\mOS\Aplicativos\VSCode.app" -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$dst\MacOS" | Out-Null
Copy-Item "$src\Info.plist" "$dst\Info.plist"
Copy-Item "$src\MacOS\Code" "$dst\MacOS\Code"
foreach ($fw in 'Electron Framework', 'Squirrel', 'Mantle', 'ReactiveObjC') {
    $s = "$src\Frameworks\$fw.framework\Versions\A"
    $t = "$dst\Frameworks\$fw.framework\Versions\A"
    New-Item -ItemType Directory -Force $t | Out-Null
    Copy-Item "$s\$fw" "$t\$fw"
}
# apps auxiliares (processos de GPU, renderização e plugins): o Electron confere que existem ao iniciar
foreach ($h in Get-ChildItem "$src\Frameworks" -Directory -Filter 'Code Helper*.app') {
    $t = "$dst\Frameworks\$($h.Name)\Contents"
    New-Item -ItemType Directory -Force "$t\MacOS" | Out-Null
    Copy-Item "$($h.FullName)\Contents\Info.plist" "$t\Info.plist"
    Copy-Item "$($h.FullName)\Contents\MacOS\*" "$t\MacOS\"
}
# -ComApp: o código do VS Code para o processo principal (~140 MiB; sem extensões nem módulos nativos)
if ($ComApp) {
    $a = "$src\Resources\app"
    New-Item -ItemType Directory -Force "$dst\Resources\app" | Out-Null
    foreach ($f in 'package.json', 'product.json', 'node_modules.asar') { Copy-Item "$a\$f" "$dst\Resources\app\" }
    # alguns arquivos de out\ passam de 260 caracteres no Windows (imagens do chat): ficam de fora
    foreach ($d in 'out', 'node_modules') { Copy-Item -Recurse "$a\$d" "$dst\Resources\app\" -ErrorAction SilentlyContinue }
}
# -ComModulosNativos: node_modules.asar.unpacked (módulos .node que o Node carrega com dlopen, ex.:
# @vscode/spdlog/build/Release/spdlog.node). Arquivos acima de -MaxModuloMiB ficam de fora (o runtime.node
# do Copilot tem ~120 MiB), assim como caminhos longos demais para o Windows.
if ($ComModulosNativos) {
    $u = "$src\Resources\app\node_modules.asar.unpacked"
    if (Test-Path $u) {
        $u = (Get-Item -LiteralPath $u).FullName           # caminho por extenso (o -App pode vir abreviado, ex.: RUTHMI~1)
        foreach ($f in Get-ChildItem -LiteralPath $u -Recurse -File -ErrorAction SilentlyContinue) {
            if ($f.Length -gt $MaxModuloMiB * 1MB) { continue }
            $rel = $f.FullName.Substring($u.Length)
            $t = "$dst\Resources\app\node_modules.asar.unpacked$rel"
            New-Item -ItemType Directory -Force (Split-Path $t) -ErrorAction SilentlyContinue | Out-Null
            # prefixo \\?\: alguns caminhos de origem passam de 260 caracteres (ex.: vscode-policy-watcher.node)
            try { [IO.File]::Copy('\\?\' + $f.FullName, '\\?\' + $t, $true) }
            catch { Write-Host "não copiado: $rel ($($_.Exception.Message))" -ForegroundColor Yellow }
        }
    } else { Write-Host "sem $u" -ForegroundColor Yellow }
}
# recursos do Electron (ICU, snapshot do V8, .pak); das traduções só en e pt_BR
# (no caminho "Electron Framework.framework\Resources": sem links simbólicos, é ali que o Chromium procura o .pak)
$res = "Frameworks\Electron Framework.framework\Resources"
New-Item -ItemType Directory -Force "$dst\$res" | Out-Null
$resSrc = "$src\Frameworks\Electron Framework.framework\Versions\A\Resources"
if (-not (Test-Path -LiteralPath $resSrc)) { $resSrc = "$src\Frameworks\Electron Framework.framework\Resources" }
Get-ChildItem -LiteralPath $resSrc -File | Copy-Item -Destination "$dst\$res\"
foreach ($l in 'en.lproj', 'pt_BR.lproj') { if (Test-Path "$resSrc\$l") { Copy-Item -Recurse "$resSrc\$l" "$dst\$res\" } }
$lib = "Frameworks\Electron Framework.framework\Versions\A\Libraries"
New-Item -ItemType Directory -Force "$dst\$lib" | Out-Null
Copy-Item "$src\$lib\libffmpeg.dylib" "$dst\$lib\"
Set-Content -Path "$esp\mOS\Sistema\Biblioteca\dyld-experimental" -Value 'VS Code' -Encoding ascii
# -Autostart: o init abre o VS Code sozinho 3 s depois do boot (testes sem teclado nem Terminal)
if ($Autostart) {
    # um processo só: o mOS ainda não tem portas Mach entre processos (os helpers do Chromium precisam)
    "Programa=/Aplicativos/VSCode.app/Contents/MacOS/Code`nArgumentos=$Args0`nAtrasoMs=3000" | Set-Content -Encoding ascii "$esp\mOS\Sistema\Biblioteca\LaunchDaemons\com.microsoft.vscode.servico"
}
# -Diagnostico: funções ausentes seguem com 0 (e são registradas) e a saída dos apps vai ao log serial
if ($Diagnostico) {
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\dyld-registrar" -Value '1' -Encoding ascii
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\stderr-serial" -Value '1' -Encoding ascii
    # o kernel despeja as threads de usuário (estado, rip e pilha) no log serial a cada 30 s
    Set-Content -Path "$esp\mOS\Sistema\Biblioteca\despejar-threads" -Value '1' -Encoding ascii
}
$mb = (Get-ChildItem "$esp\mOS\Aplicativos\VSCode.app" -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("VSCode.app: {0:N1} MiB" -f $mb) -ForegroundColor Cyan
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img (boote com -Memory 2560: o arquivo de 202 MiB fica na RAM e é copiado para o processo)" -ForegroundColor Green
