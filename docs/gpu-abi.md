# ABI de GPU (rascunho estável)

Contrato entre o kernel do mOS e o Metal da Sistemas. O Metal é quem consome.
Este documento e `kernel/include/mos_gpu.h` são a fonte. `MOS_GPU_ABI` vale 1.

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

`MOS_GPU_INFO` preenche `mos_gpu_info_t`: `abi`, `caps` (`MOS_GPU_CAP_VIRGL`,
`VENUS`, `KMS`, `IOSURF`), identificadores PCI e um nome curto. Sem GPU, a
chamada tem sucesso, `caps` fica 0 e `name` descreve só o framebuffer do
firmware.

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

`MOS_GPU_SUBMIT` envia `cmd_bytes` a partir de `cmds` para o contexto.
`MOS_GPU_SUBMIT_3D` marca fluxo virgl ou venus, conforme o `kind` do
contexto. `fence` 0 não sinaliza; outro valor pede que a fence seja
sinalizada quando a GPU concluir o lote. A syscall volta quando o lote foi
aceito, não quando a GPU terminou. Comando maior que o limite do dispositivo
ou contexto errado devolve `-MOS_EINVAL`.

O formato dos bytes em `cmds` é o do dispositivo: virgl (cabeçalho de
comando do virtio-gpu 3D) ou venus (fluxo Venus). O kernel não interpreta
shader. Rejeita lote vazio e contexto destruído.

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
