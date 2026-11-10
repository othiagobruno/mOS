# mOS — caminhos, esp e mkimg compartilhados por build.ps1 e tools/stage-electron.ps1.
# No Windows o mkimg continua mkimg.exe (zig cc -target x86_64-windows-gnu -lbcrypt) e o
# prefixo \\?\ só entra aqui. No Linux, tools/stage-image.sh é a lista de arquivos da imagem
# (docs/staging-linux.md); este arquivo ainda cobre o corpo do .ps1 se o bash não estiver.
# PS 5.1 não tem $IsWindows.

function Test-MosWindows {
    if (Test-Path variable:IsWindows) { return [bool]$IsWindows }
    return $env:OS -eq 'Windows_NT'
}

function ConvertTo-MosLongPath([string]$Path) {
    if (-not $Path) { return $Path }
    if (-not (Test-MosWindows)) { return $Path }
    if ($Path.StartsWith('\\?\')) { return $Path }
    $full = [IO.Path]::GetFullPath($Path)
    return '\\?\' + $full
}

function Ensure-MosEsp([string]$Root, [string]$OutDir) {
    $esp = Join-Path $Root (Join-Path $OutDir 'esp')
    $mos = Join-Path $esp 'mOS'
    if (-not (Test-Path -LiteralPath $mos)) {
        New-Item -ItemType Directory -Force -Path $mos | Out-Null
        $seed = Join-Path $Root 'root\mOS'
        if (Test-Path -LiteralPath $seed) {
            Get-ChildItem -Force -LiteralPath $seed | Copy-Item -Recurse -Force -Destination $mos
            Write-Host "esp/mOS criado e semeado com root/mOS ($mos)" -ForegroundColor Yellow
        } else {
            Write-Host "esp/mOS criado vazio ($mos)" -ForegroundColor Yellow
        }
    }
    $efi = Join-Path $esp 'EFI\BOOT\BOOTX64.EFI'
    if (-not (Test-Path -LiteralPath $efi)) {
        Write-Host "aviso: $efi não está no esp. O kernel sai do build.ps1 (zig). A imagem de teste é gerada mesmo assim." -ForegroundColor Yellow
    }
    return $esp
}

function Resolve-MosMkimg([string]$Root, [string]$OutDir) {
    $win = Test-MosWindows
    $name = if ($win) { 'mkimg.exe' } else { 'mkimg' }
    $bin = Join-Path $Root (Join-Path $OutDir $name)
    $src = Join-Path $Root 'tools\mkimg.c'
    $hdr = Join-Path $Root 'kernel\include\ed25519_impl.h'
    $stale = -not (Test-Path -LiteralPath $bin)
    if (-not $stale) {
        $bt = (Get-Item -LiteralPath $bin).LastWriteTime
        if ((Get-Item -LiteralPath $src).LastWriteTime -gt $bt -or (Get-Item -LiteralPath $hdr).LastWriteTime -gt $bt) { $stale = $true }
    }
    if (-not $stale) { return $bin }
    Write-Host "==> Building mkimg ($name)" -ForegroundColor Cyan
    if ($win) {
        if (-not $script:MosZig) {
            Write-Host "mkimg.exe: falta o zig PE/MZ. Rode .\build.ps1, que compila tools\mkimg.c com zig cc -target x86_64-windows-gnu -lbcrypt." -ForegroundColor Red
            exit 1
        }
        & $script:MosZig cc -O2 -target x86_64-windows-gnu $src -o $bin -lbcrypt
    } else {
        $cc = $null
        foreach ($c in @('cc', 'gcc', 'clang')) {
            $g = Get-Command $c -ErrorAction SilentlyContinue
            if ($g) { $cc = $g.Source; break }
        }
        if (-not $cc) {
            Write-Host "mkimg: não há cc, gcc nem clang no PATH para compilar tools/mkimg.c. Instale um compilador C." -ForegroundColor Red
            exit 1
        }
        & $cc -O2 $src -o $bin
    }
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $bin)) {
        Write-Host "mkimg FAILED: tools/mkimg.c não gerou $bin" -ForegroundColor Red
        exit 1
    }
    return $bin
}

function Copy-MosTree([string]$From, [string]$To) {
    if (Test-MosWindows) {
        & robocopy $From $To /E /NFL /NDL /NJH /NJS /NP /R:1 /W:1 | Out-Null
        if ($LASTEXITCODE -ge 8) { return $false }
        $global:LASTEXITCODE = 0
        return $true
    }
    New-Item -ItemType Directory -Force -Path $To | Out-Null
    Get-ChildItem -Force -LiteralPath $From | Copy-Item -Recurse -Force -Destination $To
    return $true
}

function Resolve-MosLinkFile([string]$Path) {
    return Resolve-MosLinkWalk $Path 0
}

