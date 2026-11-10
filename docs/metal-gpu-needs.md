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

Não há codificador virgl nem venus. Um dispatch de compute nesse protocolo
precisa de handles de recurso do virtio-gpu 3D, do shader no formato do
dispositivo e de um lote que caiba nos 400 bytes que o kernel aceita. A ABI
diz que o kernel não interpreta shader e que a codificação interna do
virtio-gpu fica de fora desta revisão. O SPIR-V gerado em `mslspirv.c` não é
esse formato: serve para o subconjunto de MSL já descrito (kernel, buffer
`float`/`uint`, thread id, aritmética escalar). Quem executa o comando
continua o interpretador de `mtlexec.c`.

Quando `MOS_GPU_CAP_VIRGL` ou `MOS_GPU_CAP_VENUS` estiver ligado, o `-commit`
de um compute pode chamar `MOS_GPU_SUBMIT` com `MOS_GPU_SUBMIT_3D`. O lote é
o da revisão 2: cabeçalho, tabela de handles, relocs e os comandos
`BIND` / `SHADER` / `DISPATCH` em `mos_gpu.h`. Com o cap, a fence só sobe no
IRQ do virtio-gpu. Sem o cap o kernel valida e sinaliza a fence, e não
executa o shader. O Venus desses três comandos sai com contagem de palavras
na frente de cada um.

## O que o teste de host cobre

`tools/host-metal-gpu.c` desvia `SYS_GPU` para `gpu_syscall` de
`kernel/dev/gpu_abi.c` (`MOS_GPU_HOST`). Confere o cap de IOSurface, o bind
do drawable (mesmos pixels e o mesmo id na lookup) e o SPIR-V do kernel de
exemplo. Sem QEMU.
