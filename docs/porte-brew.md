# Porte do Homebrew para o mOS

Objetivo: `brew install <fórmula>` funcionando sem modificações no mOS (x86_64, prefixo Intel `/usr/local`), com bottles
oficiais `*.sonoma.bottle.tar.gz` (Intel). Este documento é o inventário (6 nov. 2026) e a ordem de ataque.

## O que o brew chama de fato

| Etapa | Programa | Origem no macOS real | Situação no mOS |
|---|---|---|---|
| `brew` (entrada) | `/bin/bash` 3.2+ (`brew.sh`) | sistema | **falta**. Só existe `userland/sh.c` (próprio, não POSIX) |
| utilitários do `brew.sh` | `uname`, `sed`, `grep`, `awk`, `tr`, `cut`, `readlink`, `dirname`, `mkdir`, `chmod`, `ln`, `rm`, `cat`, `sort`, `head` | `/usr/bin`, `/bin` | **falta** (só `wc.c`, `seq.c`) |
| Ruby | `portable-ruby` 3.4 (Mach-O x86_64, baixado em `Library/Homebrew/vendor`) | ghcr.io | **não testado**. Precisa de dyld, libSystem, `fork`/`exec`, sinais, threads |
| atualização | `git` (fetch do `Homebrew/brew` e do tap via API JSON) | Xcode CLT | **falta**. Com `HOMEBREW_NO_INSTALL_FROM_API` desligado, o git só é usado no `brew update` |
| download | `curl` (`/usr/bin/curl`, HTTPS, redirects, ghcr.io com token anônimo) | sistema | **falta** binário. O kernel já tem TCP/DNS (`SYS_NET`) e há TLS em `build-tls` |
| extração | `tar` + gzip (bottles), `unzip`, `xz` | sistema | **falta** |
| relocação | `install_name_tool`, `codesign` (ad-hoc), `otool` | Xcode CLT | **falta**. O brew reescreve `@@HOMEBREW_PREFIX@@` nos Mach-O; dá para fazer em Ruby puro com `HOMEBREW_RELOCATE_BUILD_PREFIX`, mas `codesign` é chamado sempre |
| sistema | `sw_vers`, `sysctl hw.*`, `/usr/bin/xcode-select`, `pkgutil` | sistema | **falta**. O brew lê a versão do macOS daqui para escolher o bottle |

## Lacunas do kernel e do libSystem

Já existem (`kernel/include/syscall.h`): `fork` com COW, `pipe`, `dup2`, `chdir`/`getcwd`, `rename`, sinais
(`sigaction`/`kill`), threads, `SYS_SPAWNVEC` (argv+envp), `SYS_WAITANY`, TCP/DNS.

Feito (7 nov. 2026, testado no QEMU: `teste` nativo + `posixteste` Mach-O em `userland/tests-posix`):

- **`SYS_EXECVE` (45)**: troca a imagem no mesmo pid. Mantém fds (menos `O_CLOEXEC`), sessão, pasta e portas Mach.
  Zera tratadores de sinal, mata as outras threads e limpa GS/FPU. Mach-O e ELF. Uma falha volta erro sem estragar o processo.
- O kernel segue os links `#mOS-link:` e `#!interpretador [arg]` no `execve` e no `posix_spawn` (`exec_resolve`).
- **`SYS_WAITPID` (46)**: status POSIX (`WIFEXITED`/`WEXITSTATUS`, sinal em 0x7F), pid -1, `WNOHANG`, `ECHILD`.
- libSystem: `execve`/`execv`/`execvp`/`execvpe` (busca no `PATH`), `waitpid`/`wait`/`wait3`/`wait4`,
  `dup`/`dup2` reais. `lstat` vê `S_IFLNK`, `stat` segue links e põe 0755 em Mach-O/ELF/`#!`.

### bash rodando (7 nov. 2026)

