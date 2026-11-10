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

## XPC — dicionário com porta, pipes ainda sem o launchd

O fio `xpc.c` + launchd entre processos do mOS já existe (não é a libxpc da Apple). Não foi reescrito.

- **Funcional neste fio:** `xpc_dictionary_set_mach_send` grava o nome da porta num valor `_xpc_type_mach_send`. `xpc_dictionary_copy_mach_send` lê esse nome. A cópia do dicionário e o `xpc_equal` preservam o valor. Na serialização própria do mOS o tipo é 13 (u32). O nome é copiado, não movido: ninguém dá `deallocate` nem cria um direito novo. O wrapper de `mach_port_*` ignora a tarefa, então o número continua válido no outro processo enquanto a porta existir.
- **Limitação:** `xpc_pipe_create` devolve NULL. `xpc_pipe_create_from_port` devolve NULL. `xpc_pipe_routine`, `xpc_pipe_routine_reply` e `xpc_pipe_receive` devolvem EINVAL. Não há o protocolo privado do launchd.
- **Limitação:** `xpc_set_event_stream_handler` aceita os argumentos e não chama o bloco. Não há stream de eventos do launchd.
- `NSXPCConnection` continua um stub de processo (resume só marca a conexão). Helpers de computer use que esperam um daemon XPC da Apple não sobem sozinhos.

## Keychain — funcional, sem criptografia

`SecItemAdd` / `CopyMatching` / `Update` / `Delete` para senha genérica e de internet já estavam implementados e testados em `userland/tests-security`. O arquivo não é cifrado. Esta frente não mudou esse código.

## URLSession — parcial, como já estava

Há callbacks de dados e de download em `foundation_more.c`. Desafio de autenticação e `didReceiveResponse` não são tratados (a conexão segue). Não é um `NSURLSession` completo.

## UserNotifications — autorização local, sem push

`UNUserNotificationCenter.currentNotificationCenter`, `requestAuthorizationWithOptions:completionHandler:` e `authorizationStatus` (0 não determinado, 1 negado, 2 autorizado). O pedido grava pending no TCC (`userNotifications`). O bloco só é chamado em `cx_un_decide`. Não há servidor APNs nem `UNNotificationSettings`. Pedido, gatilho, delegate e banner estão em `docs/interface-i19-i23.md`.

## StoreKit — stub local rotulado

`SKPaymentQueue.canMakePayments` é NÃO. `addPayment:` não conclui compra. `transactions` é um array vazio. `cx_storekit_verify_receipt` devolve -1 e o texto "recibo não verificado: o mOS não valida recibo da App Store nem fala com a Apple". Não há rede com a Apple, não há recibo assinado e não há contorno de FairPlay. Apps da App Store continuam sujeitos a DRM que esta frente não quebra.

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
