# mOS - placar do tradutor arm64: roda toda a verificação e escreve a tabela de cobertura na seção
# "Estado" de docs/tradutor-arm64.md (entre <!-- placar:inicio --> e <!-- placar:fim -->).
#   powershell -File tools\a64placar.ps1 [-Jit] [-Casos 2000] [-CasosSeq 300] [-K 8] [-Semente N]
#                                        [-SemBuild] [-SemDoc] [-SemC] [-SemElf]
# Partes:
#   1. fuzzing diferencial por grupo de codificação (build\a64fuzz.exe, 1 instrução por caso)
#   2. fuzzing de sequências (K instruções misturadas, minimização)
#   3. testes ELF de build\a64tests (a64test × oráculo QEMU; com -Jit também o JIT). Um teste cujo
#      fonte tem "a64cmp: nao-comparavel" (valores do M1, contadores...) só precisa sair com 0.
#   4. suíte diferencial C (tools\a64oracle\csuite\run.ps1)
# Arquivos de saída em build\a64placar\ (exemplos de divergência em exemplos.txt).
param([switch]$Jit, [int]$Casos = 2000, [int]$CasosSeq = 300, [int]$K = 8, [string]$Semente = '',
      [switch]$SemBuild, [switch]$SemDoc, [switch]$SemC, [switch]$SemElf, [int]$Timeout = 180)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\a64oracle\common.ps1"
$root = $script:A64Root
$out = "$root\build\a64placar"
New-Item -ItemType Directory -Force $out | Out-Null
if (-not $Semente) { $Semente = [string]([DateTime]::UtcNow.Ticks % 1000000007) }

if (-not $SemBuild) {
    $a = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "$root\tools\build-a64test.ps1")
    if ($Jit) { $a += '-Jit' }
    & powershell @a | Out-Host
    if ($LASTEXITCODE -ne 0) { Write-Host 'placar: build falhou' -ForegroundColor Red; exit 2 }
}
# cópia própria do a64test (outras sessões recompilam build\a64test.exe, às vezes sem -Jit, no meio
# da rodada); com -Jit, confere que o JIT está mesmo compilado (A64JIT_STATS imprime "[jit] blocos=")
$priv = "$out\a64test.exe"
Copy-Item "$root\build\a64test.exe" $priv -Force
if ($Jit) {
    $env:A64JIT_STATS = '1'
    $chk = Invoke-A64Capture $priv @('--jit', "$root\build\a64tests\hello.elf") 60
    Remove-Item Env:\A64JIT_STATS
    if ($chk.Err -notmatch '\[jit\] blocos=') { Write-Host 'placar: build\a64test.exe sem JIT (recompile com -Jit)' -ForegroundColor Red; exit 2 }
}
$env:MOS_A64TEST = $priv
$fuzz = "$root\build\a64fuzz.exe"
function Read-Tsv([string]$p) {
    $l = [IO.File]::ReadAllLines($p) | Where-Object { $_ -ne '' }
    $h = $l[0].TrimStart([char]0xFEFF) -split "`t"
    $l | Select-Object -Skip 1 | ForEach-Object { $v = $_ -split "`t"; $o = [ordered]@{}; for ($i = 0; $i -lt $h.Count; $i++) { $o[$h[$i]] = $v[$i] }; [pscustomobject]$o }
}

# ---- 1. fuzz por grupo ----
Write-Host "placar: fuzz por grupo ($Casos casos/grupo, semente $Semente)" -ForegroundColor Cyan
$fa = @('--casos', "$Casos", '--semente', $Semente, '--relatorio', "$out\fuzz1.tsv", '--exemplos', "$out\exemplos.txt", '--quieto')
if ($Jit) { $fa += '--jit' }
$f1 = Invoke-A64Capture $fuzz $fa 7200
if (-not (Test-Path "$out\fuzz1.tsv")) { Write-Host "placar: fuzz falhou: $($f1.Err)" -ForegroundColor Red; exit 2 }
$g = Read-Tsv "$out\fuzz1.tsv"

