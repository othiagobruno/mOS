# Auto-hospedagem: desenvolver o mOS dentro do mOS

**Objetivo:** num mOS instalado, abrir este repositório, editar, compilar o sistema, instalar o resultado e testá-lo,
sem passar pelo Windows. Neste documento, "compilar o sistema" quer dizer gerar o kernel, o libSystem, os programas
de usuário, as ferramentas `/usr/bin` e o pacote de atualização.

Estado em 8 nov. 2026: no mOS já funcionam o VS Code (com o terminal integrado), bash, curl HTTPS, `git clone` e
`brew install`. O Claude Code CLI chega até o login. Falta tudo o que está abaixo.

## Ponto de partida: o build já é quase todo Zig

O `build.ps1` e os scripts que ele chama (`build-libsystem`, `build-libcxx`, `build-icu`, `build-samples`) usam um
compilador só: o `zig cc` 0.17.1 (`.tools\zig-x86_64-windows-0.17.1`). Isso inclui os alvos `x86_64-uefi-msvc`
(kernel), `x86_64-freestanding-none` (libSystem e programas) e `x86_64-macos` (WebKit, `/usr/bin`, testes).

O PowerShell só organiza os passos. Copia arquivos e chama o zig.

Há duas exceções:
- `build-swift-apps.ps1`, que usa o `swiftc.exe` do Windows;
- o `mkimg`, compilado para `x86_64-windows-gnu`, mas que é C puro (`tools/mkimg.c`).

O Zig publica uma versão `x86_64-macos` que é um executável Mach-O ligado só ao libSystem. É exatamente o tipo de
binário que o mOS carrega. A maior parte do caminho é, portanto, fazer o zig rodar no mOS e tirar o PowerShell do meio.

## Fases

### 1. Zig rodando no mOS

- Pôr `zig-x86_64-macos-0.17.1` na imagem por um script novo, `tools/stage-zig.ps1`, no molde do `stage-bash.ps1`.
  O destino é, por exemplo, `/usr/local/zig`, com `lib/` inteira, porque o zig precisa dos headers e das libcs dele.
- Subir em escada, um degrau por vez:
  1. `zig version`;
  2. `zig cc hello.c -target x86_64-macos`, e rodar o resultado;
  3. um arquivo do libSystem com `-c`;
  4. o libSystem inteiro;
  5. o kernel (`x86_64-uefi-msvc`).
- **Riscos prováveis.** O `zig cc` chama a si mesmo como subprocesso (`zig clang`, `zig ld.lld`) via `posix_spawn`.
  Usa um cache em `~/.cache/zig` com travas de arquivo (`flock`/`fcntl F_SETLK`). Usa muitas threads e muitos `mmap`
  grandes, e lê `realpath`/`getrlimit`/`sysctl hw.ncpu`. Cada falha vira um conserto no libSystem ou no kernel, como
  aconteceu com o Bun do Claude Code.
- **Memória e tempo.** O kernel tem centenas de arquivos. Medir a RAM e o tempo sob WHPX antes de otimizar. `-j1`
  pode ser o primeiro modo estável.
- **Pronto quando:** dentro da VM, o zig gera um `libSystem.mos` byte a byte igual ao do Windows, nas mesmas flags.
  O zig é determinístico, então dá para comparar por SHA-256.

**Feito em 8 nov. 2026 (QEMU/WHPX, 3 GB, 4 CPUs).** Dentro do mOS, o zig 0.17.1 para macOS gera o libSystem (112
fontes + libunwind) e o kernel (164 fontes) **byte a byte iguais** aos do Windows.

Como repetir:

```
.\build.ps1 -OutDir build-zig
.\tools\stage-zig.ps1 -OutDir build-zig [-DegrauMin N] [-Degrau N] [-RastrearZig]
.\run.ps1 -Headless -Image build-zig/mOS.img -Serial build-zig/serial.log -Memory 3072 -Port 4471 -Accel whpx
```

O roteiro é `tools/testdata/zig-escada.sh`, e as linhas `zig:` vão para o log serial. O zig vem de
ziglang.org (`zig-x86_64-macos-0.17.1.tar.xz`, SHA-256 conferido) e fica em `.tools\macos-zig-dl`. O nome não pode
começar com `zig-`, porque os scripts pegam o primeiro `.tools\zig-*` como zig do Windows. Na imagem, ele ocupa
1745 arquivos em `/usr/local/zig`: só as libcs do Darwin ficam.

