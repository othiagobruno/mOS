# mOS - SQLite (amálgama oficial, domínio público: .tools\sqlite-dl\sqlite-amalgamation-*) como a libsqlite3 do macOS:
# build\sqlite\libsqlite3.dylib (Mach-O x86_64, install name /usr/lib/libsqlite3.dylib), que o build.ps1 põe na imagem.
# O carregador do mOS (kernel/compat/macho.c) carrega do disco as dylibs de caminho absoluto que não são do libSystem.
# Opções próximas das da Apple: FTS3/FTS5, R*Tree, metadados de coluna, threadsafe, sem extensões carregáveis.
#   .\tools\build-sqlite.ps1           (só recompila se faltar a dylib)
#   .\tools\build-sqlite.ps1 -Force
#   .\tools\build-sqlite.ps1 -Arch arm64  (build\sqlite-arm64: slice arm64 do tradutor, tools\build-libs-arm64.ps1)
param([switch]$Force, [ValidateSet('x86_64', 'arm64')][string]$Arch = 'x86_64')
$tgt = if ($Arch -eq 'arm64') { 'aarch64-macos.11.0' } else { 'x86_64-macos.11.0' }
$root = Split-Path $PSScriptRoot -Parent
$zig = (Get-ChildItem "$root\.tools" -Directory -Filter 'zig-*' | Where-Object { Test-Path "$($_.FullName)\zig.exe" } | Select-Object -First 1).FullName + '\zig.exe'
$src = Get-ChildItem "$root\.tools\sqlite-dl" -Directory -Filter 'sqlite-amalgamation-*' -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
if (-not $src) { Write-Host 'falta o amálgama do SQLite em .tools\sqlite-dl (sqlite.org/download.html)' -ForegroundColor Yellow; exit 0 }
$out = "$root\build\sqlite" + $(if ($Arch -eq 'arm64') { '-arm64' } else { '' })
$dylib = "$out\libsqlite3.dylib"
if ((Test-Path $dylib) -and -not $Force) { exit 0 }
New-Item -ItemType Directory -Force $out | Out-Null
$defs = @('-DSQLITE_THREADSAFE=2', '-DSQLITE_ENABLE_FTS3', '-DSQLITE_ENABLE_FTS3_PARENTHESIS', '-DSQLITE_ENABLE_FTS4',
          '-DSQLITE_ENABLE_FTS5', '-DSQLITE_ENABLE_RTREE', '-DSQLITE_ENABLE_COLUMN_METADATA', '-DSQLITE_ENABLE_DBSTAT_VTAB',
          '-DSQLITE_ENABLE_PREUPDATE_HOOK', '-DSQLITE_ENABLE_SESSION', '-DSQLITE_ENABLE_UNLOCK_NOTIFY',
          '-DSQLITE_ENABLE_API_ARMOR', '-DSQLITE_OMIT_LOAD_EXTENSION', '-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1',
          '-DHAVE_USLEEP=1', '-DSQLITE_MAX_MMAP_SIZE=0')
& $zig cc -target $tgt -O2 -g0 -fPIC -shared @defs "$($src.FullName)\sqlite3.c" -o $dylib `
    '-Wl,-install_name,/usr/lib/libsqlite3.dylib' '-Wl,-compatibility_version,9.0.0' '-Wl,-current_version,357.0.0' 2>&1 |
    Select-String ' error' | ForEach-Object { "  $_" }
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dylib)) { Write-Host 'libsqlite3.dylib FALHOU' -ForegroundColor Red; exit 1 }
Write-Host ("OK: $dylib ({0:N0} KiB, {1})" -f ((Get-Item $dylib).Length / 1KB), $src.Name)
