# Porte do Google Chrome para o mOS

Objetivo: rodar o **Google Chrome para macOS (Intel x64)**, sem modificações, como `/Aplicativos/Google Chrome.app`.
Versão medida: Chrome 157.0.8103.39 (`googlechrome.dmg` universal, 270 MiB, de `dl.google.com`).

O Chrome é Chromium puro: o mesmo motor do Electron que o VS Code usa. Quase tudo o que falta para ele é o que
falta para o VS Code (`docs/porte-vscode.md`), então os dois portes andam juntos.

## Onde estamos (6 nov. 2026)

| Etapa | Estado |
|---|---|
| Abrir o `.dmg` do Chrome (chunks LZMA/ULMO) | ✅ `kernel/fs/xz.c`, descompressor xz/LZMA2 próprio; o volume HFS+ "Google Chrome" monta com 394 arquivos e 0 erros (723 MiB) |
| Ler o bundle (`Info.plist`, executável universal x86_64+arm64) | ✅ o carregador escolhe a fatia x86_64 |
| Executável `Google Chrome` carrega e roda | ✅ só 1 import ausente (`abort_report_np`) |
| `dlopen` do `Google Chrome Framework` (264 MiB na fatia x86_64) | ❌ **bloqueio atual**: o `dlopen` do libSystem devolve NULL |
| Framework: 2.708 imports, 1.992 ausentes | ❌ |
| Helpers (GPU, Renderer), IPC Mach, janela | ❌ (mesmo roteiro do VS Code, itens 5 a 7) |

### Validação (6 nov. 2026)

- **Descompressor xz contra a liblzma 5.8.3** (arquivos gerados pelo `bsdtar -J` do Windows): 7 de 7 casos idênticos byte a byte,
  com níveis 0, 6 e 9, multibloco (`threads=4`), streams concatenados, dados aleatórios (chunks sem compressão) e 22 MiB
  de binário. Entrada truncada ou corrompida devolve erro sem travar (testado com UBSan).
- **Arquivos extraídos contra a assinatura de código do Google**: as 159.836 páginas das 20 imagens Mach-O
  (fatias x86_64 e arm64) batem com os hashes SHA-256 do CodeDirectory.
- **Dentro do mOS (QEMU, 2 GiB)**: no boot, `dfs: montado "Google Chrome" (HFS+, ULMO)`; o volume aparece em Locais no
  Arquivos, com botão de ejetar, e mostra o `Google Chrome.app`. O DMG (269 MiB) passa do limite de 256 MiB de
  `persist.c` para arquivos carregados no boot; o teste usou um kernel com `-DMAX_LOAD=(512ull<<20)`.

### Dentro do mOS (6 nov. 2026, 17h30)

- `tools/stage-chrome.ps1` monta o Chrome em `/Aplicativos/Google Chrome.app` numa imagem de teste: só a fatia x86_64
  (309,5 MiB no total, com o framework de 264 MiB), recursos do Chromium e as traduções en/pt_BR.
- O Chrome aparece no Launchpad e na Busca com o ícone dele. Ao clicar, o processo sobe, para no `dlopen` e o mOS mostra
  "\"Google Chrome\" fechou inesperadamente. O app encerrou com o código 134 (abort)" (antes ele sumia sem aviso).
- Funções que só o Chrome pede (269, das 1.893 ausentes; as outras 1.624 o VS Code também pede e as sessões dele cobrem):
  `userland/libsystem/chrome.c` resolve a maior parte. CoreVideo e os `CF*GetTypeID` ficaram com as sessões donas
  (corevideo.c, cfcollections.c). Com tudo junto, o framework caiu de 1.992 para 1.504 imports ausentes. Quando o recurso não existe no mOS (Keychain, DiskArbitration, MIDI,
  Bonjour, assinatura de código...), a função devolve o erro documentado de "indisponível", nunca 0 (= sucesso).
  Sobram 21 classes ObjC (o carregador as liga a `NSObject`), 3 constantes que podem ser nulas e 3 que são de outras
  sessões: `CFDictionaryApplyFunction` (cfcollections.c), `IOSurfaceGetBytesPerElementOfPlane` e `dispatch_data_create`.

### Com o dlopen (7 nov. 2026, madrugada)

Dentro do mOS (QEMU, 3 GiB), aberto pelo Launchpad:

