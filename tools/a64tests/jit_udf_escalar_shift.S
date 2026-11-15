/* frente JIT: AdvSIMD escalar com bit 24 = 1 (deslocamento por imediato) e bit 23 = 1: não alocado, UDF nos dois
 * motores. O despacho escalar do JIT não olhava o bit 24 e executava como USHL Dd (3same). Achado pelo
 * tools/a64fuzz_host.c (simd.outros). */
    .text
    .globl main
main:
    .inst 0x7fe64576
    mov x0, #0
    ret
