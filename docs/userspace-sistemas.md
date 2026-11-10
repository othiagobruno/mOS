# Frente sistemas / userspace

Estado do que esta frente implementou. Não é compatibilidade universal: cada item diz se é funcional, stub ou limitação. Metal, AIR, AVFoundation, AudioToolbox, CoreAudio, VideoToolbox e decodificadores de mídia não foram editados (outra frente).

## Preferências — funcional

`NSUserDefaults` e `CFPreferences` gravam property list XML.

- Usuário atual: `NSHomeDirectory()/Library/Preferences/<domínio>.plist` (hoje `NSHomeDirectory` é `/Users/Visitante`).
- Qualquer usuário: `/Library/Preferences/<domínio>.plist`.
- `NSGlobalDomain`, `kCFPreferencesAnyApplication` e `.GlobalPreferences` são o mesmo arquivo.
- `objectForKey:` procura o domínio primário, as suítes, `.GlobalPreferences`, o domínio de qualquer usuário e, por último, o registro.
- `CopyAppValue` não vê o registro: usuário do app, `.GlobalPreferences`, qualquer usuário do app.
- `registerDefaults:` e a semente `com.apple.universalaccess` / `mouseDriverCursorSize = 1.0` ficam só em memória. O registro não dispara KVO.
- `synchronize` devolve falha se a escrita falhar. A escrita é `arquivo.tmp` + `rename` no mesmo diretório.
- Arquivo vazio vira dicionário vazio. `bplist00` não é importado e não é sobrescrito.
- Data ISO-8601 (`YYYY-MM-DDTHH:MM:SSZ`) vira segundos desde 1970 em UTC. Se o texto veio de um plist, a gravação o preserva; data criada pelo bridge sai como double em texto.

Limitações: não há `cfprefsd`. Outro processo só é visto se `mtime` ou tamanho mudarem (`stamp = mtime<<20 xor tamanho`). `CurrentHost` e `AnyHost` são o mesmo domínio. `CFPreferencesAppValueIsForced` é 0 porque não há MDM. Valor que não é property list é recusado, sem exceção. O container devolvido é cópia.

### Contrato para Storage

O VFS precisa, em `/Users/Visitante` e em `/Library`:

- `mkdir` de `Library`, `Library/Preferences` e `Library/Application Support/...`
- `fopen` / `fread` / `fwrite` / `fclose`
- `rename` atômico no mesmo volume (`path.tmp` → `path`)
- `unlink`
- `stat` com `mtime` em segundos e `st_size` corretos. Se o `mtime` não mudar e o tamanho ficar igual, a releitura entre processos não vê a gravação até um `synchronize` com o arquivo diferente.

Não há mudança de dyld nem de VFS neste trabalho.

## TCC e computer use — funcional como registro local

Arquivo: `NSHomeDirectory()/Library/Application Support/com.mos.tcc/decisions.txt`, linhas `bundle serviço granted|denied|pending`.

- Ausente = não determinado. `CGPreflightScreenCaptureAccess` e `AXIsProcessTrusted` devolvem 0.
- `CGRequestScreenCaptureAccess` e `AXIsProcessTrustedWithOptions:` com `AXTrustedCheckOptionPrompt` gravam `pending` e devolvem 0 até alguém decidir.
- `denied` não pede de novo.
- `CGDisplayCreateImage`, `CGWindowListCreateImage` e o resto da captura em `cgevent.c` não devolvem pixels sem `kTCCServiceScreenCapture` granted.
- Com acessibilidade granted, as funções AX deixam de responder `kAXErrorAPIDisabled` (-25211) e passam a `kAXErrorFailure` (-25200). A árvore não existe aqui.

### Contrato para a Interface

1. Antes de devolver pixels de tela ou de injetar a árvore de acessibilidade, ler `cx_tcc_preflight("kTCCServiceScreenCapture")` ou `cx_tcc_preflight("kTCCServiceAccessibility")`.
2. Se o estado for pending (`cx_tcc_state` devolve 3), mostrar o prompt. A decisão é `cx_tcc_decide(bundle, serviço, granted)` com granted 1 ou 0. Bundle NULL usa o bundle do processo.
3. A injeção de eventos (`CGEventPost`) não é bloqueada por este registro: o gate é a captura e a árvore.
4. Quando a árvore existir, o ponto de entrada continua `AXUIElementCopyAttributeValue` e as irmãs. Hoje, com permissão, elas devolvem -25200 de propósito.

Não há branch de Interface aberta no GitHub para alinhar este contrato (consulta em 2026-11-10: nenhum PR aberto).

## ServiceManagement — funcional no registro, launchd como já era

- `register` de agente/daemon continua chamando `cx_launchd_cmd` (load). Se o plist não existe, status 3 (`notFound`).
- O plist com `Label` é copiado para `~/Library/LaunchAgents/<Label>.plist` ou `/Library/LaunchDaemons/<Label>.plist`. O launchd já varre esses diretórios no boot (`userland/init.c`).
- O status (0, 1 ou 3) fica em `Application Support/com.mos.smappservice/status.txt` e é relido em `-status`.
- `openSystemSettingsLoginItems` continua vazio: não há UI de Ajustes nesta frente.

## Notificações CF — funcional neste processo

