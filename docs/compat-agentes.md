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
| Grok Bot: helper "Computer Use" (`CUGrokBotService`, Swift) com as dylibs Swift do mOS | 873 | 84: `CGEvent*` (injetar entrada), `AXUIElement*`, Contacts/EventKit, CryptoKit em Swift, ScreenCaptureKit |

Sobram, de propósito: o helper "Computer Use" do Grok Bot (Swift, 873) e o `cable_ble_scanner` do Antigravity (Swift),
que são recursos opcionais e esperam o runtime Swift ([swift-objc-runtime.md](swift-objc-runtime.md)).

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