| Degrau | Tempo |
|---|---|
| 1. `zig version` | 2–7 s |
| 2. hello para macOS, ligado e rodado | 150–250 s (inclui compiler_rt e ubsan_rt na 1ª vez) |
| 3. `libc.c` | 3–11 s |
| 4. libSystem inteiro (com libunwind) | 37–62 s |
| 5. kernel | 47–70 s |
| 6. 112 objetos do libSystem, um a um contra o host | 85–96 s, nenhum diferente |

O que precisou ser consertado:
- **Kernel, `copy_user_vec`:** o spawn aceitava só 384 argumentos, e o `zig clang -cc1` passa mais. O limite subiu
  para 4096, e cada string agora é guardada no tamanho exato.
- **Kernel, `rename`:** os fds guardam o caminho em texto, então um fd aberto dentro de uma pasta renomeada dava
  ENOENT. O zig faz `ftruncate` em `cache/tmp/<n>/lib*.a` depois de renomear a pasta. Agora o `rename_fix_fds`
  atualiza o caminho dos fds abertos, como no POSIX, onde o fd segue o arquivo.
- **libSystem, `mmap` de arquivo `MAP_SHARED` com escrita:** era uma cópia sem volta, e o `msync` era um stub. O zig
  copia a saída do cache assim: `ftruncate` do destino, `mmap` `MAP_SHARED`, `memcpy`, `munmap`. Os objetos saíam
  com o tamanho certo e só zeros. Agora cada mapeamento é registrado com um `dup` do fd e gravado de volta com
  `pwrite` no `msync` e no `munmap` (`shm_track`/`shm_flush` em `extra.c`).
- **libSystem:** entraram `posix_spawn_file_actions_addfchdir_np`, `fetestexcept` e `malloc_zone_statistics`.
- **Diagnósticos que ficaram:**
  - um `ftruncate` que falha vai para o log serial, com o caminho e se a pasta ainda existe;
  - o marcador `/Sistema/Biblioteca/rastrear-arquivos-zig` registra as escritas em arquivo dos processos `zig`
    (`stage-zig.ps1 -RastrearZig`).
- **Kernel reprodutível:** o link PE do kernel não é determinístico nem no Windows, porque leva um carimbo de data e
  hora e o caminho e o GUID do PDB. Com `-s -Wl,/Brepro`, duas compilações dão o mesmo SHA-256, e é com essas flags
  que a escada compara. O `build.ps1` ainda não as usa. Elas valem para a fase 2, para comparar `build.sh` com
  `build.ps1`.

### 2. Build sem PowerShell

- Recomendação: um `build.sh` (POSIX sh, que roda no bash do mOS e no Git Bash) com o núcleo do build:
  - libSystem e o hash dele em `libsystem_blob.S`;
  - kernel;
  - programas de `userland/*.c`;
  - `/usr/bin` de `cli-macos`;
  - WebKit.framework;
  - apps de exemplo em C/ObjC;
  - árvore `esp`;
  - `mkimg`.
- `build.ps1` continua no Windows para o que só existe lá: libc++, ICU, SQLite e apps Swift. Esses artefatos mudam
  pouco e podem ser copiados da imagem instalada (`/usr/lib`) em vez de recompilados.
- Alternativa considerada: `build.zig`, com uma fonte única para os dois lados. Fica descartada por enquanto, porque
  reescreve o fluxo do Windows inteiro de uma vez. Vale reavaliar depois da fase 4.
- **`mkimg` para o mOS:** compilar `tools/mkimg.c` com `-target x86_64-macos`. Ele usa `-lbcrypt` só para números
  aleatórios. Trocar por `arc4random_buf`/`getentropy` quando não for Windows.
