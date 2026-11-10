# Página de 16 KiB do processo arm64

O kernel do mOS pagina tudo em **4 KiB** (PTE, `SYS_MMAP`, `SYS_MUNMAP`, copy-on-write). Um processo **x86_64** continua vendo 4 KiB em todas as APIs. Um processo **arm64 traduzido** vê **16 KiB**, que é o que o dyld, o malloc e o V8/PartitionAlloc de um app macOS arm64 usam para alinhar `mmap`, `mprotect`, guard pages e segmentos Mach-O.

Isto não é um kernel ARM64 e não é execução nativa. O tradutor (`userland/a64`, `userland/a64rt`) continua em x86_64. A granularidade de 16 KiB mora no libSystem arm64 e no carregador, e vira várias páginas de 4 KiB no kernel.

## Onde cada valor aparece

| Quem pergunta | x86_64 | arm64 traduzido |
|---|---|---|
| `hw.pagesize` (sysctl por nome, 8 bytes) | 4096 (`darwin.c`) | 16384 (`A64_GUEST_PAGE`) |
| `hw.pagesize32` e o MIB `{CTL_HW, HW_PAGESIZE}` = `{6, 7}` (4 bytes) | 4096 | 16384 |
| `vm.pagesize` (4 bytes) | não está na tabela x86 | 16384 |
| `vm_page_size`, `vm_page_mask`, `vm_kernel_page_size`, `vm_kernel_page_mask` | 4096 / 4095 | 16384 / 16383 |
| `vm_page_shift`, `vm_kernel_page_shift` | 12 | 14 |
| `getpagesize`, `sysconf(_SC_PAGESIZE)` (`_SC_PAGESIZE` = 29) | `vm_page_size` | 16384 |
| `host_page_size`, `task_info` `TASK_VM_INFO.page_size` | 4096 | 16384 |

Não existe sysctl de `hw.pagesize` no kernel: a tabela é do libSystem. Não existe commpage arm64 (os endereços baixos não são memória de usuário). O dyld da Apple não carrega o processo; quem mapeia os segmentos é `kernel/compat/macho.c`.

A constante única é `A64_GUEST_PAGE` em `userland/a64rt/a64rt_abi.h`. O libSystem arm64 inclui `userland/libsystem/a64page.h`, que só entra no build `aarch64`.

## O que o guest arm64 faz com 16 KiB

`mmap`, `munmap`, `mprotect` e `madvise` do libSystem arm64 falam com o kernel em páginas de 4 KiB, mas só em páginas guest inteiras.

- `mmap` com comprimento 0 devolve `EINVAL`. `MAP_FIXED` com endereço fora de 16 KiB devolve `EINVAL`. Arquivo com deslocamento fora de 16 KiB devolve `EINVAL` (`MAP_ANON` ignora o deslocamento). O comprimento sobe até fechar a página guest.
- `munmap` / `mprotect` / `madvise` (`MADV_DONTNEED`, `MADV_FREE`, `MADV_FREE_REUSABLE`): endereço fora de 16 KiB é `EINVAL`; comprimento 0 é sucesso; o comprimento sobe até a página guest.
- `PROT_NONE` na arena de `mmap` (`USER_MMAP_BASE` para cima) **desmapeia** as páginas de 4 KiB da página guest (`MOS_MUNMAP_KEEP` no `mprotect`, para a faixa continuar do processo). É o que faz uma guarda faltar de verdade. A PTE do mOS não distingue leitura de escrita de execução.
- `MAP_FIXED|MAP_ANON` troca as páginas guest inteiras. Com `PROT_NONE`, o trecho fica sem páginas.
- `mach_vm_deallocate` / `vm_deallocate` seguem o Mach, não o POSIX: o início desce até a página guest e o fim sobe. `mach_vm_protect` com endereço desalinhado devolve `KERN_INVALID_ADDRESS` e o tamanho sobe até a página guest.
- `mach_vm_region` numa faixa que o kernel responde como uma página sintética de 4 KiB com proteção 7 (nada registrado) vira a página guest de 16 KiB que contém o endereço. Faixa registrada passa como está. Isto só é seguro porque o libSystem e o carregador arm64 registram múltiplos de 16 KiB.

