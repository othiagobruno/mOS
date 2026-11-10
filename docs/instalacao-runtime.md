# Instalação, armazenamento e runtime

Frente do loader, do Launch Services, do dyld e do VFS: abrir, encerrar e persistir apps instalados e no Live. Isto descreve o que este ramo faz, o que continua stub e o contrato com as outras frentes. Não é compatibilidade universal. Nenhum app da lista (WhatsApp, VS Code, Cursor, Slack, Claude, Codex, Postico, Open Bot, Setapp, Usagebar, Usage for Claude & ChatGPT, Grok Bot, ChatGPT) foi executado neste ramo.

## O que está funcional no código

- **Launch Services.** `ls_rescan` deixa de olhar só o nome e o tamanho do `Info.plist`. O carimbo mistura nome, mtime, amostra do plist e dos arquivos em `Contents/MacOS`. `ls_tick` (~1,5 s) relê o bundle quando esse carimbo muda, inclusive com o mesmo tamanho. O plist de escolhas (`~/Library/Preferences/com.apple.LaunchServices/com.apple.launchservices.secure.plist`) é relido quando o carimbo dele muda. Teste de regressão em `kernel/fs/unit_tests.c` (`test_launch_runtime`), compilado só com `-DMOS_UNIT_TESTS`. Este ambiente não compilou o kernel UEFI, então o teste não rodou aqui. Os testes de host que rodaram estão no final.
- **Encerrar.** `compat_gui_request_quit` segue `kernel/include/quitpol.h`: `GEV_QUIT`, 3 s, `SIGTERM`, 2 s, `SIGKILL`. Um segundo pedido sobe um estágio. Forçar vai ao `SIGKILL`. O Dock (⌘Q, menu) continua no pedido com folga. ⌥⌘Esc chama `app_force_quit`. Atalhos `mac:` e apps `macapp` encerram pelo pid, também com zero janelas. O Horizonte pede o encerramento do motor e não chama `on_quit` de novo. `watch_tick` roda antes de `compat_gui_quit_tick`: saída 0, -9 e -1000 não abrem o diálogo; saída -15 (`SIGTERM`) também não, se o encerramento ainda está pendente.
- **Links de framework.** Um arquivo dentro de um `.framework` cujo conteúdo é uma linha de caminho (`A`, `Versions/Current/Electron Framework`) ou `#mOS-link:` é seguido no meio do caminho (`kernel/lib/vfs.c`). O `macho.c` segue também o último componente ao ler a dependência; se o alvo não existe, devolve nulo para o `locate()` ainda tentar `Versions/A` e `Versions/Current`. O `mkimg` reescreve esses textos para `#mOS-link:` antes do manifesto, no payload e nos overlays. Um Mach-O, um plist, um bplist ou uma URL não viram link. Um arquivo de texto curto dentro de um `.framework` que seja só um caminho também vira link: é o formato que o Windows grava no lugar do symlink.
- **Swift na imagem.** `build.ps1` copia `usr/lib/swift` e `System` (SwiftUI, Combine) quando `build/universal-swift` ou `build/swift/objc/dylib` já tem `libswiftCore.dylib`. `-ComSwift` (ou `MOS_COM_SWIFT=1`) só manda o `build-swift-arm64.ps1 -SoJuntar` juntar slices já compilados. Sem esses artefatos, `tools/gen-libswift-stubs.c` grava as `libswift*` que o VS Code 1.145.2 e o inventário do OpenBot pedem. No Windows o `build.ps1` compila o gerador com zig; no Linux, `tools/build.sh` chama `tools/stage-libswift.sh` (`cc`/`gcc`) e, se a cadeia existir, copia a árvore antes. Cada símbolo exportado é um `ret` e o binário contém `STUB mOS libswift`. Uma dylib da cadeia compilada não é substituída. São cerca de 110 MB só quando a cadeia real entra; os stubs são pequenos. O sistema de arquivos da VM mora na RAM.
- **Resolução no carregador.** `kernel/include/libswift_image.h` lista os caminhos em `/usr/lib/swift`. `link_abs_dep` em `kernel/compat/macho.c` (logo depois de `usrlib_alias`) manda o basename para essa lista. Não há alias Homebrew. `libz`, `libcurl` e `libcompression` continuam em `usrlib_alias` para a frente STOR-SYSDYLIBS. Esta mudança em `macho.c` precisa de revisão do Kernel.
- **Seções `__swift5_*`.** `userland/libsystem/swiftsect.c` grava types, protos, proto, fieldmd, reflstr e as outras seções `__TEXT,__swift5_*` quando `dyldimg.c` vê a imagem, e esquece no `remove_image`. Não interpreta os descritores.
- **Wipe do esp.** Antes de apagar `build/esp`, o script guarda `mOS/Aplicativos`, `mOS/usr/lib/swift`, `SwiftUI.framework` e `Combine.framework` e os devolve depois de copiar `root/mOS`. Contas, Preferências, logs e `.conta` não são preservados: o `mkimg` recusa esse payload. Se `Remove-Item` não apaga (caminho longo), tenta `cmd rd /s /q \\?\`. Se ainda sobrar, o build para.
- **Home.** `account_ensure_dirs` cria Mesa, Documentos, Downloads, Imagens, Música, Lixeira e `Library` com Preferences, Application Support, Logs, Saved Application State, Caches e LaunchAgents. Vale para a conta nova, para a sessão e para `/Users/Visitante` no `vfs_init`.
- **Persistência.** Com a gravação ligada (instalado, primeiro boot ou Live após consentimento), `~/Library` entra no disco: `tracked_path` só exclui `/Volumes`, `/tmp` e o diretório de atualização. Caches também são gravados. Arquivo vindo da imagem ou do disco entra com mtime 0 e não é reescrito. `persist_capture_existing` marca, só depois de `persist_consent_live` ligar a gravação, nós com mtime diferente de 0 ou dirty. Não é chamado de `persist_enable`.
- **Instalar de um .dmg.** Depois de montar, o DiskImageMounter copia os `.app` do topo do volume novo e de `Applications` e `Aplicativos` para `/Aplicativos` (`ls_install_app`). Não entra dentro do bundle. O destino que já existe não é sobrescrito.
- **Agentes e login.** O init varre de novo, a cada 5 s, LaunchAgents/LaunchDaemons de sistema e de usuário, os de `Conteúdo/Library` dentro de cada `.app` em `/Aplicativos`, e `~/Library/Preferences/com.mos.loginitems` (um caminho absoluto por linha). No boot a varredura extra não inicia ninguém: o laço que já existia inicia os `RunAtLoad`. A cada 5 s só inicia job com `last_start == 0`, para não duplicar. `LSSharedFileListInsertItemURL` (`userland/libsystem/vscode_frameworks.c`, arquivo da frente Sistemas) acrescenta o caminho nesse arquivo. Remover um item ainda não reescreve o arquivo. O snapshot da lista continua vazio. O "abrir ao iniciar sessão" do Dock já é outro arquivo, da Interface (`com.mos.dock.cfg`).
- **dyld remove.** `_dyld_register_func_for_remove_image` guarda até 32 callbacks. `cx_dyld_notify_remove` os chama. O `dlclose` não chama: `MOS_DYLD_CLOSE` só tira uma referência e o kernel não desmapeia.

## O que é recusa ou stub

- **libswift* sem a cadeia compilada.** O corpo de cada símbolo exportado é `ret` (0xC3) e o arquivo contém `STUB mOS libswift`. Não executa metadados Swift. A cadeia de `build/universal-swift` ou `build/swift/objc/dylib`, quando existe, permanece no lugar do stub.
- **FairPlay.** Bundle com `Contents/SC_Info` não é copiado (`ls_install_app` devolve -2). Não há descriptografia. Apps da App Store ficam de fora.
- **.pkg / .mpkg.** O DiskImageMounter avisa e volta. Não há extrator xar/bom.
- **Forçar encerrar um app do kernel** (exceto o Horizonte) é o mesmo `on_quit` de sempre: não há `SIGKILL` para código que roda dentro do kernel.
- **Fila de encerramento cheia** (32): o pedido manda `SIGTERM` ou, se for forçar, `SIGKILL`, sem a folga.
- **`dl_tabs[96]`** em `swiftsupport.c` não mudou. Imagem além dessa tabela fica sem nome de símbolo no `dladdr`.
- **`MAX_IMAGES` 96** em `kernel/compat/macho.c` não mudou. Proposta para o Kernel, abaixo.
- **NSUserDefaults / CFPreferences** não foram implementados. O contrato de diretório está abaixo para a frente Sistemas.
- **Login item:** o arquivo é a lista. Não há API de leitura de volta para o app, e `LSSharedFileListRemove` não apaga a linha.
- **Limite de 64 jobs** no init. Um Setapp que passe disso não sobe o restante. O init avisa "já carregado" em silêncio na revarredura; os outros motivos de recusa continuam no log.

## Contrato com o Kernel

`kernel/proc/process.c` não foi editado. Já existem:

- `proc_signal(pid, sig)` em `kernel/include/proc.h`. `MOS_SIGTERM` é 15. O processo termina com `-sig` se o sinal o derruba.
- `proc_kill(pid)` termina com -9.

O guisys usa `proc_signal(pid, MOS_SIGTERM)` e depois `proc_signal(pid, MOS_SIGKILL)`. `MOS_SIGKILL` é 9. Não é preciso um `proc_kill` separado para o escalonamento.

`kernel/compat/macho.c`: o seguimento de link de framework entrou em `dep_read`. **Revisão do Kernel:** `link_abs_dep`, logo depois de `usrlib_alias`, chama `mos_libswift_resolve` (`kernel/include/libswift_image.h`) e, se o arquivo pedido não existe, procura o mesmo basename em `/usr/lib/swift`. `usrlib_alias` não foi alterado (libz, libcurl, libcompression ficam para STOR-SYSDYLIBS). Os fixups encadeados não foram mexidos. Os formatos 1 (ARM64E), 2 (PTR_64), 6 (PTR_64_OFFSET), 12 (ARM64E_USERLAND) e 13 (ARM64E_USERLAND24) já são aceitos. A frase "só 2 e 6" está desatualizada. Proposta, se um binário real estourar o teto ou cair em "formato de ponteiro encadeado":

- subir `MAX_IMAGES` de 96 para 256 (`link_t` já é heap; um vetor de 256 na pilha é pequeno);
- só então olhar um formato novo, com o número que o log imprimir.

`dl_tabs[96]` é o espelho no userspace. A frente Sistemas deve subir os dois juntos. Este ramo não mexe no teto sozinho.

## Contrato com a Interface

Uma linha em `kernel/ui/desktop.c`: `ACT_FORCEQUIT` chama `app_force_quit` em vez de `app_quit`. ⌘Q, o menu Sair e o menu do Dock continuam em `app_quit` (folga de 3 s). Vale a Interface revisar se o forçar encerrar do Dock deve continuar nessa ação.

## Contrato com Sistemas

Bytes no disco quando a persistência está ligada:

| Caminho | Quem escreve | Quando chega no disco |
|---|---|---|
| `~/Library/Preferences` | o app, via VFS | na hora, se a gravação já está ligada; senão na captura do consentimento do Live, se o arquivo tem mtime ≠ 0 ou está dirty |
| `~/Library/Application Support`, `Logs`, `Saved Application State`, `Caches`, `LaunchAgents` | o app | igual |
| `com.apple.launchservices.secure.plist` | o Launch Services, dentro de Preferences | igual |
| `~/Library/Preferences/com.mos.loginitems` | `LSSharedFileListInsertItemURL` | igual; uma linha, caminho absoluto, por item |

mtime 0 e dirty falso = veio da imagem ou do disco e não é regravado. `NSUserDefaults` e `CFPreferences` continuam da frente Sistemas; este ramo só garante o diretório e a gravação.

`userland/libsystem/vscode_frameworks.c` ganhou o escritor de login item. É arquivo da frente Sistemas: a mudança é o `fopen` em append nesse caminho. O restante das funções `LSSharedFileList*` continua como estava.

`userland/libsystem/swiftsupport.c` passou a chamar `dladdr_trie.h`. O resultado do `dladdr` é o mesmo algoritmo de antes (faixa do libSystem `0x400000100000`–`0x400001000000` devolve só o nome do arquivo). `dl_tabs` segue em 96.

`userland/libsystem/objc.c` (blocos) não foi editado.

## Diagnóstico do build Windows / libSystem

Este ambiente é Linux, sem `zig` e sem PowerShell. O fluxo `build.ps1` não rodou. Não há log desta sessão com o erro do libSystem.

O que o repositório já registra (`docs/libsys-pendencias.md`, §5): `.tools\zig-macos-dl` casa com `zig-*` antes do zig de Windows. Um `zig.exe` de macOS nesse nome fazia a primeira compilação, a do libSystem, falhar. `tools/stage-host.ps1` (`Resolve-MosZig`) e `tools/build.sh` leem os 4 primeiros bytes: no Windows só PE/MZ, no Linux só ELF, e entre os que servem fica a maior `zig version`, não a primeira pasta. `MOS_ZIG` com a magia certa vence. Os scripts de `tools/` que compilam chamam essa escolha. O procedimento no Linux está em `docs/staging-linux.md`.

O mesmo documento cita staging do Cursor com caminho acima de 260 caracteres, que `Remove-Item` não apaga. O wipe do esp agora tenta `rd /s /q \\?\` depois de guardar Aplicativos e Swift.

Se o erro real for um erro de compilação dentro de `userland/libsystem`, ele é da frente Sistemas. Sem o log, este ramo não propõe patch nesse código.

## Blocos e dladdr

`tools/dladdr_trie_test.c` monta um Mach-O sintético e percorre `dladdr_trie.h`, o mesmo parser que `swiftsupport.c` usa. `tools/block_refcount_test.c` repete a aritmética de flags do `objc.c` (bit 24, bit 28, refcount de 2 em 2) e confere que o fonte ainda tem `(1 << 24)`, `f + 2` e `f - 2`.

Não há binário de VS Code, Electron nem Chromium nesta árvore. Não houve carga real desses apps, nem no QEMU. O resultado dos testes de host não substitui essa carga.

## Testes de host que este ambiente rodou

Compilados com `gcc` e executados na raiz do repositório:

```
gcc -Wall -Wextra -o /tmp/fwlink_test tools/fwlink_test.c && /tmp/fwlink_test
gcc -Wall -Wextra -o /tmp/quitpol_test tools/quitpol_test.c && /tmp/quitpol_test
gcc -Wall -Wextra -o /tmp/dladdr_trie_test tools/dladdr_trie_test.c && /tmp/dladdr_trie_test
gcc -Wall -Wextra -o /tmp/block_refcount_test tools/block_refcount_test.c && /tmp/block_refcount_test
gcc -O2 -o /tmp/mkimg tools/mkimg.c && /tmp/mkimg /tmp/mos-fw.img /tmp/mos-fw-tree 32
```

Resultado nesta máquina (gcc, árvore descartável em `/tmp`, sem disco):

```
fwlink: ok
quitpol: ok
dladdr trie: ok (2 símbolos; VS Code/Electron/Chromium não estão nesta árvore)
block refcount: ok (espelho; Electron/Chromium/VS Code não rodaram aqui)
mkimg: manifesto do payload: 3 arquivos, 21 bytes (assinatura 2c6bcc0e3582a793…, SEM Ed25519)
mkimg: /tmp/mos-fw.img — 32 MiB, 18 clusters usados de 31463
```

A imagem de 32 MiB contém `#mOS-link:A` e conserva o magic `\xcf\xfa\xed\xfeDEMO`. Não houve assinatura Ed25519 nesse teste (sem `--key`).

