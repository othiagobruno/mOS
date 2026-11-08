# mOS - teste da cadeia Swift arm64 (tradutor arm64, docs/tradutor-arm64.md, frente LIBSYS): os testes Swift da variante
# com interop ObjC (userland\tests-swiftobjc\swift e foundation) compilados arm64 como binários da Apple (ligados nas
# dylibs de /usr/lib/swift, tools\link-swift-objc.ps1 -Arch arm64 -Dylib) e rodados no machotest pelo tradutor, com as
# dylibs universais de build\universal-swift (tools\build-swift-arm64.ps1) no VFS. Também o x86_64, de referência.
#   .\tools\build-swift-arm64-test.ps1 [-NoRun] [-Testes swift,foundation] [-SoArm64]
param([switch]$NoRun, [string[]]$Testes = @('swift', 'foundation'), [switch]$SoArm64, [int]$Timeout = 180)
$root = Split-Path $PSScriptRoot -Parent
$out = "$root\build\swift-arm64-test"
$Testes = @($Testes | ForEach-Object { $_ -split ',' })
$archs = if ($SoArm64) { @('arm64') } else { @('x86_64', 'arm64') }
New-Item -ItemType Directory -Force $out | Out-Null
$fail = 0
foreach ($t in $Testes) {
    foreach ($a in $archs) {
        $exe = "$out\$t-$a"
        Write-Host "==> $t ($a)" -ForegroundColor Cyan
        & "$PSScriptRoot\link-swift-objc.ps1" -Out $exe -Module "T$t" -Dylib -Arch $a -Sources "$root\userland\tests-swiftobjc\$t\main.swift" | Select-Object -Last 3
        if ($LASTEXITCODE -or -not (Test-Path $exe)) { Write-Host "${t}-${a}: compilação FALHOU" -ForegroundColor Red; $fail++ }
    }
}
if ($fail -or $NoRun) { exit $fail }

# VFS: o libSystem arm64 + a64rt (tradutor) e as dylibs Swift universais nos caminhos da Apple
$vfs = "$out\vfs"
$a64 = "$vfs\Sistema\Biblioteca\a64"
New-Item -ItemType Directory -Force $a64 | Out-Null
foreach ($f in @('a64rt.mos', 'libSystem-arm64.mos', 'libSystem.mos', 'machotest.exe')) { if (-not (Test-Path "$root\build\$f")) { Write-Host "falta build\$f" -ForegroundColor Red; exit 1 } }
Copy-Item "$root\build\a64rt.mos" "$a64\a64rt.mos" -Force
Copy-Item "$root\build\libSystem-arm64.mos" "$a64\libSystem.mos" -Force
if (-not (Test-Path "$root\build\universal-swift\usr\lib\swift\libswiftCore.dylib")) { Write-Host 'falta build\universal-swift\usr\lib\swift (tools\build-swift-arm64.ps1)' -ForegroundColor Red; exit 1 }
if (Test-Path "$root\build\universal") { Copy-Item "$root\build\universal\*" $vfs -Recurse -Force }   # libc++, ICU...
Copy-Item "$root\build\universal-swift\*" $vfs -Recurse -Force
foreach ($t in $Testes) {
    foreach ($a in $archs) {
        $b = "$t-$a"
        $p = Start-Process -FilePath "$root\build\machotest.exe" -ArgumentList @("`"$root\build\libSystem.mos`"", "`"$vfs`"", "`"$out\$b`"") -NoNewWindow -PassThru `
            -RedirectStandardOutput "$out\$b.log" -RedirectStandardError "$out\$b.err"
        $null = $p.Handle
        if (-not $p.WaitForExit($Timeout * 1000)) { $p.Kill(); Write-Host "${b}: passou de $Timeout s" -ForegroundColor Red; $fail++; continue }
        Get-Content "$out\$b.log", "$out\$b.err" -ErrorAction SilentlyContinue | Where-Object { $_ -match ': \d+/\d+ ok|FALHA|macho|erro|panic|sinal|SIG' } | Select-Object -First 25
        $okLine = Get-Content "$out\$b.log" -ErrorAction SilentlyContinue | Where-Object { $_ -match ': (\d+)/(\d+) ok' } | Select-Object -Last 1
        $good = $okLine -and ($okLine -match ': (\d+)/(\d+) ok') -and $Matches[1] -eq $Matches[2]
        Write-Host ("{0}: {1} (saída {2})" -f $b, $(if ($good) { 'ok' } else { 'FALHOU' }), $p.ExitCode) -ForegroundColor $(if ($good) { 'Green' } else { 'Red' })
        if (-not $good) { $fail++ }
    }
}
exit $fail