- **Pegadinhas de portar o PowerShell:**
  - caminhos com `\`;
  - `Get-FileHash` vira `shasum -a 256` ou `sha256sum` do coreutils;
  - a gravação do `libsystem_blob.S` deve ser em ASCII puro (ver a nota sobre UTF-8 no PS 5.1);
  - `git rev-parse` para a versão.
- **Pronto quando:** no Windows, `build.sh` (Git Bash) e `build.ps1` geram o mesmo `manifesto.txt`, a não ser pela
  linha de versão.

### 3. Imagem de desenvolvedor

A imagem instalada hoje não traz as ferramentas. Criar um perfil, `tools/stage-dev.ps1`, que monta uma imagem com:

- bash, coreutils, sed, grep, findutils e make, de bottles Intel antigos (ver `docs/porte-brew.md`);
- git (cerca de 619 MiB com dependências, conferir o que dá para cortar: docs, perl, `git-gui`);
- zig;
- VS Code;
- Claude Code CLI.

A imagem fica grande. Medir o tamanho final e a RAM mínima, que provavelmente fica acima de 4 GB.

- **Pronto quando:** essa imagem, instalada numa VM com disco persistente, faz `git clone` do repositório e roda
  `./build.sh` até o fim.

### 4. Instalar e testar o que foi compilado

Este é o passo mais delicado. Sem uma VM dentro do mOS, testar o kernel novo exige reiniciar a própria máquina.

- **Instalar:** o atualizador já aceita uma pasta do VFS como origem (`kernel/fs/update.c`). O `build.sh` gera
  `build/esp` e `build/manifesto.txt` no mesmo layout de pacote, assinado com `tools/chave-atualizacao.semente`.
  Em tese, `atualizar procurar /Users/<conta>/mOS/build` mais reiniciar já bastam. Conferir se o leitor de origem
  local espera a estrutura `/mOS/Sistema/Instalacao/manifesto.txt` e ajustar o `build.sh` ou o leitor.
- **Voltar atrás (obrigatório antes de usar no dia a dia).** O atualizador hoje não tem volta: um kernel que não
  boota exige o pendrive. Propostas:
  - guardar o `BOOTX64.EFI` anterior como `EFI/mOS/anterior.efi`;
  - um contador de boots sem sucesso, gravado pelo carregador e zerado quando a área de trabalho sobe, que depois de
    2 falhas volta para o anterior;
  - uma entrada no menu de boot ("mOS anterior") para escolher na mão.
- **Testar sem reiniciar o que não é kernel.** O libSystem hoje vem embutido no kernel (`libsystem_blob.S`), então
  qualquer mudança nele exige reboot. Um modo de desenvolvedor que carregue `/usr/lib/dev/libSystem.mos` para
  processos novos, quando o arquivo existir, deixaria testar o libSystem e os apps sem reiniciar. Isso cobre a maior
  parte do trabalho recente (VS Code, Claude, Cursor).
- **Opcional: QEMU dentro do mOS.** O Homebrew tem bottles Intel antigos do `qemu`. Sob TCG, sem aceleração, seria
  lento, mas serviria para um boot rápido sem interface antes de instalar. Fica para depois das outras fases. Puxa
  muitas dependências (glib, pixman e outras).
- **Pronto quando:** dentro do mOS, mudar uma string do kernel, compilar, `atualizar`, reiniciar e ver a mudança. E
  um kernel quebrado de propósito volta sozinho para o anterior.

### 5. Ferramentas de trabalho

- **Claude Code:** fazer o login do usuário e uma sessão real, com edição de arquivo e execução de comando via
  `bash`. O CLI exige AVX, então isso hoje só funciona sob TCG. Em hardware real o AVX existe. Conferir também o
  XSAVE no WHPX.
- **VS Code:** corrigir o espaçamento das letras do xterm.js e pôr `uname`/`sed` na imagem (integração de shell).
- **git:** `push` por HTTPS com credencial (o `osxkeychain` precisa do `/usr/bin/security`, que falta) ou por SSH
  (testar o `ssh` do Homebrew).
- **Estabilidade:** o libSystem não desenrola exceções C++, e o SIGSEGV intermitente do mimalloc continua sem causa
  (`docs/compat-apps.md`). Uma sessão longa de compilação é exatamente o tipo de carga que faz isso aparecer.

### 6. Depois

- Apps Swift: toolchain Swift `x86_64-macos` no mOS, junto com o runtime Swift de `docs/swift-objc-runtime.md`.
- libc++, ICU e SQLite recompilados dentro do mOS (já são zig, mas são builds longos).
- Rodar no mOS os testes que hoje rodam no host (`updimg`, `volimg`, `cfftest`).

## Ordem e dependências

```
1 zig no mOS ──► 2 build.sh ──► 3 imagem dev ──► 4 instalar/voltar ──► uso diário
                    │                               ▲
                    └── testável já no Git Bash ────┘
5 (Claude Code, VS Code, git push, estabilidade) corre em paralelo a partir da fase 3
```

A fase 2 não depende da 1 e pode começar já, no Windows (Git Bash). A volta automática da fase 4 deve existir antes
de alguém usar o mOS como máquina principal de desenvolvimento.

## Primeiro passo concreto

`tools/stage-zig.ps1` com o roteiro da escada da fase 1 em `tools/testdata/zig-escada.sh`, rodado pelo
`stage-bash.ps1 -Script`. A saída mostra até qual degrau chegou e onde parou.