Não rodou: `build.ps1`, `build-libsystem.ps1`, kernel UEFI, QEMU, `-DMOS_UNIT_TESTS` no kernel, qualquer app da lista, hardware, Secure Boot, criptografia.

### libswift (L15)

`sh tools/libswift_check.sh` (gcc). O gerador, o resolvedor de nomes e o registro de `__swift5_*` passaram. O `machocheck` contra um `libSystem.mos` vazio:

- fixture que importa os 55 nomes da lista, sem as dylibs: 55 ausentes;
- o mesmo fixture com as dylibs no diretório: 0 ausentes;
- VS Code 1.145.2 (darwin, sha256 `b6c60d84a8ffb85c5bb4106d6ff74773fec07e6c1a4bb944f8af68e3bfe5abbe`, fora do repositório, em `/tmp/vscode-app`): 53 linhas `libswift*` no relatório de ausentes; com as `libswift*` geradas no mesmo diretório, 0. A marca `[falta]` permanece: `sys_ok` do machocheck não trata essas dylibs como substituto do libSystem. O que zera é a coluna de símbolos (libswiftCore 40/0, libswiftFoundation 20/0).

`tools/stage-libswift.sh` não substituiu um `libswiftCore.dylib` que já estava no destino e completou as outras. `mkimg` de uma árvore descartável (EFI falso só para o formato aceitar o payload, mais `mOS/usr/lib/swift`) gravou `libswiftCore.dylib` na imagem de 32 MiB. Essa imagem não foi iniciada. O VS Code não foi executado.
