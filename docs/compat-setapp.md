# Setapp 4.0.0 (165): helpers, launchd e ServiceManagement (10 nov. 2026)

O pacote é o zip oficial que o cask `setapp` do Homebrew aponta, no CDN da MacPaw (o botão de
[setapp.com/download](https://setapp.com/download) monta o link por JavaScript):

```
https://dl.devmate.com/com.setapp.DesktopClient/165/1794245125/Setapp-165.zip
a124004ba0c6555e8bb05904a65e78b7d32a8eaa069b304f8e9a78e4e859735a  Setapp-165.zip
```

O `sha256sum` bate com o cask. Nenhum binário do Setapp foi executado nem entra no repositório. Não houve login:
a conta é do usuário, e o login do Setapp é no navegador, com código por e-mail.

## O que o bundle é

| | |
|---|---|
| Bundle | `com.setapp.DesktopClient`, 4.0.0 (165), `LSMinimumSystemVersion` 13.0 |
| Arquivos Mach-O | 338 regulares, todos universais x86_64 + arm64 |
| FairPlay | nenhum `LC_ENCRYPTION_INFO(_64)`: não é app da Mac App Store. Nada a contornar |
| Motor | AppKit com muito Swift e algum ObjC. Não é Electron. Sem `libswift*` embutida: usa as do sistema |
| Terceiros | Sparkle 2.6.4 (no app e de novo no `SetappUpdater`), Sentry, Alamofire, ReactiveSwift, GRDB, Kingfisher, Lottie, swift-nio |

Helpers, todos em `Contents/Library/LaunchServices/` e com `LSUIElement`:

| Helper | Rótulo do launchd | `KeepAlive` | Outras chaves |
|---|---|---|---|
| `Setapp.app` (`SetappAgent`) | `com.setapp.DesktopClient.SetappAgent` | `{Crashed: true, SuccessfulExit: false}` | 12 `MachServices` (`com.setapp.AppsManagementService` … `com.setapp.shared-service`), `ThrottleInterval` 1, `ProcessType` Interactive |
| `SetappLauncher.app` | `com.setapp.DesktopClient.SetappLauncher` | o mesmo | `MachServices` `com.setapp.LauncherTransitionService`, `RunAtLoad`, `ThrottleInterval` 1 |
| `SetappAssistant.app` | `com.setapp.DesktopClient.SetappAssistant` | — | só `ProgramArguments` |
| `SetappUpdater.app` | `com.setapp.DesktopClient.SetappUpdater` | — | `StartInterval` 21600 (6 h) |

Os quatro modelos de plist ficam em `Contents/Resources/` de cada helper, em **bplist00**, com
`ProgramArguments = [path_to_agent]`.

## Como o Setapp sobe os helpers

Não usa `SMAppService` nem `SMLoginItemSetEnabled` (nenhum binário importa esses nomes). O
`SetappInterface.framework` (`AFXAgent`, `AFXLaunchctlTaskManager`) faz o caminho clássico:

1. copia o helper para `~/Library/Application Support/Setapp/LaunchAgents/`;
2. lê o modelo com `propertyListWithData:options:format:error:`, troca `path_to_agent` e grava com
   `dataWithPropertyList:format:options:error:` + `writeToFile:options:error:` em `~/Library/LaunchAgents`;
3. roda `/bin/launchctl load|unload` por `NSTask`, e `launchctl list` / `launchctl print` (com timeout) para saber
   se o job está carregado.

Os helpers falam entre si por `NSXPCConnection initWithMachServiceName:` e `NSXPCListener` (IPCCore). O Sparkle
usa `SMJobSubmit` / `SMJobRemove` com `kSMDomainUserLaunchd` para o instalador.

## O que faltava no mOS e o que este PR faz

| Peça | Antes | Agora |
|---|---|---|
| plist binário no launchd | `load_plist` recusava `bplist` ("plist binário não suportado") | `plistmini.h` lê bplist00 (dict, array, string ASCII e UTF-16, inteiro, bool, real, data, data/hora), com limite de profundidade e de visitas e checagem de cada offset |
| `KeepAlive` em dicionário | qualquer dicionário contava como "sempre" | `SuccessfulExit` true/false e `Crashed` valem como no `launchd.plist(5)`: o agente que sai com 0 não volta; o que morre por sinal ou sai com erro volta. `SuccessfulExit` implica `RunAtLoad`; `Crashed` sozinho não inicia o job |
| `ThrottleInterval` | 1 s fixo depois do fim | conta do início do processo, como no launchd. Sem a chave, continua 1 s |
| `StartInterval` | ignorado | dispara a cada N s, sem rodar no load; se o job ainda roda, o disparo é pulado |
| `MachServices` | até 8 nomes (o agente perdia 4) | até 16 |
| `launchctl list RÓTULO` | ignorava o rótulo e listava tudo com saída 0 (o Setapp acharia qualquer job carregado) | devolve o dicionário do job, ou "Could not find service" com 113 |
| `launchctl load` de plist já lido pela varredura de 5 s | erro 17 | saída 0, como no macOS |
| `SMJobSubmit` / `SMJobRemove` | stub: falhava / dizia ok sem fazer nada | grava o job em `~/Library/Application Support/com.mos.smjob/` (fora do que o launchd varre: não volta no boot, como no macOS) e pede `LC_LOAD` / `LC_REMOVE`. Erro em `kSMErrorDomainLaunchd` com `EEXIST` / `ESRCH` |
| `SMJobCopyDictionary`, `SMCopyAllJobDictionaries` | não existia / array vazio | leem `launchctl list` do launchd |
| `SMLoginItemSetEnabled` | devolvia 1 sem fazer nada | acha o helper em `Contents/Library/LoginItems` pelo `CFBundleIdentifier` e o põe na lista de login items que o launchd lê |

`SMJobBless` continua falhando (`chrome.c`): instalar ferramenta privilegiada pede o fluxo do Authorization, e o
Setapp não o usa. O `ServiceManagement.framework` Mach-O de `userland/frameworks/` não mudou; o carregador
resolve `ServiceManagement` no libSystem (`libsystem_dylib_ok` em `kernel/compat/macho.c`).

## Testes de host

```
gcc -std=c11 -O1 -Wall -Wextra -Iuserland -Ikernel/include tools/launchjobtest.c -o /tmp/launchjobtest
/tmp/launchjobtest [Setapp.app]
gcc -std=c11 -O1 -Wall -Wextra -Wno-unused-function -Ikernel/include tools/smjobtest.c -o /tmp/smjobtest
/tmp/smjobtest
```

`launchjobtest` usa o mesmo `plistmini.h` e o mesmo `launchjob.inc` que o `init.c`. Sem argumento, roda as
regras e as fixtures (um bplist gerado com `plistlib`, equivalente ao do agente, com string UTF-16; bplist truncado,
com trailer inválido e com ciclo). Com o `Setapp.app` extraído do zip acima, lê os quatro plists binários oficiais e
confere a política de cada helper. `smjobtest` faz a ida e volta entre o que o launchd escreve em
`launchctl list RÓTULO` e o que `SMJobCopyDictionary` / `SMCopyAllJobDictionaries` leem.

## Símbolos (machocheck)

`tools/machocheck.c` no `libSystem.mos` da `main` de 10 nov. (`d9e1fe5`), com `--disco` na libunwind e em
`esp/mOS/usr/lib` (libc++, libz e as `libswift*` que o `build.sh` põe lá). O zip foi extraído com os symlinks, então
os frameworks de `Versions/Current` contam três vezes nos imports; a conta de símbolos distintos não depende disso.

```
Imagens Mach-O x86_64 no bundle: 992
Total: 261783 imports, 7378 sem implementacao (802 simbolos distintos)
Ausentes por origem (simbolos distintos): sistema 566, libc++ 0, Swift 236, unwind 0
```

Este PR não muda essa conta: os símbolos de ServiceManagement e XPC que o Setapp importa já resolviam, mas
sem corpo. Dos 566 de sistema, 418 são nomes Swift dos overlays (`Combine` 189, `SwiftUI` 103, `Foundation` 92,
`ExtensionKit` 14, `CryptoKit` 13, `ExtensionFoundation` 7). Os outros 148, C/ObjC, são CoreServices (SearchKit, `MDItem`, `kUTType*`), libxml2 (18), libSystem (ASL, `utmpx`, `regncomp`, `pthread_cancel`), CoreGraphics
(gradientes), CoreData (migração), ImageIO, WebKit legado e FinderSync. Os mais altos por imagem: `Setapp` 139,
`SetappHTTPSServer` 133, `SetappAgent` 103, `SetappLauncher` 19, `SetappAssistant` 12.

## O que só se prova numa VM

- `launchctl load` do plist real que o Setapp grava, o agente subindo e o `KeepAlive` reiniciando depois de um crash.
- Os 12 nomes Mach do agente chegando por `NSXPCConnection initWithMachServiceName:`.
- `SetappUpdater` disparando pelo `StartInterval`.
- Login, catálogo e instalação de app: exigem conta e os símbolos Swift acima.
