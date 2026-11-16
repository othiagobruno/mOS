# Arquivos e volumes — plano da frente (dono: Vinicius)

Frente 6 do `TIME.md`: VFS completo, APFS leitura+escrita, HFS+ escrita, `.dmg` (UDIF/UDZO/ULFO/APFS) leitura e
escrita, instalador `.pkg` (xar, payload, scripts), xattr/ACL/flags, FSEvents, permissões, aliases/bookmarks e
quarentena. Este arquivo diz o que existe, o que falta e em que ordem entra. Cada fase só é marcada ✅ com teste
de host passando (o build do projeto fica para o fim, pela fila `build-final.sh`).

## Estado encontrado (15/nov/2026)

| Área | Onde | Como estava |
|---|---|---|
| VFS | `kernel/lib/vfs.c`, `vfs.h` | árvore em RAM; o disco do sistema (FAT32, `\mOS\`) é lido inteiro no boot (`fs/persist.c`). Sem inode, dono, flags, datas reais (só `mtime` em ms desde o boot), xattr. Links simbólicos = arquivo `#mOS-link:`. |
| stat | `proc/process.c` `fill_stat`, `libsystem/libc.c` | `mos_stat_t` com tamanho, tipo, mtime em s e modo. `st_ino` = hash do caminho, dono 501:20 fixo, as 4 datas iguais. O modo do `chmod` some no reboot. |
| xattr | `libsystem/foundationsys.c` | `getxattr`/`setxattr`/`listxattr`/`removexattr` stub (ENOATTR/ENOTSUP). `chflags`, `chown`, `utimes`, `futimes`, `utimensat` não fazem nada. |
| ACL | `libsystem/posixmore.c` | `acl_get_file` devolve ACL vazia; nada guarda ACE. |
| FSEvents | `libsystem/fsevents.c` | varredura com `lstat` a cada 15 ms em userland (`docs/fsevents.md`); kernel não avisa nada. |
| HFS+ | `fs/hfsplus.c`, `hfsplus_wr.c` | leitura; escrita com journal (`docs/volumes-graveis.md`). Falta criar xattr, hard links, links simbólicos, gravar modo/dono/flags/datas. |
| APFS | `fs/apfs.c` | só leitura, sem snapshots/clones/criptografia; sem escrita. |
| .dmg | `fs/dmg.c` | UDIF só leitura (raw, zero, ADC, zlib, bzip2, LZFSE, LZMA). Sem UDRW gravável, sparseimage/sparsebundle, criptografia, `hdiutil create/convert`. |
| .pkg | `apps/pkgextract.c`, `tools/pkgtest.c` | xar + Payload (cpio gzip/pbzx) + Bom; scripts NÃO rodam; sem recibos, `pkgutil`, `installer`. |
| Bookmarks/alias | `libsystem/sys_zerados.c` | símbolos zerados. |
| Quarentena | `libsystem/darwin.c`, `chrome.c` | só as constantes CF. |

## Fases

1. **Metadados do VFS** (este turno) — `vnode_t` ganha inode estável, dono/grupo, flags BSD (`UF_*`/`SF_*`),
   4 datas em ns (acesso, modificação, mudança, criação) e lista de xattrs. Syscall `SYS_FSMETA` (106): `statx`,
   get/set/list/remove xattr, chflags, chown, utimes/settimes, readdir com inode. `uchg`/`schg` (imutável) e
   `uappnd` respeitados no open/unlink/rename/chmod. Persistência no disco do sistema em `\mOS\.vfsmeta`
   (registro único com CRC, troca atômica `fat_replace`), então modo, dono, flags, datas e xattrs sobrevivem ao
   reboot. libSystem: stat/lstat/fstat reais, xattr, chflags/chown/utimes/futimes/utimensat/futimens,
   `copyfile` com xattrs (`COPYFILE_XATTR`). Teste: `tools/vfsmeta_test.c`.
2. **Permissões e ACL** — checagem de acesso no kernel (dono/grupo/outros, root, sticky em `/tmp`, setuid/setgid
   no exec junto com a Renata), `access`/`faccessat` reais, umask no kernel. ACL no formato `kauth_filesec`
   guardada no xattr `com.apple.system.Security`; `acl_*` completos no libSystem, `chmod +a`, `ls -le`.
3. **FSEvents e EVFILT_VNODE** — fila de eventos no VFS (criar, apagar, renomear com par, modificar, metadados,
   xattr, dono) com ids crescentes e journal por volume; `FSEventStream*` passa a ler a fila (latência real,
   `IgnoreSelf`, `UseExtendedData`, `HistoryDone`, eventos desde um id). Entrega para o `kevent` da Renata.
4. **getattrlist/setattrlist/getattrlistbulk/searchfs** completos (ATTR_CMN_*, ATTR_FILE_*, ATTR_DIR_*, ATTR_VOL_*),
   `fsctl`, `statfs` por volume, `clonefile`/`fclonefileat`, `exchangedata`, `renamex_np` (`RENAME_SWAP`,
   `RENAME_EXCL`), hard links reais (nlink), `F_FULLFSYNC`, `F_PREALLOCATE`.
5. **HFS+ completo** — xattr (árvore de atributos crescendo, inline e fork), hard links (arquivo e pasta),
   links simbólicos, modo/dono/flags/datas gravados, FinderInfo, resource fork, compressão decmpfs na escrita.
6. **APFS leitura completa e escrita** — snapshots, clones, xattrs, links, compressão, sealed volume; escrita com
   B-tree COW, spaceman, checkpoints, `fsck` próprio no `tools/volimg.c`.
7. **.dmg leitura e escrita** — UDRW gravável (o volume grava de volta no arquivo), UDSP (sparseimage) e
   sparsebundle, ULFO/ULMO/UDZO/UDBZ, APFS dentro do dmg, conferência de checksums, criptografia AES (senha),
   `hdiutil create/convert/attach/detach/info/verify/resize`.
8. **Instalador .pkg** — xar completo (TOC, checksums, assinatura conferida), Distribution (choices,
   `installation-check`/`volume-check` em JavaScript), scripts preinstall/postinstall rodando com o ambiente do
   Installer, recibos em `/var/db/receipts`, `pkgutil`, `installer -pkg -target`, Instalador.app com progresso.
9. **Bookmarks, aliases e quarentena** — `CFURLCreateBookmarkData`/`CFURLCreateByResolvingBookmarkData`
   (formato `book` do macOS), arquivos de alias, `NSURL` bookmark/`fileReferenceURL` (`/.file/id=`), security
   scoped bookmarks aceitos; `com.apple.quarantine` gravado nos downloads (Safari/curl via LS), lido pelo
   Launch Services (aviso de app baixado, com a Natalia), `xattr -d`.
10. **fd -> vnode com contagem de referência** (repassado pelo Henrique, `docs/kernel-auditoria.md`): hoje o fd
    guarda o caminho, o `SYS_READ` refaz `vfs_lookup` a cada chamada e o fileport do `machipc` guarda uma cópia do
    caminho que não acompanha o rename. Também: `tools/fatcat.c` não compila no host (faltam `blk_read`/`blk_write`).
11. **VFS sob demanda** — tirar o "disco inteiro na RAM": nós carregados por diretório, conteúdo pelo page cache
    (`fs/pcache.c`), com o Gustavo (memória). Hoje o limite é a RAM (14 GB no PC do Thiago).

## Andamento

| Fase | Estado |
|---|---|
| 1 Metadados do VFS | ✅ kernel + libSystem + teste de host (`tools/vfsmeta_test.c`, 78 verificações, ASan/UBSan limpos). Detalhes: `docs/vfs-metadados.md`. Falta no QEMU: só no build final. |
| 2 Permissões e ACL | ✅ kernel (`vfsperm.c`) + `acl.c` + testes de host (115 + 26). |
| 3–11 | a fazer |
