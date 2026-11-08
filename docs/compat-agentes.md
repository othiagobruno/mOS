# Antigravity, Codex desktop, Grok Bot e Cursor: o que faltava (7 nov. 2026)

Continuação de [compat-apps.md](compat-apps.md) para os apps de agentes. Medido com o `tools/machocheck.c` e a
`libc++.1.dylib` do LLVM (`build\libcxx`) na pasta de varredura, como o kernel a carrega do disco.

## Builds

| App | Versão | Origem (Intel) | Motor |
|---|---|---|---|
| Antigravity | 2.29.0 (5723181030440960) | `storage.googleapis.com/antigravity-public/antigravity-hub/2.29.0-5723181030440960/darwin-x64/Antigravity.dmg` (o cask só lista `darwin-arm`; trocar o sufixo dá o Intel) | Electron 44.5.1 (Chrome 152) + `Resources/bin/language_server` (Go, 169 MiB) |
| Codex desktop | 26.623.141536 | `Codex-darwin-x64-26.623.141536.zip` | runtime **OWL** da OpenAI: Chromium 149 com layout do Chrome (`Codex Framework.framework/Versions/149.0.7827.197`, helpers dentro do framework) e um `app.asar` no estilo do Electron (`Resources/owl-electron-app.json`) |
| Grok Bot | 0.79.2 | (compat-apps.md) | Electron 42 |
| Cursor | 3.29.9 | (compat-apps.md) | Electron 43 |

O DMG do Antigravity abre com o leitor do próprio mOS: `build\dmgtest.exe antigravity.dmg <pasta>` (351 pastas,
632 arquivos, 0 erros).

## Ausentes, antes e depois

| Binário | Antes | Agora |
|---|---|---|
| Antigravity: Electron Framework | 1 | 0 (`CTFontDescriptorCreateWithNameAndSize`) |
| Antigravity: `language_server` (Go) | 260 (147 de libc++, que a libc++ do LLVM cobre) | **0** |
| Codex desktop: `Codex Framework` + helpers + GL/Vulkan | 1.374 (6 nov.) → 7 | **0** |
| Grok Bot: módulos `.node` e `sand-webauthn-signer` | 393 | só os `napi_*` (vêm do Electron) e o ImageIO, que é o `ImageIO.framework` Mach-O de `userland/frameworks` |

| Codex desktop: `Resources/codex` (o agente, Rust) e `rg` | 1 (`CFURLCreateWithBytes`) | **0** |
| Codex desktop: `native/*.node` e `app.asar.unpacked` (com as dylibs Swift de `build\swift\objc\dylib`) | ~980 | só `napi_*`, Sparkle (vem no bundle) e partes Swift/ScreenCaptureKit dos módulos opcionais |
| Grok Bot: helper "Computer Use" (`CUGrokBotService`, Swift) com as dylibs Swift do mOS | 7936 | **0** (eram 79; ver "Computer Use do Grok Bot") |

Sobra, de propósito, o `cable_ble_scanner` do Antigravity (Swift),
um recurso opcional que espera o runtime Swift ([swift-objc-runtime.md](swift-objc-runtime.md)).

## O que entrou

- `userland/libsystem/compat_agentes.c` (novo): `stat64`/`lstat64`/`fstat64`/`fstatat64` (no x86_64 a `struct stat`
  já é a de 64 bits), `mlockall`/`munlockall`, `ptrace` (só `PT_DENY_ATTACH`), `memccpy`, `nice`, `asctime_r`/`ctime_r`,
  `__floattidf`/`__floatuntidf` (int128 → double com um só arredondamento), `CFTimeZoneCopyDefault`,
  `CTFontDescriptorCreateWithNameAndSize`, as variantes de FSEvents por dispositivo e as consultas do fluxo,
  `kDADiskDescriptionMatchVolumeMountable`/`WatchVolumePath`, `IOHIDManagerSetDeviceMatching`/
  `RegisterInputReportCallback` e `SecTrustSettingsSet/RemoveTrustSettings` (`errSecUnimplemented`).
