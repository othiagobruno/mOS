# mOS - runtime do tradutor arm64 (frente RUNTIME, docs/tradutor-arm64.md): build\a64rt.mos
#   ELF x86_64 freestanding ligado em A64RT_BASE (userland\a64rt\a64rt_abi.h), com o motor (userland\a64\*.c) e
#   o runtime (userland\a64rt\*). Vai para a imagem em /Sistema/Biblioteca/a64/a64rt.mos (build.ps1).
#   -NoJit : sem o JIT mesmo que userland\a64\jit*.c exista (o motor fica só com o interpretador)
#   -Parcial : aceita símbolos do motor ainda não escritos pelas outras frentes (viram 0; só para iterar)
#   No processo, MOS_A64_ENGINE=interp|jit escolhe o motor (padrão: JIT quando compilado).
param([switch]$NoJit, [switch]$Parcial)
$root = Split-Path $PSScriptRoot -Parent
$zig = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue | Where-Object { Test-Path (Join-Path $_.FullName 'zig.exe') } | Select-Object -First 1
if ($zig) { $zig = Join-Path $zig.FullName 'zig.exe' } elseif ($env:MOS_ZIG) { $zig = $env:MOS_ZIG } else { $zig = (Get-Command zig -ErrorAction SilentlyContinue).Source }
if (-not $zig) { Write-Host 'zig nao encontrado' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force "$root\build" | Out-Null
$jit = (-not $NoJit) -and (Get-ChildItem "$root\userland\a64\jit*.c" -ErrorAction SilentlyContinue)
$src = @(Get-ChildItem "$root\userland\a64\*.c" | Where-Object { $jit -or $_.Name -notlike 'jit*' } | ForEach-Object FullName)
$src += Get-ChildItem "$root\userland\a64\*.S" -ErrorAction SilentlyContinue | Where-Object { $jit -or $_.Name -notlike 'jit*' } | ForEach-Object FullName
$src += Get-ChildItem "$root\userland\a64rt\*" -Include *.c, *.S | ForEach-Object FullName
$defs = @('-DA64_RUNTIME_MOS'); if ($jit) { $defs += '-DA64_HAVE_JIT' }
$cc = @('cc', '-target', 'x86_64-freestanding-none', '-O2', '-g0', '-ffreestanding', '-fno-stack-protector', '-nostdlib', '-static', '-no-pie', '-fpie',
        '-mcx16', '-Wall', '-Wno-unused-function', '-Wno-unused-parameter', '-fno-strict-aliasing') + $defs +
      @("-I$root\userland\a64", "-I$root\userland\a64rt", '-Wl,--image-base=0x400041000000', '-Wl,-e,_start')
$out = & $zig @cc $src -o "$root\build\a64rt.mos" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0 -and $Parcial) {
    # símbolos que outras frentes ainda não escreveram: viram funções que param com ud2 (SIGILL no runtime)
    $miss = $out | Select-String 'undefined symbol: (\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique
    if ($miss) {
        Write-Host ("aviso (-Parcial): {0} símbolo(s) do motor ainda ausentes: {1}" -f $miss.Count, ($miss -join ' ')) -ForegroundColor Yellow
        $stub = ($miss | ForEach-Object { "void $_(void) { __builtin_trap(); }" }) -join "`n"
        Set-Content -Path "$root\build\a64rt-parcial.c" -Value $stub -Encoding ascii
        $out = & $zig @cc $src "$root\build\a64rt-parcial.c" -o "$root\build\a64rt.mos" 2>&1 | ForEach-Object { "$_" }
    }
}
$out
if ($LASTEXITCODE -ne 0) { Write-Host 'a64rt build FAILED' -ForegroundColor Red; exit 1 }
$kb = [int]((Get-Item "$root\build\a64rt.mos").Length / 1024)
Write-Host ("ok: build\a64rt.mos ({0} KiB, motor: {1})" -f $kb, $(if ($jit) { 'interpretador + JIT' } else { 'interpretador' }))
