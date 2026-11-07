# Compatibilidade com apps do macOS: Cursor, Claude, Codex, VS Code, Slack, Grok Bot, WhatsApp

Objetivo: rodar sem modificações, no mOS (x86_64), os builds **Intel** oficiais destes sete apps. Este documento é o
inventário medido (6 nov. 2026), como cada um é montado e iniciado, e o roteiro. O porte detalhado do VS Code, que abre o
caminho para os Electron, está em [porte-vscode.md](porte-vscode.md). O do Chrome, que serve ao Codex, está em [porte-chrome.md](porte-chrome.md).

## Builds medidos

URLs tiradas dos casks do Homebrew (`https://formulae.brew.sh/api/cask/<nome>.json`), que apontam para o servidor de cada
fabricante. Baixados em `build/apps/dl/` e extraídos em `build/apps/x/`. DMG com o leitor do próprio mOS (`build/dmgtest.exe`),
zip com o `tar.exe` do Windows.

| App | Versão | Arquivo (x64) | Base | macOS mín. |
|---|---|---|---|---|
| VS Code | 1.145.2 | update.code.visualstudio.com | Electron 43.8.3 (Chromium 150) | 12.0 |
| Cursor | 3.29.9 | `downloads.cursor.com/production/<hash>/darwin/x64/Cursor-darwin-x64.zip` | Electron 43.8.4 (fork do VS Code) | 12.0 |
| Claude | 4.23118.0 | `downloads.claude.ai/releases/darwin/universal/...zip` | Electron 44.9.3 | 13.0 |
| Slack | 4.53.280 | `downloads.slack-edge.com/desktop-releases/mac/x64/...dmg` | Electron 45.2.1 | 13.0 |
| Grok Bot | 0.79.2 | `downloads.cursor.com/sand/stable/darwin-x64/0.79.2/Grok_Bot_0.79.2_x64.dmg` (cask `grok-bot`, homepage x.ai/bot) | Electron 42.11.16 | 12.0 |
| Codex (desktop) | 26.623.141536 | `persistent.oaistatic.com/codex-app-prod/Codex-darwin-x64-26.623.141536.zip` | **Chromium 149 com marca própria** (`Codex Framework.framework`), não Electron | 13.0 |
| Codex CLI | 0.167.0 | `github.com/openai/codex/releases/.../codex-x86_64-apple-darwin.tar.gz` | binário Rust | — |
| WhatsApp | 2.26.44.21 | `web.whatsapp.com/desktop/mac_native/release/` (DMG) | nativo: Swift + SwiftUI + UIKit (Catalyst) + Combine | 12 |

Cuidados:
- O `Codex.dmg` da página principal da OpenAI é **só arm64** (cputype `0x0100000C`). Para o mOS vale o zip `darwin-x64` do cask `codex-app`.
- O redirect `claude.ai/api/desktop/...` devolve 403 sem navegador. Use o zip do cask.
- O cask `slack` sem arquitetura baixa o universal. O `mac/x64/` é menor.

## Inventário (tools/machocheck.c, bundle inteiro)

"Ausentes" = funções e símbolos distintos importados que o libSystem do mOS não tem. A medição de 6 nov. já inclui
`security.c` e `cfdata.c` (abaixo) e a libc++ em andamento (`libcxx.c`, de outra sessão).

| App | Imagens x86_64 | Ausentes (antes → agora) | Além do VS Code (antes) | Maiores grupos extras |
|---|---|---|---|---|
| VS Code | 11+ | 1.620 → 1.950* | — | (referência) |
| Slack | 22 | 1.709 → 1.534 → **32** (7 nov.) | 123 | só libc++ 28, libcompression 3 e libz 1, que vêm de dylibs reais (`/usr/lib/libc++.1.dylib`, `userland/frameworks`) que o `machocheck` não enxerga; o resto entrou em `slack_frameworks.c` e `bzip2.c` |
| Cursor | 49 | 2.378 → 2.089 | 760 | libc++ 279, Swift 237 (só o `cursor-update-supervisor`), Security 84 |
| Codex desktop | — | 2.481 → 2.231 | 978 | libc++ 271, Swift 259 (helpers nativos), CoreText 54 |
| Grok Bot | 27 | 2.562 → 2.393 | 974 | Swift 541 (só o helper "Computer Use"), Foundation 219 |
| Claude | 27 | 4.279 → 4.040 | 2.700 | Swift 1.256 + SwiftUI 547 (`swift_addon.node`, `computer_use.node`, simulador iOS) |
| WhatsApp | 9 | 8.922 → 8.675 → 7.783 → 4.730 (dylibs Swift do mOS) → 4.444 (Combine) → 4.415 (Dispatch) → 4.265 (Foundation, marco C) → 3.694 (UIKit.framework do Catalyst) → 3.622 → 3.413 (libswiftUIKit completa, 7 nov.; ver [uikit-catalyst.md](uikit-catalyst.md)) → **3.105** com a `libc++.1.dylib` do LLVM (`tools\build-libcxx.ps1`) no diretório de varredura; o kernel ainda desvia a libc++ para o libSystem | 8.344 | SwiftUI 555, Swift Foundation 278 (quase tudo ICU) |
| **Codex CLI** | 1 | **271 → 90** | 152 | Security zerado; sobram CF, termios, rede |

