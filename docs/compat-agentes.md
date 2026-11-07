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