- Centro local: o `NSNotificationCenter` padrão. `cx_post` também chama os observadores C.
- Centro Darwin: entrega neste processo com object e userInfo NULL, e `notify_post`. O eco do notifyd é contado por nome. Limitação: um post externo do mesmo nome que chegue antes do eco pode ser descartado. Sem nome, o observador só vê posts deste processo.
- Centro distribuído: só neste processo. Não há `distnoted`; `userInfo` não atravessa processo. `NSDistributedNotificationCenter` segue sendo o centro local (limitação anterior).
- Drop/Coalesce/Hold são aceitos; a entrega é síncrona. Snapshot de 128 observadores por post.

## XPC — o que já era funcional, e o que falta

O fio `xpc.c` + launchd entre processos do mOS já existe (não é a libxpc da Apple). Não foi reescrito.

Ainda faltam, e não foram simulados: `xpc_dictionary_set_mach_send` / `copy_mach_send`, `xpc_pipe_*`, `xpc_set_event_stream_handler`. `NSXPCConnection` continua um stub de processo (resume só marca a conexão). Helpers de computer use que esperam um daemon XPC da Apple não sobem sozinhos.

## Keychain — funcional, sem criptografia

`SecItemAdd` / `CopyMatching` / `Update` / `Delete` para senha genérica e de internet já estavam implementados e testados em `userland/tests-security`. O arquivo não é cifrado. Esta frente não mudou esse código.

## URLSession — parcial, como já estava

Há callbacks de dados e de download em `foundation_more.c`. Desafio de autenticação e `didReceiveResponse` não são tratados (a conexão segue). Não é um `NSURLSession` completo.

## UserNotifications — autorização local, sem push

`UNUserNotificationCenter.currentNotificationCenter`, `requestAuthorizationWithOptions:completionHandler:` e `authorizationStatus` (0 não determinado, 1 negado, 2 autorizado). O pedido grava pending no TCC (`userNotifications`). O bloco só é chamado em `cx_un_decide`. Não há servidor APNs nem `UNNotificationSettings`.

## StoreKit — stub local rotulado

`SKPaymentQueue.canMakePayments` é NÃO. `addPayment:` não conclui compra. `transactions` é um array vazio. `cx_storekit_verify_receipt` devolve -1 e o texto "recibo não verificado: o mOS não valida recibo da App Store nem fala com a Apple". Não há rede com a Apple, não há recibo assinado e não há contorno de FairPlay. Apps da App Store continuam sujeitos a DRM que esta frente não quebra.

## Foundation / ICU — núcleo real, não os 278 símbolos

`userland/libsystem/fmt_core.inc` formata de verdade, com saídas fixas no `tools/fmttest.c`:

- inteiro e decimal com agrupamento: `en` vírgula e ponto, `pt`/`de` ponto e vírgula, `fr` espaço e vírgula
- data gregoriana numeric, abbreviated, long e complete, e hora curta/padrão, para en, pt, de e fr
- lista ("A, B, and C" / "A, B e C" / "A, B und C" / "A, B et C")
- conversão de comprimento (m, km, cm, mm, ft, in, mi) e massa (kg, g, lb, oz)
- soma decimal em base 10 (`0.1 + 0.2 = 0.3`)
- Markdown mínimo: `**negrito**`, `*itálico*`, `` `código` ``, `[texto](url)`

O mesmo texto de data/número está no overlay Swift (`FormatStyles.swift`, `Coders.swift`). Esse overlay não foi executado aqui: não há `swiftc`. O teste que rodou é o núcleo C.

Não está feito: os símbolos mangled do FoundationInternationalization (a conta de ~278 do WhatsApp), `PersonNameComponents`, plurais do ICU, calendário que não seja o gregoriano, e locale fora de en/pt/de/fr (cai em inglês, com agrupamento de pt/de para o número quando o idioma não é en nem fr). A dylib `libicucore.A.dylib` continua sendo a do `tools/build-icu.ps1`; este núcleo não a exige.

## Fora desta frente

Não há contrato novo com Metal nem com mídia. Os arquivos `metal.c`, AIR, AVFoundation, AudioToolbox, CoreAudio, VideoToolbox e decodificadores não foram alterados.

## Testes de host

Rodados com gcc 13 (`-std=c11 -Wall -Wextra -O1`) neste ambiente. Não há zig nem QEMU aqui: o `build.ps1` do libSystem e a VM não rodaram. O overlay Swift não foi compilado (`swiftc` ausente).

- `tools/prefstest.c`: 72/72
- `tools/cfnotifytest.c`: 15/15
- `tools/tcctest.c`: 18/18
- `tools/smlogintest.c`: 16/16
- `tools/fmttest.c`: 87/87

## Sobreposição com outras frentes

Consulta em 2026-11-10, PRs abertos:

- AppKit (tabelas, teclado, barra de menus) não toca estes arquivos.
- Instalação/runtime edita `vscode_frameworks.c` em `LSSharedFileList*` (itens de login em `Library/Preferences/com.mos.loginitems`, sem sufixo `.plist`). Esta frente edita o mesmo arquivo só nas funções AX. O launchd dessa branch continua varrendo `LaunchAgents` e `LaunchDaemons`, que é onde o SMAppService publica o plist.
- JIT não toca libSystem desta frente.
