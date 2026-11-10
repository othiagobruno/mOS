# MAP_SHARED e NX

Base desta branch: `origin/main` (o SHA entra no relatório do PR). O contrato de página do guest arm64 já está na main (`docs/pagesize-arm64.md`, PR #11). Esta mudança não reimplementa o arredondamento de 16 KiB nem o `PROT_NONE` que desmapeia a página guest. Ela liga o que aquele contrato deixou em aberto: a PTE não distinguia leitura, escrita e execução.

## O que o kernel passa a fazer

Página do kernel continua 4 KiB. `EFER.NXE` liga em `paging_init`, antes do `smp_init`, porque os APs copiam o `EFER` do BSP (`kernel/arch/smp.c`, sem edição aqui). O mapa de identidade é de páginas de 2 MiB sem o bit 63, então o kernel, o boot e o trampolim seguem executáveis.

Bits novos da PTE de usuário, ao lado dos que a main já tem (`PTE_COW` bit 9, `PTE_JWP` bit 10, do fork ARM64):

| Bit | Nome | Efeito |
|---|---|---|
| 11 | `PTE_SHARED` | MAP_SHARED. O `fork` não transforma a página em COW. |
| 63 | `PTE_NX` | Sem execução. A busca de instrução falta; leitura e escrita seguem o bit RW. |

`SYS_MMAP`:

- `b = 5` — arquivo `MAP_SHARED`. `c` = fd, `r10` = deslocamento alinhado a 4 KiB, `r8` = prot (1 leitura, 2 escrita, 4 execução). Pipe, terminal e `/dev/null` devolvem `EINVAL` (o libSystem cai na cópia antiga). `EBADF` e `EACCES` continuam erro.
- `b = 6` — `MAP_SHARED|MAP_ANON`. O `fork` enxerga a mesma página.
- `b = 7` — prot em `[c, c+len)`. Sem o bit 4 a página fica NX. Não desmapeia.

`SYS_MUNMAP` com `c = MOS_MUNMAP_SYNC` (`'SYNC'`) grava o trecho no arquivo e não desmapeia (`msync`). O `munmap` comum grava o trecho antes de soltar as páginas. A saída do processo grava só o que nenhum outro processo ainda mapeia, para o `msync` parcial do teste não ver escrita que o filho ainda não pediu para ir ao arquivo. A página continua na RAM do processo que ficou.

A página compartilhada de arquivo não é o buffer do vnode. A primeira falta copia para uma página do cache; as outras faltas mapeiam essa página. `file_store_at` grava de volta. `MAP_PRIVATE` (`b = 4`) não mudou: continua cópia, e `MADV_DONTNEED` não descarta MAP_SHARED.

Página anônima criada com `b = 3` e sem `b = 7` continua RWX. Texto ELF, pilha e JIT que não tiram `PROT_EXEC` seguem executando. `mprotect` e o `mmap` do libSystem com prot sem execução é que ligam o NX.

`PTE_JWP` (W^X do tradutor) não foi redefinido. `mprotect` com escrita desfaz o JWP; sem escrita, o JWP fica. Página COW não ganha RW no `mprotect` (a escrita ainda copia). `cow_break` e o fork de página JWP preservam o NX. A repetição de falta espúria não repete busca de instrução em página NX — senão a falta não acaba.

No x86 a página presente é legível. Não há `PROT_NONE`, só-escrita nem só-execução na PTE. `PROT_NONE` no guest arm64 continua sendo o desmapeamento descrito em `docs/pagesize-arm64.md`.

## libSystem

`userland/libsystem/extra.c`, por cima do `mmap`/`mprotect` de 16 KiB que a main já tem:

- `MAP_SHARED` de arquivo tenta `b = 5`, com `a64_align_cursor` e `a64_reclaim_slop` no arm64. `EINVAL` volta para a cópia com `shm_track` (pipe, terminal, `/dev/null`).
- `MAP_SHARED|MAP_ANON` usa `b = 6`, no mesmo alinhamento de 16 KiB.
- `mprotect` com prot diferente de zero chama `b = 7` no span de `a64_span` (a página guest inteira). `PROT_NONE` continua só desmapeando. O `a64_sys` zera o 5º argumento, então o prot vai por `SYS5` (`x4`).
- `msync` faz o `shm_flush` antigo e o `MOS_MUNMAP_SYNC` no mesmo span.

`macho.c` não foi editado. O carregador continua registrando a proteção arredondada à página guest sem mudar a PTE, como no commit `7d5e7c1`. Quem muda a PTE é o `mprotect` / `b = 7`.

## Paginação para disco

Não implementada. O que falta, quando for a hora:

1. Um dispositivo ou arquivo de swap e um alocador de slots (página de 4 KiB).
2. Escolha da vítima sob pressão (relógio ou LRU). Não entra memória do kernel, do boot, nem página com `mlock`.
3. Uma PTE com P=0 que não se confunda com demand-zero, com arquivo `MAP_PRIVATE` nem com `MAP_SHARED`. O deslocamento no swap tem de caber nos bits livres, e a falta distingue os quatro casos.
4. Página anônima suja vai para o swap; a falta lê de volta. `MAP_SHARED` de arquivo já tem para onde ir (o arquivo); não precisa de slot, mas não pode ser descartada enquanto outro mapeamento a usa.
5. COW: ou a página fica privada antes de ir para o swap, ou o slot conta donos como o `pref[]`. `PTE_SHARED` e `PTE_JWP` não podem ser esquecidos na volta.
6. Reuso do slot só depois que a página saiu de todas as PTEs. Crash no meio de uma escrita não pode devolver o slot velho a outra página.

O teste desta branch não exercita swap. Não há imagem de disco de swap.

`MAP_FIXED` de arquivo (b = 8 privado, b = 9 compartilhado) usa o mesmo `lazy_add_file` / `lazy_add_shared_file` depois de `lazy_sync` e de furar a faixa antiga. Não é outro cache.