\* O número do VS Code subiu porque o bundle em `build-vscode` foi preparado de novo, agora com mais módulos nativos. Não é regressão.

Para refazer:

```powershell
.\build\machocheck.exe build\libSystem.mos "build\apps\x\slack-x64\Slack\Slack.app" build\apps\rel\slack.txt
```

Para apps Swift (WhatsApp, Postico), o `machocheck` só enxerga os dylibs Swift do mOS (`build\swift\objc\dylib`, de
[swift-objc-runtime.md](swift-objc-runtime.md)) se eles estiverem na pasta varrida. Uma pasta com duas junções resolve:

```powershell
New-Item -ItemType Directory build\wa-scan
cmd /c mklink /J build\wa-scan\WhatsApp.app "build\apps\x\whatsapp\WhatsApp Installer\WhatsApp.app"
cmd /c mklink /J build\wa-scan\swift build\swift\objc\dylib
.\build\machocheck.exe build\libSystem.mos build\wa-scan build\apps\rel\whatsapp.txt
```

## Como cada um é montado e iniciado

Validado no código-fonte no GitHub para os abertos (VS Code, Electron, Codex CLI) e pelos binários para os fechados.

**Electron (VS Code, Cursor, Claude, Slack, Grok Bot)**:
- Partida: `shell/app/electron_main_mac.cc` (electron/electron). Um helper sem `--type=` sai com `EX_USAGE`. Num helper que exige
  sandbox, uma falha em `InitializeSandbox()` dá `abort()`. O Framework é ligado por `@rpath`, não por `dlopen`.
- Helpers: `<App> Helper (GPU|Renderer|Plugin).app`. Os utility processes do VS Code (shared process, pty host, file watcher,
  extension host) usam o `(Plugin)` porque pedem `allowLoadingUnsignedLibraries`.
- Rendezvous Mach (`base/apple/mach_port_rendezvous_mac.cc`): a validação do par é "só validar" por padrão. Ele chama
  `SecCodeCopyGuestWithAttributes` com o audit token e não bloqueia se falhar.
- O VS Code acrescenta `--no-sandbox` e `--disable-gpu-sandbox`, salvo com `--sandbox` (`src/mainImpl.ts`). Os outros sandboxam os
  renderers por padrão, então o `sandbox_apply` dos helpers precisa responder "sucesso".

**Fuses** (fio de `'0'`/`'1'` depois de `dL7pKGdnNz796PbbjQWNKmHXBZaB9tsX` no Electron Framework), na ordem RunAsNode,
CookieEncryption, NODE_OPTIONS, Inspect, **AsarIntegrity**, OnlyLoadAppFromAsar, V8Snapshot, FileProtocol, WasmTrap:

| App | Fuses | O que importa para o mOS |
|---|---|---|
| VS Code, Cursor | `101100011` | sem integridade de asar e sem criptografia de cookies |
| Claude, Grok Bot | `010011011` | **integridade do asar** + **cookies criptografados** + só carrega de `app.asar` |
| Slack | `010011001` | idem |

- Com AsarIntegrity ligado, `shell/common/asar/archive_mac.mm` lê `ElectronAsarIntegrity` do Info.plist do app **externo** com
  `objectForInfoDictionaryKey`: um dicionário com `Resources/app.asar`, `{algorithm = SHA256, hash}`. Dicionário ausente ou hash
  diferente dá `LOG(FATAL)`. O `NSBundle` do mOS precisa devolver esse dicionário aninhado, inclusive dentro dos helpers, e o
  SHA-256 do cabeçalho do asar precisa bater (o CommonCrypto do mOS já tem SHA-256).