- `cli.c`: `proc_listpids` (todos, por grupo, usuário ou pai), `proc_listallpids`, `proc_name`, e o `proc_pidinfo` de
  verdade para `PROC_PIDTBSDINFO`, `PROC_PIDTASKINFO`, `PROC_PIDTASKALLINFO` e `PROC_PIDT_SHORTBSDINFO` (nome, pai,
  estado, memória residente e CPU da lista de processos do kernel). Antes devolvia sempre ENOTSUP. O
  `cursor_proclist.node` (Cursor e Grok Bot) lista os processos por aqui.
- `mach.c`: `host_page_size`.
- `sockets_inet.inc`: `res_9_nsearch`/`res_9_search` montam a mensagem DNS de resposta (A e AAAA) a partir do
  resolvedor do kernel; outros tipos recebem NODATA. O `res_9_ninit` continua falhando, como antes, para o Chromium
  seguir no `getaddrinfo`.
- `vscode_fwclasses.c`: classes vazias `NSShadow`, `NSPopover`, `SFUniversalLink`, `LAAuthenticationView`,
  `ASWebAuthenticationSessionWebBrowserSessionManager` (Codex), `CNPostalAddressFormatter`, `MKLocalSearch*` (Grok Bot).
  `vscode_consts.c`: `kCGWindowAlpha`.
- `userland/frameworks/ImageIO`: `kCGImageSourceShouldCacheImmediately` e `kCGImageSourceCreateThumbnailWithTransform`.
- `kernel/compat/macho.c`: `QuickLookThumbnailing.framework` na lista do carregador (o Codex o liga sem importar
  símbolos; pede `QLThumbnailGenerator` pelo nome).
- `tools/stage-chromeapp.ps1` (novo): monta na imagem um app com o layout do Chrome que não é o Chrome (o Codex):
  nome do framework e dos helpers pelo bundle, `app.asar` e recursos do OWL; `-ComCLI` inclui o binário `codex`.

## Como repetir

```
.\build\dmgtest.exe build\apps\dl\antigravity.dmg build\apps\x\antigravity
.\build\machocheck.exe build\libSystem.mos <pasta com o .app e usr\lib\libc++.1.dylib> <relatorio.txt>
.\tools\vscode-teste.ps1 -Electron -App "build\apps\x\antigravity\Antigravity\Antigravity.app" -Nome Antigravity -OutDir build-ag -Accel whpx
.\build.ps1 -OutDir build-codex; .\tools\stage-chromeapp.ps1 -App build\apps\x\codex\Codex.app -OutDir build-codex -Autostart -Diagnostico
```

O harness do host (`machotest`) não roda o `language_server`: cai na primeira instrução de um inicializador estático
a 48 MiB do início do texto, um limite do harness, não do mOS. O teste dele é na VM.

## Cursor na VM (7 nov., tarde)

- **O Cursor 3.29.9 abre e mostra a tela de login** ("CURSOR — The best way to code with AI", Log In / Sign Up),
  com a interface "glass" completa (`workbench.glass.main.js`).
- Na primeira rodada a janela ficava branca: o `stage-electron.ps1` pula arquivos acima de `-MaxModuloMiB` (16), e o
  Cursor tem seis maiores que isso, entre eles os dois workbench (`workbench.glass.main.js` 45 MiB,
  `workbench.desktop.main.js` 37 MiB), o `resources/helpers/node` (120 MiB), o `file_service.darwin-x64.node` (39 MiB)
  e o `daemon.cjs` do agent host (20 MiB). Para o Cursor use `-MaxModuloMiB 128`.
- O staging dele é lento (14 mil arquivos, 640 MiB em `Resources/app`; ~45 min com a máquina sob pressão de memória).
  Para repetir sem refazer: copie o que faltar para `build-cursor\esp\...`, rode o `build-cursor\mkimg.exe` e o
  `vscode-teste.ps1 ... -SemBuild`.
