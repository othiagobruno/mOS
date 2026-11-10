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
| WhatsApp | 9 | 8.922 → 8.675 → 7.783 → 4.730 (dylibs Swift do mOS) → 4.444 (Combine) → 4.415 (Dispatch) → 4.265 (Foundation, marco C) → 3.694 (UIKit.framework do Catalyst) → 3.622 → 3.413 (libswiftUIKit completa, 7 nov.; ver [uikit-catalyst.md](uikit-catalyst.md)) → **3.105** com a `libc++.1.dylib` do LLVM (`tools\build-libcxx.ps1`) no diretório de varredura; o kernel também a carrega do disco desde 7 nov. | 8.344 | SwiftUI 555, Swift Foundation 278 (quase tudo ICU) |
| **Codex CLI** | 1 | **271 → 90 → 0** (10 nov., binário 0.167.0, 630 imports) | 152 | símbolos fechados; ver a medição abaixo |

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

### Machocheck refeito (10 nov. 2026, `codex-x86_64-apple-darwin` 0.167.0)

O binário oficial tem 278 MiB, um único executável x86_64, binds do `LC_DYLD_INFO_ONLY` (não chained) e nenhum import fraco.
No libSystem ligado a partir do `main` (6975 símbolos exportados): **630 imports, 0 símbolos distintos ausentes**.
A conta antiga "faltam 90" era o buraco que `cli.c` e o resto do libSystem já tinham fechado; não havia sido medido de novo.

O `machocheck` ainda dizia que seis bibliotecas "não existem" (Security 168, CFNetwork 14, SystemConfiguration 9,
CoreServices 11, IOKit 2, LocalAuthentication 0 símbolos). O carregador já as aceita em `libsystem_dylib_ok`. A lista
do `tools/machocheck.c` agora é a mesma: as 13 bibliotecas do CLI saem `[ok]`, e o total continua 630 / 0.

O que cada grupo faz, separado:

- **Funcional.** libc, libm e pthread, inclusive `stat$INODE64` (o carregador tira o `$` e acha o `stat` de 144 bytes).
  Security de senha e de TLS (`SecItem*`, `SecKeychain*`, `SSL*`, `SecTrustSettings*`). `CFURL`, `CFError`, `CFTimeZone`
  e `CFPreferences` (este último em `prefs.c`, gravado em disco). Proxies: `SCDynamicStoreCopyProxies` lê o configd e,
  sem ele, devolve o dicionário do macOS sem proxy; `CFNetworkCopyProxiesForURL` devolve a lista "sem proxy".
  `proc_*`, `waitid`, o pty e o `sandbox-exec` (o `execve` ignora a política e roda o comando). O ObjC que o CoreFoundation
  usa (`objc_msgSend`, `objc_getClass`, pools).
- **Stub.** `FSEventStream*`: o fluxo sobe e não entrega evento. `CFNetworkExecuteProxyAutoConfiguration*` devolve nulo
  (não há PAC). `IOPMAssertion*` aceita, porque o mOS não dorme sozinho. `copyfile_state_get` e `acl_get_*` devolvem o
  erro de "não há o quê". `fclonefileat`, `fsctl`, `fsetattrlist` e `proc_pid_rusage` devolvem `ENOTSUP`.
  LocalAuthentication está ligada e não importa símbolo nenhum.
- **Limitação.** A frase "Follow these steps…" só é impressa depois que o POST TLS em
  `https://auth.openai.com/api/accounts/deviceauth/usercode` responde. `codex login` (sem `--device-auth`) sobe um
  servidor em `localhost` e imprime o URL. Entrar com a conta e o polling até o token ficam para o Thiago: esta
  medição não autentica. O `connect` de `AF_UNIX` dentro do processo ainda devolve `ECONNREFUSED` (`sockets.c`);
  o `login` e o `login status` do fonte 0.167.0 não passam por aí. No harness, o processo para em
  "failed to create UnixStream" porque o machotest não emula `socketpair`; na VM o par é um pipe do kernel.
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
- ~~Onde parou (`claude --version`): o laço de eventos do Bun esperava sem prazo no kqueue.~~ Isso já não acontece.

**Estado em 7 nov. 2026 (noite):** `claude --version` responde "2.1.311 (Claude Code)" e sai com 0, sob WHPX e TCG.
`claude --help` imprime o uso completo e sai com 0 no TCG. O modo interativo e o login ainda não foram testados. O que
mudou, na ordem em que cada passo apareceu:
- **ICU de verdade.** O `--version` terminava em "símbolo não implementado no libSystem do mOS: _uloc_setDefault". O
  binário importa 184 funções da `libicucore` (`uloc_*`, `ucol_*`, `ubrk_*`, `udat_*`, `unumf_*`...), e o libSystem só
  tinha o `uidna_*` do curl. Agora `tools/build-icu.ps1` compila o ICU4C 74.2 (Unicode License; fonte em
  `.tools\icu-dl`) com o zig, como `/usr/lib/libicucore.A.dylib`. Os nomes ficam sem versão, como na da Apple
  (`U_DISABLE_RENAMING`), e os dados vão para `/usr/share/icu/icudt74l.dat`. O `build.ps1` põe os dois na imagem. O
  carregador prefere essa dylib à parte de IDN do libSystem (`LIBICU_DISK` no `macho.c`, como a libc++). O `machocheck`
  dela dá 721 importações e 0 ausentes.
