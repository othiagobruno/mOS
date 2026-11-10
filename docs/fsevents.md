# FSEvents

O kernel não avisa quando um arquivo muda. `FSEventStreamStart` entrega o callback por uma varredura em userland (`userland/libsystem/fsevents.c`), com `lstat` e `readdir`. Este texto é o que falta do lado do kernel; o kernel não foi alterado.

## O que o VFS tem

`vnode_t` (`kernel/include/vfs.h`) guarda o nome, o tamanho, `mtime` em milissegundos desde o boot (0 se veio do disco de boot) e os ponteiros da árvore. Não guarda inode. Não há `EVFILT_VNODE`, inotify nem fila de eventos: uma escrita só atualiza o nó (`kernel/lib/vfs.c`, `kernel/proc/process.c`).

`fill_stat` exporta esse `mtime` em segundos (`boot + mtime / 1000`). O `lstat` do libSystem (`to_mac_stat` em `libc.c`) copia esses segundos, põe `st_dev = 0x1000002`, calcula `st_ino` com um hash do caminho e deixa `mtime_ns` em 0.

## O que o fluxo faz

Um fio (`fse_poll`) relê as raízes do fluxo. A diferença contra a foto anterior vira o callback da Apple:

| Mudança | Flags |
|---|---|
| arquivo novo | `ItemCreated` + `ItemIsFile` |
| pasta nova | `ItemCreated` + `ItemIsDir` |
| link novo | `ItemCreated` + `ItemIsSymlink` |
| tamanho ou mtime diferente | `ItemModified` + o tipo |
| modo ou inode diferente | `ItemInodeMetaMod` + o tipo |
| sumiu | `ItemRemoved` + o tipo |
| sumiu e apareceu com o mesmo `st_ino` e `st_dev` | `ItemRenamed` + o tipo, nos dois caminhos |

Com `kFSEventStreamCreateFlagFileEvents` o caminho é o do próprio item. Sem a flag, o caminho é o da pasta (ou da raiz, se ela for um arquivo) e a flag leva `MustScanSubDirs`. `UseCFTypes` entrega um `CFArray` de `CFString`; sem ela, `char **`. Os ponteiros valem só durante o callback.

`FlushSync` varre na hora, na thread de quem chamou. `FlushAsync` só pede a varredura ao fio. A latência junta as mudanças numa foto só: criar e apagar no mesmo intervalo some, porque a segunda foto já não tem o arquivo.

`tools/fseventstest.c` compila o mesmo núcleo no Linux (`-DFSE_HOST`), onde `lstat` tem inode e nanossegundo de verdade.

## O que continua de fora

- Não há notificação. O fio acorda no máximo a cada 15 ms (`NoDefer` ou `FlushAsync`) e relê a árvore. Quanto maior a árvore, mais tarde o callback corre.
- Dois writes no mesmo segundo, com o mesmo tamanho, não aparecem. O milissegundo está em `vnode_t.mtime` e se perde em `fill_stat`; `mtime_ns` fica 0.
- No mOS o `st_ino` muda quando o caminho muda, então um rename não forma o par `ItemRenamed`. No host o inode é estável e o teste cobre o par.
- Mais de 16384 entradas, ou mais de 32 níveis, emite uma vez `MustScanSubDirs|UserDropped` e guarda a última foto que coube. Não existe `KernelDropped` de uma fila do kernel.
- `IgnoreSelf`, `MarkSelf` e `UseExtendedData` não têm efeito. O vnode não diz quem escreveu e não há dicionário estendido.
- `ScheduleWithRunLoop` e `SetDispatchQueue` só guardam o ponteiro. O callback corre no fio da varredura, ou em quem chamou `Start` ou `FlushSync`. Não muda para a fila nem para o run loop.
- `FSEventsCopyUUIDForDevice` e `FSEventsGetLastEventIdForDeviceBeforeTime` devolvem vazio: não há journal. `FSEventsPurgeEventsForDeviceUpToEventId` devolve sucesso e não apaga nada. `FSEventStreamCreateRelativeToDevice` ignora o device (há um volume só) e cai no `Create`.
- A varredura não segue link simbólico para dentro. O link em si entra como `ItemIsSymlink`.
