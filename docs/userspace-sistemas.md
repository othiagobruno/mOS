# Frente sistemas / userspace

Estado do que esta frente implementou. Não é compatibilidade universal: cada item diz se é funcional, stub ou limitação. Metal, AIR, AVFoundation, AudioToolbox, CoreAudio, VideoToolbox e decodificadores de mídia não foram editados (outra frente).

## Preferências — funcional

`NSUserDefaults` e `CFPreferences` gravam property list XML.

- Usuário atual, `kCFPreferencesAnyHost` ou host nulo: `NSHomeDirectory()/Library/Preferences/<domínio>.plist` (hoje `NSHomeDirectory` é `/Users/Visitante`).
- Host atual (`kCFPreferencesCurrentHost`): `NSHomeDirectory()/Library/Preferences/ByHost/<domínio>.<token>.plist`. O token sai de `gethostname`, só com letras, dígitos, `.`, `_` e `-`. No mOS o hostname é `mos`.
- Qualquer usuário: `/Library/Preferences/<domínio>.plist`. Qualquer usuário no host atual: `/Library/Preferences/ByHost/<domínio>.<token>.plist`.
- `NSGlobalDomain`, `kCFPreferencesAnyApplication` e `.GlobalPreferences` são o mesmo arquivo, com a variante ByHost à parte.
- `objectForKey:` procura ByHost do app, o domínio do usuário, as suítes, ByHost de `.GlobalPreferences`, `.GlobalPreferences`, o domínio de qualquer usuário e, por último, o registro.
- `CopyAppValue` não vê o registro: ByHost do app, usuário do app, ByHost de `.GlobalPreferences`, `.GlobalPreferences`, qualquer usuário do app.
- `registerDefaults:` e a semente `com.apple.universalaccess` / `mouseDriverCursorSize = 1.0` ficam só em memória. O registro não dispara KVO.
- `synchronize` devolve falha se a escrita falhar. A escrita é `arquivo.tmp` + `rename` no mesmo diretório e continua em XML.
- Arquivo vazio vira dicionário vazio. `bplist00` com trailer válido é lido. `bplist00` ilegível não é sobrescrito. A gravação seguinte de um domínio sujo sai em XML.
- Data ISO-8601 (`YYYY-MM-DDTHH:MM:SSZ`) e a data binária (`bplist` 0x33, CFAbsoluteTime) viram segundos desde 1970 em UTC. Se o texto veio de um plist, a gravação o preserva; data criada pelo bridge sai como double em texto.

Limitações: não há `cfprefsd`. O carimbo da releitura é o hash do conteúdo do arquivo, então dois processos veem a mesma chave mesmo com `mtime` e tamanho iguais. `CFPreferencesAppValueIsForced` é 0 porque não há MDM. Valor que não é property list é recusado, sem exceção. O container devolvido é cópia. `NSUserDefaults synchronize` grava o domínio do usuário, não o ByHost.

### Contrato para Storage

O VFS precisa, em `/Users/Visitante` e em `/Library`:

- `mkdir` de `Library`, `Library/Preferences`, `Library/Preferences/ByHost` e `Library/Application Support/...`
- `fopen` / `fread` / `fwrite` / `fclose`
- `rename` atômico no mesmo volume (`path.tmp` → `path`)
- `unlink`
- leitura do arquivo depois do `rename`. O carimbo das preferências é o hash dos bytes, não o `mtime`.

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

## XPC — Mach entre processos, sem o fio da Apple

O fio continua o de `xpc.c` (os dois lados são esta lib). Não usa AF_UNIX e não mexe em `sockets.c`. O número de uma porta não atravessa sozinho: no fio, o tipo 13 vai com um descritor `COPY_SEND`, e o kernel entrega um direito de envio com o nome do processo que recebe. No mesmo processo, `copy_mach_send` devolve o mesmo nome.