- **20 funções do libSystem** em `userland/libsystem/bunsys.c`: `__getdirentries64` (o readdir do Bun lê a pasta direto
  do descritor), `__pthread_fchdir`, `__ulock_wait2`, `memset_pattern4/8`, `inet_aton`, `DNSService*` (respondem "serviço
  não está rodando", e o Bun usa o `getaddrinfo`), entre outras.
- **`pthread_self()` no formato do macOS** (`extra.c`). Cada thread ganhou um bloco com o layout da `struct _pthread`, com
  o TSD em +0xe0, e o GS aponta para o TSD. O mimalloc do Bun guarda o heap da thread em `pthread_self()+0x300`. Antes,
  o `pthread_self()` era `main_thread + tid`, endereços a um byte uns dos outros: a escrita de uma thread aparecia
  deslocada na leitura de outra, e o `--help` caía lendo `0x40000727`, metade de um ponteiro.
- **AVX no kernel** (`arch/cpu.c`, `fpu_init`; `arch/x86.S`). Com XSAVE e AVX na CPU, o kernel liga CR4.OSXSAVE e
  XCR0 = x87 | SSE | AVX. A troca de contexto passa a salvar o estado com XSAVE numa área de 1024 bytes alinhada a 64
  abaixo de cada quadro (`FPU_AREA` em `proc.h`; os 512 primeiros bytes continuam a imagem FXSAVE dos sinais). O
  cabeçalho XSAVE é zerado antes de cada XSAVE: ele só escreve os bits do `XSTATE_BV` que estão na máscara, e o XRSTOR
  dava #GP com o lixo da pilha. Sem XSAVE/AVX na CPU, segue o FXSAVE.
- **AVX é obrigatório para este binário.** O darwin-x64 do Claude Code é o Bun comum, não o "baseline": ele gera AVX
  sem consultar a CPU (`vmovq`, `c4 c1 f9 6e c3`) e avisa "CPU lacks AVX support". Sob o WHPX o hipervisor aceita ligar
  o AVX no XCR0, mas o estado não vem (o CPUID 0Dh continua dizendo 576 bytes, e o XRSTOR dá #GP). Por isso o
  `fpu_init` confere a área e fica no FXSAVE, e o `run.ps1` tira o AVX do CPUID do convidado no WHPX
  (`-avx,-avx2,-fma,-f16c,-avx512f`). Resultado: no WHPX o `--version` funciona, mas o `--help` cai com #UD no AVX. No
  TCG (`run.ps1` sem `-Accel`, mais lento) e em hardware real com AVX, ele roda.

- **`claude -p oi` (TCG):** a CPU aparece com "avx avx2". Ele parava em "Unexpected __ulock_wait() return code": o Futex
  do Zig chama o `__ulock_wait2` com `ULF_NO_ERRNO`, e com essa flag o macOS devolve `-errno` no retorno, mas o mOS
  devolvia -1. Corrigido no `vscode_libc.c`.
- **Regressão das mudanças acima (WHPX):** o VS Code abre e o terminal integrado roda bash. O fluxo do Claude + Chrome
  em 2816 MB chega à página do Google, com 538 a 780 MiB livres e nenhuma queda.

**Estado em 8 nov. 2026:** no TCG, `claude -p oi` vai até a recusa por falta de login: "Not logged in · Please run
/login", com código 1, cerca de 30 s depois de abrir. No modo interativo, num pseudoterminal, ele desenha a tela de boas-vindas e a
escolha de tema ("Let's get started. Choose the text style..."). A seta para baixo move a seleção e troca a prévia de
sintaxe. O Enter confirma o tema e abre "Welcome to Claude Code v2.1.311 / Select login method". O teste para aí, porque
o login é do usuário. O redimensionamento chega como SIGWINCH, e ele redesenha na largura nova. O ^C chega como SIGINT:
durante a inicialização, ele sai com 130. O que foi preciso:
- **Entrada padrão dos serviços** (`userland/init.c`, `docs/servicos.md`). O `-p` lê o stdin inteiro antes de começar. Um
  job do init lia o terminal do console, que nunca chega ao fim, e ficava parado. Agora um job de plist recebe
  `/dev/null`, como no launchd do macOS, ou o `StandardInPath`. Um `.servico` pode pedir `Entrada=/dev/null`.
- **`madvise` que descarta** (`extra.c`; kernel: `SYS_MUNMAP` com `MOS_MUNMAP_DISCARD`). Depois do stdin, o `-p` girava
  para sempre numa thread só. O despejo de threads (`/Sistema/Biblioteca/despejar-threads`) mostrava sempre o mesmo RIP.
  A memória do convidado foi lida pelo QMP (`human-monitor-command` com `cpu-index` da CPU que roda o processo:
  `x/20i`, `x/64xb`, `info tlb`). Era a sondagem do `PropertyTable::rehash` do JavaScriptCore. O vetor de índices
  (0 = vazio) veio do `mi_zalloc` do mimalloc 3.5.3 com ponteiros velhos dentro, então não havia vaga e o laço não
  terminava. O mimalloc devolve memória ao sistema com `madvise` (purge) e depois a trata como zerada. O `madvise` do mOS
  não fazia nada. Com `MIMALLOC_PURGE_DELAY=-1` ele já passava. Agora `MADV_DONTNEED`, `MADV_FREE` e
  `MADV_FREE_REUSABLE` tiram as páginas privadas das faixas anônimas sob demanda, que voltam zeradas no próximo toque,
  como o XNU faz com o `MADV_FREE`. Páginas compartilhadas, COW e de arquivo ficam.
- **`MAP_FIXED|MAP_ANON` não solta a reserva** (`MOS_MUNMAP_KEEP`). O libSystem trocava o trecho por páginas zeradas com
  um `munmap`, e o `munmap` de um trecho no topo devolvia os endereços ao `mmap_next`. O próximo `mmap` de outro
  componente caía em memória que ainda era do chamador. O claude faz isso na partida (`MAP_FIXED no topo (pid 3,
  0x400188001000 + 0x1000)` no log). O PartitionAlloc do Chromium descomita do mesmo jeito.
- **`__ulock_wait` no futex do kernel** para os tipos privados de 32 bits (`UL_COMPARE_AND_WAIT`, `UL_UNFAIR_LOCK`).
  Girando com `sched_yield`, as threads ociosas do Bun disputavam a trava grande do kernel com a thread principal. Os de
  64 bits e o compartilhado entre processos ainda giram, agora comparando os 64 bits.
- **PtyRun** (`userland/tests-pty/ptyrun.c`, `tools/build-ptyrun.ps1`). Roda um programa num pseudoterminal, como o
  node-pty, segue um roteiro (`esperar`, `quieto`, `tecla`, `se`, `redimensionar`, `tela`, `eco`, `fim`) e imprime a
  tela num emulador VT100 mínimo no log serial (`tela|`). Ele responde às perguntas de terminal (DA, DSR, XTVERSION,
  OSC 10/11). Roteiros: `tools/testdata/claude-tui.txt`, `claude-tui-curto.txt` e `claude-p-mimalloc.txt`.
- **Regressão (WHPX, 8 nov.):** o VS Code abre com essas mudanças (`tools/vscode-teste.ps1 -OutDir build-vscreg`). O
  workbench e o diálogo "Welcome to VS Code" ficam desenhados por 7 minutos, sem queda.
- **`thread_suspend` / `thread_resume` / `thread_get_state` de verdade** (kernel `SYS_THREADCTL`, libSystem
  `vscode_libc.c`). O coletor do JavaScriptCore e o sampler do Chromium usam isso. A thread suspensa sai da CPU por IPI
  e só deixa de rodar na volta ao ring 3 (`sched_kexit`), depois que a syscall terminou. A primeira versão tirava a
  thread já na entrada da syscall, que então voltava com `rax` igual ao número dela: o Codex desktop caía 3 vezes em 3.
  Com a correção, a sessão dos apps rodou o Codex 3 vezes em 3 sob WHPX, 150 s cada, com o `threadctl` exercitado e
  nenhuma falta.
- **AVX inteiro nos sinais.** A moldura do sinal agora leva o cabeçalho XSAVE e as metades altas dos YMM
  (`usigframe_t.xext`). Antes, um tratador que usava AVX devolvia os YMM dele ao código interrompido.
  `userland/tests-signals/avx.c`, via `tools/stage-teste-macho.ps1 -Flags '-mavx2'` no TCG: 121 tratadores sujaram
  todos os YMM no meio do laço de conferência, e nenhuma volta achou o padrão alterado.
- **Destrutores de TSD na saída de thread** (`cx_thread_run_dtors`, em até 4 voltas, como o libpthread). Com isso o
  mimalloc abandona o heap da thread que termina.

Em aberto:
- **SIGSEGV intermitente no modo interativo**, durante a primeira tela, com 4 CPUs: 3 quedas em cerca de 12 rodadas, e
  outras depois. Com `-Cpus 1` não houve queda em 5 rodadas, o que aponta para SMP, mas a amostra é pequena. Os detalhes:
  - Sempre é uma lista livre do mimalloc com um ponteiro inválido. A falta veio no `malloc` rápido (o `page->free`) e
    ao juntar os `free` de outras threads.
  - Os ponteiros (`0x400103340a10`, `0x400103360AF0`, `0x400103700bd0`, `0x400103401f10`) são blocos da arena de 1 GiB
    do mimalloc (`0x400140000000`) com o bit 30 apagado, ou seja, 1 GiB abaixo.
  - O rastreamento (`stage-claude-code.ps1 -RastrearMunmap`, com as linhas `mm:` no log) mostra que nada foi mapeado
    nessa faixa: o ponteiro já nasceu errado, não é memória desmapeada.
  - Descartados:
    - o descarte do `madvise` (cai igual com `MIMALLOC_PURGE_DELAY=-1`);
    - sinais (nenhum chega antes);
    - o `thread_suspend` (o Bun não chama);
    - os travamentos do libSystem;
    - a troca da base do GS.
  - O `__ulock_wait` em laço (`CX_ULOCK_SPIN=1`) e o JIT desligado (`BUN_JSC_useJIT=0`) ainda não têm rodadas
    suficientes.
  - Próximo passo: congelar a thread na falta e ler a página do mimalloc pelo QMP.
- Para sair com dois ^C, o segundo precisa chegar logo. No TCG, até 300 ms entre teclas viram segundos, e ele só mostra
  "Press Ctrl-C again to exit".
- O AVX continua obrigatório: sob WHPX só passa o `--version`. O login (`/login`) é do usuário.

Para repetir (TCG, `run.ps1` sem `-Accel`): `.\build.ps1 -OutDir build-cc`. Para o `-p`, rode
`.\tools\stage-claude-code.ps1 -Binario "build\apps\x\claude-code\claude" -OutDir build-cc -Argumentos "-p oi
--debug-to-stderr" -SizeMiB 700 [-DespejarThreads]`. Para o modo interativo, rode o mesmo comando com `-Tui` no lugar de
`-Argumentos`. Depois, `.\run.ps1 -Headless -Memory 2048 -Image build-cc/mOS.img -Serial build-cc/serial.log -Port
<livre>`. O binário traz o `__TEXT` em 0x400001000000: o deslocamento no arquivo é o RIP menos esse valor.

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
`kernel/compat/macho.c` em que a libc++ vem do disco. Desde 7 nov. o kernel faz o mesmo: a libc++ e a libc++abi vêm de
`/usr/lib/libc++.1.dylib` quando ela existe no disco (`libcxx_name` no `macho.c`, no link e no `dlopen`), e o desvio para o
libSystem (`cxxabi.c`/`libcxx.c`) só vale quando ela falta. No Slack, o `cf-prefs.node` carrega com 2 imagens (ele e a
libc++) e 0 símbolos ausentes. O machotest comum passa a fazer o mesmo quando acha a dylib.

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
5. **SQLite:** o amálgama oficial 3.54.1 (domínio público, em `.tools\sqlite-dl`, SHA3-256 conferido) vira
   `build\sqlite\libsqlite3.dylib` (`tools\build-sqlite.ps1`, chamado pelo `build.ps1`, que a põe em `/usr/lib` da
   imagem). As 84 importações dela existem no libSystem, e a partida já cria bancos com ela. Também existem
   `dispatch_io_*`, `dispatch_read/write` (`dispatch_io.c`) e `CFReadStream*`/`CFWriteStream*` sobre o
   `NSInputStream` (`cfstream.c`).
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

### Terceira rodada (7 nov. 2026): desempenho, preferências e o log do próprio WhatsApp

- **Liberação quadrática:** todo `-dealloc` percorria a lista inteira de observadores do `NSNotificationCenter`
  (`str_dealloc` aparecia em metade das amostras). Agora um conjunto de alvos com hash deixa a liberação comum em
  O(1) (`foundation.c`).
- **`createDirectoryAtPath:withIntermediateDirectories:YES`** só criava o último nível, e o contêiner do App Group
  (`Library/Group Containers/group.net.whatsapp.WhatsApp.shared`) nunca existia. Agora cria os intermediários e
  aceita a pasta existente. Com isso o WhatsApp cria as travas, `Message/Media` e o **próprio log**, em
  `…/Logs/WhatsApp_2/whatsapp-*.log`, que é a melhor ferramenta para seguir a partida.
- **`NSUserDefaults`:** `removeObjectForKey:` não fazia nada; `setObject:nil` não removia; o KVO disparava mesmo sem
  mudança. Entraram `dictionaryRepresentation`, `dataForKey:`, `URLForKey:`, os domínios persistentes etc.
- Também: `createFileAtPath:contents:attributes:`, `setAttributes:ofItemAtPath:error:`,
  `attributesOfFileSystemForPath:error:` com tamanhos (o app registrava `NSFileSystemFreeSize/error/nil-value`) e
  EBADF no `dispatch_io` com fd inválido.

**Onde está agora:** o log do WhatsApp mostra a partida até `app-group-preparer//…/marked-as-new-install-or-upgrade`.
Logo depois começa um laço em `/props-store//gabp.o`: a chave de configuração vem nil, ele usa o padrão, grava
`ABExposureKey: nil`, posta `WAExposureKeyDidChange` (sem observadores no `NSNotificationCenter`) e repete por um
bloco que se reagenda. Não é o KVO do `NSUserDefaults`, que só disparou para `LogCounter`. O componente vazio em
`/props-store//` é só a junção de `'%@%@%@'` (`"/props-store/"` + `"/gabp.o"` + `"/default"`), não um identificador
vazio.

O que se sabe do laço (quarta rodada):
- Ele roda na fila serial `com.whatsapp.properties-store.gabp.o`: um bloco (thunk em SharedModules +0x20f97) se
  reagenda nela. O trampolim do dispatch evita o estouro de pilha, mas não o laço.
- O armazenamento tem a mensagem `skip set/ABExposureKey: %@`, ou seja, pula a gravação quando o valor não muda; no
  mOS ele sempre grava `nil`, como se o valor atual nunca fosse nil.
- **Não é determinístico:** numa rodada a partida passou do `gabp.o`, seguiu para `/props-store//abp.p` e caiu
  dentro do `malloc` com o heap já corrompido (chamado de `stringByAppendingPathComponent:`). Há uma escrita fora
  de bloco antes desse ponto, ainda não localizada.
- O log grava `00:00:00.000` e o nome `whatsapp-1970-01-01-00-00-00`, embora o `clock_gettime(CLOCK_REALTIME)`, o
  `time()` e a ponte `Date ↔ NSDate` estejam certos. Falta descobrir de qual relógio ou formatador o logger tira a
  hora: se for o `NSDateFormatter`/`DateFormatter` do mOS formatando o epoch, a lógica do app com validade de
  propriedades pode estar vendo datas de 1970.
- Corrigidos nesta rodada: `NSNumber.unsignedLongLongValue` e afins (o app via 0 MB de espaço livre),
  `sortedArrayUsingDescriptors:`/`sortUsingDescriptors:` e EBADF no `dispatch_io` com fd inválido.

## Roteiro (do mais perto para o mais longe)

1. **Codex CLI** (0 símbolos ausentes em 10 nov.; ver a medição acima). O que resta para a conta real é o login, fora desta medição:
   - ~~termios (`tcgetattr/tcsetattr/cfmakeraw/openpty/ttyname_r/tcgetpgrp`)~~: feitos de verdade, com pseudoterminais
     no kernel (`kernel/proc/pty.c`, `userland/libsystem/pty.c`; ver "Terminal integrado" em [porte-vscode.md](porte-vscode.md));
     o mesmo caminho abre o shell no terminal integrado do VS Code, do Cursor e do Claude (node-pty);
   - ~~`getaddrinfo` e TLS~~: feitos (SecureTransport real; o login por código de dispositivo chega à OpenAI). Concluir
     o login com a conta e o polling do token fica para quem tem a conta;
   - ~~CFURL, CFError, CFTimeZone~~ e ~~CFPreferences~~ (`prefs.c`, valor gravado);
   - FSEvents é stub (sobe e não entrega) e os proxies são funcionais ("sem proxy" quando o configd não responde);
   - ~~`proc_pidinfo/proc_pidpath/proc_listchildpids`, `flock/lockf`~~; `copyfile_state_get` e `acl_get_*` devolvem erro
     (não há ACL nem estado de cópia);
   - ~~`/usr/bin/sandbox-exec`~~: o `execve` ignora a política e roda o comando.
2. **Slack**: depois do VS Code chegar à janela, sobram só ~123 a mais: libc++ (em andamento), `CFPreferences*`, CoreSpotlight vazio,
   `AVFAudio` (constantes) e `SMAppService`. Ele exige a integridade do asar e o Safe Storage (este já está pronto).
3. **Cursor**: o mesmo caminho do VS Code. Os ~760 a mais são libc++ dos módulos `.node` e Security (já coberto). O Swift fica só no
   supervisor de atualização, que é opcional.
4. **Grok Bot** e **Claude**: Electron mais integridade do asar. O Swift deles mora em módulos opcionais (computer use, simulador
   iOS), mas o `swift_addon.node` do Claude talvez carregue na partida e precise do runtime Swift (sessão OpenSwiftUI).
   Estado em 7 nov.: o Claude 4.23118 sobe, monta os menus em pt-BR e carrega claude.ai/login numa WebContentsView. O
   `swift_addon.node` falta, e o app segue sem ele. Faltava, e já foi resolvido:
   - `-[NSString compare:options:]`: o Electron 44 confere o SHA-256 do `ElectronAsarIntegrity` contra um hash embutido, e as
     chaves saíam fora de ordem;
   - a fonte de dispatch liberada antes do cancel handler (`source_pthread`);
   - o RDX do `sendto` (ver Slack);
   - a WebContentsView filha (o claude.ai) sempre oculta: o `NSThemeFrame` falso do `nsurl.c` devolvia `isHidden` lixo, e o
     `-[WebContentsViewCocoa updateWebContentsVisibility]` via um ancestral oculto. O teste mínimo `tools/testdata/electron-wcv`
     (BrowserWindow + WebContentsView sobre o Electron do Cursor) reproduz e confere;
   - `orderOut:` era um no-op (janelas escondidas ficavam na tela). Agora o window server esconde e mostra
     (`GUI_FOCUS_HIDE`/`GUI_FOCUS_SHOW`), e `occlusionState` segue a visibilidade.
   Os addons em Swift (`swift_addon.node`, `computer_use.node`) ficam fora da imagem até o runtime Swift deles existir: o dyld
   ligava ~466 símbolos ausentes a 0 e o processo caía. Ele também se recusa a subir com `--log-net-log`.

**Rede dos apps Chromium/Electron (7 nov.)**: toda escrita de socket voltava ERR_NETWORK_CHANGED (-21). No macOS de 13.3 a
26.1, o Chromium confere o RDX depois de `sendto()` (o XNU deixa 0 ali) para contornar um bug do kernel. O `sendto` do libSystem
agora devolve RAX:RDX. Com isso o Slack carrega a tela de login de app.slack.com e o Chrome abre https://example.com/. Para
investigar rede: `--log-net-log=/Users/Visitante/n.json` com `vscode-teste.ps1 -Persist`; o arquivo sai da imagem pelo
`build\fatcat.exe` (`tools/fatcat.c`).

**Fontes web CFF (7 nov.)**: as fontes web do Slack (e de muitos sites) são OpenType com contornos CFF (`OTTO`). Antes o
kernel as recusava e o Blink caía na fonte seguinte da lista. O `kernel/gfx/font_cff.inc` (incluído por `font.c`) faz:
- a leitura dos INDEX e DICT do CFF 1;
- o interpretador de charstrings Type 2, com subrotinas locais e globais, flex, CID com FDSelect e aritmética;
- as cúbicas viram segmentos para o mesmo rasterizador da glyf;
- o `font_sane` confere a estrutura inteira, e o interpretador limita leituras, pilha, profundidade, operações e segmentos.

Para testar no host, sem VM: `tools/cfftest.c` com `tools/woff2otf.js` (os RobotoSlab da documentação do QEMU são CFF
subrotinizados: 1052 glifos sem erro). Um fuzz com a tabela colada a uma página sem acesso não leu fora dela.

**Login pelo navegador (7 nov.)**: o `-[NSWorkspace openURL:]` era um stub que devolvia NO, então o "Sign in" do Slack e o
"Continue with Google" do Claude não abriam nada. Agora há três peças:
- `userland/libsystem/openurl.c` faz `openURL:` e afins, as funções `LS*` e o `NSAppleEventManager` sobre o Launch Services
  do kernel (`kernel/compat/launchsvc.c`, `SYS_LS`), que sabe quem declara cada esquema em `CFBundleURLSchemes`;
- o processo principal de cada app registra `com.mos.abrir-url.<CFBundleIdentifier>` no bootstrap ao terminar de abrir, e a
  URL chega como o Apple Event GURL (o Electron emite `open-url`) ou por `application:openURLs:` (o Chrome);
- `/usr/bin/open` (`userland/cli-macos/open.c`) faz o mesmo da linha de comando.

Conferido na VM (7 nov., 1 e 4 núcleos):
- **Slack:** mostra a tela de boas-vindas, com o título na fonte web CFF dele ("fonte de app registrada (id 35, 1123
  glifos, ..., CFF)"). O "Sign In to Slack" abre `https://app.slack.com/...` no navegador padrão, e `open slack://open`
  chega ao app (`[abrir-url] recebida`).
- **Claude:** chega ao "Sign In". O "Continue with Google" abre `https://claude.com/...` no navegador, e `open claude://...`
  chega ao app.
- **Navegador de teste:** `shell.openExternal('https://...')` de um app Electron abre o `tools/testdata/navegador-teste`,
  que declara http/https e mostra as URLs.

O que falta para o login de ponta a ponta é o navegador. O padrão do Launch Services é o Horizonte, e o app.slack.com
responde "This browser is no longer supported" (o claude.ai fica em branco). A troca do motor do navegador do sistema
pelo Chromium tem sessão própria. O login em si é do usuário.
Atualização (7 nov., tarde): o motor do Horizonte agora é o Chromium de código aberto, em
`/Sistema/Biblioteca/Horizonte/Chromium.app` (`tools/stage-chromium.ps1`; `-MotorHorizonte build\chromium\Chromium.app` no
vscode-teste). O "Sign In to Slack" abre a página real de login, e a volta `slack://` chega ao Slack. O motor próprio do
Horizonte fica de reserva. Com o motor, o Slack precisa de `-Memory 2304`: em 2048 MB os dois não cabem. Desenho,
argumentos e correções em [navegador-chromium.md](navegador-chromium.md).

Google Chrome como navegador escolhido pelo usuário (7 nov., noite; LSHandlers com `com.google.Chrome`, Slack + Chrome em
2560 MB, 4 núcleos):
- O diálogo "Abrir Slack?" do Chrome agora desenha inteiro: título, "Um site quer abrir esse aplicativo.", "Cancelar" e
  "Abrir Slack". O clique em "Abrir Slack" leva a URL ao Slack: "[abrir-url] aberta slack://open" no Chrome, "ls:
  slack://open entregue a Slack" e "[abrir-url] recebida slack://open" no Slack, que vem para a frente. O Chrome segue
  aberto.
- O que mudou no AppKit (`appkit.c`): uma janela sem moldura (style sem `NSWindowStyleMaskTitled`, como os diálogos e
  balões do views) não ganha a barra do window server (`GUI_WF_NO_TITLE`). As teclas vão à janela-chave quando a do
  topo não aceita ser chave (as popups do views ficam por cima e roubavam o teclado). O clique numa janela que aceita
  torna-a chave. As janelas filhas (`addChildWindow:ordered:`) voltam por cima da mãe quando ela vem à frente.
- O `NSURL` ganhou `scheme`, `host`, `port`, `user`, `password`, `query`, `fragment`, `resourceSpecifier` e
  `relativeString` (o `net::GURLWithNSURL` do Chromium lê `scheme` em toda conversão).
- Teclado no diálogo: o Tab e o Espaço chegam a ele (é a janela-chave), mas os botões não ganham foco pelo Tab sem o
  "Acesso total pelo teclado", como no macOS. Use o clique.
- Como testar a volta: o Chrome pula de propósito URLs de protocolo externo que chegam do sistema
  (`StartupBrowserCreatorImpl::OpenTabsInBrowser`, contra laço), então `open -b com.google.Chrome slack://open` não mostra
  diálogo nenhum, como no macOS. Use uma página que navega, como faz a página de login: `-AbrirURL "-b com.google.Chrome
  data:text/html,<a%20href=slack://open%20id=a>Abrir</a><script>a.click()</script>"` e um clique em "Abrir Slack"
  (`move:873,450` duas vezes e `click`). Sem aspas nem espaços na URL: o init divide os argumentos nos espaços e tira as
  aspas. Há também `-AbrirURL2`/`-AbrirApos2Ms` para um segundo `open`, mas com WHPX o atraso do init não é
  confiável (o relógio do kernel corre ou atrasa; um `open` marcado para 170 s não saiu em 8 min).
- **Claude + Chrome, 3584 MB:** a página `data:` com `claude://claude.ai/new` mostra "Abrir Claude?", e o clique leva a URL
  ao Claude ("ls: claude://claude.ai entregue a Claude", "[abrir-url] recebida"). No Claude, "Reject" nos cookies, "Get
  started" e "Continue with Google" abrem `claude.ai/login/app-google-auth…` no Chrome, que segue para a página real do
  Google ("Fazer Login com o Google", "Prosseguir para Claude", "E-mail ou telefone"). Dali em diante o login é do
  usuário. A VM ficou com uns 800 MiB livres.
- **Slack + Chrome, 3584 MB:** o "Sign In to Slack" abre `app.slack.com/ssb/signin…` no Chrome, e a página renderiza
  inteira ("Insira seu e-mail para entrar", Google, Apple, banner de cookies). A VM ficou com uns 1,6 GB livres.
- **Memória:** antes, com 2816 MB, o Claude e o Chrome juntos esgotavam a VM. O log mostrava "kmalloc: sem memória", e
  o `mach_make_memory_entry_64` devolvia 6 (`KERN_RESOURCE_SHORTAGE`). O serviço de rede do Chrome caía no `CHECK` de
  `platform_shared_memory_region_apple.cc` e ficava reiniciando, e o claude.ai mostrava "Can't reach Claude. Check your
  connection." Os arquivos dos apps moram na RAM da VM (Claude.app 538 MiB e Chrome 310 MiB na imagem), e o `__TEXT` de
  cada framework existia duas vezes: no arquivo e nas páginas montadas.
  Agora o kernel mapeia o `__TEXT` direto das páginas do arquivo:
  - os arquivos grandes vêm do disco em buffers de `kmalloc_pages` (`fat.c`, `mm/memory.c`), alinhados à página e
    liberados página a página pelas contagens de `pref[]`;
  - o `tcache` do `macho.c` usa essas páginas sem copiar (`tcache_borrow`);
  - o `vfs_append` nunca escreve num buffer desses no lugar;
  - `write`, `pwrite` e `ftruncate` gravam no buffer do próprio arquivo (`file_write_at`/`file_room` em
    `proc/process.c`). Antes, cada escrita copiava o arquivo inteiro para um bloco contíguo novo e regravava tudo, e
    eram esses os pedidos de `kmalloc` de vários MB que falhavam com o SQLite do Chrome e o LevelDB do Claude. O buffer
    cresce em dobras de até 16 MiB, e um buffer de `kmalloc_pages` é copiado antes da primeira escrita;
  - no mmap de arquivo (`mm/lazy.c`), uma página inteira de um buffer desses entra no processo em cópia na escrita, sem
    página nova.

  O log mostra "Electron Framework: __TEXT (198 MiB) mapeado das páginas do arquivo, sem cópia" e o mesmo para o Google
  Chrome Framework (248 MiB). Com 2816 MB, o fluxo do Claude chegou à página do Google com 543 a 773 MiB livres, sem
  falha de alocação.
  Outra medição do mesmo fluxo em 2816 MB, linhas `despejo: threads … (memória livre X de 2809 MiB)`, antes e depois
  das mudanças acima:

  | Momento | Antes | Depois |
  |---|---|---|
  | Claude subindo (30 s) | 1016 MiB | 1178 MiB |
  | Só o Claude, parado no login (60–120 s) | 877–884 MiB | 1079–1081 MiB |
  | Chrome abrindo o login (150–180 s) | 408 → 236 MiB | 444 → 479 MiB |
  | Chrome no login do Google (210–270 s) | 321–343 MiB | 567–596 MiB |

  Antes, 316 pedidos de `kmalloc` de 2,9 a 6,8 MB falhavam, o `vm_map` do `platform_shared_memory_region_apple.cc`
  errava e um renderer do Chrome não subia ("sem memória para o libSystem"). Dois helpers caíam, e a aba ficava em
  "Ah, não! … Código de erro: 11". Depois, nenhuma falha de `kmalloc`, nenhum `KERN_RESOURCE_SHORTAGE`, o serviço de
  rede do Chrome sobe uma vez só, nenhum processo cai e a aba mostra "Fazer Login com o Google".
  O Slack + Chrome também cabe em 2816 MB (antes pedia 3584): o "Sign In to Slack" abre `app.slack.com/ssb/signin…` e
  a página renderiza inteira, com 1,3 a 1,8 GB livres.
  Depois, o `fat.c` passou a pôr em páginas alinhadas os arquivos a partir de 256 KiB (antes 16 MiB), para o `lazy.c`
  pegar o `icudtl.dat`, os `.pak` e o snapshot do V8, e o libSystem passou a ser montado uma vez e compartilhado entre os
  processos ("libSystem (3312 KiB) montado uma vez e compartilhado"). Com isso, o fluxo do Claude em 2816 MB ficou com
  767 a 814 MiB livres antes do login e 599 a 630 MiB na página do Google, sem falha de `kmalloc`.
- **Diálogo atrás do navegador:** um clique na janela do Chrome fora do diálogo trazia o navegador para cima dele, e o
  diálogo sumia com a página ainda escurecida. Agora o window server conhece a dona de cada janela
  (`GUI_FOCUS_OWNER`, chamado pelo `addChildWindow:` do AppKit), e o `wm_focus` sobe as filhas visíveis acima dela,
  como no Cocoa. As teclas vão à janela-chave mesmo quando a filha está no topo.

Para repetir: `vscode-teste.ps1 -Electron ... -Accel whpx -Port <livre> -Memory 2048 -AbrirURL slack://open
-AbrirAposMs 30000`, com `-Passos "move:380,499;wait:2000;move:380,499;wait:2000;click"` para o "Sign In to Slack" (o
`move` do QMP às vezes perde os passos em x; repita o `move`). `-Cpus 1` separa problemas de SMP.
5. **Codex desktop**: o porte do Chrome (`ChromeMain`) mais CoreText.
6. **WhatsApp**: runtime Swift, Foundation do Swift, Combine, SwiftUI (OpenSwiftUI) e UIKit/Catalyst. É o mais longe.

## Apps de barra de menus (pedido de 10 nov. 2026)

Nenhum destes apps está no repositório e nenhum foi executado: não há login, mensagem nem compra, e não há bundle gratuito legítimo aqui. A superfície AppKit que eles usam (item da barra, menu, popover, `LSUIElement`) está no modelo de `appkit_status.c`. O ícone desenhado na barra do sistema continua por conta do kernel, que não tem essa faixa.

| App | Versão | Origem do bundle | Arquitetura (fonte) | Barra de menus | Helpers / login | Sparkle | FairPlay | AppKit | Kernel | Sistemas | Storage |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Open Bot | — | sem link no pedido; nenhum bundle | **não identificado** | — | — | — | — | bloqueado | — | — | — |
| Setapp (cliente) | não publicada na página de download | [setapp.com/download](https://setapp.com/download), cerca de 300 MB, macOS 10.13+. Não baixado | cliente Mac da Setapp/MacPaw. O site não diz Electron, AppKit nem SwiftUI. A [integração Electron](https://docs.setapp.com/docs/integration-with-an-electron-app) é de app de fornecedor, não do cliente | não confirmado no site oficial | `SetappAgent`, `SetappLauncher`, `SetappUpdater` (`com.setapp.DesktopClient.*.plist`, Setapp Limited), em relatos EtreCheck de usuários: [discussions.apple.com/thread/251765308](https://discussions.apple.com/thread/251765308) e [macuser.de/threads/safari-crash.887161](https://www.macuser.de/threads/safari-crash.887161/) (5 out. 2021) | não confirmado. O processo visto é o `SetappUpdater`, não o Sparkle | não é app da Mac App Store | modelo funcional (item, menu, popover, política accessory); app bloqueado, sem bundle | bloqueado: sem barra de status desenhada e sem o binário | `SMAppService` / itens de login | launchd, XPC e o registro do helper |
| Usagebar | não publicada | [usagebar.com](https://usagebar.com/), US$ 9 via [Stripe](https://buy.stripe.com/bJeaEY5kVbtZ1iG3zI8og09). Sem bundle gratuito | o site não nomeia Electron, AppKit, SwiftUI nem Catalyst | sim: "from Your Menu Bar". Some se a barra está cheia | não publicados. Lê o item de Keychain `Claude Code-credentials` | não publicado | não | modelo funcional; app bloqueado (pago, sem bundle) | desenho da barra | Keychain / Security | — |
| Usage for Claude & ChatGPT | a ficha diz "NEW IN VERSION 4", sem `CFBundleShortVersionString` | Mac App Store [id 6755173244](https://apps.apple.com/us/app/usage-for-claude-chatgpt/id6755173244), amir hayek, grátis com compra no app, 31,4 MB. Não baixado | nativo Apple: iPhone, iPad, Mac, Apple Watch e visionOS; macOS 15+. Não é Electron (Watch, widgets e iCloud no mesmo binário de 31,4 MB). A ficha não escreve "SwiftUI" | sim no Mac: limites na barra de menus | não publicados. Histórico via iCloud da conta | não se aplica (loja) | sim. Apps da Mac App Store são assinados com uma variante de FairPlay ([FairPlay, seção Apps](https://en.wikipedia.org/wiki/FairPlay)) | bloqueado: sem contornar o DRM e sem bundle legítimo solto | tradutor, se um dia houver binário | iCloud, contas, widgets | loader e a criptografia FairPlay |
| WhatsApp | 2.26.44.21 no inventário de 6 nov. 2026 | já descrito acima | Swift + SwiftUI + UIKit/Catalyst. Não é Electron e não é app de barra de menus | não | helpers do próprio app | não citado | não é o foco desta lista | stub: a barra de menus nova não desbloqueia o app | tradutor / JIT | runtime Swift, Combine, Catalyst | loader / dyld |

### Open Bot

O pedido escreveu "Open Bot" sem URL. Há mais de um produto com nome parecido (OpenBot de robótica, open.bot, OpenBot da Chazakah). O mais próximo de um app de Mac é o [OpenBot](https://openbot.run/) (uma palavra só): desktop para macOS 13+, Windows e Linux, versão 0.66.0 em 9 nov. 2026 ([changelog](https://openbot.run/changelog)). O 0.59.0 descreve um ícone na barra de menus e o Dock que some ao fechar a janela, no ajuste "Show OpenBot in the menu bar". A [página de download](https://openbot.run/download) não diz Electron, Tauri nem Swift. Isso não chega para cravar que "Open Bot" é esse app, nem para afirmar o framework. Fica **não identificado**. Nenhum instalador foi baixado.

### O que cada frente ainda segura

- **AppKit (esta frente).** Funcional no modelo: a `NSStatusBar` guarda até 32 itens, `removeStatusItem:` tira o item, o clique no botão (ou `popUpStatusItemMenu:`) abre o menu e avisa `menuWillOpen:`, item oculto não abre, o `NSPopover` respeita applicationDefined / transient / semitransient, e `setActivationPolicy:` grava regular, accessory (`LSUIElement`, sem Dock) e prohibited (recusa `activate`). O desenho do ícone na faixa do sistema continua stub: o kernel não tem a barra.
- **Kernel.** Tradutor/JIT para o que for binário nativo, e o desenho da barra de menus. Sem comando de blend, como no marco anterior. Não há bundle para traduzir.
- **Sistemas.** Keychain do Usagebar, iCloud e contas do app da loja, `SMAppService` (já existe em memória no libSystem; o `SMLoginItemSetEnabled` do ServiceManagement ainda é stub). Nada disso foi chamado contra um serviço real.
- **Storage.** Loader, dyld, VFS, XPC e os launch agents `com.setapp.DesktopClient.SetappAgent`, `SetappLauncher` e `SetappUpdater`. O login do Setapp é no navegador com código por e-mail ([artigo de suporte](https://support.setapp.com/hc/en-us/articles/360013944440-Sign-in-and-out-of-Setapp-on-a-Mac)); não foi feito. FairPlay do app da loja não foi contornado.

## Cursor, Grok Bot e o desktop do ChatGPT/Codex (10 nov. 2026)

Estes três entram na lista prioritária junto dos apps de barra de menus. Cursor, Grok Bot e o Codex desktop de 6 nov. já estão na tabela de builds medidos. O desktop novo do ChatGPT não estava. Nenhum deles foi executado e ninguém fez login. O zip do ChatGPT x64 não foi instalado: só o diretório central e o `Info.plist` foram lidos por HTTP range no arquivo oficial, para ver a arquitetura sem rodar o app.

Não são apps da Mac App Store. O pacote oficial sai de `persistent.oaistatic.com` e de `downloads.cursor.com`. FairPlay não é a trava deles. A criptografia FairPlay continua valendo só para o app da loja "Usage for Claude & ChatGPT" (id 6755173244), registrado acima, e não foi contornada.

| App | Versão vista agora | Origem | Arquitetura | Barra de menus | Helpers | Sparkle | FairPlay | AppKit | Kernel | Sistemas | Storage |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ChatGPT desktop (sucessor do Codex) | 26.1103.60933, build 25894. Zip de 8 nov. 2026 | cask `chatgpt`: `persistent.oaistatic.com/codex-app-prod/ChatGPT-darwin-x64-26.1103.60933.zip`, macOS 13+. Bundle `com.openai.codex` | casca Electron (`app.asar`, `ElectronAsarIntegrity`, `owl-electron-app.json` com `runtimeName` owl, empacotado de `codex-apps/electron`) em cima do `Codex Framework.framework` 157.0.8103.39. `NSPrincipalClass` é `BrowserCrApplication`. Não há `Electron Framework.framework` | não. O `Info.plist` não tem `LSUIElement`. Há `NSDockTilePlugIn` = `CodexDockTilePlugin.docktileplugin` | `Codex (GPU)`, `(Renderer)`, `(Service)`, `(Alerts)`, mais as variantes Aperitif; `Codex Computer Use.app` (com `SwiftProtobuf`); `launch-services-helper` | sim. `Sparkle.framework`, `Updater.app`, XPC `Downloader`/`Installer`, `SUPublicEDKey` no plist | não neste zip | stub. Janela e eventos do caminho Electron/Chromium já existem em parte; o plugin do Dock não tem `NSDockTile` no libSystem. A barra de menus nova não é o que este app pede | bloqueado: renderer, GPU e tradutor do Chromium 157 | Keychain se a casca criptografa cookies; o helper de Computer Use é Swift | loader, dyld, XPC do Sparkle e o authorization plugin `CodexComputerUseAuthorizationPlugin` |
| Codex desktop (medição de 6 nov.) | 26.623.141536 | a tabela acima. O [artigo da OpenAI](https://help.openai.com/en/articles/20001276-moving-to-the-new-chatgpt-desktop-app) diz que atualizar o app Codex o transforma no ChatGPT desktop novo | naquela medição, Chromium 149 com `Codex Framework.framework`, não Electron. O build 26.1103 acima é o mesmo produto, já com a casca Electron | não era o foco da medição | `Codex (GPU\|Renderer\|Service\|Alerts).app` | sim, na medição de 6 nov. | não | o mesmo stub do ChatGPT desktop | porte do Chrome (`ChromeMain`) e CoreText | Swift nos helpers nativos (259 símbolos na medição) | loader do Framework e o runtime Node `cua_node` |
| Cursor | cask 3.29.15; a tabela mediu 3.29.9 | `downloads.cursor.com`, bundle `com.todesktop.230313mzl4w4u92`, macOS 12+ | Electron 43.8.4, fork do VS Code, na medição de 6 nov. Fuses `101100011` (sem integridade de asar e sem criptografia de cookies) | não. É o app de janela do editor | helpers Electron `(GPU\|Renderer\|Plugin)` | não identificado. O cask marca `auto_updates`; o Swift medido está só no `cursor-update-supervisor` | não | stub no mesmo caminho do VS Code: janela, mouse, teclado e menu como dado. Abrir, operar e fechar o editor inteiro continua fora | renderer, IPC e memória (`docs/porte-vscode.md`) | libc++ dos módulos `.node` e Security. O Swift não está no processo principal | loader, dyld e o supervisor de update |
| Grok Bot | cask 0.79.3; a tabela mediu 0.79.2 | `downloads.cursor.com/sand/stable/...`, homepage [x.ai/bot](https://x.ai/bot), bundle `com.anysphere.sand`, macOS 12+ | Electron 42.11.16 na medição de 6 nov. Fuses `010011011`: integridade do asar, cookies criptografados, só carrega de `app.asar` | não publicado no site oficial nem no cask. Um app Electron não oficial com ícone de barra não é este | helper nativo "Computer Use" | não identificado no cask nem na medição | não | stub, o mesmo caminho Electron. A integridade do asar já depende do `NSBundle` devolver `ElectronAsarIntegrity` (está em `foundation.c`) | o mesmo renderer Electron, mais a checagem do hash do asar | Keychain ("Safe Storage") e os 541 símbolos Swift do helper de Computer Use | loader e o asar |

### De onde saiu cada conclusão

- **ChatGPT desktop é o Codex atualizado.** A [ajuda da OpenAI](https://help.openai.com/en/articles/20001276-moving-to-the-new-chatgpt-desktop-app) (lida em 10 nov. 2026) diz: atualizar o app Codex o transforma no ChatGPT desktop, com Chat, Work e Codex. O app anterior passa a se chamar ChatGPT Classic. O cask `chatgpt` do Homebrew baixa o zip `ChatGPT-darwin-x64-26.1103.60933.zip` do mesmo bucket `codex-app-prod` e desinstala `com.openai.codex`.
- **O que o zip contém.** O diretório central (5141 entradas) tem `Resources/app.asar`, `Frameworks/Sparkle.framework` e `Frameworks/Codex Framework.framework/Versions/157.0.8103.39/`, com helpers `Codex (GPU).app`, `(Renderer)`, `(Service)`, `(Alerts)` e as variantes Aperitif. Não há entrada `Electron Framework.framework`. O `Info.plist` confirma `CFBundleIdentifier` `com.openai.codex`, `CFBundleShortVersionString` 26.1103.60933, `LSMinimumSystemVersion` 13.0, `NSPrincipalClass` `BrowserCrApplication`, `ElectronAsarIntegrity` para `Resources/app.asar` e `NSDockTilePlugIn`. Não há chave `LSUIElement`. O `owl-electron-app.json` diz `runtimeName` `owl` e `packagedFrom` `.../codex-apps/electron/out/ChatGPT-darwin-x64/ChatGPT.app`.
- **A imprensa chamou o app de Electron.** O [9to5Mac de 17 jul. 2026](https://9to5mac.com/2026/07/17/openai-fixes-chat-access-in-the-chatgpt-app-for-mac/) escreve que o redesenho trocou o cliente nativo por um app Electron bem maior. Isso bate com a casca (`app.asar` e o diretório `electron/` no owl). O runtime que o zip carrega é o Codex Framework 157, não o Electron Framework.
- **ChatGPT Classic** fica de fora do x86_64. O cask `chatgpt-classic` é 1.2026.184, só arm64, macOS 14+, bundle `com.openai.chat`, com `launchctl` `com.openai.chat-helper`. As [notas de versão](https://help.openai.com/en/articles/9703738-chatgpt-macos-app-release-notes) de 19 dez. 2024 falam de um atalho aberto a partir da barra de menus e de personalizar a toolbar da janela. O 9to5Mac chama esse cliente de nativo. O dmg não foi aberto, então o framework (AppKit, SwiftUI ou outro) não foi lido no binário.
- **Cursor e Grok Bot** não foram reabertos. A arquitetura é a da medição de 6 nov. O cask só move a versão: Cursor 3.29.9 → 3.29.15, Grok Bot 0.79.2 → 0.79.3, os dois com `auto_updates` e o mesmo tipo de URL.

### O que falta para rodar, sem repetir a barra de menus

A `NSStatusBar` implementada neste branch não é a trava destes três. O ChatGPT desktop é uma janela (e um plugin de Dock). Cursor e Grok Bot são Electron de janela. O que continua de fora:

- **AppKit.** Janela, mouse, teclado e menu como dado já existem em parte. Falta o `NSDockTile` que o plugin `CodexDockTilePlugin` espera, e o desenho real. A casca do ChatGPT declara `ElectronAsarIntegrity`; o `NSBundle` que entrega esse dicionário ao helper já está em `foundation.c`, fora deste diff.
- **Kernel.** Tradutor e o processo renderer. No ChatGPT o Framework é Chromium 157 (`BrowserCrApplication`), no caminho do porte do Chrome. No Cursor e no Grok Bot é o renderer Electron, no caminho do VS Code. O ícone do Dock também é desta frente.
- **Sistemas.** Keychain do cookie criptografado (Grok Bot, e a casca do ChatGPT se o fuse de cookie estiver ligado; o fuse deste build não foi relido). Swift do Computer Use, do `cursor-update-supervisor` e dos helpers do Codex. Sparkle é framework próprio, com UI de atualização.
- **Storage.** Loader, dyld, VFS, os helpers `(GPU|Renderer|Service|Alerts)`, o XPC do Sparkle e o authorization plugin do Computer Use. Sem isso o zip oficial não chega a `main`.