- Com CookieEncryption ligado, a partida busca a chave "&lt;App&gt; Safe Storage" no **Keychain**. Agora funciona (abaixo).

**Codex desktop**: o layout é o do Google Chrome (`Codex Framework.framework/Versions/149.0.7827.197/`, `app_mode_loader`,
helpers `Codex (GPU|Renderer|Service|Alerts).app`, Sparkle para atualizar), então o caminho é o do porte do Chrome
(`dlopen` do Framework + `ChromeMain`). Traz `Resources/codex-cli` e um runtime Node (`cua_node`).

**Codex CLI**: um binário Rust (`codex-rs`). O wrapper npm `codex-cli/bin/codex.js` só faz `execve` dele. Para o mOS:
- Sandbox: `codex-rs/sandboxing/src/seatbelt.rs` roda `/usr/bin/sandbox-exec -p <política> -- comando`. O mOS precisa de um
  `/usr/bin/sandbox-exec` que execute o comando (pode ignorar a política), ou o usuário escolhe o modo sem sandbox.
- Login: crate `keyring` (`apple-native`), que é `SecItem*`. Agora funciona.

**WhatsApp**: nativo, sem Chromium. O caminho é o do runtime Swift + OpenSwiftUI (outras sessões, [openswiftui-deps.md](openswiftui-deps.md))
mais o UIKit do Catalyst, que já existe ([uikit-catalyst.md](uikit-catalyst.md): cena, janela, entrada, controles, Auto Layout,
rolagem, tabelas, coleções e navegação; ainda sem modais nem animações).

## Feito nesta etapa

- **`userland/libsystem/security.c`**: Security.framework.
  - Keychain de senhas de verdade: `SecItemAdd/CopyMatching/Update/Delete` (classes `genp`/`inet`, `kSecReturnData/Attributes/Ref`,
    `kSecMatchLimitOne/All`, duplicata = `errSecDuplicateItem`) e a API antiga `SecKeychainFindGenericPassword/AddGenericPassword/
    ItemModifyAttributesAndData/ItemDelete/ItemFreeContent`, que é a do OSCrypt do Chromium. Os itens ficam em
    `$HOME/Library/Keychains/mos-keychain.txt`, gravado por arquivo temporário + `rename`. **Sem criptografia**: o mOS ainda não
    tem senha de sessão.
  - Assinatura de código: todo código é "não assinado" (`errSecCSUnsigned`). `SecCodeCopySelf`/`GuestWithAttributes`/
    `StaticCodeCreateWithPath` criam os objetos. `SecCodeCopySigningInformation` devolve um dicionário sem identidade, como o macOS faz.
  - SecTrust guarda a cadeia, mas avalia como "não confiável". `SecTrustSettingsCopy*` devolve `errSecNoTrustSettings`.
    SecKey/Identity/PKCS12/transforms e SecureTransport (`SSL*`) devolvem os erros documentados. Authorization não concede direitos.
  - As 177 constantes `kSec*` que os apps importam estão em `security_consts.inc`, com os valores reais da Apple onde conhecidos.
- **`userland/libsystem/cfdata.c`**: `CFData*` (cria, copia, mutável, acrescenta, troca, ajusta tamanho) e `CFDate*`, em ponte com o
  NSData/NSDate do mOS. Não existiam, e os sete apps os importam.
- **`objc.c` (`cx_class_named`)**: programas sem seções ObjC (C/Rust que só usam CoreFoundation, como o Codex CLI) recebiam `nil` em
  toda classe, porque o runtime só subia com seções ObjC. Agora ele sobe na primeira procura, como o `swiftbridge.c` já fazia.
- `chrome.c`: os stubs de Keychain substituídos saíram de lá. `SecKeychainCopyDefault`/`GetStatus` dizem "um Keychain, destrancado".
- **Teste**: `tools/build-sectest.ps1` monta `SecTest.app` (`userland/tests-security/main.c`, compila igual no macOS). No harness dá
  `sectest: 26/26 ok`, e a segunda execução (`reler`) lê o item gravado pela primeira: `1/1 ok`. O sample Lista dá a mesma saída
  de antes.

