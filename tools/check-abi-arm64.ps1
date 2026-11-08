# mOS - confere que o libSystem arm64 (triplo arm64-apple-macos11-elf, tools\build-libsystem-arm64.ps1) tem a MESMA ABI
# que código compilado com -target aarch64-macos (Mach-O, o que os apps arm64 do macOS usam).
#
# Como: compila cada arquivo C das duas formas até o IR do LLVM e compara. A ABI de C (classificação de argumentos e
# retorno, HFA, sret/x8, byval, extensão signext/zeroext feita pelo chamador, variádicos com va_list = char*, long double
# = double, char com sinal, layout de structs) é decidida pelo clang e fica escrita no IR; o backend escolhe a convenção
# de chamada (CC_AArch64_DarwinPCS, variádicos na pilha, x18 reservado) pelo SO do triplo, que é o mesmo nos dois.
# Diferenças aceitas, por não serem ABI: o triplo, a mangling do datalayout (m:o x m:e, o "_" do Mach-O) e o alinhamento
# PREFERIDO de i8/i16 em globais (i8:8:32 no ELF: só globais ficam mais alinhadas; o alinhamento de ABI é o mesmo).
# Qualquer outra linha diferente é falha.
#   .\tools\check-abi-arm64.ps1                 # tools\a64tests\libsys_abi*.c + todos os .c do userland\libsystem
#   .\tools\check-abi-arm64.ps1 -Only plist.c   # um arquivo (mostra o diff)
param([string]$Only, [switch]$Quiet)
$root = Split-Path $PSScriptRoot -Parent
$zigDir = Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1
$zig = Join-Path $zigDir.FullName 'zig.exe'
$inc = Join-Path $zigDir.FullName 'lib\include'
$ls = "$root\userland\libsystem"
$tmp = "$root\build\abi-arm64"
New-Item -ItemType Directory -Force $tmp | Out-Null
$common = @('-mcpu=apple-m1', '-O2', '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-fPIC', '-ffixed-x18', '-fno-common',
    '-fsigned-char', '-w', '-isystem', $inc, "-I$ls\kshim", "-I$ls", '-S', '-emit-llvm')

function Normalize([string]$path) {
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($l in [IO.File]::ReadAllLines($path)) {
        if ($l -cmatch '^(target (datalayout|triple)|source_filename|; ModuleID)') { continue }
        if ($l -cmatch '^!\d+ = ') { continue }                              # metadados (hashes de identificação)
        $l = $l -creplace '@"\\01', '@"'                                     # nome literal de __asm__("...") no Mach-O
        $l = $l -creplace '@"([A-Za-z$._][\w$.]*)"', '@$1'                    # aspas desnecessárias
        if ($l -cmatch '^@' -or $l -cmatch '\bptr @[\w.$]+, align \d+') { $l = $l -creplace ', align \d+', '' }
        # i8/i16 (e vetores de i8) locais também ganham o alinhamento preferido (alloca e os acessos): não é ABI
        if ($l -cmatch '(alloca|load|store( volatile)?) (i(8|16)\b|\[\d+ x i(8|16)\]|<\d+ x i8>)') { $l = $l -creplace ', align \d+', '' }
        # variáveis locais (alloca) e os memcpy/memset sobre elas: o alinhamento de uma variável local não é ABI (o layout
        # dos structs, que é, está nas definições de tipo e nos getelementptr, comparados)
        if ($l -cmatch '\balloca\b') { $l = $l -creplace ', align \d+', '' }
        if ($l -cmatch '@llvm\.mem(set|cpy|move)') { $l = $l -creplace '\balign \d+ ', '' }
        $l = $l -creplace '^@llvm\.compiler\.used = ', '@llvm.used = '         # __attribute__((used)): Mach-O x ELF
        $out.Add($l)
    }
    return $out
}

$files = @()
if ($Only) { $files = $Only -split ',' | ForEach-Object { if (Test-Path $_) { (Resolve-Path $_).Path } elseif (Test-Path "$ls\$_") { "$ls\$_" } else { "$root\tools\a64tests\$_" } } }
else { $files = @(Get-ChildItem "$root\tools\a64tests\libsys_abi*.c" -ErrorAction SilentlyContinue | ForEach-Object FullName) + @(Get-ChildItem "$ls\*.c" | ForEach-Object FullName) }
$ok = 0; $bad = 0; $skip = 0; $funcs = 0
foreach ($f in $files) {
    $n = [IO.Path]::GetFileNameWithoutExtension($f)
    & $zig clang --target=arm64-apple-macos11 @common $f -o "$tmp\$n.macho.ll" 2>$null
    $r1 = $LASTEXITCODE
    & $zig clang --target=arm64-apple-macos11-elf @common $f -o "$tmp\$n.elf.ll" 2>$null
    if ($r1 -ne 0 -or $LASTEXITCODE -ne 0) { $skip++; Write-Host "  (pulado: $n nao compila)" -ForegroundColor DarkGray; continue }
    $a = Normalize "$tmp\$n.macho.ll"; $b = Normalize "$tmp\$n.elf.ll"
    $funcs += @($a | Where-Object { $_ -cmatch '^define ' }).Count
    $d = Compare-Object $a $b -SyncWindow 20 -CaseSensitive
    if ($d) {
        $bad++
        Write-Host "DIFERENTE: $n ($(@($d).Count) linhas)" -ForegroundColor Red
        $d | Select-Object -First 40 | ForEach-Object { "  $($_.SideIndicator) $($_.InputObject)" }
    } else { $ok++; if (-not $Quiet) { Write-Host "  igual: $n" } }
}
Write-Host "ABI arm64 (IR do clang, Mach-O x ELF): $ok arquivo(s) iguais ($funcs funcoes definidas), $bad diferente(s), $skip pulado(s)" -ForegroundColor $(if ($bad -or $skip) { 'Red' } else { 'Green' })
if ($bad -or $skip) { exit 1 }