- Falta: o Extension Host (`Cursor Helper (Plugin)`) sai com código 1 antes de ativar extensões, três vezes; sem ele
  não há o "Connect transport" (login, modelos, analytics). O tracing do Sentry no processo principal também falha
  com `Cannot find module './machine-id/getMachineId'`, embora o arquivo exista e o driver FAT o leia (`fatcat`);
  não é fatal.

### Extension Host do Cursor: as quatro causas (7 nov., noite)

Para ler a saída do Extension Host, que morria sem log, o `extensionHostProcess.js` da imagem de teste virou um
carregador que fazia `import()` do original e gravava o erro em `/Users/Visitante/exthost-diag.log` (com `-Persist`,
lido pelo `build\fatcat.exe`). As causas, em ordem:

1. **VFS parava em 16 níveis** (`kernel/fs/persist.c`): `.../node_modules/@sentry/node/node_modules/@opentelemetry/
   resources/build/src/detectors/platform/node/machine-id` está no nível 17, e os 42 arquivos mais fundos da imagem
   nunca entravam no VFS (o diretório existia vazio). O `import` do OpenTelemetry falhava e o host saía com código 1.
   Agora o limite é 64 (o boot tem 1 MiB de pilha) e o que passar disso aparece no log.
2. **zlib ausente**: o `file_service.darwin-x64.node` liga `/usr/lib/libz.1.dylib`; o carregador já a manda para
   `/usr/local/opt/zlib/lib`, mas a imagem base não tinha o arquivo. O `build.ps1` passa a copiar o bottle do Homebrew.
3. **dup de kqueue**: o tokio (`file-service/src/init.rs`) falhava em criar o runtime com EBADF, porque o mio clona o
   kqueue com `F_DUPFD_CLOEXEC` e o `dup()` mandava ao kernel um descritor que só o libSystem conhece. O `kqueue.c`
   ganhou `cx_kqueue_dup`, com contagem de referências.
4. **memória da VM**: com 2,7 GiB, o processo principal parou em `mach_make_memory_entry_64: (6)` e o host em
   `memcpy`/`kmalloc: sem memória`. Os ~950 MiB do Cursor ficam na RAM (livre cai de 1.720 para 499 MiB com seis
   processos, mesmo com o Electron Framework compartilhado). O Cursor precisa de `-Memory 3400` ou mais.

Com 1 a 3 corrigidos, o Extension Host ativa as extensões (`cursor-retrieval`, `cursor-checkout`, `vscode.git`...) e
o Cursor conversa com `api2.cursor.sh` (as respostas são `[unauthenticated]`, como deve ser antes do login).

### Cursor: estado final do dia

Com `-Memory 3400` e o bash do Homebrew na imagem (`tools\stage-bash.ps1 -OutDir build-cursor -Bottles <pasta só com
bash, readline, ncurses, gettext, coreutils, gmp e zlib>`, depois tirar o `sh.brew.bash.servico` e refazer o mkimg),
o Cursor abre na tela de login **sem nenhum aviso**: o ambiente do shell resolve (`SHELL=/usr/local/bin/bash`), o
Extension Host fica de pé os 7 minutos da rodada e as chamadas a `api2.cursor.sh` voltam `[unauthenticated]`.
Pendentes: o login (do usuário), o git (o bottle tem 619 MiB, grande demais para a RAM da VM como está) e o
`/usr/bin/security` que o ProxyResolver chama para ler certificados.

## Antigravity na VM (7 nov., noite)

**O Antigravity 2.29.0 abre a tela "Welcome to Antigravity"** (Continue with Google / Use Business Account) com o
`language_server` rodando (forks, porta própria, `~/Library/Logs/Antigravity/language_server.log`).

- O Antigravity 2 só abre a janela depois que o `language_server` responde. O `stage-electron.ps1` não copia
  `Resources/bin`; copie à mão para `build-ag\esp\mOS\Aplicativos\Antigravity.app\Contents\Resources\bin`.