function Resolve-MosLinkWalk([string]$Path, [int]$Depth) {
    if ($Depth -ge 8 -or -not (Test-Path -LiteralPath $Path)) { return $Path }
    if (Test-Path -LiteralPath $Path -PathType Container) { return (Resolve-Path -LiteralPath $Path).Path }
    $item = Get-Item -LiteralPath $Path
    if ($item.Length -gt 400) { return $item.FullName }
    $bytes = [IO.File]::ReadAllBytes((ConvertTo-MosLongPath $item.FullName))
    if ($bytes.Length -ge 4) {
        $b0 = $bytes[0]
        if ($b0 -eq 0xCF -or $b0 -eq 0xCA -or $b0 -eq 0xFE -or $b0 -eq 0xCE -or $b0 -eq 0x7F) { return $item.FullName }
    }
    $txt = ([Text.Encoding]::ASCII.GetString($bytes)).Trim()
    if ($txt.StartsWith('#mOS-link:')) { $txt = $txt.Substring(10).Trim() }
    if ($txt.Length -lt 1 -or $txt.Length -gt 300) { return $item.FullName }
    $acc = Split-Path $item.FullName
    $parts = @($txt -split '/')
    for ($i = 0; $i -lt $parts.Count; $i++) {
        $part = $parts[$i]
        if (-not $part -or $part -eq '.') { continue }
        if ($part -eq '..') { $acc = Split-Path $acc; continue }
        $step = Join-Path $acc $part
        if ((Test-Path -LiteralPath $step -PathType Container) -and -not (Test-Path -LiteralPath $step -PathType Leaf)) {
            $acc = (Resolve-Path -LiteralPath $step).Path
            continue
        }
        if (Test-Path -LiteralPath $step -PathType Leaf) {
            $resolved = Resolve-MosLinkWalk $step ($Depth + 1)
            if ($i -eq ($parts.Count - 1)) { return $resolved }
            if (Test-Path -LiteralPath $resolved -PathType Container) { $acc = $resolved; continue }
            return $item.FullName
        }
        return $item.FullName
    }
    if (Test-Path -LiteralPath $acc -PathType Container) { return (Resolve-Path -LiteralPath $acc).Path }
    if (Test-Path -LiteralPath $acc -PathType Leaf) { return Resolve-MosLinkWalk $acc ($Depth + 1) }
    return $item.FullName
}

function Install-MosCursorExtras([string]$EspMos, [string]$Shims, [string]$Bottles) {
    $bin = Join-Path $EspMos 'usr\bin'
    $lbin = Join-Path $EspMos 'usr\local\bin'
    $opt = Join-Path $EspMos 'usr\local\opt\git'
    New-Item -ItemType Directory -Force -Path $bin, $lbin, (Join-Path $opt 'bin') | Out-Null
    Copy-Item -Force -LiteralPath (Join-Path $Shims 'which') (Join-Path $bin 'which')
    Copy-Item -Force -LiteralPath (Join-Path $Shims 'security') (Join-Path $bin 'security')
    $gitDst = Join-Path $opt 'bin\git'
    $bottleGit = $null
    if ($Bottles) { $bottleGit = Join-Path $Bottles 'git\bin\git' }
    if ($bottleGit -and (Test-Path -LiteralPath $bottleGit)) {
        Copy-Item -Force -LiteralPath $bottleGit $gitDst
        $coreSrc = Join-Path $Bottles 'git\libexec\git-core'
        if (Test-Path -LiteralPath $coreSrc) {
            $coreDst = Join-Path $opt 'libexec\git-core'
            New-Item -ItemType Directory -Force -Path $coreDst | Out-Null
            $mainLen = (Get-Item -LiteralPath $bottleGit).Length
            foreach ($f in Get-ChildItem -File -LiteralPath $coreSrc) {
                $to = Join-Path $coreDst $f.Name
                if ($f.Name -ne 'git' -and $f.Length -eq $mainLen) {
                    [IO.File]::WriteAllText($to, '#mOS-link:/usr/local/opt/git/bin/git')
                } else {
                    Copy-Item -Force -LiteralPath $f.FullName $to
                }
            }
        }
        Write-Host "git do bottle em /usr/local/opt/git (libexec de mesmo tamanho virou #mOS-link)" -ForegroundColor Cyan
    } else {
        Copy-Item -Force -LiteralPath (Join-Path $Shims 'git') $gitDst
        Write-Host "git mínimo (tools/brew-shims/git) em /usr/local/opt/git; bottle Homebrew não foi passado" -ForegroundColor Yellow
    }
    $link = '#mOS-link:/usr/local/opt/git/bin/git'
    [IO.File]::WriteAllText((Join-Path $lbin 'git'), $link)
    [IO.File]::WriteAllText((Join-Path $bin 'git'), $link)
}

$script:MosStageHostDir = $PSScriptRoot

