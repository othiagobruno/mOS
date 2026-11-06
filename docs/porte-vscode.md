# Porte do VS Code para o mOS

Objetivo: rodar o **Visual Studio Code para macOS (Intel x64)**, sem modificações, como `/Aplicativos/VSCode.app`.
Versão usada nas medições: Code 1.145.2 (`com.microsoft.VSCode`), Electron/Chromium embutido.

## Onde estamos (6 nov. 2026)

| Etapa | Estado |
|---|---|
| Ler o bundle (`Info.plist`, executável) | ✅ o mOS reconhece o app como "Code 1.145.2" |
| Carregar o executável **e** as bibliotecas de dentro do app (`@rpath`, `@executable_path`, `@loader_path`) | ✅ 6 imagens, 207 MiB: `Code`, `Electron Framework`, `Squirrel`, `Mantle`, `ReactiveObjC`, `libffmpeg.dylib` |
| Ligar símbolos entre as imagens (export trie, reexports) e com o libSystem do mOS | ✅ |
| Registrar as classes Objective-C de todas as imagens | ✅ |
| Rodar os inicializadores e entrar no código do Chromium | ✅ |
| Base Mach/Darwin, `malloc` zones, `sysctl`, `CFRunLoop`, `dispatch_once`, `kqueue`, `mach_msg` (no processo) | ✅ (arquivos novos no libSystem, veja abaixo) |
| Threads do Chromium com TLS do macOS (`gs:[chave*8]`) e `thread_local` por thread | ✅ (kernel troca a base do GS por thread) |
| Node.js/libuv inicializa (pipes), telas enumeradas (`NSScreen`, `CGDisplay*`) | ✅ |
| Processo principal do VS Code estável no mOS | ✅ roda minutos sem chamar nenhuma função ausente e sem falhar |
| Código JS do VS Code (`Contents/Resources/app`) na imagem e executando, laço do AppKit (`-[NSApplication run]` de `nsurl.c` atendendo o CFRunLoop) | ✅ |
| Thread principal reconhecida (`pthread_main_np`, `+[NSThread isMainThread]`, `performSelectorOnMainThread:`, `CFRunLoopPerformBlock` no loop certo) | ✅ (antes toda thread "era a principal" e o Chromium rodava `[NSApp run]` na thread NetworkConfigWatcher, levando o V8 à thread errada) |
| Primeiro processo auxiliar (`Code Helper`, serviço de rede) lançado com `posix_spawn` | ✅ |
| Rendezvous Mach entre processos (`bootstrap_look_up ...MachPortRendezvousServer.<ppid>`) | ❌ sessão "IPC Mach"; `getppid()` agora vem do kernel (antes era sempre 1) |
| Demais auxiliares (GPU, renderização) e janela na tela | ❌ |
| Conteúdo de janelas CoreAnimation/IOSurface na tela (CALayer, CALayerHost, IOSurface por porta Mach) | ✅ app de teste no QEMU; com o VS Code ainda não visto (seção "CoreAnimation e IOSurface") |

No Terminal do mOS:

```
visitante@mOS ~ % macinfo /Aplicativos/VSCode.app
...
Veredito: modo experimental — carrega, mas para na primeira chamada a uma das 2076 funções ausentes
→ abre só no modo experimental (para na 1ª função que o mOS não tem)
visitante@mOS ~ % run /Aplicativos/VSCode.app
[3] /Aplicativos/VSCode.app iniciado (app do macOS, ring 3)
dyld[mOS]: símbolo não implementado no libSystem do mOS: _pthread_mach_thread_np
```

### Falha atual (6 nov. 2026, 17:50)

O processo principal (pid 3) fica vivo e lança o `Code Helper` do serviço de rede (pid 4). O helper morre em
`base/apple/mach_port_rendezvous_mac.cc:256`: `bootstrap_look_up com.microsoft.VSCode.MachPortRendezvousServer.3`
devolve 1102 (serviço desconhecido) → "Mach rendezvous failed, terminating process". Falta o bootstrap Mach entre
processos (kernel `machipc.c` + `mach.c`) e a fonte `DISPATCH_SOURCE_TYPE_MACH_RECV` no `dispatch.c`, com que o servidor
do processo principal atende os pedidos. Depois disso vêm os helpers de GPU/renderização e a janela (IOSurface/CoreAnimation).