| Etapa | Estado |
|---|---|
| `dlopen` do framework (264 MiB) | ✅ 13 símbolos ausentes (de 1.992 no começo) |
| ICU (`icudtl.dat`) | ✅ depois de `NSBundle` achar `Resources/` numa pasta `Versions/<v>` (foundation.c) |
| Crashpad | ✅ `fork` + `chrome_crashpad_handler` sobem; `shutdown` em socketpair não dá mais EBADF (sockets.c) |
| Singleton do perfil | ✅ `stat` devolve o modo pedido em `mkdir`/`chmod` (0700 do `mkdtemp`; libc.c) |
| `base/mac/mac_util.mm` | ✅ sysctls que faltavam (darwin.c) |
| Swizzle do menu Serviços | ✅ não era exceção: o `int3` em `0x4483d77` era o `CHECK` do bloco de `+[ChromeSwizzleServicesMenuUpdater install]`, que exige as classes privadas `_NSServiceEntry` (`-bundleIdentifier`) e `_NSServicesMenuUpdater` (`-populateMenu:withServiceEntries:forDisplay:`, `-plugMenu:intoMenu:`); criadas em `chrome.c` (`cx_chrome_init`, chamado por `cx_objc_boot`) |
| Menu principal | ✅ o Chrome monta o `NSMenu` (avisos de `NSMenuItem setAlternate:/setHidden:/setImage:`, `NSMenu itemWithTag:`, `sectionHeaderWithTitle:` ainda não implementados, sem cair) |
| **Agora** | ⏳ o processo fica vivo (minutos) sem janela e sem iniciar nenhum `Google Chrome Helper` (GPU/Renderer): falta descobrir em que ele espera |

### Helpers sobem (7 nov. 2026, manhã)

| Bloqueio | Causa no mOS | Correção |
|---|---|---|
| Thread principal girando sem parar | `dispatch_async` na fila principal antes do CFRunLoop executava na hora; o `-rebuildMenu` do Chrome se reagenda | dispatch.c: de dentro de um bloco já executado assim, vai para a fila |
| Laço infinito no menu | NSMenu sem `itemAtIndex:`/`removeItemAtIndex:`/`numberOfItems`, NSMenuItem sem `isSeparatorItem` | appkit.c: NSMenu/NSMenuItem completos (índice, tag, título, separador, oculto, alternativo, imagem, cabeçalho de seção) |
| Helper não carregava `libaperitif.dylib` | `process_t.path` tinha 128 bytes; o caminho do helper tem 198 e `@executable_path` saía cortado | proc.h (e tools/host_proc.h): 384; macho.c: `executable_path=` com 420 |
| Helper: `CHECK(bundle)` do framework | `mainBundle` cortava no PRIMEIRO `.app/` (o Chrome de fora); o helper lia o Info.plist errado e não se via como processo de fundo (`LSUIElement`) | foundation.c `b_root`: o `.app` que contém `Contents/MacOS/<exe>` |
| Helper: `thread_local` lia lixo | slot 101 da TSD usado por dois donos: tabela de TLS (darwin.c) e objeto NSThread (nsurl.c) | darwin.c: TLS no slot 110; lista dos slots fixos documentada |
| `dispatch_retain` não fazia nada e `dispatch_release` liberava na hora | — | dispatch.c/extra.c: contagem de referências; objetos estáticos nunca liberados |

**Agora:** o processo principal sobe, inicia o Crashpad e dois `Google Chrome Helper (Aperitif)`, que ficam vivos. O
principal cai na detecção de corrupção do PartitionAlloc (código `0x13`, `0x9cc34a0`, chamada de `0x83b51` logo depois
de `os_unfair_lock_unlock` da raiz): a "lista livre" tem `0x6D4F5351756588` = `QMAGIC` das filas do dispatch.c + 0x13.
Já descartado: liberar fila cedo (com `dispatch_release` sem free nenhum a queda continua), `free`/`in_heap` (brk e mmap
não se sobrepõem) e o COW do `fork` (todas as páginas marcadas). Próximo passo: achar quem escreve uma `cx_queue_t`
num bloco do PartitionAlloc (tracepoint no `dispatch_queue_create` com `@rax` na volta, comparado com o endereço do
bloco corrompido, que sai de `r14`/`r12` em `0x83b28`).

Atualização (com o libSystem de 7 nov., WHPX): com `despejar-threads`, o dispatch.c registra o endereço de cada fila
(`dispatch[diag]`). As filas ficam no heap brk do mOS (`0x40000142ccf0`, `0x40000143ce30`...), fora do PartitionAlloc;
então não é o malloc do mOS entregando memória do Chrome, e sim o PartitionAlloc tratando uma fila do mOS como bloco livre.
Desde que as filas ganharam `isa` (`OS_dispatch_queue_*`), o ARC do Chrome as libera com `objc_release`; a próxima pista
é o caminho `-release` → `dealloc` → `object_dispose` → `cx_object_free` nesses objetos sem cabeçalho de objeto do
mOS (as pilhas das threads que giravam já mostravam `cx_object_free`/`object_dispose`). A queda continua em `0x9cc3297`.

Rodada seguinte (WHPX): o release ObjC das filas foi descartado (objc_release só age com `OBJ_MAGIC` ou classe CRR).
O mzone.c agora reconhece o dono de verdade (`malloc_zone_from_ptr`, `z_claimed`, `malloc_zone_claimed_address`), sem
regressão no harness nem no VS Code (o renderer dele deixou de cair). A queda do Chrome não mudou, mas o tracepoint no
detector (`+0x9cc34a0`) fechou o que o PartitionAlloc acha na lista livre: `OBJC_CLASS_$_OS_dispatch_queue_main+0x10`,
um objeto "fila principal" do mOS (classes `OS_dispatch_*`, da sessão do runtime Swift/ObjC, swiftobjc.c) dentro de um
bloco que o PA considera livre. Próximo passo, com essa sessão: de onde vêm instâncias com esse isa, se passam pela zona
padrão (trocada pelo PA) e se algum release/dealloc delas chama `free` em objeto estático.