- **Pipe:** `xpc_pipe_create` / `xpc_pipe_create_from_port` / `xpc_pipe_routine` / `xpc_pipe_receive` / `xpc_pipe_routine_reply` falam esse fio. Não é o protocolo privado do launchd.
- **Eventos:** `xpc_set_event_stream_handler` guarda o bloco e o chama. `xpc_event_stream_post` entrega aqui e, se outro processo registrou o stream, manda em `com.mos.xpc.ev.<stream>`. Não há o launchd de eventos da Apple.
- **NSXPC:** `NSXPCListener` faz check-in e `NSXPCConnection` faz o proxy. Seletor com até 4 argumentos objeto; o bloco de resposta é o último e é chamado na hora. Sem filtro de protocolo. `anonymousListener` não cria endpoint. Opções de `initWithMachServiceName:options:` ficam guardadas e o cliente não as repassa ao serviço.
- **Transporte para o AX** (`xpctransport.h`, a Interface pluga em `axtransport.h`): `cx_xpc_peer_ready`, `cx_xpc_peer_listen`, `cx_xpc_peer_call`, `cx_xpc_peer_send`. Serviço `com.mos.xpc.peer.<pid>`. `call` espera o `send` de volta. Esta frente não editou os arquivos de AX.

## Keychain — funcional, arquivo versão 2 cifrado com o nome de login

`SecItemAdd` / `CopyMatching` / `Update` / `Delete` para senha genérica e de internet continuam em `security.c`. O arquivo `$HOME/Library/Keychains/mos-keychain.txt` na versão 2 é `mOS-keychain 2\n` + nonce + ciphertext + MAC. A chave é HMAC-SHA256 do nome de login (`USER`, senão `LOGNAME`, senão o último componente de `HOME`, senão `visitante`), com sal fixo `mOS-keychain-v2`. Não é senha e não é credencial: quem sabe o login abre o arquivo. A versão 1 (texto) ainda é lida e a gravação seguinte passa para a versão 2. MAC ruim marca o arquivo como bloqueado e a gravação devolve `errSecNotAvailable`, sem apagar o que estava no disco.

## URLSession — dados, download, resposta e dois desafios

Há callbacks de dados e de download em `foundation_more.c`. Antes do HTTP de uma URL `https`, o delegate recebe `NSURLAuthenticationMethodServerTrust` (`URLSession:didReceiveChallenge:`, ou o método da tarefa se o da sessão não existir). Não há objeto `SecTrust`: `serverTrust` devolve nil. Resposta 401 com `WWW-Authenticate: Basic` chama `NSURLAuthenticationMethodHTTPBasic` (o método da tarefa primeiro). `UseCredential` repete o pedido uma vez com `Authorization`. `Cancel`/`Reject` entregam `NSURLErrorDomain` -999 e não entregam o corpo. `URLSession:dataTask:didReceiveResponse:completionHandler:` roda na entrega: 0 cancela o corpo, 1 entrega, 2 trata como download. O bloco é de pilha e vale durante a chamada do delegate. Não é um `NSURLSession` completo.

## CoreML — só o dicionário da voz

`MLDictionaryFeatureProvider` (`coreml.c`) aceita `initWithDictionary:error:` com chaves string, `featureValueForName:`, `featureNames`, `dictionary`, `objectForKeyedSubscript:` e `count`. `MLModel` / `predictionFromFeatures:` continuam o stub vazio de `vscode_fwclasses.c`. A libc++ do onnxruntime e do sherpa-onnx não foi mexida.

## UserNotifications — autorização local, sem push

`UNUserNotificationCenter.currentNotificationCenter`, `requestAuthorizationWithOptions:completionHandler:` e `authorizationStatus` (0 não determinado, 1 negado, 2 autorizado). O pedido grava pending no TCC (`userNotifications`). O bloco só é chamado em `cx_un_decide`. Não há servidor APNs nem `UNNotificationSettings`. Pedido, gatilho, delegate e banner estão em `docs/interface-i19-i23.md`.

## StoreKit, QuickLookUI e Core Data — Slack, Claude Desktop e Cursor