Corrigido nesta etapa: toda thread se dizia a principal (`pthread_main_np` sempre 1, `+[NSThread isMainThread]` sempre
YES), então o Chromium criava um `MessagePumpNSApplication` na thread NetworkConfigWatcher e rodava `[NSApp run]` lá, que
acabava executando JS sem isolate do V8 (falha em `Electron Framework+0xBF0A4A`). Também: `performSelectorOnMainThread:`
passa pela fila principal do GCD, `CFRunLoopPerformBlock` roda no loop pedido, `getppid()` vem do kernel (`SYS_PROCLIST`).

## Tamanho do que falta (medido com `tools/machocheck.c`)

Caminho crítico (o que o executável `Code` carrega ao abrir):

- **65** bibliotecas do sistema pedidas; **55** não existem no mOS (IOKit, Security, IOSurface, Metal, CoreVideo,
  CoreMedia, AVFoundation, SystemConfiguration, CFNetwork, CoreServices, Carbon, libcups...).
- **6.038** imports; **2.273** sem implementação (**2.017** funções distintas). Os maiores grupos:
  libSystem 371, AppKit 293, CoreFoundation 173, CoreGraphics 149, Security 100, IOKit 75, ApplicationServices 75,
  CoreText 73, CoreVideo 65, CoreMedia 53, libcups 51, Foundation 45.
- O libSystem do mOS exporta hoje 866 símbolos.

O bundle inteiro (com helpers, `.node` nativos, ripgrep, Copilot etc.) tem 46 imagens Mach-O, 81 bibliotecas do
sistema e 5.955 imports sem implementação.

## O que já foi feito no libSystem e no kernel (6 nov. 2026)

Arquivos novos em `userland/libsystem/` (todos documentados no cabeçalho):

| Arquivo | Conteúdo |
|---|---|
| `mach.c` | portas (nomes), `pthread_mach_thread_np`, `vm_page_size`, `mach_vm_region`, semáforos Mach, `mach_msg` dentro do processo, `notify(3)`, IOKit mínimo, SIP |
| `mzone.c` | `malloc_zone_t` com o layout real (o Chromium troca os ponteiros da zona padrão) |
| `darwin.c` | UTF-8 (`mbtowc`...), `os_log` desligado, `sysctl`/`sysctlbyname` (mOS se apresenta como Darwin 23 / macOS 14), atributos de pthread, `getrlimit`, constantes `CFString` reais, ponte CFString/CFBundle/CFDictionary, `thread_local` por thread (`cx_tlv_get`) |
| `tlv.S` | `cx_tlv_bootstrap`, o thunk de TLV que preserva todos os registradores |
| `nsurl.c` | `NSURL` (arquivos), `CFURL*`, `NSBundle` por URL, `NSThread`, `NSProcessInfo` (versão, estado térmico) |
| `literals.c` | literais constantes do Xcode 15+ (`@{}`, `@[]`, `@42` em `__objc_dictobj`...) |
| `dispatch.c` | `dispatch_once` completo; filas, `async`/`sync`/`after` executando na hora |
| `cfrunloop.c` | `CFRunLoop` por thread: fontes, timers, observadores, `RunInMode`, `WakeUp` |
| `kqueue.c` | `kqueue`/`kevent64` no processo: `EVFILT_USER`, `MACHPORT`, `TIMER`, `WRITE` |
| `cgdisplay.c` | `CGDisplay*` e métodos de `NSScreen` |

Mudanças em arquivos que já existiam: `foundation.c` (NSBundle por instância), `extra.c` (`mmap` com reserva/`mprotect`,
TSD por thread via GS, `pthread_cond_timedwait` com prazo), `libc.c` (`pipe` real, `abort` informa quem chamou),
`objc.c` (ganchos `cx_darwin_init` e `cx_darwin_literals`, feitos pela sessão dona do runtime).
Kernel: `SYS_MMAP` com reserva (`b=1`) e alocação de trecho (`b=2`); `thread_t.gs_base` com `CR4.FSGSBASE` e troca do
`IA32_GS_BASE` em `switch_to`; `/Sistema/Biblioteca/stderr-serial` copia a saída dos apps para o log serial.

Diagnóstico: `tools/stage-vscode.ps1 -Diagnostico` liga `/Sistema/Biblioteca/dyld-registrar` (funções ausentes avisam e
devolvem 0 em vez de encerrar) e o `stderr-serial`. `tools/machocheck.c --calls <imagem> <endereço>` lista as chamadas a
imports perto de um endereço (para achar quem chamou `abort`); `--sym <libSystem.mos> <endereço>` simboliza falhas no libSystem.

## Roteiro (ordem em que o Electron vai precisar)

