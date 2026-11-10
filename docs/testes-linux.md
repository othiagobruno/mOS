# Testes do machotest no Linux

O `tools/machotest.c` roda no Windows (VEH, `VirtualAlloc`) e no Linux (seccomp + `SIGSYS` para o `int 0x80` do libSystem, `mmap`). Os dois usam o mesmo `tools/host_enter.S`: no Linux a convenção é System V (`rdi`, `rsi`, `rdx`); no Windows continua `rcx`, `rdx`, `r8`.

A lista de asserções de cada teste mora no fonte do teste (`CHECK` em `userland/tests-catalyst/main.m`, os `.swift` do OpenSwiftUI). `tools/test-*.ps1` e `tools/test-*.sh` compilam esses mesmos arquivos. O `.sh` não tem uma segunda lista.

## Compilar

Zig 0.14 ou mais novo. O alvo do libSystem é `x86_64-freestanding-none` (o `x86_64-apple-macos11-elf` do `tools/build-libsystem.ps1` não existe no Zig 0.17). O `patch-ehframe.ps1` é PowerShell e não entra neste caminho: sem ele, exceção C++/ObjC no harness fica sem `.eh_frame` reescrito. Os testes abaixo não dependem disso.

```bash
mkdir -p build
zig cc -target x86_64-freestanding-none -O2 -ffreestanding -fno-stack-protector \
    -fno-builtin -nostdlib -static -fpie -mno-red-zone -w \
    -Iuserland/libsystem/kshim \
    -Wl,--image-base=0x400000100000 -Wl,-e,cx_start -Wl,--no-gc-sections \
    userland/libsystem/*.c userland/libsystem/*.S \
    -o build/libSystem.mos

zig cc -target x86_64-linux-gnu -O2 -std=gnu11 -DMOS_HOST_TEST -fno-strict-aliasing \
    -Ikernel/include -Itools \
    tools/machotest.c tools/host_unix.c tools/host_gpu.c tools/host_qos.c tools/host_id.c tools/host_bootstrap.c tools/host_enter.S kernel/compat/macho.c \
    -o build/machotest -pthread
```

No Windows o comando continua o de `docs/porte-vscode.md` (`zig cc -target x86_64-windows-gnu`). O `#ifdef _WIN32` preserva a VEH, o `VirtualAlloc` e o watchdog por `SuspendThread`.

## Catalyst

```bash
bash tools/test-catalyst.sh
```

O script compila `userland/frameworks/UIKit/*.m` (sem ARC: o fonte usa `release` explícito), liga o teste com `-fobjc-arc` e roda `build/machotest`. O placar é a linha `catalyst: N/N ok`. Avisos do compilador vão para `build/catalysttest/compilacao.txt`.

Nesta branch o `main.m` tem 33 `CHECK` (a `main` atual). O script imprimiu `catalyst: 33/33 ok` e saiu 0. O processo ainda morre depois, no laço, com o mesmo `SIGSEGV` em `cx_lookup` descrito abaixo.

