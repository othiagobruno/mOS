# Metadados do VFS: inode, dono, flags, datas e xattr

Fase 1 de `docs/arquivos-volumes-plano.md`. Todo `vnode_t` (`kernel/include/vfs.h`) tem agora:

| Campo | O que é |
|---|---|
| `ino` | inode único e estável: rename e mover não mudam; a raiz é 2; gravado no `.vfsmeta` |
| `uid`, `gid` | dono; nó novo recebe o `euid` de quem cria e o grupo da pasta-mãe (regra BSD); padrão 501:20 |
| `flags` | `st_flags` do BSD (`UF_IMMUTABLE`, `UF_APPEND`, `UF_HIDDEN`, `SF_*`...) |
| `atime_ns`, `mtime_ns`, `ctime_ns`, `btime_ns` | as 4 datas em ns desde 1970 (`vfs_now_ns`: RTC lido uma vez + contador do boot, sempre crescente) |
| `xattr` | lista ordenada de atributos estendidos (nome até 127 bytes, valor até 64 MiB, 128 MiB por nó) |

## Regras (as do xnu)

- `setxattr`: `XATTR_CREATE` com nome existente = `EEXIST`; `XATTR_REPLACE` sem ele = `ENOATTR`; `position` só no
  `com.apple.ResourceFork` (lido e gravado em pedaços); `com.apple.FinderInfo` tem 32 bytes (senão `ERANGE`) e,
  só com zeros, conta como ausente; o bit `kIsInvisible` dele liga `UF_HIDDEN`. `com.apple.system.*` só com root.
- `getxattr` com buffer pequeno = `ERANGE`; com buffer NULL devolve o tamanho. `listxattr` idem.
- `chflags`: só o dono (ou root); `SF_*` só root, e um `SF_IMMUTABLE`/`SF_APPEND` trava o arquivo para o usuário.
- `chown`: sem root só troca o grupo, para um grupo da conta (o grupo efetivo, staff 20, everyone 12, admin 80);
  sem root, trocar o dono tira setuid/setgid.
- Imutável (`uchg`/`schg`): open para escrita, truncar, gravar, chmod, chown, utimes, setxattr, apagar e renomear dão
  `EPERM`. Append-only (`uappnd`/`sappnd`): escrever só no fim e com `O_APPEND`; não apaga nem renomeia.
- chmod só pelo dono ou root (antes qualquer processo podia).
- Datas: escrita/truncar mudam mtime+ctime; criar/apagar/renomear um filho muda mtime+ctime da pasta; rename e
  qualquer metadado mudam o ctime. `utimes` não deixa a criação depois da modificação (como o HFS+/APFS).

## ABI: `SYS_FSMETA` (106)

`syscall.h`: `MOS_FSM_STATX`, `GETXATTR`, `SETXATTR`, `LISTXATTR`, `REMOVEXATTR`, `CHFLAGS`, `CHOWN`, `SETTIMES`,
`READDIR` (nome + inode + tipo DT_*). Alvo por fd ou caminho; o kernel não segue link no último componente (o
libSystem resolve antes, `cx_follow_path`). O núcleo é `vfs_fsmeta` em `kernel/lib/vfsmeta.c`; o `process.c` só
copia de/para o usuário. `mos_stat_t` não mudou de tamanho.

## libSystem

`userland/libsystem/fsmeta.c`: `getxattr`/`setxattr`/`listxattr`/`removexattr` e as `f*`, `XATTR_NOFOLLOW`;
`chown`/`lchown`/`fchown`/`fchownat`; `chflags`/`lchflags`/`fchflags`/`chflagsat`;
`utimes`/`lutimes`/`futimes`/`utime`/`utimensat`/`futimens` (`UTIME_NOW`, `UTIME_OMIT`); `cx_set_birthtime`;
`cx_copy_meta` (o `copyfile` com `COPYFILE_XATTR`/`COPYFILE_STAT`). `stat`/`lstat`/`fstat` (`libc.c`) preenchem
inode real, dono, flags, nlink, volume e as 4 datas com ns; o modo gravado pelo chmod/creat vale, sem ele continua o
padrão com o bit de execução pelo conteúdo. Os stubs antigos saíram de `foundationsys.c`, `vscode_libc.c`,
`chrome.c` e `posixmore.c`.

## Persistência: `\mOS\.vfsmeta`

`fs/persist.c` guarda num arquivo só (fora do VFS) os nós com metadados próprios (`v->meta`: criados ou mudados
depois do boot): caminho, tipo, modo, dono, flags, inode, as 4 datas e os xattrs, com CRC-32. Regravado inteiro no
`persist_sync` quando algo mudou, em `.vfsmeta.new` + `fat_replace` (queda = o antigo ou o novo). No boot é lido
antes da árvore (os inodes novos começam depois dos gravados) e aplicado depois dela. Corrompido = ignorado (log).
Volumes de `/Volumes` não entram: lá os metadados ficam na RAM até o driver gravá-los (HFS+, fase 5).

## Teste

```
gcc -O1 -g -DMOS_HOST_TEST -Itools/vfsmeta_host -Ikernel/include -Itools tools/vfsmeta_test.c \
    kernel/lib/vfs.c kernel/lib/vfsmeta.c kernel/proc/cred.c -o /tmp/vfsmeta_test && /tmp/vfsmeta_test
```

78 verificações (xattr, FinderInfo, resource fork com position, quarentena, flags, chown, datas, statx, nlink,
rename com o mesmo inode, readdir com tipo de link, `.vfsmeta` ida e volta com CRC e sem colisão de inode,
nenhum vazamento ao apagar), também com `-fsanitize=address,undefined`.

## Ainda não

- `d_ino` do `readdir` do libSystem continua o índice (o `MOS_FSM_READDIR` já dá o inode; troca na fase 4).
- `.vfsmeta` é regravado inteiro a cada mudança (registro incremental fica para quando pesar).
- Verificado só no host e com `clang -fsyntax-only` nos arquivos do kernel e do libSystem; no QEMU, no build final.