Feito: itens 1 e 2 (com as limitações acima) e a parte de reserva de memória do item 3. Próximos, nesta ordem:
pôr `Contents/Resources/app` na imagem (precisa de mais RAM ou de arquivos lidos do disco sob demanda), `dlopen` de
módulos nativos `.node` (`MH_BUNDLE`), processos auxiliares com IPC Mach entre processos, e a janela (AppKit + IOSurface).

1. **Base Darwin/Mach no libSystem**: `pthread_mach_thread_np`, `mach_task_self`, `mach_port_allocate/insert_right/deallocate`,
   `task_info`, `thread_info`, `mach_absolute_time`/`mach_timebase_info`, `vm_allocate`/`mach_vm_*`, `sysctl`/`sysctlbyname`,
   `_NSGetExecutablePath`, APIs do dyld (`_dyld_image_count`, `_dyld_get_image_header`, `dladdr`, `dlopen`/`dlsym` para as
   bibliotecas do bundle). É o que o próximo `run` vai pedir, uma a uma.
2. **Threads e sincronização completas** (o Chromium sobe dezenas de threads): pthreads, variáveis de condição, chaves TLS,
   e o TLS por thread no carregador (hoje o modelo é de uma thread só).
3. **Memória para o V8**: reservar muito espaço virtual sem ocupar RAM (a "gaiola" de ponteiros comprimidos reserva 4 GiB),
   `mmap` com proteções, `mprotect`, `MAP_JIT` (código executável gerado em tempo de execução). Exige paginação sob demanda.