# ---- 2. sequências ----
Write-Host "placar: sequencias (K=$K, $CasosSeq casos/grupo)" -ForegroundColor Cyan
$fs = @('--casos', "$CasosSeq", '--k', "$K", '--semente', "$([long]$Semente + 1)", '--relatorio', "$out\fuzzk.tsv", '--exemplos', "$out\exemplos-seq.txt", '--quieto')
if ($Jit) { $fs += '--jit' }
$fk = Invoke-A64Capture $fuzz $fs 7200
$seq = if (Test-Path "$out\fuzzk.tsv") { Read-Tsv "$out\fuzzk.tsv" } else { @() }

# ---- 3. testes ELF ----
$elfRows = @()
if (-not $SemElf) {
    Write-Host 'placar: testes ELF' -ForegroundColor Cyan
    $a64test = Get-A64Test
    foreach ($e in Get-ChildItem "$root\build\a64tests\*.elf" | Sort-Object Name) {
        $src = Get-ChildItem "$root\tools\a64tests\$($e.BaseName).*" -Include *.c, *.S -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $src) { continue }          # sobra de um teste apagado
        $nc = (Select-String -Path $src.FullName -Pattern 'a64cmp: nao-comparavel' -SimpleMatch -Quiet)
        $row = [ordered]@{ teste = $e.BaseName; interp = ''; jit = ''; nota = '' }
        $q = $null
        if (-not $nc) { $q = Invoke-A64Oracle $e.FullName $Timeout }
        $engines = @(@{ k = 'interp'; a = @('--argv0', 'prog', $e.FullName) })
        if ($Jit) { $engines += @{ k = 'jit'; a = @('--jit', '--argv0', 'prog', $e.FullName) } }
        foreach ($en in $engines) {
            $r = Invoke-A64Capture $a64test $en.a $Timeout
            if ($r.TimedOut) { $row[$en.k] = 'tempo' }
            elseif ($nc) { $row[$en.k] = if ($r.Code -eq 0) { 'ok' } else { "saída $($r.Code)" }; $row.nota = 'não comparável (autoverificação)' }
            elseif ($q.TimedOut) { $row[$en.k] = '?'; $row.nota = 'oráculo estourou o tempo' }
            elseif ($r.Out -eq $q.Out -and $r.Err -eq $q.Err -and $r.Code -eq $q.Code) { $row[$en.k] = 'igual' }
            else { $row[$en.k] = 'DIFERE'; $row.nota = "saída $($r.Code) × qemu $($q.Code)" }
        }
        if (-not $Jit) { $row.jit = '—' }
        $elfRows += [pscustomobject]$row
        Write-Host ("  {0,-28} interp={1} jit={2} {3}" -f $row.teste, $row.interp, $row.jit, $row.nota)
    }
}

# ---- 4. suíte C ----
$cRows = @()
if (-not $SemC) {
    Write-Host 'placar: suite C' -ForegroundColor Cyan
    $ca = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "$root\tools\a64oracle\csuite\run.ps1", '-Tsv', "$out\csuite.tsv")
    if ($Jit) { $ca += '-Jit' }
    & powershell @ca | Out-Host
    if (Test-Path "$out\csuite.tsv") { $cRows = Read-Tsv "$out\csuite.tsv" }
}

