# ABI de GPU (rascunho estável)

Contrato entre o kernel do mOS e o Metal da Sistemas. O Metal é quem consome.
Este documento e `kernel/include/mos_gpu.h` são a fonte. `MOS_GPU_ABI` vale 2.
As estruturas de syscall são as da revisão 1. A revisão 2 define o lote de
`MOS_GPU_SUBMIT`.

A syscall é `SYS_GPU` (62). O primeiro rascunho dizia 61; a `main` já usava
61 para `SYS_UNIX`. Os códigos `MOS_GPU_*` não mudaram. Convenção: `int 0x80`,
número em `rax`, argumentos em `rdi`, `rsi`, `rdx`. `rdi` é o código `MOS_GPU_*`.
`rsi` aponta para a estrutura da operação. `rdx` é quantos bytes o chamador
escreveu. O retorno em `rax` é 0 ou `-errno` (`MOS_EINVAL`, `MOS_ENOMEM`,
`MOS_EFAULT`, `MOS_EBUSY`, `MOS_ETIMEDOUT`, `MOS_ENOENT`, `MOS_EAGAIN`).
Handles voltam dentro da estrutura.

As estruturas só crescem no fim. O kernel usa o tamanho que o chamador
declarou e zera o que faltar. Mudar o sentido de um campo existente exige
subir `MOS_GPU_ABI` e uma nota nesta página.

Nada disto descreve o rasterizador em software do libSystem. Quem fala com
a GPU usa esta syscall.

## Consulta

`MOS_GPU_INFO` preenche `mos_gpu_info_t`: `abi` (`MOS_GPU_ABI`, agora 2),
`caps` (`MOS_GPU_CAP_VIRGL`, `VENUS`, `KMS` e, com a IOSurface compartilhada,
`IOSURF`), identificadores PCI e um nome curto. Sem GPU 3D, `VIRGL` e `VENUS`
ficam desligados. `IOSURF` permanece enquanto essa superfície existir.

## Contexto

`MOS_GPU_CTX_CREATE` com `kind` = `MOS_GPU_CTX_VIRGL` ou `MOS_GPU_CTX_VENUS`
devolve `ctx` no processo. O contexto não atravessa `fork` nem `exec`.
`MOS_GPU_CTX_DESTROY` derruba o contexto e recusa submits novos. Um submit
já aceito ainda pode sinalizar a fence.

## Buffers (BO)

`MOS_GPU_BO_CREATE` pede `size` múltiplo de 4096. `flags` combina
`MOS_GPU_BO_READ`, `WRITE`, `SHARED` e `GPU`. `GPU` pede memória da GPU
(VRAM, stolen ou recurso 3D). Sem esse bit o BO é memória do sistema visível
pela GPU. `ctx` 0 cria um BO independente de contexto 3D; outro valor prende
o BO àquele contexto.

`MOS_GPU_BO_MAP` devolve `addr` e `len` no espaço do chamador.
`MOS_GPU_BO_UNMAP` desfaz essa faixa. `MOS_GPU_BO_DESTROY` exige que não
haja mapeação nem IOSurface apontando para o handle.

`MOS_GPU_BO_EXPORT` exige `MOS_GPU_BO_SHARED` e devolve `token`.
`MOS_GPU_BO_IMPORT` noutro processo, com o mesmo `token`, devolve um handle
local para as mesmas páginas. O token vale enquanto existir pelo menos um
handle.

## Submit e fences

`MOS_GPU_FENCE_CREATE` devolve `fence` com `seq` 0. `MOS_GPU_FENCE_WAIT`
bloqueia até `seq` ser maior que o valor visto na criação, ou até
`timeout_ms` (`~0u` não tem prazo). Estouro devolve `-MOS_ETIMEDOUT`.
`MOS_GPU_FENCE_SIGNAL` incrementa `seq` (evento compartilhado entre
processos quando a fence foi criada com `MOS_GPU_FENCE_SHARED` e o token
ainda não existe nesta revisão: o compartilhamento de fence é o bit, o
transporte fica para uma revisão que só acrescente campos no fim).
`MOS_GPU_FENCE_DESTROY` acorda quem espera com `-MOS_ENOENT` se a fence
sumir durante a espera; quem já observou `seq` não é afetado.

`MOS_GPU_SUBMIT` envia `cmd_bytes` a partir de `cmds`. Com `MOS_GPU_SUBMIT_3D`
o buffer é um lote da revisão 2 (`MOS_GPU_ABI` 2). Cabe em 400 bytes.
Lote vazio, maior que isso, contexto errado ou lote inválido devolve
`-MOS_EINVAL`. Handle da tabela que o processo não tem devolve `-MOS_ENOENT`.
A fence só incrementa `seq` depois da validação. Falha não sinaliza.

O lote é little-endian e múltiplo de 4, nesta ordem:

1. `mos_gpu_batch_hdr_t`: `version` = `MOS_GPU_BATCH_VER` (1), `kind` igual ao
   do contexto, `bytes` igual a `cmd_bytes`, `ncmd` (1..8), `nbo` (0..8),
   `nreloc` (0..8), `pad0` e `pad1` zero.
