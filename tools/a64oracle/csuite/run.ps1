# mOS - suíte diferencial C do tradutor arm64 (tools\a64oracle\csuite\c_*.c).
# Cada programa compila para x86_64-windows (referência, roda nativo) e para aarch64 em -O0/-O2/-O3
# (-mcpu=apple_m2: -O3 gera NEON, LSE, etc.); as versões arm64 rodam no a64test (interpretador e,
# com -Jit, JIT) e no oráculo QEMU. Todas as saídas têm que ser iguais à do x86_64.
#   powershell -File tools\a64oracle\csuite\run.ps1 [-Jit] [-SemQemu] [-Programa c_fp] [-Tsv arq]
# Leitura do resultado: a64test difere e QEMU bate → bug do tradutor; QEMU também difere → o
# programa não é portável (corrigir o teste).
param([switch]$Jit, [switch]$SemQemu, [string]$Programa = '', [string]$Tsv = '', [int]$Timeout = 300)
. "$PSScriptRoot\..\common.ps1"
$root = $script:A64Root
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig).Source }
$out = "$root\build\a64csuite"
New-Item -ItemType Directory -Force $out | Out-Null
$common = @('-ffp-contract=off', '-fno-math-errno', '-fno-builtin', '-fno-sanitize=all', '-Wall', '-Wno-unused-function')
$progs = Get-ChildItem "$PSScriptRoot\c_*.c" | Where-Object { $Programa -eq '' -or $_.BaseName -eq $Programa }
$opts = @('O0', 'O2', 'O3')
$rows = @()
$a64test = Get-A64Test
$fails = 0
foreach ($p in $progs) {
    $n = $p.BaseName
    & $zig cc -target x86_64-windows-gnu -O2 -mfma -mcx16 $common $p.FullName -o "$out\$n-x86.exe" 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { Write-Host "$n x86_64: compilacao FALHOU" -ForegroundColor Red; $fails++; continue }
    $ref = Invoke-A64Capture "$out\$n-x86.exe" @() $Timeout
    foreach ($o in $opts) {
        $elf = "$out\$n-$o.elf"
        & $zig cc -target aarch64-freestanding-none -mcpu=apple_m2 "-$o" -g0 -ffreestanding -nostdlib -static -fno-pic -fno-stack-protector `
            -mno-outline-atomics $common "-Wl,--image-base=0x200000000" "-Wl,-e,_start" `
            "$root\tools\a64tests\crt.S" $p.FullName -o $elf 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) { Write-Host "$n -$o arm64: compilacao FALHOU" -ForegroundColor Red; $fails++; continue }
        $res = [ordered]@{ programa = $n; opt = $o }
        $engines = @(@{ k = 'interp'; a = @('--argv0', 'prog', $elf) })
        if ($Jit) { $engines += @{ k = 'jit'; a = @('--jit', '--argv0', 'prog', $elf) } }
        foreach ($e in $engines) {
            $r = Invoke-A64Capture $a64test $e.a $Timeout
            $res[$e.k] = if ($r.TimedOut) { 'tempo' } elseif ($r.Out -eq $ref.Out -and $r.Code -eq 0) { 'igual' } else { 'difere' }
            if ($res[$e.k] -ne 'igual') {
                $fails++
                $x = $ref.Out -split "`n"; $y = $r.Out -split "`n"
                $first = 0; while ($first -lt $x.Count -and $first -lt $y.Count -and $x[$first] -eq $y[$first]) { $first++ }
                $got = if ($first -lt $y.Count) { $y[$first] } else { '<fim>' }
                $exp = if ($first -lt $x.Count) { $x[$first] } else { '<fim>' }
                Write-Host ("  {0} -{1} {2}: linha {3}: x86=[{4}] arm64=[{5}] saida={6} {7}" -f $n, $o, $e.k, ($first + 1), $exp.Trim(), $got.Trim(), $r.Code, ($r.Err -split "`n")[0]) -ForegroundColor Yellow
            }
        }
        if (-not $SemQemu) {
            # o qemu-system-aarch64 do Windows calcula FMADD/FMLA de f32/f64 com o fma() do mingw
            # (arredonda duas vezes) quando FPSR.IXC já está ligado: as linhas "fma ..." só são
            # comparadas entre x86_64 e a64test (o x86_64 usa vfmadd, exato)
            $q = Invoke-A64Oracle $elf $Timeout
            $strip = { param($s) (($s -split "`n") | Where-Object { $_ -notlike 'fma *' }) -join "`n" }
            $res['qemu'] = if ($q.TimedOut) { 'tempo' } elseif ((& $strip $q.Out) -eq (& $strip $ref.Out) -and $q.Code -eq 0) { 'igual' } else { 'difere' }
            if ($res['qemu'] -ne 'igual') { Write-Host "  $n -$o qemu difere do x86_64: teste nao portavel" -ForegroundColor Magenta; $fails++ }
        }
        $rows += [pscustomobject]$res
        $line = ($res.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join '  '
        $ok = -not ($res.Values -contains 'difere' -or $res.Values -contains 'tempo')
        Write-Host $line -ForegroundColor $(if ($ok) { 'Green' } else { 'Red' })
    }
}
if ($Tsv) { $rows | ConvertTo-Csv -Delimiter "`t" -NoTypeInformation | ForEach-Object { $_ -replace '"', '' } | Set-Content -Encoding UTF8 $Tsv }
if ($fails) { exit 1 } else { exit 0 }
