# Serviços do sistema: launchd, XPC, notifyd, logd, configd

Como o mOS sobe e liga os serviços no estilo do macOS. Tudo roda sobre as portas Mach do kernel
(`kernel/proc/machipc.c`) e o registro de nomes do bootstrap.

## launchd (`userland/init.c`)

O init (pid do primeiro processo de usuário) faz o papel do launchd. Lê os jobs de:

| Pasta | Formato |
|---|---|
| `/Sistema/Biblioteca/LaunchDaemons` | `.servico` do mOS (`Programa=`, `Argumentos=`, `ManterVivo=sim`, `AtrasoMs=`, `Entrada=`) e `.plist` |
| `/System/Library/LaunchDaemons`, `/Library/LaunchDaemons`, `/Library/LaunchAgents` | `.plist` do launchd |
| `/Users/<nome>/Library/LaunchAgents` | `.plist` do launchd |

Chaves de plist entendidas: `Label`, `Program`, `ProgramArguments`, `BundleProgram` (relativo ao `.app` que contém o
plist, para o `SMAppService`), `MachServices`, `RunAtLoad`, `KeepAlive` (bool; um dicionário de condições conta como
ligado), `EnvironmentVariables`, `WorkingDirectory`, `StandardInPath`, `StandardOutPath`, `StandardErrorPath`, `Disabled`.
Só plist XML (o leitor nativo é `userland/plistmini.h`); um `bplist` é recusado com aviso no log.

Entrada padrão: como no launchd do macOS, um job de plist (e um serviço sob demanda do `LC_SUBMIT`) lê de `/dev/null`
quando não há `StandardInPath`. Um `.servico` continua com o terminal do console, a menos que tenha `Entrada=<arquivo>`.
Sem fim de arquivo, quem lê o stdin inteiro antes de começar (o `claude -p` do Claude Code) ficava parado para sempre.

Ambiente dos jobs: `HOME`, `USER`, `PATH`, `LANG`… (o mesmo padrão dos apps), as `EnvironmentVariables` e
`XPC_SERVICE_NAME=<Label>`.

### Ativação sob demanda

Cada nome de `MachServices` é reservado no bootstrap com `MACH_BS_RESERVE` (syscall `SYS_MACH_BOOTSTRAP`): o kernel
cria a porta do serviço e dá o direito de **receber** ao launchd, que a põe num conjunto de portas e espera.

1. Um cliente faz `bootstrap_look_up` (envio para a porta) e manda a primeira mensagem; ela fica na fila.
2. O launchd vê mensagem numa porta de um job parado e inicia o job.
3. O daemon faz `bootstrap_check_in` do nome: o kernel tira o direito de receber do launchd e o entrega ao daemon,
   **com a fila** (a mensagem do passo 1 está lá).
4. Quando o daemon termina, `machipc_exit` devolve o direito ao launchd (mesmo nome, mesmo conjunto) em vez de matar
   a porta. Os envios dos clientes continuam válidos; a próxima mensagem inicia o daemon de novo.

`KeepAlive` reinicia o job 1 s depois de sair (desiste após 5 saídas em menos de 10 s). Um job sob demanda que morre
logo ao subir espera um pouco mais a cada vez (desiste após 10).

### Controle: `launchctl` (`/usr/bin/launchctl`)

O launchd atende o serviço Mach `com.apple.launchd.mos` (protocolo `LC_*` em `userland/svcproto.h`):
`list`, `load`/`unload` (e `bootstrap`/`bootout`), `start`, `stop`, `remove`, `print`, `kickstart [-k]`. O
`SMAppService` (agentes e daemons do bundle: `Contents/Library/LaunchAgents|LaunchDaemons/<nome>.plist`) usa o mesmo
`LC_LOAD`/`LC_UNLOAD`; o registro vale até o desligamento (não é gravado para o próximo boot).

## XPC (`userland/libsystem/xpc.c`)

