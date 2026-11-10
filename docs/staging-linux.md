# Build e stage no Linux

Este arquivo é o roteiro que um agente segue nesta máquina. O que está abaixo rodou aqui, com zig 0.17.1 ELF, `cc`, Python 3, QEMU e OVMF. Não havia `pwsh`. Não é uma promessa de que todo Linux, todo zig ou todo app fecha.

## Quem é a fonte de cada lista

| Onde | O que faz |
|---|---|
| `build.ps1` | Build no Windows. No Linux, com `pwsh`, usa o mesmo `Resolve-MosZig`, o `\\?\` só no Windows, o `mkimg` nativo e não aborta se libc++, sqlite, ICU, WebKit, exemplos ou Swift faltarem. O kernel e o libSystem continuam obrigatórios. |
| `tools/build.sh` | O mesmo núcleo quando não há `pwsh`: libSystem, kernel UEFI, `mkimg` com `cc`, userland, imagem em `--out`. |
| `tools/stage-electron.ps1` | Lista de arquivos do app no Windows (`\\?\` e `mkimg.exe`). No Linux, se existir `bash`, delega para `tools/stage-image.sh` e sai. |
| `tools/stage-image.sh` | A mesma lista no Linux: cria `esp/mOS` se faltar, copia o alvo de um link de texto por cima do arquivo, `mkimg` nativo, overlay de teste. |
| `tools/stage-host.ps1` | `Resolve-MosZig`, caminho longo, `esp` e `mkimg`. Os outros `tools/*.ps1` chamam `Resolve-MosZig` em vez de pegar a primeira pasta `zig-*`. |

O prefixo `\\?\` só entra no Windows (`ConvertTo-MosLongPath`). No Linux o `mkimg` é `cc -O2 tools/mkimg.c`, sem `.exe`. Se não houver `cc`, `gcc` nem `clang`, o script para com essa frase.

## Zig do host

`Resolve-MosZig` e `pick_zig` em `tools/build.sh` leem 4 bytes. No Windows aceitam PE/MZ (`4D 5A`). No Linux aceitam ELF (`7F 45 4C 46`). Um Mach-O em `.tools/zig-macos-dl/zig.exe` é avisado e ignorado. Se houver mais de um binário do host, fica o de maior `zig version`. `MOS_ZIG` com a magia certa vence os demais.

Nesta máquina a árvore de prova (não vai para o git; `.tools/` está no `.gitignore`) tinha `zig-macos-dl/zig.exe` (Mach-O), zig 0.14.1 ELF e zig 0.17.1 ELF. O build imprimiu:

```
aviso: .tools/zig-macos-dl/zig.exe é macho; este host precisa de elf
zig: .tools/zig-x86_64-linux-0.17.1/zig (elf, 0.17.1)
```

## libunwind e o zig 0.14.1

`tools/build-libsystem.ps1` e `tools/build.sh` compilam o libunwind com `zig clang --target=x86_64-apple-macos11-elf`. No zig 0.17.1 esse `clang` gera ELF (foi o que entrou na imagem). O `zig cc -target` recusa o triplo nos dois.

No zig 0.14.1, reproduzido com o tarball `zig-x86_64-linux-0.14.1`, os erros são estes:

`zig cc -target x86_64-apple-macos11-elf` (arquivo `.c` real):

```
error: unable to parse target query 'x86_64-apple-macos11-elf': UnknownOperatingSystem
```

`zig clang --target=x86_64-apple-macos11-elf` sem o include do zig (o 0.14.1 não tem `any-darwin-any`; o include cai em `/usr/include`):

```
/usr/include/features-time64.h:20:10: fatal error: 'bits/wordsize.h' file not found
```

incluído a partir de `lib/libunwind/src/UnwindLevel1.c:21` via `inttypes.h`.

O mesmo `zig clang` com `-isystem .../lib/libc/include/any-macos-any`:

```
error: backend data layout 'e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128' does not match expected target description 'e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128'
```

Quando a sonda de `UnwindLevel1.c` devolve um desses três textos, C e C++ passam a `zig clang --target=x86_64-unknown-none-elf` com `-D__APPLE__ -D__MACH__ -D__ELF__`. O `.S` continua com `-U__APPLE__ -U__MACH__`. O log da sonda fica em `build/libunwind-alvo-erro.txt` (ou em `--out`). Nesta imagem o 0.17.1 aceitou o alvo Apple e o contorno não entrou.

O zig 0.17.1 também trata declaração implícita como erro. `userland/libsystem/fi_swift.c` repetia `objc_msgSend`, `sel_registerName` e `cx_cstr` com tipos diferentes dos cabeçalhos; as redeclarções saíram. `userland/init.c` ganhou `strrchr` e um `snprintf` só para os três `%s` que ele usa, porque `mos.h` não os declara.

## Comandos que rodaram

```
tools/build.sh --out /tmp/mos-linux-build
```

Saída: `BOOTX64.EFI` (11 MiB), `mkimg` nativo, `mOS.img` de 128 MiB. Manifesto: 32 arquivos, assinatura Ed25519. `userland/init.c` compilou e o boot registrou `proc: iniciado init (pid 1)`.

```
tools/stage-image.sh --out /tmp/mos-stage --size 256 \
  --app /tmp/mos-tiny/Cursor.app \
  --efi /tmp/mos-linux-build/BOOTX64.EFI \
  --cursor-extras --bottles /tmp/mos-bottle
```

O app era um Mach-O arm64 mínimo com o framework em link de texto. O script repara o link, grava `mos-stage-linux-ok` e o overlay `com.mos.teste.sem-assistente`. Manifesto: 28 arquivos, Ed25519.

Boot (KVM; o usuário desta VM não está no grupo `rdma`, então foi `sudo -n`; sem isso o QEMU responde `Could not access KVM kernel module: Permission denied` e o caminho é `-accel tcg`):

```
cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/OVMF_VARS.fd
sudo -n qemu-system-x86_64 -machine q35 -m 512 -cpu max -smp 1 -accel kvm \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/tmp/OVMF_VARS.fd \
  -drive format=raw,file=/tmp/mos-linux-build/mOS.img,if=ide,index=0 \
  -display none -serial file:/tmp/mos-serial.log -snapshot -no-reboot -net none
```

O processo fica de pé depois do desktop. `timeout 40` devolve 124 e o serial já tem o banner. Não há gravação em disco físico.

Log serial de `tools/build.sh` (KVM, imagem de 128 MiB, init compilado nesta árvore):

```
mOS: iniciando o Nucleo...
mOS 0.1 "Dunas" — Nucleo x86_64
origem: CONFIÁVEL — sata0 (SATA), partição 1 [1048576, +133151744)
proc: iniciado init (pid 1, 2848 KiB)
Boot concluído em 188 ms, heap 18280 KiB
Desktop pronto em 551 ms
```

Log serial de `tools/stage-image.sh` (mesmo EFI, KVM, 256 MiB):

```
mOS: iniciando o Nucleo...
mOS 0.1 "Dunas" — Nucleo x86_64
origem: CONFIÁVEL — sata0 (SATA), partição 1 [1048576, +267369472)
Boot concluído em 171 ms, heap 17895 KiB
Desktop pronto em 530 ms
```

## O que foi medido

`tools/medir-imagens.py` compila `tools/machoload_host.c` com `kernel/compat/macho.c` (`-DMOS_HOST_TEST`) e chama `macho_load`. O libSystem arm64 e o `a64rt.mos` desse harness são ELF mínimos (um `PT_LOAD` e o símbolo `cx_start`), não os binários do build. `MACHOTEST_TOLERANT=1` e `MOS_ARCH=arm64`.

**L06.** `tools/extrair-framework.py --reparar` copia o alvo por cima do arquivo de texto. Num framework sintético, `file` passou de `ASCII text, with no line terminators` para `Mach-O 64-bit arm64 dynamically linked shared library`, e `macho_load` carregou 3 imagens. No zip `VSCode-darwin-arm64` (13 arquivos do fecho, não o `.app` inteiro) o `Electron Framework` na raiz do framework fez o mesmo: texto antes, Mach-O arm64 depois, e `macho_load` de `Contents/MacOS/Code` terminou `ok` com **7 imagens**. O log também diz `2558 símbolo(s) ausente(s) no libSystem` e três frameworks de sistema ausentes (QuickLookUI, StoreKit, CoreData). A interface do VS Code não subiu.

**L08.** `MAX_IMAGES` continua 96. A união sintética (os 24 `libswift*` de `tools/link-swift-dylibs-objc.ps1`, SwiftUI, Combine, StoreKit, QuickLookUI e as libs de processo do Electron) carregou **35 imagens**. A sonda de um executável mais 96 dylibs devolve `bibliotecas demais no bundle (máximo 96)`. O fecho real do VS Code acima ficou em 7. `userland/libsystem/swiftsupport.c` (`dl_tabs[96]`) não foi editado. Não há motivo, nesta medição, para subir o teto.

**L12.** O bottle Homebrew de ~619 MiB não está na árvore. A imagem de teste leva `tools/brew-shims`: `git --version` imprime `git version 2.47.0 (mOS shim)`, `security find-certificate -a -p` imprime um PEM vazio, e `which git` acha o git no `PATH`. Um bottle sintético com cópias do mesmo tamanho em `libexec/git-core` virou `#mOS-link:/usr/local/opt/git/bin/git`, menos o arquivo cujo nome é `git`. Isso não é o git do Homebrew nem o `security` da Apple.
