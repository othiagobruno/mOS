# Copia XPC services (Contents/XPCServices e os de dentro de frameworks, inclusive Updater.app)
# e bundles de authorization plugin para o app que já está no esp.
# O plugin só é copiado. Não há instalação privilegiada e ninguém o carrega.
function Copy-MosBundleServices {
    param([Parameter(Mandatory = $true)][string]$SrcContents, [Parameter(Mandatory = $true)][string]$DstContents)
    $logged = @{}
    function Copy-MosOne([string]$From, [string]$To) {
        $parent = Split-Path -Parent $To
        if ($parent -and -not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
        $ok = $false
        if (Get-Command Copy-X64 -ErrorAction SilentlyContinue) {
            try { Copy-X64 $From $To; $ok = $true } catch { $ok = $false }
        }
        if (-not $ok) { Copy-Item -LiteralPath $From -Destination $To -Force }
        if ($From -match 'AuthorizationPlugin' -and -not $logged.ContainsKey($From)) {
            Write-Host "authplugin copiado (sem privilégio): $From"
            $logged[$From] = $true
        }
    }
    function Copy-MosTree([string]$From, [string]$To) {
        if (-not (Test-Path -LiteralPath $From)) { return }
        $root = (Resolve-Path -LiteralPath $From).Path
        foreach ($f in @(Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue)) {
            $rel = $f.FullName.Substring($root.Length).TrimStart('\', '/')
            Copy-MosOne $f.FullName (Join-Path $To $rel)
        }
    }
    Copy-MosTree (Join-Path $SrcContents 'XPCServices') (Join-Path $DstContents 'XPCServices')
    $fw = Join-Path $SrcContents 'Frameworks'
    if (Test-Path -LiteralPath $fw) {
        foreach ($d in @(Get-ChildItem -LiteralPath $fw -Directory -ErrorAction SilentlyContinue)) {
            $dest = Join-Path $DstContents "Frameworks\$($d.Name)"
            Copy-MosTree (Join-Path $d.FullName 'XPCServices') (Join-Path $dest 'XPCServices')
            Copy-MosTree (Join-Path $d.FullName 'Updater.app') (Join-Path $dest 'Updater.app')
            $vers = Join-Path $d.FullName 'Versions'
            if (Test-Path -LiteralPath $vers) {
                foreach ($v in @(Get-ChildItem -LiteralPath $vers -Directory -ErrorAction SilentlyContinue)) {
                    $vd = Join-Path $dest "Versions\$($v.Name)"
                    Copy-MosTree (Join-Path $v.FullName 'XPCServices') (Join-Path $vd 'XPCServices')
                    Copy-MosTree (Join-Path $v.FullName 'Updater.app') (Join-Path $vd 'Updater.app')
                }
            }
            if ($d.Name -like '*.app') {
                Copy-MosTree (Join-Path $d.FullName 'Contents\XPCServices') (Join-Path $dest 'Contents\XPCServices')
                Copy-MosTree (Join-Path $d.FullName 'Contents\Library\SecurityAgentPlugins') (Join-Path $dest 'Contents\Library\SecurityAgentPlugins')
                Copy-MosTree (Join-Path $d.FullName 'Contents\Library\AuthorizationPlugins') (Join-Path $dest 'Contents\Library\AuthorizationPlugins')
            }
        }
    }
    foreach ($rel in @(
            'Library\SecurityAgentPlugins',
            'Library\AuthorizationPlugins',
            'PlugIns')) {
        Copy-MosTree (Join-Path $SrcContents $rel) (Join-Path $DstContents $rel)
    }
}