- `xpc_connection_create_mach_service(nome, …, 0)`: `bootstrap_look_up` + aperto de mão (`HELLO` com a porta do
  cliente, `WELCOME` com a porta do par no servidor). O prazo é de 30 s, para cobrir o launchd iniciar o daemon.
- Ouvinte (`XPC_CONNECTION_MACH_SERVICE_LISTENER`) e `xpc_main`: `bootstrap_check_in`; cada cliente vira uma conexão
  "par" com thread de recepção própria. O nome do `xpc_main` vem de `XPC_SERVICE_NAME` (ou do `CFBundleIdentifier`).
- **XPC services em bundles**: `xpc_connection_create(nome)` sem o nome no bootstrap procura em
  `<app>/Contents/XPCServices/*.xpc` o bundle com `CFBundleIdentifier` = nome e pede ao launchd (`LC_SUBMIT`) um job
  sob demanda com o executável dele; depois o `look_up` acha o nome.
- Morte do outro lado: as duas pontas pedem aviso de nome morto. O cliente de um serviço recebe
  `XPC_ERROR_CONNECTION_INTERRUPTED` (as respostas pendentes também) e o próximo envio reconecta — o launchd reinicia
  o daemon. Do lado do servidor, o par de um cliente que morreu recebe `XPC_ERROR_CONNECTION_INVALID`.
- O fio é próprio do mOS (os dois lados são esta libxpc). Um dicionário com `xpc_dictionary_set_mach_send`
  sai com descritor `COPY_SEND`: quem recebe ganha um direito de envio no espaço dele. No mesmo processo,
  `copy_mach_send` devolve o mesmo nome, sem criar outro direito. Não há direito de arquivo (`fd`) no dicionário.
- `xpc_pipe_create` faz `bootstrap_look_up`. `xpc_pipe_routine` manda o dicionário e espera a resposta num send-once.
  O servidor faz `xpc_pipe_receive` e `xpc_pipe_routine_reply`. Não é o protocolo privado do launchd da Apple.
- `xpc_set_event_stream_handler` guarda o bloco e o chama. `xpc_event_stream_post` entrega neste processo e, se outro
  processo fez check-in de `com.mos.xpc.ev.<stream>`, manda por Mach. Não há o daemon de eventos do launchd.
- `NSXPCConnection` / `NSXPCListener` falam por estas conexões. O proxy `MOSXPCProxy` encaminha seletor com até 4
  argumentos objeto; o bloco de resposta, se houver, é o último e tem de ser chamado na hora. Não filtra protocolo
  e não é o fio binário da Apple. `anonymousListener` não publica endpoint.
- Bytes entre pids, para a Interface ligar em `axtransport.h`: `userland/libsystem/xpctransport.h`
  (`cx_xpc_peer_ready`, `cx_xpc_peer_call`, `cx_xpc_peer_send`, `cx_xpc_peer_listen`). Serviço
  `com.mos.xpc.peer.<pid>`. Não usa AF_UNIX nem `sockets.c`.

## Daemons

Programas nativos (`userland/*.c` → `/Sistema/Programas`), iniciados sob demanda pelos plists de
`root/mOS/Sistema/Biblioteca/LaunchDaemons`:

| Daemon | Nome Mach | O quê |
|---|---|---|
| `notifyd` | `com.apple.system.notification_center` | estado e geração por nome; avisos por porta (msgh_id = token) e por sinal |
| `logd` | `com.apple.system.logger` | grava os eventos de `os_log`, `syslog` e `asl` no store |
| `configd` | `com.apple.SystemConfiguration.configd` | `SCDynamicStore`: chaves livres + rede e proxies reais |

### notify(3) (`userland/libsystem/svcclient.c`)

`notify_post`, `register_check` (a geração; a 1ª `notify_check` diz "mudou", como no macOS), `register_mach_port`,
`register_dispatch`, `register_file_descriptor` (o token em ordem de rede no pipe), `register_signal`,
`get_state`/`set_state`, `cancel`. Os registros de dispatch e fd usam uma porta por processo e uma thread que recebe
os avisos.

### Log unificado (`userland/libsystem/oslog.c`, `userland/logd.c`, `/usr/bin/log`)

