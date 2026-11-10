# Roteiro de hardware — só o Thiago

Este arquivo é um roteiro. Não é evidência de uma execução. Quem escreveu não tem a máquina, não gravou pendrive e não viu o firmware. Secure Boot, criptografia e hardware real continuam sem validação até o Thiago rodar e guardar o log.

O pendrive é apagado na opção 1. Não apontar Rufus/Etcher para o disco do sistema.

## 1. Build no Windows

Na raiz do repositório, PowerShell:

```powershell
.\build.ps1
```

Zig de Windows em `.tools\zig-*`, com `zig.exe` PE (os dois primeiros bytes `MZ`). Um `zig.exe` de macOS em `.tools\zig-macos-dl` é ignorado com aviso amarelo. Se o libSystem falhar mesmo com o zig certo, o erro é do código em `userland/libsystem` (frente Sistemas): guardar as primeiras linhas vermelhas.

Se a cadeia Swift já estiver compilada (`build\universal-swift\usr\lib\swift\libswiftCore.dylib` ou `build\swift\objc\dylib\usr\lib\swift\libswiftCore.dylib`), ela entra na imagem e o script avisa o tamanho. Sem esses arquivos, a imagem sai sem Swift e o aviso diz isso. Para só juntar slices já prontos:

```powershell
.\build.ps1 -ComSwift
```

`-ComSwift` não liga um modo em tempo de execução. Não compila a cadeia; chama `tools\build-swift-arm64.ps1 -SoJuntar`.

Saída: `build\mOS.img` e `build\esp\`.

## 2. Gravar

Opção 1, a do README: Rufus em modo DD, ou balenaEtcher, com `build\mOS.img`. Apaga o pendrive.

Opção 2: formatar o pendrive em FAT32 (GPT ou MBR) e copiar o conteúdo de `build\esp\` (`EFI` e `mOS`) para a raiz.

No firmware: menu de boot (F12, F11, F9 ou Esc), pendrive em modo UEFI. Desativar Secure Boot. O mOS não é assinado. Boot com Secure Boot ligado não é um teste deste sistema.

## 3. O que olhar no primeiro boot

O assistente abre antes do login.

- Live: nada é gravado. A conta some ao desligar.
- Live com persistência: só depois do consentimento, e só no pendrive de onde o sistema subiu. Sem RDRAND/RDSEED o mOS recusa a persistência (não cria senha). Ter a instrução não prova a qualidade da entropia.
- Instalar: o assistente pede o disco e pede para digitar o identificador (`sata0`, `sata0p2`, …) antes da primeira gravação. Isto apaga o alvo escolhido. Não fazer nesta rodada se a pergunta for só "o pendrive sobe".

Se a origem do boot não for identificada com certeza, persistência e instalação ficam bloqueadas e o sistema fica na RAM. Anotar a linha do log serial que diz isso.

## 4. Apps, ainda no QEMU, antes do hardware

O hardware não substitui o QEMU para a primeira carga. Receitas que já estão no repositório (não foram reexecutadas por quem escreveu este roteiro):

```powershell
.\build.ps1 -OutDir build-electron
.\tools\stage-electron.ps1 -App "build\apps\x\slack-x64\Slack\Slack.app" -OutDir build-electron -Autostart -Diagnostico
.\run.ps1 -Image build-electron/mOS.img -Memory 2700
```

VS Code: `tools\vscode-teste.ps1` (ver `docs/libsys-pendencias.md` §1). Cursor precisa de `-MaxModuloMiB 128`. Codex desktop usa `tools\stage-chromeapp.ps1`, não o stage Electron. WhatsApp: o harness citado em `docs/libsys-pendencias.md` (`build\wa-run`) ou imagem com a cadeia Swift presente.

O que este ramo acrescenta nessa imagem, se o artefato Swift existir: `libswift*.dylib` em `/usr/lib/swift` e `SwiftUI.framework`. Módulos `.node` que ligam `libswiftCore` passam a ser copiados pelo `stage-electron.ps1` quando essa biblioteca está no esp.

Para o teste de Launch Services dentro do kernel:

```powershell
.\build.ps1 -UnitTests -Run
```

No log serial, procurar `unit:` e a linha `RESUMO`. `APROVADO` é o harness. Não é o app aberto.

## 5. O que só a máquina física responde

Anotar, com o log serial se houver, sem marcar como sucesso o que não aconteceu:

1. O pendrive entra pelo `\EFI\BOOT\BOOTX64.EFI` no menu UEFI, com Secure Boot desligado.
2. O assistente aparece e o Live chega à mesa.
3. Um `.dmg` com um `.app` no topo (não um app da App Store) monta e o app aparece em Aplicativos. Um `.app` com `Contents/SC_Info` não é copiado; o log deve ter `SC_Info` e `FairPlay`. Não contornar.
4. Um `.pkg` flat (xar com Payload em cpio gzip ou pbzx e Bom) coloca o app em Aplicativos. O log diz quantos arquivos entraram e que os scripts não rodaram. Um pacote com `..` no caminho, ou com `SC_Info`, é recusado e não escreve fora de Aplicativos.
5. Encerrar pelo ⌘Q espera e sai. ⌥⌘Esc força. Um app que ignore o evento deve receber SIGTERM e, cerca de 2 s depois, SIGKILL. Não há como ver o sinal sem o log do processo; a janela sumir e o Dock apagar a bolinha é o observável.
6. Com persistência consentida, um arquivo em `~/Library/Preferences` continua lá depois de reiniciar o Live persistente. Sem consentimento, não continua.
7. SwiftUI / um `.node` que liga `libswiftCore` só vale se o passo 1 copiou a cadeia. Sem o aviso "Swift na imagem" no build, o teste não diz nada sobre Swift.

Apps da App Store (FairPlay) ficam de fora de propósito. Setapp, se instalar outro app por `.dmg` com o `.app` no topo, cai no passo 3. Se instalar por `.pkg` sem `SC_Info`, cai no passo 4 e o app entra em Aplicativos, sem rodar o script do instalador.