2. `nbo` handles `u32` do processo (a tabela do próprio lote).
3. `nreloc` vezes `mos_gpu_reloc_t`: `cmd` é o índice do comando, `off` é um
   byte alinhado dentro dele (a partir do cabeçalho de 16 bytes do comando),
   `bo_index` é índice na tabela, `bo_off` cabe no BO. A palavra de 32 bits
   nesse `off` tem de ser o próprio `bo_index`.
4. Comandos. Cada um começa com `mos_gpu_cmd_hdr_t` (`op`, `bytes` do comando
   inteiro, `virgl`, `venus`). `bytes` é múltiplo de 4. A soma termina no fim
   do lote.

Compute mínimo:

| op | carga | virgl | venus |
|---|---|---|---|
| `MOS_GPU_CMD_BIND` (1) | `mos_gpu_bind_t`: slot 0..31, índice, offset, range > 0 | `MOS_GPU_VIRGL_SET_SHADER_BUFFERS` (32) | `MOS_GPU_VENUS_BIND` (1) |
| `MOS_GPU_CMD_SHADER` (2) | `mos_gpu_shader_t`: shader num BO, local size > 0 nos três eixos | `MOS_GPU_VIRGL_BIND_SHADER` (31) | `MOS_GPU_VENUS_SHADER` (2) |
| `MOS_GPU_CMD_DISPATCH` (3) | `mos_gpu_dispatch_t`: grupos 1..65535 | `MOS_GPU_VIRGL_LAUNCH_GRID` (35) | `MOS_GPU_VENUS_DISPATCH` (3) |

No lote virgl o campo `venus` é 0 e `virgl` é o número da tabela. No lote
venus é o contrário. Outro opcode é `-MOS_EINVAL`. O kernel não lê o shader.
Os números virgl são os de `SET_SHADER_BUFFERS`, `BIND_SHADER` e
`LAUNCH_GRID` do protocolo virgl. O venus desta revisão é o subconjunto da
tabela, não o fluxo `vn_cs` completo.

Sem o cap virgl ou venus, o lote válido só sinaliza a fence. Com o cap, o que
vai para o `SUBMIT_3D` do virtio-gpu é a tradução, não o lote cru. Virgl sai
como `VIRGL_CMD0`: `SET_SHADER_BUFFERS` (tipo compute, slot, um recurso, handle
do BO, offset, range), `BIND_SHADER` (handle, tipo compute) e `LAUNCH_GRID`
(tamanho local do último shader, ou 1,1,1, e a grade do dispatch). Venus sai
como opcode `MOS_GPU_VENUS_*` seguido dos mesmos campos, com o handle do BO
no lugar do índice. O shader em si não é executado.
`fence` 0 não sinaliza. Com a fila em polling e o cap do `kind` ligado, a
fence incrementa quando a resposta chega, antes do retorno. Sem o cap, o
lote válido só sinaliza a fence. `FENCE_WAIT` com `seq` ainda em 0 e
`timeout_ms` diferente de 0 bloqueia a thread (`wait_pid` -12) até o sinal,
a destruição (`-MOS_ENOENT`) ou o prazo (`-MOS_ETIMEDOUT`). `timeout_ms` 0
devolve `-MOS_EAGAIN` na hora.

## IOSurface na memória da GPU

`MOS_GPU_IOSURF_CREATE` liga um BO já criado a uma superfície: largura,
altura, `stride`, `format` (`MOS_GPU_FMT_BGRA8` ou `RGBA8`). O `id` é único
no sistema. Os pixels são o conteúdo do BO; não há segunda cópia. Destruir
a superfície não destrói o BO. Destruir o BO com superfície viva devolve
`-MOS_EBUSY`.

`MOS_GPU_IOSURF_EXPORT` devolve `token`. `MOS_GPU_IOSURF_LOOKUP` com `token`
(ou com `id` no processo que criou) preenche a estrutura e, no outro
processo, o campo `bo` com um handle local importado. `MOS_GPU_IOSURF_DESTROY`
só vale para quem criou.

Isto é a superfície que o compositor e o Metal compartilham. Não substitui
o `IOSurface` de software do libSystem; quem precisa dos dois associa o
`id` desta ABI ao objeto de userland por conta própria.

## Modo de vídeo

`MOS_GPU_KMS_GET` lê o modo do conector (`MOS_GPU_CONN_INTERNAL` ou
`EXTERNAL`). `MOS_GPU_KMS_SET` pede largura, altura, stride, formato e um BO
(`bo` 0 mantém o framebuffer do firmware). É troca de framebuffer e de modo,
sem aceleração 3D. Conector sem hardware devolve `-MOS_ENOENT`.

## O que esta revisão não fixa

Codificação interna do virtio-gpu, endereços de registrador Intel ou AMD, e
o momento em que o kernel passa a implementar cada operação. Enquanto uma
operação não existir no kernel, o retorno é `-MOS_ENOSYS` se esse código
estiver definido; se não estiver, `-MOS_EINVAL`. Os números `MOS_GPU_*` e os
campos já listados permanecem.