O bash 5.3.9 do Homebrew roda no mOS com o readline 8.3.3, o ncurses 6.6 e o libintl do gettext 1.0. São bottles
Intel `sonoma` do ghcr.io, sem modificação. A versão atual da fórmula (5.3.20) não tem mais bottle Intel para macOS:
use as tags antigas (`ghcr.io/v2/homebrew/core/<f>/manifests/<versão>`, entrada `<versão>.sonoma`).
`tools/stage-bash.ps1` monta a imagem e roda `tools/testdata/bash-teste.sh` no boot. Funcionam: aritmética, laços,
funções, `$(...)`, `[[ =~ ]]` com `BASH_REMATCH`, `case` com glob, arrays, comando externo com `$?`,
`>`/`>>`/`<`, pipeline com programa externo. Saída `0`.

- Loader (`kernel/compat/macho.c`): carrega dylibs de caminho absoluto fora do sistema a partir do disco.
  Troca `@@HOMEBREW_PREFIX@@` por `/usr/local` e `@@HOMEBREW_CELLAR@@` por `/usr/local/Cellar`.
  `/usr/lib/libncurses.5.4.dylib`, que o readline pede, vai para `/usr/local/opt/ncurses/lib/libncursesw.6.dylib`.
- libSystem (`posixmore.c`, `sigjmp.S`): regex POSIX (BRE/ERE com grupos e retrocesso), `fnmatch`, `tsearch`,
  funções de caracteres largos, locale C/UTF-8, `sigsetjmp`/`setjmp`, usuários/grupos, `pselect`, `ttyname` etc.
  O machocheck dá 0 ausentes para o bash e as quatro dylibs.
- **No Terminal** (comando `bash [args]` do msh, `kernel/apps/terminal.c`): abre `/usr/local/bin/bash --noediting +m -i`
  na sessão de tty. O Terminal ainda é de linha (não emula VT100), então a edição e o histórico são os do Terminal e o
  readline fica desligado. A saída passa por um filtro que tira `\r`, sequências ESC e aplica o backspace. Processos com
  sessão de tty recebem `TERM=xterm-256color` (isatty verdadeiro) e o `PATH` padrão começa por `/usr/local/bin`.
  Testado no QEMU: `$BASH_VERSION`, `$((x*2))`, `cd … && pwd`, `[[ -t 0 ]]`, função multilinha com prompt `>`, `exit`.
- Falta para o readline de verdade: emulação VT100 no Terminal e tty em modo cru (termios `ICANON`/`ECHO` no kernel).

### coreutils e ruby rodando (7 nov. 2026)

`tools/stage-bash.ps1 -Script tools/testdata/brew-ferramentas.sh` instala e testa no boot, com 40/40 casos passando no QEMU:

- **coreutils 9.11** (+ gmp 6.3.0): os 118 binários vão para `/usr/local/bin` como `gX`, com `X -> gX` (o `gnubin`).
  Testados: ls/ls -l, uname, tr, sort, wc, head, tail, cp, mv, rm, ln -s, readlink, sha256sum, md5sum,
  basename/dirname, seq, date, expr, factor (gmp), du, env -i, id, test.
- **portable-ruby 3.4.5** (o mesmo que o `brew` baixa; build `el_capitan` x86_64) com zlib 1.3.2 e libffi 3.8.0:
  `-v`, puts, String, JSON, File, Dir, `system`, crase, File.symlink/readlink, Zlib, Thread, Fiber, Pathname.
- Correções que isso exigiu:
  - `FILE->_flags` agora tem `__SEOF`/`__SERR`. O gnulib usa os macros `feof_unlocked`/`ferror_unlocked` do macOS,
    e sem isso o `sort` lia para sempre. O `FILE` também ganhou o resto dos 152 bytes do `__sFILE`, zerado.
  - `SYS_FDPATH` (47) dá o caminho de um fd. Com ele: `fcntl(F_GETPATH)`, `fchdir`, `fdopendir`, `dirfd` real e
    `openat`/`fstatat`/`unlinkat`/`renameat`/`readlinkat`/`symlinkat` relativos a um dirfd (fts do `du`).
  - `getattrlist`/`fgetattrlist` respondem `ATTR_VOL_CAPABILITIES`: o volume preserva maiúsculas e não as
    diferencia. O `Dir.glob` do rubygems parava com ENOTSUP.
  - `/usr/lib/libz.1.dylib` e `/usr/lib/libffi.dylib` vão para as fórmulas do Homebrew, como o ncurses.
  - Os prints `[thread]` do libSystem só aparecem com `CX_TRACE_THREADS=1`.