function Get-MosBinaryKind([string]$Path) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) { return 'missing' }
    $fs = [IO.File]::OpenRead((ConvertTo-MosLongPath $Path))
    $b = New-Object byte[] 4
    $n = $fs.Read($b, 0, 4)
    $fs.Close()
    if ($n -ge 2 -and $b[0] -eq 0x4D -and $b[1] -eq 0x5A) { return 'pe' }
    if ($n -ge 4 -and $b[0] -eq 0x7F -and $b[1] -eq 0x45 -and $b[2] -eq 0x4C -and $b[3] -eq 0x46) { return 'elf' }
    if ($n -ge 4 -and (($b[0] -eq 0xFE -and $b[1] -eq 0xED -and $b[2] -eq 0xFA) -or ($b[0] -eq 0xCF -and $b[1] -eq 0xFA -and $b[2] -eq 0xED) -or ($b[0] -eq 0xCE -and $b[1] -eq 0xFA -and $b[2] -eq 0xED) -or ($b[0] -eq 0xCA -and $b[1] -eq 0xFE -and $b[2] -eq 0xBA))) { return 'macho' }
    return 'other'
}

function Get-MosHostKind {
    if (Test-MosWindows) { return 'pe' }
    if (Test-Path variable:IsLinux) { if ($IsLinux) { return 'elf' } }
    $uname = $null
    try { $uname = & uname -s 2>$null } catch { $uname = $null }
    if ($uname -eq 'Linux') { return 'elf' }
    if ($uname -eq 'Darwin') { return 'macho' }
    return 'elf'
}

function Get-MosZigVersion([string]$Bin) {
    try {
        $out = (& $Bin version 2>$null | Out-String)
        if ($out -match '(\d+)\.(\d+)\.(\d+)') { return [version]"$($Matches[1]).$($Matches[2]).$($Matches[3])" }
    } catch { }
    return [version]'0.0.0'
}

function Get-MosZigLibDir([string]$Bin) {
    $dir = Split-Path -Parent $Bin
    if (Test-Path -LiteralPath (Join-Path $dir 'lib\libunwind')) { return $dir }
    $parent = Split-Path -Parent $dir
    if ($parent -and (Test-Path -LiteralPath (Join-Path $parent 'lib\libunwind'))) { return $parent }
    return $dir
}

# Não é a primeira pasta zig-*. No Windows só PE/MZ; no Linux só ELF. Se houver mais de um
# binário do host, fica o de versão maior (zig version), não o primeiro nome de pasta.
function Resolve-MosZig([string]$Root) {
    if (-not $Root) {
        if ($script:MosStageHostDir) { $Root = Split-Path $script:MosStageHostDir -Parent }
        else { $Root = (Get-Location).Path }
    }
    $want = Get-MosHostKind
    $cands = @()
    if ($env:MOS_ZIG) {
        $k = Get-MosBinaryKind $env:MOS_ZIG
        if ($k -eq $want) { $cands += $env:MOS_ZIG }
        elseif ($k -ne 'missing') { Write-Host "aviso: MOS_ZIG=$env:MOS_ZIG é $k; este host precisa de $want" -ForegroundColor Yellow }
    }
    foreach ($name in @('zig', 'zig.exe')) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd -and (Get-MosBinaryKind $cmd.Source) -eq $want) { $cands += $cmd.Source }
    }
    $tools = Join-Path $Root '.tools'
    if (Test-Path -LiteralPath $tools) {
        foreach ($dir in Get-ChildItem -LiteralPath $tools -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue) {
            foreach ($name in @('zig.exe', 'zig')) {
                $cand = Join-Path $dir.FullName $name
                if (-not (Test-Path -LiteralPath $cand)) { continue }
                $k = Get-MosBinaryKind $cand
                if ($k -eq $want) { $cands += $cand }
                else { Write-Host "aviso: $cand é $k e não serve neste host ($want). Um zig de outro sistema em .tools\zig-* deixa de ser o primeiro da lista." -ForegroundColor Yellow }
            }
        }
    }
    $best = $null
    $bestVer = [version]'0.0.0'
    foreach ($c in ($cands | Select-Object -Unique)) {
        $ver = Get-MosZigVersion $c
        if (-not $best -or $ver -gt $bestVer) { $best = $c; $bestVer = $ver }
    }
    if (-not $best) {
        Write-Host "zig do host não encontrado. Windows: zig.exe PE/MZ. Linux: binário ELF. Use MOS_ZIG, o PATH ou .tools\zig-* (não a primeira pasta)." -ForegroundColor Red
        return $null
    }
    $dir = Get-MosZigLibDir $best
    Write-Host "zig: $best ($want, $bestVer)" -ForegroundColor DarkGray
    return [pscustomobject]@{ Path = $best; Dir = $dir; Version = $bestVer; Kind = $want }
}