4. **Arquivos do app no disco, não na RAM**: hoje `\mOS\` inteiro é copiado para a RAM no boot. Com `Resources/` (pacotes
   `.pak`, `icudtl.dat`, snapshot do V8, o código JS do VS Code) o app passa de 400 MB.
5. **Multiprocesso**: o Electron abre os helpers (`Code Helper (GPU/Renderer/Plugin).app`) com `posix_spawn` e conversa com
   eles por portas Mach. Precisa de IPC Mach (ou um equivalente) entre processos, sockets `AF_UNIX`, `kqueue`, `poll`.
6. **Gráficos**: o caminho mais curto é desligar a GPU (`--disable-gpu`) e usar composição por software com o SwiftShader
   que já vem no bundle, entregando os quadros por um IOSurface "falso" ligado às janelas do mOS; mais CoreAnimation mínimo.
7. **Frameworks**: CoreFoundation, Foundation e AppKit bem mais completos (janelas, eventos, menus), CoreText para fontes
   (o Skia do Chromium usa CoreText no macOS), CoreGraphics; Security/SystemConfiguration/IOKit podem começar como stubs
   que devolvem "não disponível".
8. **Node.js** no processo principal e no *extension host* (mesmos requisitos de threads, sockets e memória).

Ordem de grandeza: meses de trabalho no libSystem e no kernel. O ganho desta etapa é que agora cada passo é medido
contra o binário real.

## Como reproduzir

```powershell
# baixe o VS Code para macOS (Intel): https://update.code.visualstudio.com/latest/darwin/stable, e extraia com tar -xf
# (os erros de "Can't create" do tar são só os links simbólicos dos frameworks; os binários reais ficam em Versions/A)
.\build.ps1 -OutDir build-vscode
.\tools\stage-vscode.ps1 -App "<pasta>\Visual Studio Code.app"
.\run.ps1 -Image build-vscode/mOS.img -Memory 2560
```

Sem QEMU, no Windows, com o mesmo `kernel/compat/macho.c`:

```powershell
zig cc -target x86_64-windows-gnu -O2 -DMOS_HOST_TEST -Ikernel/include -Itools tools/machotest.c tools/host_enter.S kernel/compat/macho.c -o build/machotest.exe
$env:MACHOTEST_TOLERANT = 1      # modo experimental (bibliotecas do sistema ausentes viram stubs)
$env:MACHOTEST_MAP = 1           # mostra onde cada imagem foi carregada
$env:MACHOTEST_STUB_LOG = 1      # stubs registram a chamada e devolvem 0 em vez de encerrar
$env:MACHOTEST_PATH = '/Visual Studio Code.app/Contents/MacOS/Code'
.\build\machotest.exe build\libSystem.mos "<pasta que contém o .app>" "<pasta>\Visual Studio Code.app\Contents\MacOS\Code"
```

Inventário completo de imports ausentes por biblioteca:

```powershell
zig cc -O2 -target x86_64-windows-gnu tools/machocheck.c -o build/machocheck.exe
.\build\machocheck.exe build\libSystem.mos "<pasta>\Visual Studio Code.app" faltando.txt
```

## O que mudou no carregador para chegar aqui

`kernel/compat/macho.c` virou um "mini-dyld":

- carrega `MH_DYLIB` de dentro do bundle, resolvendo `@rpath` pelos `LC_RPATH` da imagem e do executável, com
  `@executable_path`/`@loader_path`; num `.framework`, procura também em `Versions/A` e `Versions/Current` (o FAT32 não tem
  links simbólicos);
- liga os símbolos pela export trie de cada imagem (com reexports) antes de procurar no libSystem;
- monta todas as imagens em sequência a partir de `0x400001000000` e roda os inicializadores na ordem de dependência;
- passa ao runtime as seções Objective-C de todas as imagens;
- classe Objective-C que o mOS não tem é ligada a `NSObject` (antes virava um stub de código e o runtime caía ao ler o isa);
- **modo experimental**: se `/Sistema/Biblioteca/dyld-experimental` existir, bibliotecas do sistema que o mOS não tem não
  impedem o app de abrir; as funções delas encerram o processo dizendo o nome (`dyld[mOS]: símbolo não implementado...`).
  Sem o arquivo, o comportamento é o de antes: o app é recusado com a lista do que falta.

## dlopen em tempo de execução (6 nov. 2026)

O Node.js carrega os módulos nativos (`.node`, `MH_BUNDLE`) com `dlopen` + `dlsym("napi_register_module_v1")`, e o
executável do Google Chrome faz `dlopen` do "Google Chrome Framework" + `dlsym("ChromeMain")`. Agora isso funciona:

- **Kernel** (`kernel/compat/macho.c`): o grafo de imagens (`link_t`) não é mais liberado no fim do `macho_load`; fica em
  `process_t.dyld` até a última thread sair (`free_address_space`) e é copiado no `fork`. Os comandos de carga e a export trie
  de cada imagem passam para uma cópia do kernel (`image_persist`): o arquivo no VFS pode ser regravado sem deixar ponteiros
  soltos. `MAX_IMAGES` foi para 96.
- **Syscall `SYS_DYLD` (42)**, operações `MOS_DYLD_*` em `kernel/include/syscall.h`:
  - `OPEN` carrega `MH_DYLIB` ou `MH_BUNDLE` com as dependências do bundle, resolvendo `@rpath` (LC_RPATH da imagem que pediu e
    do executável), `@loader_path` (relativo a quem chamou o dlopen) e `@executable_path`. As imagens novas vão para a região de
    mmap do processo (`p->mmap_next`, acima de 4 GiB), recebem os fixups (chained ou dyld_info) contra todas as imagens já
    carregadas + libSystem e ganham uma **área de stubs própria** (zeros para constantes ausentes, registros TLS por imagem,
    stubs de símbolos ausentes, inicializadores e seções ObjC). Mesmas regras da partida: biblioteca do sistema que o mOS não tem
    recusa o dlopen, salvo no modo experimental. `/usr/lib/libSystem.B.dylib`, `Foundation.framework` e afins devolvem um handle
    que procura só no libSystem. `RTLD_NOLOAD` procura entre as carregadas.
  - `SYM` procura pela export trie: handle = a imagem e as dependências dela (em largura) e depois o libSystem;
    `RTLD_DEFAULT` = todas as imagens na ordem de carga e depois o libSystem; também `RTLD_SELF`, `RTLD_NEXT`, `RTLD_MAIN_ONLY`.
    O `_` do Mach-O é acrescentado pelo kernel.
  - `CLOSE` (contador; nada é descarregado, como o dyld faz com imagens que têm ObjC/Swift), `COUNT`, `IMAGE` (cabeçalho +
    caminho), `FIND` (imagem que contém um endereço), `ERROR` (mensagem do último erro).
- **libSystem** (`userland/libsystem/dlfcn.c`): `dlopen/dlsym/dlclose/dlerror/dlopen_preflight`, `_dyld_image_count`,
  `_dyld_get_image_header/name/vmaddr_slide` e `_dyld_get_image_header_containing_address` (saíram de `extra.c`) cobrem as
  imagens da partida **e** as do dlopen (antes `_dyld_image_count` devolvia 1, só o executável). Para cada carga com imagens
  novas, na ordem do dyld: seções ObjC (`cx_objc_boot` → `cx_objc_init_image`, com +load), callbacks de
  `_dyld_register_func_for_add_image` (`dyldimg.c` agora guarda os callbacks; o runtime Swift acha as seções `__swift5_*`
  das imagens novas por aqui) e os inicializadores. `thread_local` das imagens novas usa os registros da área de stubs delas
  (`TLV_IMAGES` em `darwin.c` foi para 128).
- **Harness**: `tools/machotest.c` emula `SYS_DYLD` chamando o mesmo `macho_dyld_syscall`, e o mmap dele usa o mesmo
  `mmap_next` do carregador.
- **Teste**: `tools/build-dltest.ps1` monta `DLTest.app` (`userland/tests-dlopen/`): um `MH_BUNDLE` com construtor, classe ObjC
  com `+load`, `thread_local`, dependência `@rpath/libdep.dylib` e um símbolo do executável achado por busca plana (como os
  `.node` com `-undefined dynamic_lookup`). O app confere 23 itens; argumentos extras são `.node`/dylibs para carregar.

```powershell
.\tools\build-dltest.ps1 -Dest build\dltest
$env:MACHOTEST_PATH = '/DLTest.app/Contents/MacOS/DLTest'
.\build\machotest.exe build\libSystem.mos build\dltest build\dltest\DLTest.app\Contents\MacOS\DLTest     # dlopen-teste: 23/23 ok
# módulos reais do VS Code (copiados para build\dltest\node\), no modo experimental:
$env:MACHOTEST_TOLERANT = 1
.\build\machotest.exe build\libSystem.mos build\dltest build\dltest\DLTest.app\Contents\MacOS\DLTest /node/spdlog.node /node/pty.node
```

Resultado no harness: `spdlog.node`, `watchdog.node`, `pty.node` e `vscode-sqlite3.node` carregam, os inicializadores rodam e
`napi_register_module_v1` é achado. No mOS (QEMU, `build-dlopen`, DLTest iniciado por um LaunchDaemon com o `.node` de
`/Aplicativos/VSCode.app/.../node_modules.asar.unpacked` como argumento): `dlopen-teste: 24/24 ok` com `spdlog.node` e com
`pty.node` (o `init` corta `Argumentos` em 128 bytes: um módulo por serviço). O que falta para eles funcionarem de verdade não é do carregador: `libc++.1.dylib`
(~70–100 símbolos `std::__1` por módulo; sem o modo experimental o dlopen é recusado por isso) e, fora do Electron, a N-API
(`napi_*`), que no VS Code real vem do Electron Framework por busca plana. `tools/stage-vscode.ps1 -ComModulosNativos` copia
`node_modules.asar.unpacked` para a imagem (arquivos acima de `-MaxModuloMiB`, padrão 32, ficam de fora).

Limites conhecidos: `dlclose` não descarrega; `dladdr` (em `swiftsupport.c`) ainda não usa `MOS_DYLD_FIND`/a trie para dar o
nome da imagem e do símbolo; `RTLD_LOCAL` é tratado como global (busca plana).

## IPC Mach entre processos (portas Mach)

O Electron abre os `Code Helper (GPU/Renderer/...)` com `posix_spawn` e eles fazem *rendezvous* com o processo principal por
portas Mach (`base/apple/mach_port_rendezvous_mac.cc`): o principal registra `com.<bundle>.MachPortRendezvousServer.<ppid>` no
bootstrap e cada helper faz `bootstrap_look_up` e troca `mach_msg` com direitos de porta. Antes, as portas só existiam **dentro**
de um processo (filas em `mach.c`, bootstrap em `darwin.c`), então o helper caía em `bootstrap_look_up ... 1102` → "Mach rendezvous
failed". Agora há portas Mach **no kernel**.

**Kernel (`kernel/proc/machipc.c`, syscalls 80..95 em `kernel/include/syscall.h`).** Cada processo tem um espaço de nomes de
portas (`mspace_t`): nome (u32) → porta + direitos (receive / send com *urefs* / send-once / port set / nome morto). Uma porta
(`kport_t`) é fila de mensagens + contadores de direitos + conjunto + pedidos de notificação; também representa conjuntos de
portas, objetos do kernel (task 0x103, host 0x203, thread 0x1000+tid, bootstrap 0x303) e entradas de memória compartilhada.

- `SYS_MACH_MSG` — `mach_msg` completo. No envio, o kernel copia a mensagem e troca nomes por portas: o header (destino +
  direito de resposta) e, se `MSGH_COMPLEX`, os descritores — de porta (12 bytes), de memória *out-of-line* (16 bytes, copiada
  para páginas físicas próprias) e de vetores de portas. Enfileira no destino; quem recebe sem mensagem **bloqueia** (`TH_WAITING`,
  `wait_pid = -7`), com prazo acordado pelo escalonador (`wake_sleepers` em `sched.c`). Na recepção os direitos entram no espaço
  de quem recebe, a memória OOL é mapeada (páginas compartilhadas, contadas em `pref[]` como as de COW) e é acrescentado o
  *trailer* de auditoria de 52 bytes — o `audit_token` leva o pid de quem enviou (`audit_token_to_pid`, usado pelo rendezvous).
- `SYS_MACH_PORT` — `mach_port_allocate/deallocate/mod_refs/insert_right/extract_right/construct/destruct`, tipos e refs,
  conjuntos (`insert/move/extract_member`), notificações (`request_notification`: sem-remetentes e nome-morto), limites e
  contexto, status/peek/pending (o `cx_mach_pending` do kqueue consulta `MACH_OP_PENDING`), e as portas especiais da task
  (`task_get/set_special_port`, `posix_spawnattr_setspecialport_np` via `MACH_OP_SPAWN_SPECIAL`).
- `SYS_MACH_BOOTSTRAP` — registro de serviços **global** no kernel: `check_in` dá o direito de receber ao servidor, `look_up`
  dá um direito de enviar a quem procura, `register` publica um direito que o chamador já tem.
- `SYS_MACH_VM` — entradas de memória compartilhada (`mach_make_memory_entry_64` / `mach_vm_map`) que viajam como direito numa
  mensagem e são mapeadas nos dois processos (base::SharedMemory).
- **Herança**: filhos de `posix_spawn`/`fork` herdam o direito de bootstrap do pai (ou a porta dada a
  `posix_spawnattr_setspecialport_np`) — `machipc_spawn`, chamado em `proc_spawn_in` e no `fork` de `process.c`. `machipc_exit`
  derruba os direitos ao processo sair: portas sem receptor viram mortas e disparam as notificações pendentes.

**libSystem.** `userland/libsystem/mach.c` passou a ser o dono de `mach_msg`, `mach_port_*`, `bootstrap_*` e
`task_get/set_special_port` (a seção de bootstrap saiu de `darwin.c`), todos delegando às syscalls acima; os nomes especiais
(task/host/thread, semáforos, entradas de memória) continuam resolvidos no libSystem. O servidor do rendezvous escuta a porta
com `dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, ...)`: `userland/libsystem/dispatch.c` agora cria uma fonte de
verdade para esse tipo (uma thread espera na porta com `MACH_OP_WAIT` e dispara o handler — na fila principal via
`dispatch_async_f`, para o V8 rodar na thread certa); os outros tipos de fonte continuam `NULL`.

**Teste** (`userland/machtest.c`, programa nativo do mOS; rode `machtest` no Terminal): faz `fork`, o pai registra um serviço
(`check_in`) e o filho o encontra (`look_up`) e envia uma mensagem **complexa** — direito de resposta send-once no header, um
descritor de porta (transfere um send novo) e um bloco OOL com uma string; o pai confere tudo e responde, o filho confere a
resposta. Imprime `machtest: PASS`. (O harness sem QEMU `tools/machotest.c` não cobre este caminho: ele é do carregador Mach-O,
não do escalonador/portas; o teste roda no QEMU.)

Estado: kernel + libSystem compilando (`build.ps1 -OutDir build-ipc`). Falta confirmar no ciclo integrado do VS Code
(`tools/vscode-teste.ps1`) que os `Code Helper` passam do `bootstrap_look_up`/`mach_msg` sem cair — feito junto com a sessão do
caminho principal, que mantém o VM do Electron de pé.

## CoreAnimation e IOSurface: conteúdo da janela na tela (6 nov. 2026)

No macOS o Chromium não desenha a janela com `drawRect:`: o compositor (viz), mesmo por software (`--disable-gpu`), entrega cada
quadro num **IOSurface** e o processo do navegador o mostra numa árvore de **CALayer** pendurada na `NSView` da janela
(`ui::DisplayCALayerTree`: layer raiz da view → layer invertido de tamanho 0 com `autoresizingMask` Width|HeightSizable →
layer com `contents = IOSurface`, `contentsGravity = topLeft`, `contentsScale`; ou um `CALayerHost` com o `contextId` de um
`CAContext` do processo da GPU). O mOS não tinha QuartzCore nem IOSurface; agora tem, com composição por software.

Arquivos novos em `userland/libsystem/`:

| Arquivo | Conteúdo |
|---|---|
| `iosurface.c` | Classe `IOSurface` (IOSurfaceRef é ponte com ela: `CFRetain`/`CFRelease`), `IOSurfaceCreate` lendo o dicionário (largura, altura, bytes por linha/elemento, formato, `kIOSurfacePlaneInfo`, `AllocSize`), `Lock`/`Unlock` (com *seed*), `GetBaseAddress(OfPlane)`, `GetWidth/Height/BytesPerRow/...(OfPlane)`, `GetID`/`IOSurfaceLookup`, contagem de uso, `SetValue`/`CopyValue`, `SetPurgeable`, `AlignProperty`, as 23 constantes `kIOSurface*`; portas Mach (abaixo) |
| `quartzcore.c` | `CALayer` (bounds/position/anchorPoint/frame, `transform` com escala e translação, `sublayerTransform`, `zPosition`, sublayers, `contents` = IOSurface ou NSImage, `contentsRect/Scale/Gravity`, `opacity`, `hidden`, `masksToBounds`, `backgroundColor`, borda, `geometryFlipped`, `autoresizingMask`, `setNeedsDisplay`/`display` pelo delegate, conversão de pontos), `CALayerHost`, `CAContext` (`contextWithCGSConnection:options:`, `contextId`, `layer`), `CATransaction` (begin/commit/flush, `setDisableActions:`, bloco de conclusão), `CATransformLayer`, `CAMetalLayer`/`CAOpenGLLayer` (sem drawable), `CAEDRMetadata`, `CATransform3D*`, `CACurrentMediaTime`, `CGSMainConnectionID`, constantes `kCA*`; integração com o NSView e o compositor |
| `corevideo.c` | `CVDisplayLink` (uma thread a 60 Hz chama o callback, como no macOS), `CVPixelBuffer` sobre IOSurface (+ pool, anexos, `CreateWithBytes`), constantes `kCV*`; "sem GPU": `MTLCreateSystemDefaultDevice` = nil, `MTLCopyAllDevices` = vazio, `CGLChoosePixelFormat` = erro (o Chromium escolhe o SwiftShader/software) |
| `cfcollections.c` | Coleções do CoreFoundation em ponte com o Foundation do mOS: `CFDictionaryCreate(Mutable)(Copy)`, `Set/Add/Replace/RemoveValue`, `ApplyFunction`, `GetKeysAndValues`; `CFArrayCreate(Mutable)(Copy)`, `Append/Insert/Set/RemoveValue`, `ApplyFunction`; `CFNumberCreate/GetValue/IsFloatType`, `CFBooleanGetValue`; `CFGetTypeID` e os `*GetTypeID` com os números da Apple (String 7, Null 16, Dictionary 18, Array 19, Data 20, Boolean 21, Number 22, URL 29, UUID 34, Date 42); `kCFType*CallBacks` |

**Como o conteúdo chega à tela.** `NSView` ganha `wantsLayer`/`setWantsLayer:`/`layer`/`setLayer:`/`makeBackingLayer` (a ligação
view → layer fica numa tabela do `quartzcore.c`; o estado das views continua do `appkit.c`). O `appkit.c` chama
`cx_layer_draw_hook` (gancho marcado "gancho QuartzCore") em `draw_view_tree`: fase 0 antes do desenho da view (fundo do layer
raiz) e fase 1 depois do `drawRect:` (conteúdo e sublayers), antes das subviews. Na fase 1 a árvore é achatada num buffer ARGB
pré-multiplicado do tamanho da área visível da view (já recortada pelo clip do AppKit), com mapa afim por eixo (y para cima ou
para baixo conforme `isFlipped`/`geometryFlipped`), amostragem por vizinho mais próximo e composição *over* com opacidade. O
QuartzCore despacha o lote pendente do AppKit (`cx_gui_flush`, que repõe o recorte vigente — o kernel zera o clip a cada
`GUI_DRAW`) e manda o buffer em faixas `GC_IMAGE` de menos de `GUI_MAX_BATCH` (512 KiB), pulando faixas transparentes.
Redesenho: qualquer mudança num layer e todo `IOSurfaceUnlock` de escrita marcam a view hospedeira com `setNeedsDisplay:` (de
qualquer thread: só liga a flag), e o laço do AppKit redesenha. Sem animações (tudo vale na hora, como `setDisableActions:YES`).

**IOSurface entre processos.** A memória de um surface é uma página de cabeçalho (`surf_hdr_t`: tamanho, formato, planos)
seguida dos pixels. `IOSurfaceCreateMachPort` cria uma **entrada de memória** do kernel (`mach_make_memory_entry_64`, da sessão IPC
Mach) sobre o bloco e põe marca|pid|id no contexto da porta; cada chamada devolve um direito novo, que o chamador desaloca, como
no macOS. `IOSurfaceLookupFromMachPort`: se a porta é deste processo (contexto), devolve o mesmo objeto; senão mapeia a entrada
(`mach_vm_map`), lê o cabeçalho e monta um IOSurface que enxerga os **mesmos pixels** — é o caminho do `Code Helper (GPU)` para o
processo principal. Sem IPC no kernel (harness `machotest`), a porta é o nome local `0x7F000000 | id`. Os `CAContext` ainda são
só do processo: um `CALayerHost` mostra o layer de um `CAContext` do mesmo processo (vale para `--in-process-gpu`); entre
processos o Chromium por software manda o IOSurface (`CALayerParams.io_surface_mach_port`), não o `contextId`.

**Teste** (`userland/testes/CamadaIOSurface/main.m`, compila igual no macOS): `NSWindow` + `NSTextField` + `NSView` com
`wantsLayer`, layer raiz cinza, um sublayer com `contents` = IOSurface 240×180 BGRA pintado à mão (degradê, faixas que andam a
cada 0,5 s, quadrado branco no canto superior esquerdo para conferir a orientação, borda amarela), um layer invertido com
autoresize (como o do Chromium) contendo um `CALayerHost` que mostra um `CAContext` cujo layer tem outro IOSurface recebido por
`IOSurfaceCreateMachPort`/`LookupFromMachPort`, e um véu branco com `opacity` 0,5 por cima.

```powershell
.\tools\build-camada-teste.ps1                         # build\testes\CamadaIOSurface.app
.\build.ps1 -OutDir build-ca; .\tools\build-camada-teste.ps1 -Imagem build-ca
.\tools\qemu-test.ps1 -OutDir build-ca -Port 5923 -Memory 768 -Steps "wait:40000;shot:camada"
```

![CamadaIOSurface no QEMU](img-camada-iosurface.png)

No QEMU (captura acima) a janela mostra os dois IOSurfaces nas posições e na orientação certas, a animação redesenha a janela
e a porta do surface é uma entrada de memória do kernel. No harness (`tools/machotest.c`, `MACHOTEST_FRAMES=1`) os quadros saem
com `img`; os samples Lista, Formulario e Bloco dão saída idêntica com e sem o gancho (comparado com um libSystem compilado com
`-DQC_SEM_GANCHO`, que desliga o `cx_quartzcore_init`).

Medido com `tools/machocheck.c` no bundle inteiro do VS Code: QuartzCore 21 imports / **0** ausentes, IOSurface 48 / **0**,
CoreVideo 65 / **0**, OpenGL 2 / 0; do Metal sobram só 19 classes `MTL*Descriptor` (o carregador as liga ao `NSObject`, e
`MTLCreateSystemDefaultDevice` = nil faz o Chromium não usá-las).

**Lição do carregador**: símbolos `__attribute__((weak))` do libSystem **não** são exportados aos apps (o `CGSMainConnectionID`
fraco virou "símbolo não implementado"); use definições fortes.

**Falta / próximos passos**
- Rodar o VS Code (`tools/vscode-teste.ps1 ... -OutDir build-ca -Port 5923`) até a janela e conferir os avisos
  `objc[mOS]: -[CALayer ...] ainda não implementado` no log: não deu nesta sessão por falta de RAM livre no host (0,5–0,9 GB
  livres; o VM do VS Code usa 2,7 GB).
- `CAContext` entre processos (`CALayerHost` mostrando um contexto de outro processo) — só se o Chromium usar o caminho remoto
  (GPU de verdade); por software ele manda o IOSurface.
- `CGImage` como `contents` (o CoreGraphics do mOS ainda não tem `CGImageCreate`); `cornerRadius`, sombras, filtros, `mask` e
  transformações com rotação são aceitos e ignorados no desenho; amostragem sem filtro linear; formatos de 8 bytes (`RGhA`) e
  `l10r` não são desenhados.
- `@{}` vazio compilado pelo Xcode 15+ usa o símbolo `___NSDictionary0__`, que o libSystem não exporta (o app de teste usa
  `[NSDictionary dictionary]`).