- `tools/machotest.c`: `MACHOTEST_WATCHDOG=<ms>` congela o programa travado e mostra o rip, a pilha e a última
  syscall. O harness ainda não emula `pipe` nem `SYS_FDPATH`.
- Pendente: o `du` informa 0 para arquivos de verdade (falta conferir `st_blocks`). Também não existem sed,
  grep, awk, tar, gzip, curl nem git no mOS.

### sed, grep, tar, gzip, xz, curl e git (7 nov. 2026)

`tools/fetch-bottles.sh <fórmulas>` baixa os bottles Intel (`.sonoma`, ou `.all` para ca-certificates) com as
dependências de execução tiradas do `sh.brew.tab` do manifesto. `tools/stage-bash.ps1 -Script tools/testdata/brew-rede.sh`
instala tudo em `/usr/local/opt/<f>` (git-core vira links para o `git`: 560 MB → 4 MB) e cria `/bin/sh`, `/bin/bash`
e `/usr/bin/env`. Os bottles Intel já usam o prefixo `/usr/local`, então não é preciso relocar texto.

No QEMU, 20/20 casos locais passam: GNU sed 4.10, grep 3.12 (com `-P`/pcre2), tar 1.35 (`czf`/`xzf`/`cJf`/`tJf`),
gzip 1.14 e xz 5.8.3. O `curl --version` lista OpenSSL 3.6.3, zlib, brotli, zstd, nghttp2 e libssh2.

Correções desta etapa:
- Kernel: `/dev/null` (o git não abre sem ele). `mtime` real (o gzip recusava 1970). `kill(pid, 0)`.
  `fd_copy` pegava a referência do pipe depois de fechar o destino (kfree duplo no `dup2` do bash).
  Threads mortas esperando num pipe saem do `rwait`/`wwait`. `trim_zombies` não descarta mais o zumbi que o pai
  ainda vai recolher (o `tar` perdia o `gzip` filho: "Cannot waitpid").
- Loader: `@@HOMEBREW_CELLAR@@/<f>/<v>/X` cai em `/usr/local/opt/<f>/X`. Aliases de `/usr/lib` para libcurl e
  libexpat. `libresolv`, `libicucore`, `LDAP.framework`, `Kerberos.framework` e `GSS.framework` são aceitos.
- libSystem: `st_ino` por caminho e `st_dev` (o grep e o tar achavam que liam a própria saída).
  `fstat` de 0-2 redirecionados mostra o pipe ou arquivo real. Sinais de verdade: `sigaction`/`signal`/`kill`/
  `raise` com tradução de números (macOS ↔ kernel) e um restorer. `alarm`/`setitimer` por thread auxiliar.
  Também `glob`, `execlp`, `stpncpy`, `rpmatch`, `getopt`, `openlog`, `uidna_*` (ASCII) e stubs de LDAP.

Corrupção de heap resolvida (7 nov. 2026); os 28 casos locais passam, incluindo `git init/add/commit/log/status`:
- **Causa**: `process_t old = {0}` na pilha do kernel, dentro do `execve`. Com `PROC_MAX_FDS = 256`, a struct tem
  ~73 KB e a pilha do kernel 32 KiB, então zerar a struct escrevia zeros abaixo da pilha, em cima de blocos do heap
  (magic 0, "kfree: ponteiro inválido", pilhas "já liberadas" no `reap` e falta de página depois). Agora o espaço
  antigo é trocado com `np`, que está no heap. **Nunca pôr `process_t` na pilha do kernel.**
- Os caminhos das dependências absolutas do loader (`link_abs_dep`) ficam no heap: o `link_add` é recursivo pela
  cadeia de dylibs.
- **git add (segfault)**: o clang da Apple troca `memset(p, 0, n)` por `__bzero(p, n)` e usa o retorno como se fosse
  o do memset. O `__bzero` do libSystem era `void`, e o `rep stosb` inlinado deixava `rax = 0`, então o
  `mem_pool_calloc` do git devolvia NULL. Agora `bzero`/`__bzero` devolvem o destino.