**Correção do diagnóstico (7 nov.):** não é corrupção de lista livre. Em `+0x83968` o valor é comparado com
`0x3FFE00000` (limite de alocação do PartitionAlloc) e vai para a rotina de falta de memória (`+0x839c4`/`+0x9cc34a0`):
é o **tamanho** de uma alocação. Ele é sempre o primeiro word de um objeto fila principal do mOS mais 0x10 (o PA soma 16 e
alinha a 8): `QMAGIC+0x13` com o layout antigo, `OS_dispatch_queue_main+0x10` com o `isa`. Alguma API entrega ao Chrome a
fila principal (ou objeto de dispatch com o mesmo começo) onde ele espera outra estrutura e lê o 1º campo como tamanho;
a chamada vem de uma thread que não é a principal. Para achar a cadeia, falta um tracepoint condicional (os atuais disparam
na 1ª passagem, que é uma alocação comum): ex. só quando `rdi > 0x3FFE00000`, em kernel/proc/tracepoint.c.

Ferramentas: `tools\stage-chrome.ps1 -AutoAbrir` (o `init` abre o Chrome no boot, sem depender do mouse no QEMU) e
os tracepoints (`/Sistema/Biblioteca/diag-tracepoints`), que agora também valem para imagens do `dlopen` (macho.c).

Para achar a causa de cada queda: `llvm-objdump -d --start-address=... ` (LLVM em `C:\Program Files\LLVM\bin`) no
framework de `build-chrome\esp\...`; o endereço do log menos `0x400100000000` é o endereço no arquivo.

Como o Chrome inicia: o executável `Contents/MacOS/Google Chrome` (0,3 MiB) só monta o caminho
`../Frameworks/Google Chrome Framework.framework/Versions/<versão>/Google Chrome Framework`, chama `dlopen`,
procura `ChromeMain` com `dlsym` e pula para lá. Sem `dlopen` em tempo de execução ele para nesse ponto:

```
dlopen .../Google Chrome Framework: dlopen não é suportado no mOS.
Abort trap: 6
```

## Próximo passo: `dlopen` em tempo de execução

Combinado com o porte do VS Code (6 nov. 2026): o `dlopen`/`dlsym` em tempo de execução é feito lá, em
`kernel/compat/macho.c`, porque o VS Code precisa da mesma coisa (módulos `.node`, `MH_BUNDLE`). Quando existir, o teste aqui
é carregar o `Google Chrome Framework` e chegar ao `ChromeMain`. Flags que o Chromium deve precisar no mOS por enquanto:
`--no-sandbox` e, sem IPC Mach entre processos, `--single-process`/`--in-process-gpu` (em teste no porte do VS Code).

Desenho proposto:

1. O `link_t` de `kernel/compat/macho.c` passa a viver junto do processo (hoje é liberado depois do spawn).
2. Uma syscall `SYS_DLOPEN(caminho)` carrega a imagem e as dependências dela depois da última imagem,
   liga os símbolos contra as imagens já carregadas + libSystem e devolve um handle com os inicializadores e as
   seções ObjC.
3. No libSystem, `dlopen` registra as seções ObjC, roda os inicializadores e devolve o handle;
   `dlsym` percorre a export trie da imagem; `_dyld_image_count`/`_dyld_get_image_header` passam a ver as imagens novas.

Memória: só a fatia x86_64 do framework já tem 264 MiB, e hoje `\mOS\` inteiro vai para a RAM no boot. O script de
preparação deve gravar só a fatia x86_64 (afinar os binários universais), e o boot precisa de `-Memory 3072` ou mais.

## Como reproduzir

```powershell
# extrair o .app com o leitor de DMG do próprio mOS (no Windows)
zig cc -target x86_64-windows-gnu -O2 -DMOS_HOST_TEST -Ikernel/include -Itools tools/dmgtest.c `
  kernel/fs/{apfs,blkdev,bzip2,dfs,dmg,hfsplus,inflate,lzfse,xz,fat}.c <stub com rtc_now/rand32> -o build/dmgtest.exe
.\build\dmgtest.exe googlechrome.dmg saida      # use um caminho sem espaços ou relativo

# inventário do que falta
.\build\machocheck.exe build\libSystem.mos "saida\Google Chrome\Google Chrome.app" faltando.txt

# rodar o executável no carregador do mOS, sem QEMU
$env:MACHOTEST_TOLERANT = 1; $env:MACHOTEST_STUB_LOG = 1
$env:MACHOTEST_PATH = '/Google Chrome.app/Contents/MacOS/Google Chrome'
.\build\machotest.exe build\libSystem.mos "saida\Google Chrome" "saida\Google Chrome\Google Chrome.app\Contents\MacOS\Google Chrome"
```