O Electron desses três apps liga `QuickLookUI.framework` e `StoreKit.framework`. O Mantle (e, no Claude, o `SimulatorFrameworkBridge-iOS`) liga `CoreData.framework` sem importar símbolo nenhum. O loader recusava a biblioteca quando o arquivo não estava na imagem. Os três prefixos estão em `libsystem_dylib_ok` e na lista `sys_ok` do `machocheck`. Se o Mach-O do framework existir no disco, ele é carregado; senão o símbolo cai no libSystem.

**StoreKit — stub, sem compra.** `canMakePayments` é NÃO. `SKProductsRequest start` devolve zero produtos, a lista pedida como inválida, e chama `productsRequest:didReceiveResponse:` e `requestDidFinish:` na hora, na mesma thread. `addPayment:` entrega uma transação no estado 2 (falha) e volta. `restoreCompletedTransactions` chama `paymentQueue:restoreCompletedTransactionsFailedWithError:`. Não há estado comprado (1) nem restaurado (3). `appStoreReceiptURL` devolve nil e não cria arquivo. `cx_storekit_verify_receipt` devolve -1 e o texto "recibo não verificado: o mOS não valida recibo da App Store nem fala com a Apple". Não há rede com a Apple, não há recibo assinado e não há contorno de FairPlay. A mesma política está em `userland/frameworks/StoreKit/StoreKit.m` para quando a dylib está na imagem.

**QuickLookUI — stub, sem janela.** `sharedPreviewPanel` devolve um painel. `makeKeyAndOrderFront:` e `orderFront:` só marcam visível e retornam; não chamam `NSWindow` e não entram em loop. `reloadData` pergunta a contagem uma vez e, no máximo, o item 0. `enterFullScreenMode:withOptions:` devolve NÃO. A prévia não é desenhada.

**Core Data — SQLite no libSystem, memória na dylib de disco.** Slack e Claude não importam classe de Core Data. O libSystem (`coredata_fw.c` + `coredata_store.inc` + o amalgama em `sqlite3_mos.c`) grava um arquivo SQLite (`mos_object`, `mos_attr`) no caminho pedido, ou em `~/Library/Application Support/<nome>/store.moscd`. Um arquivo antigo `MOSCD1` é migrado na primeira abertura. `NSInMemoryStoreType` não toca disco. `sqlite3_open_v2` abre o banco. Predicado usa o `NSPredicate` já existente. A dylib `CoreData.framework` do disco, a do Postico, continua só em memória.

`tools/cdtest.c` exercita a loja no host (inserir, gravar, reabrir, apagar, limite, migração de MOSCD1 e o arquivo aberto pelo `sqlite3` do host). `tools/sktest.c` confere a política e o texto do recibo. Isso não é o `build.ps1`, não é a VM e não é hardware.

## Foundation / formatação — 204 símbolos do WhatsApp, 17 com miolo

O `machocheck` do WhatsApp 2.26.45.13 (x86_64) não acha o módulo `FoundationInternationalization` (`$s29…`). A conta antiga de ~278 era uma estimativa. A lista medida, gravada em `tools` a partir desse binário, tem **204 símbolos distintos** de `libswiftFoundation` / `Foundation` em FormatStyle, ISO8601, Measurement, NSDecimal, AttributedString, PersonNameComponents e comparação com locale. Contra um libSystem de 1 símbolo, os 204 apareciam no relatório de ausentes. Contra `fi_swift.c` + `fi_exports.c`, o mesmo `machocheck` no executável principal lista **0** desses 204.

`userland/libsystem/fmt_core.inc` formata de verdade, com saídas fixas no `tools/fmttest.c`:

- inteiro e decimal com agrupamento: `en` vírgula e ponto, `pt`/`de` ponto e vírgula, `fr` espaço e vírgula
- data gregoriana numeric, abbreviated, long e complete, e hora curta/padrão, para en, pt, de e fr
- lista ("A, B, and C" / "A, B e C" / "A, B und C" / "A, B et C")
- conversão de comprimento (m, km, cm, mm, ft, in, mi) e massa (kg, g, lb, oz)
- soma decimal em base 10 (`0.1 + 0.2 = 0.3`)
- Markdown mínimo: `**negrito**`, `*itálico*`, `` `código` ``, `[texto](url)`