- `os_log_create`, `os_log_type_enabled` (debug desligado, como sem perfil de debug), `_os_log_impl` e variantes:
  o buffer do compilador (`__builtin_os_log_format`) é decodificado e a mensagem refeita com o formato; as anotações
  `%{public}`, `%{private}`, `%{bool}`, `%{errno}` são entendidas, e o mOS **não** esconde valores privados.
  `OS_ACTIVITY_MODE=disable` desliga; `OS_ACTIVITY_DT_MODE` ecoa no stderr.
- `syslog`/`vsyslog`/`openlog`(`LOG_PERROR`)/`setlogmask` e `asl_*` vão ao mesmo logd; esses também saem em
  `/private/var/log/system.log` no formato BSD.
- Store: `/private/var/db/diagnostics/mos-log.txt`, uma linha por evento com campos separados por tabulação
  (`ms-unix pid nível processo subsistema categoria mensagem`), girando em 2 MiB para `mos-log.0.txt`.
- `log show [--last 5m] [--info] [--debug] [--process P] [--predicate 'subsystem == "x" && eventMessage CONTAINS "y"']
  [--style default|compact|syslog|json|ndjson]`, `log stream`, `log erase`.

### SystemConfiguration (`userland/libsystem/sysconfig.c`, `userland/configd.c`)

O configd lê a rede do kernel a cada 3 s (`MOS_NET_INFO`) e publica `State:/Network/Global/IPv4` (`PrimaryInterface`
en0, `PrimaryService`, `Router`), `State:/Network/Global/DNS`, `State:/Network/Interface`,
`State:/Network/Interface/en0/{IPv4,Link}`, `State:/Network/Service/<id>/{IPv4,DNS}`, `Setup:/System`
(`ComputerName`) e `Setup:/Network/HostNames`. Proxies (`State:/Network/Global/Proxies`): o dicionário `Proxies` de
`/Library/Preferences/SystemConfiguration/preferences.plist`, senão o padrão do macOS sem proxy. Mudanças avisam quem
chamou `SCDynamicStoreSetNotificationKeys` (chaves exatas ou regex) e postam
`com.apple.system.config.network_change` no notifyd.

No libSystem: `SCDynamicStoreCreate*`, `CopyValue`, `CopyMultiple`, `CopyKeyList`, `SetValue`, `AddValue`,
`RemoveValue`, `NotifyValue`, `SetMultiple`, `CopyProxies`, `CopyComputerName`, `CopyLocalHostName`,
`SetNotificationKeys`, `CreateRunLoopSource`, `SetDispatchQueue`, `CopyNotifiedKeys`, `SCDynamicStoreKeyCreate*`,
`SCError`/`SCErrorString`, `CFNetworkCopySystemProxySettings`.

## Protocolo (`userland/svcproto.h`)

Mensagens Mach comuns: header (24 bytes, `msgh_id` = `SVC_MSG_ID`) | `svc_body_t` (op, a, b, c, q, n1, n2) | dois blocos
de bytes. Um direito que vai junto (registro de porta no notifyd/configd) segue num descritor de porta. Pedido com
resposta leva a porta de resposta no `local` (send-once). `svcnative.h` tem os mesmos auxiliares para os programas
nativos; `cx_svc_call` (libSystem) guarda o envio de cada serviço em cache.

## Teste de ponta a ponta

```
.\build.ps1 -OutDir build-svc
.\tools\build-launchdtest.ps1 -OutDir build-svc
.\run.ps1 -Headless -Image build-svc/mOS.img -Serial build-svc/serial.log
```

`LaunchdTeste.app` (`userland/tests-launchd/cliente.c`) sobe no boot e imprime `launchdteste: N/M PASS`: XPC com um
daemon sob demanda (sync e assíncrono), INTERRUPTED e reinício pelo launchd, XPC service do bundle, `notify_post`
acordando o servidor (que responde com outro post), estado do notify, `os_log` no store e no `log show`, configd
(rede e proxies) e `launchctl list`.