## Rede e Codex CLI no QEMU

- **Sockets TCP de verdade.** A syscall nova `SYS_NET` (43) fica em `kernel/net/usersock.c`. Ela expõe aos apps a pilha TCP/DNS
  do kernel, com conexões por processo, fechadas quando o processo sai. A trava `g_net_lock` protege o `net_poll`, e `TCP_SOCKS`
  foi de 16 para 64.
  - No libSystem, `sockets_inet.inc` implementa AF_INET/TCP: connect bloqueante ou com `O_NONBLOCK`, `SO_ERROR`, send/recv,
    sendmsg/recvmsg, getpeername, `getaddrinfo`/`getnameinfo`/`inet_pton`/`inet_ntop`.
  - Os sockets também respondem a `read`/`write`/`fcntl`/`ioctl` (libc.c) e a poll e kqueue (`cx_fd_readable`/`cx_fd_writable`).
  - Teste: `tools/build-nettest.ps1 -OutDir build-net`, com o app `userland/tests-net`. No QEMU deu **16/16**, com `HTTP/1.1 200`
    de example.com nos modos bloqueante, não-bloqueante + poll e kqueue.
- **Codex CLI roda no mOS** (`tools/stage-codex-cli.ps1`, binário oficial 0.167.0 sem modificações; QEMU com 1,5 GB):
  `codex --version` imprime `codex-cli 0.167.0` (código 0), e `codex login status` imprime `Not logged in`, a resposta certa
  sem login.
  - Para chegar lá: `cli.c` (as 87 funções que faltavam), Security/CFNetwork/SystemConfiguration/CoreServices/IOKit/
    LocalAuthentication na lista de bibliotecas do carregador, `MAX_LOAD` de 256 para 512 MB (o binário tem 277 MB),
    `fcntl(F_DUPFD)` coerente com `dup()` (o tokio clonava o kqueue como fd 0) e `pthread_attr_setstacksize` respeitado.
## TLS para os apps (SecureTransport) e XPC

- **SecureTransport de verdade** (`userland/libsystem/securetransport.c`). O app continua dono do socket (callbacks de
  `SSLSetIOFuncs`). O libSystem lê **um registro TLS por vez** (5 bytes de cabeçalho e depois exatamente o corpo; o `read_func`
  do security-framework repete a leitura até encher o pedido, então pedir mais travaria um socket bloqueante) e o entrega ao kernel.
  - O kernel roda o motor do navegador (`kernel/net/tls.c`: TLS 1.3/1.2, cadeia até as raízes da Mozilla, nome, datas, OCSP
    grampeado) sobre um "socket de memória". As operações novas do `SYS_NET` são `MOS_NET_TLS_NEW/FEED/DRAIN/HANDSHAKE/SEND/RECV/
    ERROR/FREE` (`kernel/net/usersock.c`), e `tcp_state/send/recv` desviam ids `>= TCP_MEM_BASE` para `memsock_*`. Um motor TLS só
    no sistema, com uma lista de confiança só. A trava `g_tls_lock` serializa o motor.
  - Semântica do security-framework: `errSSLWouldBlock` retomável, `kSSLSessionOptionBreakOnServerAuth` →
    `errSSLPeerAuthCompleted` uma vez, e `SSLCopyPeerTrust` devolve uma confiança marcada como válida, porque o kernel já
    validou a cadeia (`cx_trust_mark_verified` em `security.c`). Um `SecTrustCreateWithCertificates` do próprio app continua
    "não confiável".
  - Limites: só cliente, sem certificado de cliente, sem ALPN (o app cai para HTTP/1.1), sem retomada de sessão.
- **Raízes do sistema**: `SecTrustSettingsCopyCertificates(kSecTrustSettingsDomainSystem)` devolve as 118 raízes da Mozilla do
  kernel (`MOS_NET_TLS_ROOTS`), e `SecTrustSettingsCopyTrustSettings` devolve um array vazio ("confiar como raiz"). É o caminho do
  `rustls-native-certs`.
- **Teste**: `tools/build-tlstest-app.ps1 -OutDir build-tls` (`userland/tests-tls`). No QEMU deu **16/16**: TLS 1.3 com
  example.com, `HTTP/1.1 200` bloqueante e não-bloqueante, nome errado recusado.