A ponte swiftcc x86_64 está em `fi_abi.inc` e `fi_swift.c`. **Funcional** (ligado a esse núcleo), nos nomes mangled que o WhatsApp importa:

- `Date.formatted()` e `Date.formatted(date:time:)` — data e hora gregorianas, estilos numérico / abreviado / longo
- `Date.ISO8601FormatStyle.format` e `parse` — `YYYY-MM-DDTHH:MM:SSZ`; outro texto vira 0
- `NSDecimal` a partir de Int e Double, `+`, `-`, `*`, `/`, `==`, `<` — a conta é em base 10 (`0.1 + 0.2 = 0.3`, `1/4 = 0.25`)
- `Measurement.value`, `init(value:unit:)` e `converted(to:)` — comprimento e massa das unidades acima; outra unidade devolve o valor original

**Stub:** os outros 187 nomes existem para o dyld resolver. Cada um devolve string Swift vazia (`rax = 0`, `rdx = 0xE000…`) ou nil em `rax`. Isso inclui descritores de metadata (`Ma`, `Mn`, `Mc`), `PersonNameComponents.formatted()`, markdown de `AttributedString`, `ListFormatStyle` e `IntegerFormatStyle`. Chamar um accessor de metadata e usar o ponteiro nulo continua faltando: não há o runtime Swift desses tipos. No arm64 os 204 nomes existem e só dão `ret`.

O toolchain Swift 6.0.3 (Linux) confirmou o layout da small string. O `swiftc` desse toolchain não emite Mach-O de macOS sem o SDK da Apple, então a convenção foi escrita em C e assembly.

Limitações que continuam: plurais do ICU, calendário que não seja o gregoriano, e locale fora de en/pt/de/fr (cai em inglês, com agrupamento de pt/de para o número quando o idioma não é en nem fr). A dylib `libicucore.A.dylib` continua sendo a do `tools/build-icu.ps1`; este núcleo não a exige. O overlay Swift (`FormatStyles.swift`) não foi compilado.

## Fora desta frente

Não há contrato novo com Metal nem com mídia. Os arquivos `metal.c`, AIR, AVFoundation, AudioToolbox, CoreAudio, VideoToolbox e decodificadores não foram alterados.

## Testes de host

Rodados com gcc 13 (`-std=c11 -Wall -Wextra -O1`) neste ambiente. Não há zig nem QEMU aqui: o `build.ps1` do libSystem e a VM não rodaram. O `swiftc` 6.0.3 do toolchain Linux está no ambiente e serviu para ler o layout da small string; ele não gera o Mach-O de macOS sem o SDK da Apple, então o overlay `FormatStyles.swift` não foi compilado.

- `tools/prefstest.c`: 72/72
- `tools/cfnotifytest.c`: 15/15
- `tools/tcctest.c`: 18/18
- `tools/smlogintest.c`: 16/16
- `tools/fmttest.c`: 94/94
- `tools/fitest.c`: 22/22, no núcleo e em dois thunks swiftcc (`Date.formatted(date:time:)` e `NSDecimal.init(Int)`), com `-mno-red-zone`

O `machocheck` (`tools/machocheck.c`) rodou no WhatsApp 2.26.45.13. Antes, com libSystem de 1 símbolo, o relatório do bundle tinha 9989 linhas e os 204 nomes desta lista. Depois, com os objetos novos, o executável principal não lista nenhum dos 204. `libswiftFoundation.dylib` nesse executável: 1057 imports, 884 ainda sem implementação (os 204 desta lista saíram dessa conta).

## Sobreposição com outras frentes

Consulta em 2026-11-10, PRs abertos:

- AppKit (tabelas, teclado, barra de menus) não toca estes arquivos.
- Instalação/runtime edita `vscode_frameworks.c` em `LSSharedFileList*` (itens de login em `Library/Preferences/com.mos.loginitems`, sem sufixo `.plist`). Esta frente edita o mesmo arquivo só nas funções AX. O launchd dessa branch continua varrendo `LaunchAgents` e `LaunchDaemons`, que é onde o SMAppService publica o plist.
- JIT não toca libSystem desta frente.