- O `language_server` é **Go não-PIE** (sem `MH_PIE`, `dyld_info` com só 4 rebases) e o mOS desliza tudo para
  `IMAGE_BASE`. O carregador (`nonpie_slide` em `kernel/compat/macho.c`) agora desliza os ponteiros absolutos dos
  dados. A regra foi afinada contra os falsos positivos que apareceram, um por rodada:
  1. `0x100000000` como par de `int32 {0, 1}` (inicializador C++): nada abaixo da primeira seção é alvo;
  2. pares `{nameOff, 1}` em `__go_type` caindo no `__text`: lá, valor no código precisa ser início de função
     (`LC_FUNCTION_STARTS`) — "name offset out of range";
  3. tabelas de saltos do Go em `__rodata` (endereços no meio das funções) e o `maxpc` da moduledata: a regra 2 só
     vale em `__go_type`;
  4. `{PkgPath, Mcount=1}` do `uncommontype` que por acaso cai num início de função: em `__go_type` o valor também
     precisa ter a forma do `equal` (seguido do `gcdata`) ou do `Hasher` (depois de três ponteiros) — "invalid type
     descriptor";
  5. a pclntab (`__lrodata_gopcln`) é fluxo de bytes: fica de fora ("index out of range" no `funcfile`). O
     `__noptrdata` fica dentro: "sem ponteiros" é só para o coletor, e ele guarda a lista de `inittask`;
  6. funcvals isolados em `__go_type` (o `Hasher` de um `mapType` aponta para um `{fn}` no meio dos nomes): aceitos
     quando o endereço deles é alvo de outro ponteiro.
  Resultado: 955.502 ponteiros deslizados, 2.809 valores recusados, 197.699 funções.
- Memória: `-Memory 3400` (como o Cursor).

## Grok Bot na VM (7 nov., noite)

**O Grok Bot 0.79.2 abre a tela de login** ("Sua equipe de Bots sempre ativos…", "Fazer login") em pt-BR, com
`-Memory 3400` e `stage-electron.ps1 -MaxModuloMiB 200`. Faltavam:
- `-[NSString pathComponents]` (o mOS devolvia nil e a partida parava);
- `NSSearchPathForDirectoriesInDomains(NSApplicationDirectory)`: devolvia `~/Library`, o `isInApplicationsFolder()`
  do Electron dizia não e o app ficava no diálogo modal "Mover o Grok Bot para a pasta Aplicativos?" (o thread
  principal aparece em `modal_loop` no despejo de threads).
Pendente: o login (do usuário). O helper "Computer Use" já liga todos os símbolos (seção abaixo).

## Codex desktop na VM (7 nov., noite)

**O Codex desktop 26.623 (runtime OWL) abre "Get started with Codex"** (Sign in with ChatGPT), com o agente
`codex app-server` (0.142.5) inicializado pelo stdio.

```
.\build.ps1 -OutDir build-codex
.\tools\stage-chromeapp.ps1 -App build\apps\x\codex\Codex.app -OutDir build-codex -Autostart -Diagnostico -ComCLI -SizeMiB 1536
.\tools\vscode-teste.ps1 -Electron -App build\apps\x\codex\Codex.app -Nome Codex -Servico com.mos.chromeapp-teste -OutDir build-codex -Memory 3400 -Accel whpx -SemBuild
```

- GPU: sem `--disable-gpu` o ANGLE tenta Metal e OpenGL e o processo cai (proteção geral no thread da GPU); só com
  ele, o Chromium 149 nega o "acesso à GPU" (`--enable-unsafe-swiftshader` libera o SwiftShader).
- O `codex app-server` saía com código 0: **o `socketpair` do mOS era um pipe só**, e o libuv usa socketpair para o
  stdin dos filhos. Agora o kernel cria um par bidirecional (`SYS_PIPE` com `MOS_PIPE_SOCKPAIR`, `FD_PIPE_RW`). Isso
  vale para qualquer filho do Node/Electron que leia stdin (servidores de linguagem por stdio, ferramentas do agente).
