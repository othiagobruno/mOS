# File Manager do Carbon (FSRef) — `userland/libsystem/filemgr.c`

APIs obsoletas do CoreServices/CarbonCore que apps antigos, plug-ins e instaladores ainda ligam. Teste de host:
`sh tools/filemgr_test.sh` (91 verificações, sobre o VFS real do kernel, com ASan/UBSan).

| Grupo | Funções |
|---|---|
| Referência | `FSPathMakeRef`, `FSPathMakeRefWithOptions` (`kFSPathMakeRefDoNotFollowLeafSymlink`), `FSRefMakePath`, `FSIsFSRefValid`, `FSCompareFSRefs`, `CFURLGetFSRef`, `CFURLCreateFromFSRef` |
| Catálogo | `FSGetCatalogInfo` (todos os campos do bitmap, nome, pai), `FSSetCatalogInfo` (datas, permissões/dono, FinderInfo, trava) |
| Objetos | `FSMakeFSRefUnicode`, `FSCreateFileUnicode`, `FSCreateDirectoryUnicode`, `FSDeleteObject`, `FSRenameUnicode`, `FSMoveObject`, `FSExchangeObjects` |
| Forks | `FSGetDataForkName`, `FSGetResourceForkName`, `FSOpenFork`, `FSOpenForkReadOnly`, `FSReadFork`, `FSWriteFork`, `FSGet/SetForkPosition`, `FSGet/SetForkSize`, `FSAllocateFork`, `FSFlushFork`, `FSCloseFork`, `FSCreateFork`, `FSDeleteFork`, `FSIterateForks` |
| Iteradores | `FSOpenIterator` (plano e `kFSIterateSubtree`), `FSGetCatalogInfoBulk`, `FSCloseIterator` |
| Folder Manager | `FSFindFolder`, `FSFindFolderExtended` (domínios usuário/local/sistema; cria a pasta quando pedido) |

Como funciona:

- **FSRef**: `"mOSf"`, fsid do volume, inode de 64 bits. O inode do mOS é único no VFS inteiro e não muda com
  rename/move, então o FSRef acompanha o arquivo como no HFS+; o caminho sai do `fsgetpath` em cada uso. Objeto
  apagado: `fnfErr`.
- **Nomes**: UTF-16 com pares substitutos; o `/` do Carbon é o `:` do POSIX (como o HFS+). A raiz de um volume
  devolve o nome do volume.
- **Resource fork** = xattr `com.apple.ResourceFork` (em memória enquanto o fork está aberto, gravado no flush e
  no close); **FinderInfo** = `com.apple.FinderInfo` (32 bytes). Trava do Finder = `UF_IMMUTABLE` (`FSDeleteObject`
  devolve `fLckdErr`).
- **Layouts x86_64** (`#pragma pack(2)`): `UTCDateTime` 8 bytes (segundos desde 1904 + fração 1/65536),
  `FSPermissionInfo` 20, `FSCatalogInfo` 148, `HFSUniStr255` 512, `FSIORefNum` int. `FSSpec` não existe no
  x86_64 moderno: os parâmetros são zerados.
- Volume de boot = `-100`; `nodeID` da raiz = 2, pai da raiz = 1 (`fsRtDirID`/`fsRtParID`).