- `rename` substitui o destino (POSIX). O git grava `X.lock` e renomeia por cima.
- Ferramentas de depuração:
  - O pânico mostra a base do kernel, os registradores e os endereços do kernel na pilha.
  - `kfree` inválido mostra o magic e o chamador. O `reap` avisa quando uma thread já tem a pilha liberada.
  - `kheap_check(onde)` (mm/memory.c) e `heap_probe` (proc/process.c) conferem as listas do heap.
  - `tools/kernel-map.ps1` compila o kernel com PDB e traduz `base+0x…` em função:linha (dbghelp).
  - `tools/machosym.c` traduz endereços de um Mach-O pela LC_SYMTAB.
  - O harness tem `/dev/null` e `MACHOTEST_WATCHDOG`.

### Rede: curl https, git clone (7 nov. 2026)

A pilha TCP do kernel estava certa: o `curl` embutido no Terminal baixava HTTPS. Os travamentos estavam no
libSystem. Com isto, 34/34 em `tools/testdata/brew-rede.sh`, incluindo curl HTTPS (API do Homebrew, HTTP/2 com o
GitHub), `git ls-remote` e `git clone --depth 1` por HTTPS:
- **O_NONBLOCK em pipes** (`fcntl F_SETFL`, `FIONBIO`, `open`): a leitura de pipe vazio vira EAGAIN. O curl drena o
  par de "despertar" (`socketpair` = pipe) até EAGAIN, e sem isso a segunda leitura bloqueava.
- **`inet_pton(AF_INET6)`** devolvia -1, e o curl tratava todo nome de host como IPv6 (o TLS falhava na verificação
  do nome). Agora há IPv6 de verdade em `inet_pton`/`inet_ntop`.
- **FD_CLOEXEC** (`SYS_FDFLAGS` 50, `F_GETFD`/`F_SETFD`, `FIOCLEX`). O git espera EOF no pipe de aviso do exec do filho.
  O `posix_spawn` não passa fds close-on-exec, e o `dup2` limpa a flag.
- **Sockets em 768–831**, abaixo do `FD_SETSIZE`: o `select`/`fd_set` do git não chegava a 0x5000.

### brew install de ponta a ponta (7 nov. 2026)

`tools/stage-bash.ps1 -Script tools/testdata/brew-install.sh` seguido de `tools/stage-brew.ps1`. Este monta
`/usr/local/Homebrew` (clone do Homebrew/brew com `core.autocrlf=false`) e o portable-ruby **4.0.9** (Intel `big_sur`,
sha256 tirado do próprio repositório) com `current` como link. Também cria `SystemVersion.plist` (macOS 14.7.6),
`sw_vers`, `sysctl`, `pkgutil` e `xcode-select` de mentira, e as "Command Line Tools" de mentira: um `clang` que só
informa a versão e um SDK só com o `SDKSettings.json` (o brew no macOS 14 Intel, Tier 3, as exige mesmo para bottles).
Por fim, links em `/usr/bin` e `/bin`. No QEMU (WHPX): `brew --version`, `brew config`, `brew install tree`,
`brew install jq` (com a dependência `oniguruma`) e os programas rodando, `brew list` e `brew uninstall`, tudo com
código 0. Só `tree` e `jq` (e as dependências delas) têm bottle `.sonoma` na versão atual: ~3.800 fórmulas ainda têm.

O que foi preciso:
- **Links de pasta no VFS** (`kernel/lib/vfs.c`): o walk segue links `#mOS-link:` nos componentes do meio.
  `vfs_lookup_dir` também segue no último (rename para dentro de `opt/<f>`). O `chdir`/`opendir` do libSystem segue o
  link final, e o `realpath` resolve link por componente (o brew compara realpaths).
- `/dev/fd/N` e `/dev/std*` no `open` do kernel (o bash usa `< <(…)`).
- **fork herda o GS** (TLS) da thread que chamou. O `IO.popen("-")` do ruby caía logo depois do fork.
- **unlink/rmdir POSIX**: o `SYS_UNLINK` apaga pastas inteiras. O brew poda pastas com `rmdir` e chegou a apagar o
  próprio Homebrew. O recursivo explícito é `cx_remove_tree` (NSFileManager).
