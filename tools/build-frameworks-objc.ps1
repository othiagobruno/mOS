# mOS - frameworks do sistema como Mach-O de verdade (docs/swift-objc-runtime.md, fase 7), escritos para o mOS:
# userland\frameworks\<Nome>\*.m|*.c (+ *.swift: overlay Swift, como o CoreData da Apple, que traz o seu no próprio
# framework). Cada um vira /System/Library/Frameworks/<Nome>.framework/Versions/A/<Nome> (install name da Apple) na
# mesma raiz das dylibs Swift (build\swift\objc\dylib), que o carregador do mOS lê do VFS. Se a libz deste
# arch já foi compilada (tools\build-zlib.ps1), copia para usr\lib. Sem bottle do Homebrew.
#   .\tools\build-frameworks-objc.ps1 [-Only CoreData]
param([string[]]$Only, [string]$Arch = 'x86_64')
$root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\swift-arch.ps1" $Arch
if (-not (Get-Command Resolve-MosZig -ErrorAction SilentlyContinue)) { foreach ($mosHost in @("$PSScriptRoot\stage-host.ps1","$PSScriptRoot\..\stage-host.ps1","$PSScriptRoot\..\..\stage-host.ps1","$PSScriptRoot\..\..\..\stage-host.ps1")) { if (Test-Path -LiteralPath $mosHost) { . $mosHost; break } } }
$mosZigPick = Resolve-MosZig $root
if (-not $mosZigPick) { exit 1 }
$zig = $mosZigPick.Path
$tc = Get-ChildItem "$root\.tools\swift\LocalApp\Programs\Swift\Toolchains" -Directory | Select-Object -First 1
$objc = "$SwiftBuild\objc"
$dst = "$objc\dylib"
$tmp = "$objc\frameworks-obj"
New-Item -ItemType Directory -Force $tmp | Out-Null
if ($Only) { $Only = @($Only | ForEach-Object { $_ -split ',' }) }
$fails = 0
foreach ($fw in Get-ChildItem "$root\userland\frameworks" -Directory) {
    $name = $fw.Name
    if ($Only -and $Only -notcontains $name) { continue }
    $objs = @()
    $bad = $false
    foreach ($f in Get-ChildItem $fw.FullName -File | Where-Object { $_.Extension -in '.m', '.c' }) {
        $o = "$tmp\${name}_$($f.BaseName).o"
        $lang = if ($f.Extension -eq '.m') { @('-x', 'objective-c') } else { @() }
        $log = & $zig cc -target $ZigTarget -O2 -c @lang "-I$root\userland\sdk\include" -Wno-objc-root-class $f.FullName -o $o 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) { $log | Select-String 'error' | Select-Object -First 8 | ForEach-Object { "  $_" }; $bad = $true } else { $objs += $o }
    }
    $sw = @(Get-ChildItem $fw.FullName -Filter *.swift | ForEach-Object FullName)
    if ($sw -and -not $bad) {
        $o = "$tmp\${name}_swift.o"
        $log = & "$($tc.FullName)\usr\bin\swift-frontend.exe" -frontend -c -target $SwiftTriple -module-name $name -parse-as-library -O `
            -enable-library-evolution -import-underlying-module -I "$($fw.FullName)\include" -I $objc -I "$objc\overlays" `
            -I "$root\.tools\swift-src\stdlib\public\SwiftShims\swift\shims" -module-cache-path "$objc\modcache" `
            -Xcc "-isystem$objc\sdk\include" -Xcc -Wno-nullability-completeness -Xcc -DMOS_SWIFT_OBJC=1 `
            -emit-module-path "$tmp\$name.swiftmodule" -o $o @sw 2>&1 | ForEach-Object { "$_" }
        $err = $log | Select-String -Pattern ':\d+:\d+: error: |<unknown>:0: error: '   # não a linha de código citada
        if ($LASTEXITCODE -ne 0 -or $err) { $err | Select-Object -First 8 | ForEach-Object { "  $($_.Line)" }; $bad = $true } else { $objs += $o }
    }
    if ($bad) { Write-Host "${name}: FALHOU" -ForegroundColor Red; $fails++; continue }
    # pasta lib<X> -> /usr/lib/lib<X>.dylib; as outras -> /System/Library/Frameworks/<X>.framework/Versions/A/<X>
    if ($name -like 'lib*') { $dir = "$dst\usr\lib"; $file = "$name.dylib"; $install = "/usr/lib/$name.dylib" }
    else { $dir = "$dst\System\Library\Frameworks\$name.framework\Versions\A"; $file = $name; $install = "/System/Library/Frameworks/$name.framework/Versions/A/$name" }
    # install-path (opcional): 1ª linha = caminho de instalação (ex.: o UIKit do Catalyst, em /System/iOSSupport/...);
    # as outras = caminhos onde o mesmo binário também fica (o VFS não tem links simbólicos)
    $alias = @()
    if (Test-Path "$($fw.FullName)\install-path") {
        $ip = @(Get-Content "$($fw.FullName)\install-path" | Where-Object { $_ -match '^/' })
        $install = $ip[0]; $alias = @($ip | Select-Object -Skip 1)
        $dir = $dst + (Split-Path $install -Parent).Replace('/', '\'); $file = Split-Path $install -Leaf
    }
    New-Item -ItemType Directory -Force $dir | Out-Null
    $log = & $zig cc -target $ZigTarget -shared -o "$dir\$file" "-Wl,-install_name,$install" '-Wl,-undefined,dynamic_lookup' @objs 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { $log | Select-Object -First 8 | ForEach-Object { "  $_" }; Write-Host "${name}: link FALHOU" -ForegroundColor Red; $fails++; continue }
    foreach ($a in $alias) {
        $ad = $dst + (Split-Path $a -Parent).Replace('/', '\')
        New-Item -ItemType Directory -Force $ad | Out-Null
        Copy-Item "$dir\$file" "$ad\$(Split-Path $a -Leaf)" -Force
    }
    Write-Host ("{0}: ok ({1:N0} KB) -> {2}" -f $name, ((Get-Item "$dir\$file").Length / 1KB), $install)
}
# libz deste arch, se o build já a gerou (a imagem padrão a põe em /usr/lib pelo build.ps1)
$z = "$root\build\zlib-$Arch\libz.1.dylib"
if (Test-Path $z) {
    New-Item -ItemType Directory -Force "$dst\usr\lib" | Out-Null
    Copy-Item $z "$dst\usr\lib\libz.1.dylib" -Force
    Write-Host 'libz: /usr/lib/libz.1.dylib a partir do fonte'
} else { Write-Host "libz: sem build\zlib-$Arch (tools\build-zlib.ps1)" -ForegroundColor Yellow }
exit $fails
