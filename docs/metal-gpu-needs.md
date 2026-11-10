# O que o Metal precisa da ABI de GPU

Contrato: `docs/gpu-abi.md` e `kernel/include/mos_gpu.h` (`MOS_GPU_ABI` 2).
`userland/libsystem/gpucall.c` inclui esses headers e chama `SYS_GPU` de
`kernel/include/syscall.h`. O número é 62. 61 é `SYS_UNIX`. Não há espelho
das estruturas nem outro número no userland.

`MOS_GPU_INFO` anuncia `MOS_GPU_CAP_IOSURF`. `IOSurfaceCreate` de um plano
BGRA (o drawable do `CAMetalLayer`) cria o BO, mapeia e cria a IOSurface da
ABI. `IOSurfaceGetID` é esse id. `IOSurfaceGetBaseAddress` é o mapeamento do
BO. O cabeçalho Mach continua numa página local; os pixels não são copiados
de novo no `-present`.

O `MTLDevice` (fila, encoders, interpretador) não foi duplicado.

## Submit de compute

`-commit` de um command buffer com `MTLComputeCommandEncoder` monta, por
dispatch, o lote da revisão 2 (cabeçalho, handles, relocs, `BIND`, `SHADER`,
`DISPATCH`) e chama `MOS_GPU_SUBMIT` com `MOS_GPU_SUBMIT_3D` em `SYS_GPU`.
`-waitUntilCompleted` espera a fence. O shader no BO é o SPIR-V de
`mslspirv.c` quando o kernel coube nele; senão são 4 bytes, porque o kernel
não lê o shader. O lote cabe em 400 bytes e leva no máximo 5 buffers.

Com `MOS_GPU_CAP_VIRGL` ou `MOS_GPU_CAP_VENUS` o lote segue para o
dispositivo (a tradução do kernel, não o lote cru) e o interpretador não
repete o dispatch. A fence sobe no IRQ do virtio-gpu. Sem esses caps o
kernel valida e sinaliza a fence na hora, e `mtlexec.c` executa o kernel
para o resultado ficar certo. Textura, sampler e threadgroup memory não
entram no lote.

## O que o teste de host cobre

`tools/host-metal-gpu.c` desvia `SYS_GPU` para `gpu_syscall` de
`kernel/dev/gpu_abi.c` (`MOS_GPU_HOST`). Confere o cap de IOSurface, o bind
do drawable (mesmos pixels e o mesmo id na lookup), o SPIR-V do kernel de
exemplo e os bytes do lote de um dispatch (BIND, SHADER, DISPATCH, handles
e relocs) junto com o resultado do interpretador. Sem QEMU e sem GPU real.