- `*at()` relativos a dirfd: `mkdirat`, `utimensat`, `fchmodat`, `faccessat`, `linkat` (GNU tar).
- `getattrlist` com `ATTR_CMN_OBJTAG`/`ATTR_DIR_ENTRYCOUNT` (`Dir.empty?` do ruby), `getgrgid`/`getgrnam`,
  `ptsname_r`, `_dyld_shared_cache_contains_path` (`/usr/lib` e `/System/Library` contam como shared cache).
- Os symlinks do repositório do brew (modo 120000) viram links do mOS no stage.

Restos conhecidos: o `brew doctor` avisa que o sistema de arquivos diferencia maiúsculas e que as CLT "não suportam o
macOS 14". Nenhum dos dois atrapalha a instalação de bottles. Casks, `brew update` (git do tap) e fórmulas compiladas
do código-fonte (sem clang no mOS) ficam para depois.

Faltam, na ordem em que o bash e o ruby tropeçam:

1. **Grupos de processo e `SIGCHLD`** entregue ao pai (`setpgid`, `tcsetpgrp`), para o controle de jobs do bash.
2. **Symlinks no VFS**: hoje são arquivos marcados no libSystem. Funciona para o brew, mas `rename`/`unlink` de pasta
   via link e `realpath` ainda não sabem deles.
3. **Permissões gravadas**: `chmod`/`fchmod`/`umask`/`utimes` não persistem (o vnode não tem modo). O bit de execução é deduzido do conteúdo.
4. **`access(X_OK)`** ainda só confere se o arquivo existe.
5. **Descritores**: `fcntl` (`F_GETFL`, `FD_CLOEXEC`, `F_DUPFD`), `O_APPEND`/`O_EXCL`/`O_CLOEXEC`, `flock`, `select`/`poll` sobre pipes.
6. **Terminal**: `isatty`, `tcgetattr`, `ioctl(TIOCGWINSZ)` no console (o brew mede a largura para as barras).
7. **Ambiente de usuário**: `getpwuid`, `getuid`/`geteuid` estáveis, `$HOME`, `/etc/paths`.
8. **`sysctl`/`uname`** coerentes com um macOS 14 Intel (o brew recusa macOS desconhecido).

## Ordem de ataque

1. **Medir**: baixar `portable-ruby`, o `bash` e os utilitários de um macOS Intel (ou os bottles `bash`, `coreutils`, `gnu-sed`,
   `grep`, `gawk`, `gnu-tar`, `xz`, `curl`, `git` do próprio Homebrew), rodar `tools/machocheck.c` em todos e registrar a
   tabela de ausentes aqui, como em [compat-apps.md](compat-apps.md).
2. **Kernel**: `execve`, `waitpid` POSIX, symlinks, `chmod`/modos. Teste: `userland/testes/` com fork+exec+wait e `ln -s`.
3. **bash**: rodar o `bash` Mach-O no harness até `bash -c 'echo $((1+1)); ls | wc -l'` passar. Depois coreutils/sed/grep/awk.
4. **ruby**: `portable-ruby -e 'puts RUBY_VERSION; system("true"); File.symlink(...)'`, depois `require "openssl"`, `"socket"`.
5. **rede**: `curl -L https://formulae.brew.sh/api/formula.jws.json` e um download de ghcr.io.
6. **tar/xz/gzip** e o comando `codesign`/`install_name_tool` (stubs que reescrevem load commands, sem assinatura real).
7. **`sw_vers`/`sysctl`/`xcode-select`** devolvendo macOS 14.x Intel com CLT "instalado".
8. **Ponta a ponta**: instalar o brew em `/usr/local/Homebrew`, `brew --version`, `brew install hello`, `brew install wget`,
   `brew upgrade`, `brew uninstall`, `brew doctor` sem erros. Fórmulas a partir do código-fonte (que precisam de `clang`) ficam
   fora do escopo inicial: só bottles.

## Critério de "100%"

- `brew doctor` limpo, `brew update` com git, `brew install`/`upgrade`/`uninstall`/`cleanup` de bottles.
- Casks ficam para depois: dependem de `hdiutil`/montagem de DMG e do `/Applications`.