- Pendentes: o login (do usuário); os helpers em Swift (`native/bare-modifier-monitor`, `launch-services-helper`,
  `*.node`) pedem as `libswift*` de `build\swift\objc\dylib`, que o stage ainda não põe na imagem; o Sparkle.

## Codex desktop: runtime Swift, Sparkle e uma falha tripla (7 nov., noite)

- Com as `libswift*` do mOS na imagem (`stage-chromeapp.ps1` agora as copia, mais `libcompression` e o
  `Sparkle.framework` do app), os helpers em Swift `native/bare-modifier-monitor` e `native/launch-services-helper`
  sobem. O `machocheck` mostra os 9 binários de `native/` e os 5 do Sparkle só com `napi_*` ausentes.
- **Falha tripla da VM uns 3 min depois da partida:** o `write_dirty` (`kernel/fs/persist.c`) é recursivo e reservava
  2 KiB por nível, nas threads do kernel com 32 KiB de pilha; com o limite de profundidade em 64, o `node_modules`
  do Codex estourava a pilha. A gravação saiu da recursão (`write_one`), e o `mkdirs_parent` ficou iterativo.
- O Sparkle pedia várias APIs do Foundation (`NSURL scheme`, `NSURLComponents queryItems`, codificação percentual,
  `NSCharacterSet characterSetWithCharactersInString:`, `NSURLSession` com delegate), agora em
  `userland/libsystem/compat_objc_extra.c` (registrado no fim do `cx_objc_boot`). Os callbacks de delegate do
  `NSURLSession` ainda não existem, então o Sparkle informa "error in retrieving update information".
