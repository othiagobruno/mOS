# mOS - põe o Homebrew (Homebrew/brew + portable-ruby 4.0.9 Intel) na imagem, por cima do tools/stage-bash.ps1.
#   git clone --depth 1 https://github.com/Homebrew/brew build\brew\Homebrew
#   (portable-ruby: o sha256 está em Library/Homebrew/vendor/portable-ruby-x86_64-darwin; ver docs/porte-brew.md)
#   .\tools\stage-bash.ps1 -OutDir build-exec -Script tools\testdata\brew-install.sh -SizeMiB 1024
#   .\tools\stage-brew.ps1 -OutDir build-exec
# Monta: /usr/local/Homebrew, vendor/portable-ruby/<versão> + current (link), /usr/local/bin/brew,
# /System/Library/CoreServices/SystemVersion.plist (macOS 14.7.6), /usr/bin/sw_vers, /usr/sbin/sysctl e links
# de /usr/bin e /bin para as ferramentas do Homebrew já instaladas em /usr/local/bin.
param([string]$OutDir = 'build-exec', [string]$Brew = 'build\brew\Homebrew', [string]$Ruby = 'build\brew\x4\portable-ruby',
      [int]$SizeMiB = 1024)
$root = Split-Path $PSScriptRoot -Parent
$esp = Join-Path $root "$OutDir\esp"
$mos = "$esp\mOS"
if (-not (Test-Path "$mos\usr\local\bin\bash")) { Write-Host "rode antes: tools\stage-bash.ps1 -OutDir $OutDir" -ForegroundColor Red; exit 1 }
function New-MosLink([string]$at, [string]$target) { [IO.File]::WriteAllText($at, "#mOS-link:$target", [Text.Encoding]::ASCII) }

$hb = "$mos\usr\local\Homebrew"
robocopy (Join-Path $root $Brew) $hb /E /NFL /NDL /NJH /NJS /NP | Out-Null
# symlinks do repositório (modo 120000; ex.: vendor/gems/mechanize -> mechanize-2.14.2/): no Windows o git os grava
# como texto com o alvo; viram links do mOS (o kernel segue no meio do caminho, o libSystem no fim)
$links = git -C (Join-Path $root $Brew) ls-files -s | Where-Object { $_ -match '^120000 ' } | ForEach-Object { ($_ -split "`t", 2)[1] }
foreach ($l in $links) {
    $f = Join-Path $hb ($l -replace '/', '\')
    if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { continue }
    $t = ([IO.File]::ReadAllText($f)).Trim().TrimEnd('/')
    New-MosLink $f $t
}
Write-Host "links do repositório: $($links.Count)"
$ver = (Get-Content "$hb\Library\Homebrew\vendor\portable-ruby-version" -TotalCount 1).Trim()
$rsrc = Join-Path $root "$Ruby\$ver"
if (-not (Test-Path $rsrc)) { Write-Host "falta o portable-ruby $ver em $Ruby" -ForegroundColor Red; exit 1 }
$vr = "$hb\Library\Homebrew\vendor\portable-ruby"
New-Item -ItemType Directory -Force $vr | Out-Null
robocopy $rsrc "$vr\$ver" /E /XD '*.dSYM' /NFL /NDL /NJH /NJS /NP | Out-Null
New-MosLink "$vr\current" $ver                       # pasta-link: o kernel segue no meio do caminho (lib/vfs.c)
New-MosLink "$mos\usr\local\bin\brew" '/usr/local/Homebrew/bin/brew'

New-Item -ItemType Directory -Force "$mos\System\Library\CoreServices", "$mos\usr\bin", "$mos\usr\sbin", "$mos\bin" | Out-Null
Copy-Item -Force "$PSScriptRoot\brew-shims\SystemVersion.plist" "$mos\System\Library\CoreServices\"
Copy-Item -Force "$PSScriptRoot\brew-shims\sw_vers" "$mos\usr\bin\sw_vers"
Copy-Item -Force "$PSScriptRoot\brew-shims\sysctl" "$mos\usr\sbin\sysctl"
Copy-Item -Force "$PSScriptRoot\brew-shims\pkgutil" "$mos\usr\sbin\pkgutil"
Copy-Item -Force "$PSScriptRoot\brew-shims\xcode-select" "$mos\usr\bin\xcode-select"
# casks: hdiutil/diskutil sobre o leitor de .dmg do kernel (imgtool), ditto/mkbom para copiar do volume,
# xattr e codesign que aceitam e não fazem nada (o VFS não guarda atributos; o kernel não confere assinaturas)
foreach ($t in 'hdiutil', 'ditto', 'mkbom', 'xattr', 'codesign', 'sudo') { Copy-Item -Force "$PSScriptRoot\brew-shims\$t" "$mos\usr\bin\$t" }
Copy-Item -Force "$PSScriptRoot\brew-shims\diskutil" "$mos\usr\sbin\diskutil"
New-Item -ItemType Directory -Force "$mos\Applications" | Out-Null
# Command Line Tools de mentira: o brew (macOS 14, Tier 3) as exige mesmo para bottles; o clang só informa a versão
New-Item -ItemType Directory -Force "$mos\Library\Developer\CommandLineTools\usr\bin" | Out-Null
Copy-Item -Force "$PSScriptRoot\brew-shims\clang" "$mos\Library\Developer\CommandLineTools\usr\bin\clang"
# SDK só com o SDKSettings.json (o brew doctor confere a versão; não há cabeçalhos nem bibliotecas)
$sdk = "$mos\Library\Developer\CommandLineTools\SDKs\MacOSX14.5.sdk"
New-Item -ItemType Directory -Force $sdk | Out-Null
[IO.File]::WriteAllText("$sdk\SDKSettings.json", '{"CanonicalName":"macosx14.5","DisplayName":"macOS 14.5","Version":"14.5"}', [Text.Encoding]::ASCII)
# ferramentas que o macOS tem em /usr/bin e /bin: links para as do Homebrew (GNU) em /usr/local/bin
$usrbin = 'curl', 'git', 'env', 'tar', 'gzip', 'xz', 'sed', 'grep', 'egrep', 'uname', 'id', 'which', 'readlink', 'basename',
          'dirname', 'head', 'tail', 'sort', 'tr', 'wc', 'cut', 'uniq', 'touch', 'tee', 'stat', 'printf', 'true', 'false',
          'install', 'du', 'od', 'xargs', 'find', 'awk', 'mktemp', 'nproc', 'realpath', 'shasum', 'md5', 'unzip', 'zip', 'timeout'
$bin = 'ls', 'cat', 'mkdir', 'rm', 'rmdir', 'cp', 'mv', 'ln', 'chmod', 'date', 'echo', 'pwd', 'sleep', 'test', 'expr', 'hostname', 'ps'
foreach ($t in $usrbin) { if (Test-Path "$mos\usr\local\bin\$t") { New-MosLink "$mos\usr\bin\$t" "/usr/local/bin/$t" } }
foreach ($t in $bin) { if (Test-Path "$mos\usr\local\bin\$t") { New-MosLink "$mos\bin\$t" "/usr/local/bin/$t" } }
& (Join-Path $root "$OutDir\mkimg.exe") (Join-Path $root "$OutDir\mOS.img") $esp $SizeMiB
if ($LASTEXITCODE -ne 0) { Write-Host 'mkimg FAILED' -ForegroundColor Red; exit 1 }
Write-Host "OK: $OutDir\mOS.img com o Homebrew ($ver) em /usr/local/Homebrew" -ForegroundColor Green
