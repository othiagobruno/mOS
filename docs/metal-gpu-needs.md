# O que o Metal precisa da ABI de GPU

Fonte do contrato: `docs/gpu-abi.md` e `kernel/include/mos_gpu.h`, do agente K-GPU
(`kernel/gpu`, revisão `MOS_GPU_ABI` 1). O Metal não define outra ABI. O cliente
está em `userland/libsystem/gpucall.c`. Se o header do kernel está na árvore, a
camada chama `SYS_GPU` (61). Se a syscall não existe ou devolve `-MOS_EINVAL` /
`-ENOSYS`, e também enquanto o header ainda não foi mergeado, as mesmas
estruturas rodam num backend de CPU neste processo.

O desenho do `MTLDevice` continua o que já está na main (`metal.c`, `metalexec.c`,
`mtlshader.c`): buffers, texturas, fila, command buffer, encoders e o
`CAMetalLayer` sobre o `IOSurface` de software. A camada nova não duplica essa API.

## O que já conversa com a ABI

| Operação | No CPU (teste de host e fallback) | Quando o kernel responder |
|---|---|---|
| `MOS_GPU_INFO` | `abi` 1, `caps` 0, nome `cpu` | usa `caps` do kernel |
| contexto virgl/venus | handle local; não atravessa processo | handle do kernel |
| BO criar/mapear/exportar/importar | páginas alinhadas a 4096 neste processo; o token só vale aqui | memória da GPU ou compartilhada |
| submit | rejeita lote vazio; aceita bytes e sinaliza a fence. Não executa virgl nem venus | o dispositivo executa |
| fence criar/sinal/esperar/destruir | `seq` neste processo. Espera sem sinal devolve erro na hora (não bloqueia) | espera de verdade |
| IOSurface da ABI | id, BO, formato BGRA8/RGBA8 | mesma estrutura, páginas do kernel |
| `CAMetalLayer` | o QuartzCore segue com o IOSurface de software. `mos_gpu_present_layer` só copia para a ABI se `MOS_GPU_CAP_IOSURF` estiver ligado | o compositor do kernel ainda não é chamado: `KMS_SET` troca a tela inteira |

## Diferenças em relação à ABI

- A ABI diz que os bytes de `MOS_GPU_SUBMIT` são virgl ou venus e que o kernel
  não interpreta shader. O Metal gera SPIR-V de um subconjunto de MSL
  (`mslspirv.c`, licença do próprio mOS, sem SPIRV-Tools vendido e sem GPL) e
  continua executando a árvore no interpretador. Não há codificador virgl/venus:
  a própria ABI deixa a codificação interna do virtio-gpu fora desta revisão.
- `LocalSize` do SPIR-V é 1×1×1. O tamanho do grupo que vale é o do
  `dispatchThreadgroups` no interpretador.
- SPIR-V desta revisão: kernel, `device float*` / `device uint*` com
  `[[buffer(n)]]`, `[[thread_position_in_grid]]` `uint` ou `uint3`, aritmética
  escalar, índice, atribuição, `if`/`else` e local escalar. Vértice, fragmento,
  textura, barreira, laço e `return` com valor ficam de fora, com nota em
  `msl_fn_spirv_note`. O interpretador existente cobre esses casos.
- O `IOSurface` de software (`iosurface.c`) e o da ABI são objetos diferentes.
  A ABI pede que o Metal associe o `id`. Isso só acontece no present quando o
  kernel anuncia `MOS_GPU_CAP_IOSURF`. Sem o bit, não há segunda cópia por quadro.
- Fence compartilhada: a ABI reserva o bit e deixa o transporte para uma revisão
  que só acrescente campos. O CPU não exporta fence.
- `MOS_GPU_KMS_GET` / `KMS_SET`: o CPU devolve não encontrado. O Metal não chama
  `KMS_SET` a partir de uma janela, porque isso substituiria o framebuffer.

## O que o kernel precisa ter ligado para o Metal sair do CPU

1. `kernel/include/mos_gpu.h` na main (o `#include` de `gpucall.c` passa a valer sozinho).
2. `SYS_GPU` = 61 em `syscall.h`, como no branch `kernel/gpu`.
3. `MOS_GPU_INFO` com sucesso. `caps` 0 significa framebuffer sem 3D: o Metal
   permanece no interpretador.
4. Para composição acelerada: `MOS_GPU_CAP_IOSURF` e, num passo seguinte do
   compositor, um jeito de mostrar um `id` de IOSurface sem `KMS_SET` da tela
   inteira. O QuartzCore já desenha o IOSurface de software na janela.
5. Para o submit substituir o interpretador: um codificador virgl ou venus no
   userland, que esta revisão não inclui, e `MOS_GPU_CAP_VIRGL` ou
   `MOS_GPU_CAP_VENUS`.