# ---- tabela ----
function N($x) { if ($x -eq $null -or $x -eq '') { 0 } else { [int]$x } }
function Pct($a, $b) { if ((N $b) -eq 0) { '—' } else { '{0:N0}%' -f (100.0 * (N $a) / (N $b)) } }
$md = New-Object System.Text.StringBuilder
$now = [DateTime]::Now.ToString('yyyy-MM-dd HH:mm')
[void]$md.AppendLine("### Placar da verificação")
[void]$md.AppendLine('')
[void]$md.AppendLine("Gerado por ``tools/a64placar.ps1`` em $now (semente $Semente, $Casos casos por grupo$(if ($Jit) { ', com JIT' } else { ', sem JIT' })). Oráculo: QEMU 11.1 ``-cpu max`` em EL0 sob ``tools/a64oracle/mon.c``. Colunas: **exec** = casos em que a instrução deveria executar (o LLVM com os recursos de ``A64_FEATURES`` a conhece e o QEMU executa); **impl.** = desses, quantos o interpretador executa; **batem** = estado final igual ao do QEMU; **div.** = executa e diverge; **UDF ind.** = QEMU executa e nós damos UDF; **dev. UDF** = deveria ser UDF (recurso não anunciado, ou o QEMU dá UDF) e executamos; **FMA?** = divergência só em lanes de FMA que o QEMU do Windows calcula errado e que não deu para isolar; **JIT≠** = JIT diferente do interpretador.")
[void]$md.AppendLine('')
[void]$md.AppendLine('| Frente | Grupos | Casos | Exec | Impl. | Batem | Div. | UDF ind. | Dev. UDF | FMA? | JIT≠ |')
[void]$md.AppendLine('|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
foreach ($fr in @('INT', 'FP', 'SIMD')) {
    $rs = @($g | Where-Object { $_.frente -eq $fr })
    if (-not $rs.Count) { continue }
    $s = @{}; foreach ($c in 'gerados', 'esperado_exec', 'implementadas', 'batem', 'divergem', 'udf_indevido', 'deveria_udf', 'suspeito_fma', 'jit_divergem') { $s[$c] = ($rs | ForEach-Object { N $_.$c } | Measure-Object -Sum).Sum }
    [void]$md.AppendLine("| $fr | $($rs.Count) | $($s.gerados) | $($s.esperado_exec) | $($s.implementadas) ($(Pct $s.implementadas $s.esperado_exec)) | $($s.batem) ($(Pct $s.batem $s.esperado_exec)) | $($s.divergem) | $($s.udf_indevido) | $($s.deveria_udf) | $($s.suspeito_fma) | $(if ($Jit) { $s.jit_divergem } else { '—' }) |")
}
$seqDiv = ($seq | ForEach-Object { (N $_.divergem) + (N $_.deveria_udf) } | Measure-Object -Sum).Sum
$seqN = ($seq | ForEach-Object { N $_.gerados } | Measure-Object -Sum).Sum
$seqJ = ($seq | ForEach-Object { N $_.jit_divergem } | Measure-Object -Sum).Sum
[void]$md.AppendLine('')
[void]$md.AppendLine("Sequências (K=$K instruções válidas misturadas por caso, divergências minimizadas): $seqN casos, **$seqDiv divergentes**$(if ($Jit) { ", $seqJ com JIT≠interp" }).")
[void]$md.AppendLine('')
[void]$md.AppendLine('<details><summary>Por grupo de codificação</summary>')
[void]$md.AppendLine('')
[void]$md.AppendLine('| Grupo | Frente | Casos | Exec | Impl. | Batem | Div. | UDF ind. | Dev. UDF | FMA? | JIT≠ |')
[void]$md.AppendLine('|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
foreach ($r in $g) {
    $bad = (N $r.divergem) + (N $r.udf_indevido) + (N $r.deveria_udf) + (N $r.jit_divergem)
    $name = if ($bad) { "**$($r.grupo)**" } else { $r.grupo }
    [void]$md.AppendLine("| $name | $($r.frente) | $($r.gerados) | $($r.esperado_exec) | $($r.implementadas) | $($r.batem) | $($r.divergem) | $($r.udf_indevido) | $($r.deveria_udf) | $($r.suspeito_fma) | $(if ($Jit) { $r.jit_divergem } else { '—' }) |")
}
[void]$md.AppendLine('')
[void]$md.AppendLine('</details>')
if ($elfRows.Count) {
    $okE = @($elfRows | Where-Object { $_.interp -in 'igual', 'ok' -and ($_.jit -in 'igual', 'ok', '—') }).Count
    [void]$md.AppendLine('')
    [void]$md.AppendLine("Testes ELF (``build/a64tests``, a64test × oráculo): **$okE de $($elfRows.Count)** passam.")
    $badE = @($elfRows | Where-Object { -not ($_.interp -in 'igual', 'ok' -and ($_.jit -in 'igual', 'ok', '—')) })
    if ($badE.Count) {
        [void]$md.AppendLine('')
        [void]$md.AppendLine('| Teste | Interp | JIT | Nota |')
        [void]$md.AppendLine('|---|---|---|---|')
        foreach ($r in $badE) { [void]$md.AppendLine("| $($r.teste) | $($r.interp) | $($r.jit) | $($r.nota) |") }
    }
}
if ($cRows.Count) {
    $okC = @($cRows | Where-Object { $_.interp -eq 'igual' -and ($_.PSObject.Properties.Name -notcontains 'jit' -or $_.jit -eq 'igual') -and ($_.PSObject.Properties.Name -notcontains 'qemu' -or $_.qemu -eq 'igual') }).Count
    [void]$md.AppendLine('')
    [void]$md.AppendLine("Suíte diferencial C (``tools/a64oracle/csuite``: x86_64 nativo × arm64 -O0/-O2/-O3 no a64test e no QEMU): **$okC de $($cRows.Count)** binários iguais.")
    $badC = @($cRows | Where-Object { -not ($_.interp -eq 'igual' -and ($_.PSObject.Properties.Name -notcontains 'jit' -or $_.jit -eq 'igual') -and ($_.PSObject.Properties.Name -notcontains 'qemu' -or $_.qemu -eq 'igual')) })
    foreach ($r in $badC) { [void]$md.AppendLine("- $($r.programa) -$($r.opt): interp=$($r.interp)$(if ($r.PSObject.Properties.Name -contains 'jit') { " jit=$($r.jit)" }) qemu=$($r.qemu)") }
}
$table = $md.ToString().TrimEnd()
[IO.File]::WriteAllText("$out\placar.md", $table + "`n", (New-Object Text.UTF8Encoding $false))
Write-Host $table

if (-not $SemDoc) {
    $doc = "$root\docs\tradutor-arm64.md"
    $t = [IO.File]::ReadAllText($doc, [Text.Encoding]::UTF8)
    $begin = '<!-- placar:inicio -->'; $end = '<!-- placar:fim -->'
    $block = "$begin`n$table`n$end"
    $i = $t.IndexOf($begin); $j = $t.IndexOf($end)
    if ($i -ge 0 -and $j -gt $i) { $t = $t.Substring(0, $i) + $block + $t.Substring($j + $end.Length) }
    else {
        $k = $t.IndexOf('## Estado')
        if ($k -lt 0) { $t = $t.TrimEnd() + "`n`n## Estado`n`n$block`n" }
        else {
            # depois da tabela de estado (primeira linha em branco após a última linha que começa com '|')
            $lines = $t.Substring($k) -split "`n"; $pos = $k; $seen = $false
            foreach ($l in $lines) { $pos += $l.Length + 1; if ($l.StartsWith('|')) { $seen = $true } elseif ($seen -and $l.Trim() -eq '') { break } }
            if ($pos -gt $t.Length) { $pos = $t.Length }
            $t = $t.Substring(0, $pos) + "`n$block`n`n" + $t.Substring($pos)
        }
    }
    [IO.File]::WriteAllText($doc, $t, (New-Object Text.UTF8Encoding $false))
    Write-Host "placar: tabela escrita em docs\tradutor-arm64.md" -ForegroundColor Green
}
$bad = (($g | ForEach-Object { (N $_.divergem) + (N $_.deveria_udf) + (N $_.jit_divergem) }) | Measure-Object -Sum).Sum + $seqDiv
if ($bad) { exit 1 } else { exit 0 }