- **Codex CLI: `codex login --device-auth` funciona.** Ele negocia TLS 1.3 com `auth.openai.com` (cadeia até a ISRG Root X2),
  recebe o código de dispositivo e mostra a tela "Follow these steps to sign in with ChatGPT using device code
  authorization". Antes parava em "error sending request".
- **XPC** (`userland/libsystem/xpc.c`). O modelo de objetos inteiro: null, bool, int64, uint64, double, date, data, string,
  uuid, fd, array, dictionary e error, com `xpc_copy`, `xpc_equal`, `xpc_hash`, `xpc_copy_description` e os `apply` com blocos.
  São objetos ObjC da classe `OS_xpc_object`, então ARC/`objc_release` funcionam. `XPC_BOOL_*` e `XPC_ERROR_*` são estáticos e
  imortais.
  - Conexões: o mOS não tem um launchd com serviços XPC, então elas se comportam como num Mac sem o serviço. Depois de
    `resume`, o handler recebe `XPC_ERROR_CONNECTION_INVALID` (uma vez), e as respostas, inclusive a síncrona, são esse erro.
  - Teste: `tools/build-xpctest.ps1` + `machotest` (`userland/tests-xpc`): **24/24**.

## Claude Code (CLI) no mOS

Build: `downloads.claude.ai/claude-code-releases/2.1.311/darwin-x64/claude` (cask `claude-code`; o SHA-256 confere). É um
Mach-O x86_64 único de 274 MB: o Bun (Zig + JavaScriptCore) com o `cli.js` embutido (`/$bunfs/root/cli`). O TLS é o
BoringSSL do próprio Bun, sem SecureTransport. Preparação: `tools/stage-claude-code.ps1` (QEMU com 1,5 GB).

- **machocheck: 356 ausentes.** libicucore 184 (o ICU do sistema: Intl, normalização, quebra de texto, fusos), libSystem 87
  (`setjmp`/`longjmp`, `wait4`, `popen`, `getifaddrs`, `__ulock_*`, `thread_suspend`/`thread_get_state` do coletor do JSC,
  `DNSService*`...), libc++ 82.
- **O que já foi destravado** para ele subir no QEMU:
  - TSD da thread principal no GS desde a partida. O Bun lê `gs:[0]` (o `pthread_self` do macOS) antes de chamar qualquer
    pthread; o `cx_tsd()` agora roda no `cx_start`.
  - **Memória anônima sob demanda.** O SYS_MMAP ganhou `b = 3` (`kernel/mm/lazy.c`): o mmap anônimo com acesso só reserva
    endereços, e cada página vira RAM na primeira falta, também quando o kernel copia para o processo (`uvm_copy_in/out`).
    O JavaScriptCore reserva 1–4 GB assim; antes, todas essas chamadas falhavam.
  - **Faixas grandes alinhadas ao próprio tamanho** (potência de 2, até 4 GB). O alocador do JSC exige o endereço alinhado
    a N (`test rax, N-1` → `int3`).
- **Onde parou** (`claude --version`): o JSC inicializa e o grafo de módulos embutido é encontrado. O laço de eventos do Bun
  registra a porta Mach do "waker" (`EVFILT_MACHPORT` + `MACH_RCV_MSG`) e espera sem prazo, na primeira e única chamada ao
  kqueue, sem ter criado nenhuma thread e antes de o JavaScript fazer qualquer E/S. Próximo passo: descobrir o que o laço
  espera (uma tarefa enviada a uma thread que não foi criada, ou um envio à porta do waker que o kqueue não vê).

## WhatsApp: primeira partida no harness (7 nov. 2026)

Raiz montada em `build\wa-run`: junções `System` e `usr\lib\swift` para `build\swift\objc\dylib`, o app em
`Applications\WhatsApp.app`, e a `libc++.1.dylib` do LLVM copiada como `usr\lib\libc++.1.dylib` e `libc++abi.dylib`.

```bash
MACHOTEST_STUB_LOG=1 MACHOTEST_TOLERANT=1 MACHOTEST_MAP=1 MACHOTEST_WATCHDOG=240000 \
  MACHOTEST_PATH=/Applications/WhatsApp.app/Contents/MacOS/WhatsApp \
  ./build/machotest-wa.exe build/libSystem.mos build/wa-run build/wa-run/Applications/WhatsApp.app/Contents/MacOS/WhatsApp
```