Na branch `cursor/uikit-catalyst-rest-2c8c` (PR #21, commit `909d919`, worktree só de leitura, sem commit nela) o mesmo harness com os fontes de lá deu:

```
catalyst: 55/55 ok
```

Todas as 55 linhas `ok` saíram. Em seguida o laço do `NSApplication` caiu:

```
[abrir-url] serviço com.mos.abrir-url.com.mos.app indisponível (1102)
objc[mOS]: -[NSNotification lastObject] ainda não implementado no mOS (devolvendo nil)
[machotest] exceção sinal 11 em rip=4000002f7a9d (endereço 0)
  em libSystem: cx_lookup+0x1d
[machotest] código de saída: -11
```

O código -11 vira 245 no `$?` do shell (8 bits). O placar já tinha sido impresso. Isso é o libSystem (`NSNotification`), não o carregador.

## Sonda de IPC (L20)

`tools/testdata/ipc-harness.c` chama `mach_port_construct`, `socketpair` e um futex entre duas threads.

```bash
zig cc -target x86_64-macos -O1 -fno-stack-protector \
    tools/testdata/ipc-harness.c -o build/ipc-harness -Wl,-undefined,dynamic_lookup
./build/machotest build/libSystem.mos build build/ipc-harness
```

Saída obtida:

```
[machotest] mach_port_construct ok nome=69632 qlimit=8
ipc: construct kr=0 nome=69632 type_kr=0 type=0x30000
ipc: socketpair r=0 fds=3,4 foi=4 voltou=4 texto=ping/pong
ipc: volta wr=4 rd=4
ipc: futex create=0 wake=1 join=0 wait_r=0 thread_ret=0
ipc: ok
[machotest] código de saída: 0
```

`type=0x30000` é recepção (`0x20000`) mais envio (`0x10000`), o `MPO_INSERT_SEND_RIGHT`. O `socketpair` anda nos dois sentidos. O futex acorda a outra thread (`wake=1`, `wait_r=0`).

## Electron

O binário usado foi o Electron 28.3.3 darwin-x64 oficial (`Electron.app`), com `MACHOTEST_PATH=/Aplicativos/Electron.app/Contents/MacOS/Electron` e a raiz do VFS apontando para essa árvore. Três frameworks que o `libsystem_dylib_ok` não cobre (StoreKit, CoreData, ImageCaptureCore) precisam existir como dylib vazio no VFS, senão o `macho_load` recusa o processo. Não são implementação: só satisfazem o `LC_LOAD_DYLIB`.

```bash
export MACHOTEST_PATH=/Aplicativos/Electron.app/Contents/MacOS/Electron
export MACHOTEST_WATCHDOG=25000
./build/machotest build/libSystem.mos "$VFS" \
    "$VFS/Aplicativos/Electron.app/Contents/MacOS/Electron" \
    --no-sandbox --disable-gpu --in-process-gpu --enable-logging
```

Passou de `mach_port_construct`. A saída real, até o erro seguinte:

```
macho: 9 imagens carregadas (executável + bibliotecas do bundle)
macho: 95 símbolo(s) ausente(s) no libSystem do mOS (ex.: __Unwind_Resume)
objc: classe não definida ao adicionar -setTimeoutInterval:
[machotest] mach_port_construct ok nome=69632 qlimit=5
[42:1110/141635.649000:ERROR:thermal_state_observer_mac.mm(89)] ThermalStateObserverMac unable to register to power notifications. Result: 1000000
[machotest] exceção sinal 11 em rip=40000333e553 (endereço 18)
  rdi=402905c0fc80 rsi=18 rdx=fffffffc rax=18 rbx=0
[machotest] falha na thread 13; encerrando
```

O processo chega a 13 threads (o `pthread_create` do libSystem usa o futex e o `socketpair` do harness). O próximo erro é esse `SIGSEGV` na thread 13, com `rbx == 0` e falta no endereço `0x18`, logo depois do `ThermalStateObserverMac` recusar o registro de energia (`Result: 1000000`). Não há `FATAL: mach_port_construct`.

Antes disso o harness também precisou de duas syscalls que o Electron usa no caminho do ICU e do número de CPUs, senão ele morria mais cedo e nem chegava na porta:

- `SYS_MMAP` com `b = 4` copia o arquivo (`icudtl.dat`). Sem isso o ICU lia página zerada e abortava com `U_INVALID_FORMAT_ERROR`.
- `SYS_FDPATH` devolve o caminho do descritor. `SYS_CPUS` devolve `sysconf(_SC_NPROCESSORS_ONLN)`.

Nada disso exigiu mudança em `kernel/compat/macho.c` nem em `kernel/proc/process.c`. O kernel já tem essas syscalls; o que faltava era o harness.

## Electron até a janela (VS Code, Cursor, Slack — 10 nov. 2026)

`tools/test-electron-janela.sh <libSystem.mos> <machotest> <App.app> [segundos]` monta a raiz e roda o app x86_64
oficial com `--no-sandbox --disable-gpu --in-process-gpu --enable-features=NetworkServiceInProcess`. Passa se sair
`[gui] janela 1`. Para o VS Code, o CryptoKit compilado entra com `MOS_CRYPTOKIT=<build>/CryptoKit`.

O que o harness precisou para isso (o kernel já fazia; faltava aqui):

- `MACH_OP_PEEK`: fila vazia é `KERN_FAILURE`. Antes caía no `KERN_INVALID_ARGUMENT` e o `WaitableEvent` do Chromium
  dava CHECK (`mach_port_peek: (os/kern) invalid argument`).
- `USER_END` igual ao do kernel (8 entradas do PML4, 4 TiB). Com 512 GiB, a reserva do V8 passava do fim e todo
  `dlopen` de módulo `.node` falhava com "sem memória para a imagem".
- Entradas de memória: a permissão da entrada vale no `MACH_VM_MAP` (pedir escrita numa entrada só-leitura é
  `KERN_INVALID_RIGHT`, o `CheckPlatformHandlePermissionsCorrespond` do Chromium confere) e a entrada feita de um
  endereço já mapeado usa as mesmas páginas.
- `SYS_PWRITE`, `SYS_FTRUNCATE`, `SYS_AIO` (o `fsync` do libSystem), `SYS_CHMOD`/`SYS_FCHMOD` e o `mode` do stat;
  `SYS_MKDIR` devolve `ENOENT` sem o pai (o `mkdir -p` do Node depende disso).

Achado no `kernel/compat/macho.c` (vale no mOS também): depois do `image_persist`, `export_is_stub` lia o `__TEXT`
fora do buffer que sobrou (só comandos de carga e trie). O `dlopen` do primeiro `.node` contra o Electron Framework
derrubava o harness.

Onde cada app chega (libSystem com os seletores de `tools/test-electron-m3.sh`):

| App | Módulos `.node` (ausentes) | Janela | Para em |
|---|---|---|---|
| VS Code 1.145.3 x64 | policy-watcher, spdlog, sqlite3, keymapping, os-proxy-resolver (0) | `"Visual Studio Code" 1200x800` | renderer: o harness não lança o Helper (`SYS_SPAWNVEC`) |
| Cursor 3.29.15 x64 | policy-watcher, spdlog, cursor-proclist, sqlite3 (0) | `"Cursor Agents" 1280x800` | o mesmo |
| Slack 4.53.287 x64 | cf-prefs, slack-desktop-utils, file-handler-info, electron-native-auth, keymapping (0) | menus (6); a janela espera o `ready-to-show` do renderer | o mesmo |

O conteúdo da janela, o terminal, as extensões e o login só se provam na VM, onde os Helpers sobem.

## O que não rodou

| Script | Motivo |
|---|---|
| `tools/test-uikit-swift.sh` | Não há `swiftc` nem `build/swift/full` nesta máquina. O `.ps1` usa o toolchain em `.tools/swift/.../swiftc.exe`. |
| `tools/test-openswiftui-deps.sh` | O mesmo toolchain, mais as bibliotecas de `tools/build-openswiftui-deps.ps1`. |
| `tools/test-openswiftui-path.sh` | O mesmo. O placar esperado pelo `.ps1` é a linha `TUDO OK` do `tests/path/main.swift`. |
| QEMU / imagem | Não faz parte deste harness. Os scripts de VM continuam os do repositório. |

`pwsh` não estava instalado. Os `.sh` são o caminho no Linux; os `.ps1` continuam o do Windows.
