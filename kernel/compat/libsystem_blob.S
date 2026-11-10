/* libSystem.mos sha256: ddc9d162e4522998 */
/* libSystem.mos sha256:  */
/* mOS - libSystem.mos (userland/libsystem, compilado por tools/build-libsystem.ps1) embutido no kernel */
    .section .rdata, "dr"
    .p2align 4
    .globl libsystem_mos
libsystem_mos:
    .incbin "build/libSystem.mos"
    .globl libsystem_mos_end
libsystem_mos_end:
    .byte 0
