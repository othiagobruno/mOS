# Bookmarks, alias e quarentena

Fase 9 de `docs/arquivos-volumes-plano.md`.

## Bookmarks (`userland/libsystem/bookmark.c`)

- Formato `book` do macOS (o mesmo do `CFURLCreateBookmarkData` desde o 10.6): cabeçalho de 48 bytes, itens
  tipados (texto, dados, números, datas em double big-endian desde 2001, listas, URL) e TOC com as chaves da Apple:
  componentes do caminho (0x1004), inodes do caminho (0x1005), propriedades do arquivo (0x1010), criação (0x1040),
  volume (caminho, URL, nome, UUID, tamanho, propriedades, raiz), usuário e uid, opções e, com
  `NSURLBookmarkCreationWithSecurityScope`, a extensão de escopo (0xf080).
- **Resolver**: pelo caminho, conferindo o inode; movido ou renomeado → achado pelo inode (`fsgetpath`) e
  `isStale = YES`; outro arquivo no mesmo caminho → esse, com `isStale`; sumiu → `NSFileNoSuchFileError` (4).
  Os inodes do mOS são estáveis (vão no `.vfsmeta`), então um bookmark resolve depois do reboot.
- CF: `CFURLCreateBookmarkData`, `CFURLCreateByResolvingBookmarkData`, `CFURLCreateBookmarkDataFromFile`,
  `CFURLWriteBookmarkDataToFile`, `CFURLCreateResourcePropertiesForKeysFromBookmarkData`,
  `CFURLCreateResourcePropertyForKeyFromBookmarkData` (nome, caminho, volume, criação, inode, é pasta/arquivo/link/
  pacote/app/alias/oculto), `CFURLStart/StopAccessingSecurityScopedResource` (sem sandbox: sempre permitido),
  `CFURLCreateFileReferenceURL`, `CFURLCreateFilePathURL`, `CFURLIsFileReferenceURL`.
  `CFURLCreateBookmarkDataFromAliasRecord` devolve NULL (AliasRecord do Carbon não existe no mOS).
- NSURL: `bookmarkDataWithOptions:includingResourceValuesForKeys:relativeToURL:error:`,
  `+URLByResolvingBookmarkData:options:relativeToURL:bookmarkDataIsStale:error:`, `initByResolvingBookmarkData:…`,
  `+bookmarkDataWithContentsOfURL:error:`, `+writeBookmarkData:toURL:options:error:`,
  `+URLByResolvingAliasFileAtURL:options:error:`, `+resourceValuesForKeys:fromBookmarkData:`,
  `start/stopAccessingSecurityScopedResource`, `fileReferenceURL`, `filePathURL`, `isFileReferenceURL`.

## Alias do Finder e URL de referência

- Arquivo de alias = o bookmark no conteúdo + `com.apple.FinderInfo` com tipo `alis`, criador `MACS` e `kIsAlias`,
  como o Finder grava; `cx_alias_is` reconhece.
- `fileReferenceURL` = `file:///.file/id=<volume>.<inode>/`; qualquer caminho que comece por `/.file/id=` é
  resolvido no `cx_path` do libSystem (vale para open, stat e o resto).

## Quarentena (`userland/libsystem/quarantine.c`, `userland/cli-macos/xattr.c`)

- `com.apple.quarantine` é um xattr de verdade (fase 1): `"<flags>;<hora hex>;<agente>;<UUID>"`.
- libquarantine: `qtn_file_alloc/free/clone`, `init_with_path/fd/data`, `to_data`, `apply_to_path/fd`,
  `get/set_flags`, `get/set_timestamp`, `get/set_agent_name`, `get/set_agent_bundle_identifier`,
  `get/set_identifier`, `qtn_error`, `qtn_file_*_mount_point`, e `qtn_proc_*` (com `qtn_proc_apply_to_self`, os
  arquivos que o processo cria nascem em quarentena), com e sem o `_` na frente.
- `/usr/bin/xattr` com as opções do macOS (`-l -p -w -d -c -r -s -v -x`): o Homebrew e o usuário tiram a quarentena
  com `xattr -d com.apple.quarantine App.app`.
- Quem decide o aviso "baixado da Internet" ao abrir é o Launch Services/Gatekeeper (frente da Natalia); quem expõe
  `NSURLQuarantinePropertiesKey`/`NSURLIsAliasFileKey` no `resourceValuesForKeys:` é o NSURL (frente do Eduardo).

## Testes

`sh tools/attrlist_test.sh` (71 verificações; inclui criar/resolver bookmark, mover e renomear com `isStale`,
apagado, corrompido, alias ida e volta, URL de referência com caminho dentro, quarentena gravada e lida).
`zig cc -target x86_64-macos -fsyntax-only userland/cli-macos/xattr.c`.
