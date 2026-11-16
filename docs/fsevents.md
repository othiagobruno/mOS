# FSEvents

## Como funciona

O kernel registra cada mudança do VFS num journal (`kernel/lib/vfsevents.c`): id crescente, flags
`kFSEventStreamEventFlag*` da Apple, inode, pid de quem fez e caminho. O `FSEventStream` do libSystem
(`userland/libsystem/fsevents_core.inc`, `fse_kscan`) lê só o que veio depois do último id que viu
(`SYS_FSMETA` `MOS_FSM_EVENTS`/`MOS_FSM_EVENTID`): não varre mais a árvore.

| Mudança no VFS | Evento |
|---|---|
| criar arquivo/pasta | `ItemCreated` + tipo |
| escrever, truncar | `ItemModified` (escritas seguidas, ainda não lidas, juntam num evento) |
| apagar | `ItemRemoved`, um por item da árvore (acima de 2048, um `MustScanSubDirs` na raiz) |
| renomear/mover | dois `ItemRenamed` seguidos (caminho velho e novo, mesmo inode) |
| xattr | `ItemXattrMod`; `com.apple.FinderInfo` também `ItemFinderInfoMod` |
| chown | `ItemChangeOwner` + `ItemInodeMetaMod`; chmod, chflags, utimes, ACL: `ItemInodeMetaMod` |
| montar/ejetar volume | `Mount`/`Unmount` em `/Volumes/<nome>` (os itens do volume não viram eventos) |
| apagado com fd aberto (`/tmp/.orfaos`) | `ItemRemoved` |

- **Ids** continuam depois do reboot (o próximo id vai no `\mOS\.vfsmeta`). Um id de antes do boot, ou que o anel
  (8192 eventos) já perdeu, chega como `KernelDropped|MustScanSubDirs` na raiz; depois vem o que existe.
- **sinceWhen**: repete o journal desde o id e termina com um `HistoryDone`.
- **IgnoreSelf** pula os eventos do próprio processo; **MarkSelf** marca `OwnEvent`.
- **WatchRoot**: a raiz (ou uma pasta acima) apagada ou renomeada dá `RootChanged`.
- **UseExtendedData** (com `UseCFTypes`): CFArray de CFDictionary com `path` e `fileID`
  (`kFSEventStreamEventExtendedDataPathKey`, `kFSEventStreamEventExtendedFileIDKey`).
- Sem `FileEvents`, o caminho é a pasta e a flag leva `MustScanSubDirs` (o id é o do evento mais novo).
- Latência junta os eventos até o prazo; `NoDefer` e `FlushAsync` acordam o fio em até 15 ms; `FlushSync` lê na hora.
- Exclusões (`SetExclusionPaths`) valem para o caminho e o que estiver abaixo.

## O que continua de fora

- `ScheduleWithRunLoop` e `SetDispatchQueue` só guardam o ponteiro: o callback corre no fio do stream ou em quem chamou
  `Start`/`FlushSync` (o run loop e o Dispatch são das frentes do Eduardo e da Fernanda).
- `FSEventsCopyUUIDForDevice` devolve vazio e `FSEventsPurgeEventsForDeviceUpToEventId` não apaga nada (o journal é
  só da RAM; o que sobrevive ao reboot é o id).
- Mudanças feitas direto num volume de `/Volumes` por outro sistema não aparecem (só as que passam pelo VFS).

## Testes

- `sh tools/fsevents_kernel_test.sh`: o ramo do mOS sobre o VFS e o journal reais do kernel (23 verificações:
  FileEvents, rename, xattr, exclusão, IgnoreSelf, MarkSelf, histórico, lacuna, modo pasta, latência, WatchRoot).
- `gcc -std=c11 -O1 -DFSE_HOST tools/fseventstest.c -pthread`: o modo varredura no Linux (42).
- `tools/vfsmeta_test.c` cobre o journal no kernel (juntar, rename, árvore, mudo, lacuna, id no `.vfsmeta`).