- `NSURLSession` com delegate agora entrega os callbacks (`didReceiveData:`, `didFinishDownloadingToURL:`,
  `didCompleteWithError:`) em `foundation_more.c`; o Sparkle baixa o appcast. O `NSURLRequest` ganhou init de
  instância, `timeoutInterval` e cópias que retêm os campos (antes uma cópia liberada deixava o corpo da requisição
  pendurado e o `http_do` caía com #GP).
- O appcast é lido com `NSXMLDocument` (`initWithData:options:error:` + `nodesForXPath:`), que era só um esboço.
  `userland/libsystem/xmldom.c` traz a árvore: `NSXMLNode`/`NSXMLElement`/`NSXMLDocument` com filhos, atributos,
  `stringValue`, `elementsForName:`, `attributeForName:`, `XMLString` e XPath sem predicados (`/`, `//`, `*`, `@`,
  `text()`). Nomes com prefixo (`sparkle:version`) ficam qualificados, como na Apple.
- Com o working tree inteiro (que já dá boot), o Codex fica estável na tela "Get started" por mais de 4 min, sem #GP.
- **Pendente:** num worktree limpo (kernel e libSystem só do que está commitado), a janela do Codex deixou de
  aparecer, com ou sem Sparkle e runtime Swift; a rodada boa usava o working tree inteiro, que tem correções de
  AppKit/QuartzCore ainda não commitadas de outra sessão (o boot desse working tree, que caía por causa do XSAVE em
  andamento, voltou com o recuo para FXSAVE). Quando essas correções entrarem, a janela vale também no tree limpo.

## Computer Use do Grok Bot (7 nov., noite)

O helper `Grok Bot Computer Use.app/Contents/MacOS/CUGrokBotService` (Swift) ia de 79 símbolos ausentes a **0**:
- **Entrada sintética para o sistema todo:** `GUI_DEVICE`/`DEV_INPUT_POST` (op 3, `kernel/dev/gamepad.c`) empurra um
  `input_event_t` na fila global de entrada, a mesma do teclado e do mouse de verdade. `userland/libsystem/cgevent.c`
  (novo) monta os `CGEvent` de mouse (cliques, arrastos, botão do meio), rolagem (`CGEventCreateScrollWheelEvent2`,
  pixels → linhas), teclado (código virtual do Mac → tecla do mOS) e texto (`CGEventKeyboardSetUnicodeString`, até
  Latin-1), e `CGEventPost`/`CGEventPostToPid`/`CGWarpMouseCursorPosition` os injetam. Num kernel sem a operação, a
  tecla volta ao caminho antigo (só a janela-chave do próprio app). As versões antigas saíram de `slack_frameworks.c`,
  `vscode_frameworks.c` e `chrome.c`.
- **Acessibilidade e automação:** `AXIsProcessTrusted` continua 0, e os `AXUIElement*` novos respondem
  `kAXErrorAPIDisabled`; `AEDeterminePermissionToAutomateTarget` dá `errAEEventNotPermitted`. O helper segue pelo
  caminho "sem permissão", como num Mac antes de autorizar.
- **C e ObjC** (`compat_agentes.c`, `vscode_fwclasses.c`): constantes do Contacts/AppKit/CoreLocation, classes vazias do
  ScreenCaptureKit/Contacts/EventKit/CoreLocation, `CGContextAddPath` (quadráticas viram cúbicas),
  `CGContextEOFillPath`, `vImageBoxConvolve_Planar8`, e falhas limpas de `sqlite3_open_v2` (o mOS não tem SQLite) e
  `VTCreateCGImageFromCVPixelBuffer`.
- **Swift:** `CryptoKit.framework` novo (`userland/frameworks/CryptoKit`: `HashFunction`, `Digest`, `SHA256` sobre o
  `CC_SHA256_*`); overlay `CoreGraphics` com `CGPathFillRule`, `fillPath(using:)`, `move(to:transform:)`,
  `addArc(tangent1End:…)`, `contains(_:using:transform:)` e `CGAffineTransform.identity` (no ramo com interop ObjC o
  `CFCGTypes.h` não renomeia mais `CGAffineTransform`/`CGVector` para `MOSCG…`, para o mangling ser o da Apple);
  `Dispatch` com `ProcessEvent`, `makeSignalSource`, `makeProcessSource`; `os` com `Logger.logObject` e
  `OSAllocatedUnfairLock` (`@frozen`, um `ManagedBuffer`: os apps inlinam os métodos); `Foundation` com a ponte
  `DateComponents` ↔ `NSDateComponents`, `FileHandle.write(contentsOf:)` e `init?(_bridgedNSError:)` sem `__shared`
  (o mangling da Apple). Três getters/inits Swift de frameworks que no mOS são o libSystem estão em assembly no
  `compat_agentes.c` (`NSScreen.auxiliaryTopLeftArea/RightArea` = nil, `CAFrameRateRange.init(minimum:maximum:preferred:)`,
  `CMSampleBuffer.imageBuffer`), com a convenção swiftcc conferida no `swift-frontend`.
- Captura de tela (`SCScreenshotManager`) ainda não existe: as classes estão vazias e o helper recebe nil.
- **Verificado na VM (8 nov.):** `tools/testdata/cgevent/cgevent-teste.c` como serviço do launchd (`Programa=/usr/bin/
  cgevent-teste`, `Argumentos=492 856 300 650`) clica no ícone da Calculadora no Dock por `CGEventPost` — a Calculadora
  abre — e deixa o cursor em (300, 650); `CGDisplayCreateImage` devolve 1440 x 900. A captura (`GUI_DEVICE`/
  `DEV_SCREEN_CAPTURE`, op 4) e o ScreenCaptureKit mínimo (`screencapturekit.c`: `SCShareableContent` com um
  `SCDisplay`, `SCContentFilter`, `SCStreamConfiguration`, `SCScreenshotManager`) dão ao helper o "ver" do computer use.
  `CGPreflightScreenCaptureAccess` responde 1: o mOS não tem permissão de gravação de tela. Em aberto: o mesmo clique
  sobre "Sign in another way" do Codex não mudou a tela (a ver se é o botão ou o hit-test do conteúdo web).