O `MACHOTEST_TOLERANT` deixa passar as 122 bibliotecas do sistema que faltam, e o `MACHOTEST_STUB_LOG` faz cada
símbolo ausente devolver 0 e ser registrado. O `machotest-wa.exe` é um machotest feito de uma cópia do
`kernel/compat/macho.c` em que a libc++ vem do disco: o kernel ainda a desvia para o libSystem (`libsystem_dylib_ok`).
Essa mudança no kernel está combinada com a sessão do VS Code e ainda não foi feita.

O que a partida mostrou, na ordem:

1. **Bibliotecas ausentes:** sem o modo tolerante o carregador recusa o app (libxml2, libsqlite3, libz, CoreMedia e mais
   118). No modo tolerante carregam 33 imagens: o executável, SharedModules, WAAppKitBridge, as dylibs Swift, o UIKit e
   CoreData/Combine/AudioToolbox/ImageIO do mOS.
2. **libc++:** a primeira chamada (`std::chrono::steady_clock::now`) já precisa da libc++ real. Com a dylib do LLVM vinda
   do disco, as chamadas passam.
3. **Foundation:** faltavam `createDirectoryAtURL:…`, `initWithBytesNoCopy:…`, `initWithObjects:` (varargs),
   `containerURLForSecurityApplicationGroupIdentifier:`, `NSThread name/callStackReturnAddresses`,
   `NSPurgeableData +dataWithLength:`, `dataUsingEncoding:`, `stringByAppendingPathExtension:`,
   `arrayByAddingObjectsFromArray:`, `numberWithUnsignedLong:`, `NSMutableDictionary initWithCapacity:`,
   `NSBundle localizations`, `NSLocale localeWithLocaleIdentifier:/componentsFromLocaleIdentifier:/scriptCode`,
   `rangeOfString:options:`, `stringByPaddingToLength:…`, `URLsForDirectory:inDomains:`, `NSKeyedUnarchiver
   setClass:forClassName:`, `CFStringConvertEncodingToNSStringEncoding` e `CFLocaleCreateComponentsFromLocaleIdentifier`.
   Todos foram feitos (`foundation_extra.c`, `cfencoding.c`), além de `kern.iossupportversion` = 17.0 no sysctl
   (`darwin.c`).
4. **Runtime ObjC:** o mOS não tinha `+resolveInstanceMethod:`/`+resolveClassMethod:` e não anexava as propriedades das
   categorias. As preferências do WhatsApp são propriedades `@dynamic` declaradas em categorias e protocolos, com os
   acessores gerados em tempo de execução. As duas coisas agora existem (`objc.c`). Também se protege uma classe ainda
   sem nome em `objc_getClass`.
5. **Ainda ausente na partida:** a libsqlite3 (`sqlite3_libversion_number`, `sqlite3_config`). Não há fonte nem
   binário do SQLite no disco; embuti-lo (domínio público) depende de baixar o amálgama. `dispatch_io_*`,
   `dispatch_read/write` (`dispatch_io.c`) e `CFReadStream*`/`CFWriteStream*` sobre `NSInputStream` (`cfstream.c`)
   já existem.
6. **Só no harness:** as syscalls 12, 31, 33, 39, 48, 81 (Mach) e 83 não são emuladas pelo `machotest`. No QEMU elas
   existem.

### Segunda rodada (7 nov. 2026): seis defeitos do runtime achados pela partida

Cada um foi rastreado sem símbolos no executável. O método: mapa de carga (`MACHOTEST_MAP`), endereço de retorno →
imagem → trecho desmontado (bytes extraídos com `dd` e desmontados como objeto), seções e listas de ivars com
`llvm-objdump --macho`.

1. **Token do `NSNotificationCenter`:** `addObserverForName:object:queue:usingBlock:` devolvia a struct interna
   (calloc), sem isa; o app a soltava como objeto (`swift_unknownObjectRelease`). Agora o token é um `NSObject` que a
   central retém até `removeObserver:` (`foundation.c`).