O `mmap_next` do processo é um cursor só, em passos de 4 KiB, e o `rt_alloc` do tradutor também anda nele. Cada `mmap` novo do guest realinha o cursor a 16 KiB (uma sonda de 28 KiB, abaixo do alinhamento de 2 MiB do kernel) e devolve os 4 KiB que o kernel soma depois da faixa. Um buraco de menos de 16 KiB abaixo do cursor não volta para o próximo `mmap`; só `MAP_FIXED` o reaproveita. Faixas a partir de 2 MiB continuam com o alinhamento de potência de 2 do kernel, que já é múltiplo de 16 KiB.

`pthread_create` arm64 reserva a pilha mais uma guarda no começo da faixa. A guarda padrão é uma página guest. `pthread_attr_setguardsize(0)` pede pilha sem guarda; qualquer outro tamanho sobe até 16 KiB. `pthread_get_stacksize_np` não inclui a guarda. A pilha principal do processo fica abaixo de `USER_MMAP_BASE`: o kernel recusa `munmap` ali, então um `MAP_FIXED` sobre ela devolve o endereço e **não** cria uma guarda que falte.

## Carregador

`macho.c` só estende a imagem quando o processo é arm64 (`ls_cur`). O `vmaddr` dos segmentos não muda (fixups e slide continuam iguais). O `vmax` da imagem sobe até o fim da página guest do último byte, e a proteção registrada de cada segmento sobe do mesmo jeito sem entrar no segmento seguinte. `__PAGEZERO` não entra. O `vmsize` gravado no segmento, inclusive o do `__TEXT` usado pelo cache de tradução, não muda.

Os segmentos arm64 da Apple já nascem alinhados a 16 KiB entre si. A base da imagem (`USER_BASE + 0x01000000`) também é. O slide preserva o alinhamento. O espaço até a próxima imagem já anda de 64 KiB, então os bytes a mais da última página cabem nesse intervalo.

## O que este contrato não faz

- Não muda a página do kernel nem a de um processo x86_64.
- Não cria commpage.
- `MAP_FIXED` de **arquivo** e `mach_vm_map` sem `VM_FLAGS_ANYWHERE` continuam sem honrar o endereço pedido: o kernel não tem `mmap` de arquivo em endereço fixo. O endereço novo é 16 KiB alinhado.
- A pilha principal não ganha guarda que falte. A das threads criadas por `pthread_create`, sim.
- Proteção RW contra RX continua só registrada para `mach_vm_region`. Quem falta de verdade é `PROT_NONE` na arena de `mmap`.
- Um `mmap` feito pelo binário com `svc` direto, sem o libSystem, continua em 4 KiB. Os apps passam pelo libSystem.
- `pthread_attr_setstacksize` abaixo de 64 KiB ainda cai na pilha padrão de 512 KiB (regra antiga do libSystem, independente da página).

## Testes

No Linux, sem o kernel, a política de `a64page.h` roda contra um modelo do cursor de `SYS_MMAP` / `SYS_MUNMAP`:

```
gcc -std=c11 -Wall -Wextra -Werror -O2 -o /tmp/a64page_test tools/a64page_test.c
/tmp/a64page_test
```

Dentro do mOS (build PowerShell/zig, não roda nesta VM Linux): `userland/tests-a64rt/main.cpp`, argumento `pagina`. Cobre os valores de cada API, `mmap` alinhado, `MAP_FIXED`, `mprotect` parcial, `munmap`, `mach_vm_deallocate` e a guarda da pilha de uma thread. O `build.ps1` + `tools/build-a64rt-teste.ps1` colocam esse binário na imagem.