2. **Classes Swift com layout em tempo de execução:** campo de tipo resiliente (aqui `metadataLock`, cujo
   deslocamento fica no `__bss`). O `class_ro_t` traz `RO_HAS_SWIFT_INITIALIZER` (bit 6) e o inicializador em
   `ro + 72`. O mOS realizava a classe na carga com o layout estático; agora quem realiza é o inicializador do Swift, no
   primeiro uso, como no `realizeClassMaybeSwift` da Apple. As categorias dessas classes esperam a realização, e
   `objc_getClass` acha a classe pendente. O `_objc_realizeClassFromSwift` realiza direto: chamar o inicializador de
   novo fazia o Swift esperar por ele mesmo (`objc.c`).
3. **`dispatch_async` em fila que não é a principal** rodava o bloco na hora, então um bloco que se reagenda virava
   recursão até estourar a pilha. Agora há um trampolim por thread: o async pedido de dentro de um bloco roda quando
   ele termina, na ordem; `dispatch_sync`, `dispatch_group_wait` e `dispatch_semaphore_wait` escoam antes
   (`dispatch.c`, `extra.c`; `dispatchteste` 30/30 com o laço de 100 mil voltas e a ordem serial).
4. **Observador de bloco com fila** era chamado dentro do `post`. Agora o bloco vai para a fila (a principal, se for a
   `mainQueue`) (`foundation.c`).
5. **Constantes de notificação ausentes** (`NSCurrentLocaleDidChangeNotification` e outras, `wa_consts.c`): no modo
   tolerante o nome chegava nil, o observador recebia tudo e o contexto de i18n se realimentava.
6. **UIKit carregado duas vezes:** o executável e o SharedModules pedem `UIKit.framework/UIKit` e
   `UIKit.framework/Versions/A/UIKit`. O `locate()` do carregador (`kernel/compat/macho.c`) agora procura uma imagem
   já carregada por todos os nomes do framework antes de ler do disco: 32 imagens em vez de 33.

Também entraram: `NSCache`, `NSDate isEqualToDate:/dateByAddingTimeInterval:`, `stringByDeletingPathExtension`,
`writeToURL:atomically:encoding:error:`, `contentsOfDirectoryAtURL:…`, `getBytes:length:/range:`, os `init` de
`NSMapTable`/`NSHashTable` e `UIApplication.backgroundTimeRemaining`.

**Onde está agora:** sem quedas; depois do SQLite ausente, o app fica num laço longo de formatação de strings
(`withVaList` → `String(format:)`) dentro de código genérico do SharedModules, e o watchdog o encerra. Ainda não
se sabe se é trabalho legítimo, lento no harness, ou um laço sem fim causado por algum stub.

## Roteiro (do mais perto para o mais longe)

1. **Codex CLI** (faltam 90). É o primeiro app da lista que pode rodar inteiro:
   - termios (`tcgetattr/tcsetattr/cfmakeraw/openpty/ttyname_r/tcgetpgrp`) para a interface de terminal;
   - ~~`getaddrinfo` e TLS~~: feitos (SecureTransport real; o login por código de dispositivo chega à OpenAI). Falta concluir
     um login de ponta a ponta e conferir o polling do token;
   - CFURL, CFError, CFPreferences, CFTimeZone;
   - FSEvents (pode começar sem eventos) e proxies (`CFNetworkCopyProxiesForURL`, `SCDynamicStore`: "sem proxy");
   - `proc_pidinfo/proc_pidpath/proc_listchildpids`, `flock/lockf`, `copyfile_state_*`, `acl_*` (sem ACL);
   - `/usr/bin/sandbox-exec`.
2. **Slack**: depois do VS Code chegar à janela, sobram só ~123 a mais: libc++ (em andamento), `CFPreferences*`, CoreSpotlight vazio,
   `AVFAudio` (constantes) e `SMAppService`. Ele exige a integridade do asar e o Safe Storage (este já está pronto).
3. **Cursor**: o mesmo caminho do VS Code. Os ~760 a mais são libc++ dos módulos `.node` e Security (já coberto). O Swift fica só no
   supervisor de atualização, que é opcional.
4. **Grok Bot** e **Claude**: Electron mais integridade do asar. O Swift deles mora em módulos opcionais (computer use, simulador
   iOS), mas o `swift_addon.node` do Claude talvez carregue na partida e precise do runtime Swift (sessão OpenSwiftUI).
5. **Codex desktop**: o porte do Chrome (`ChromeMain`) mais CoreText.
6. **WhatsApp**: runtime Swift, Foundation do Swift, Combine, SwiftUI (OpenSwiftUI) e UIKit/Catalyst. É o mais longe.
