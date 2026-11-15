/* frente JIT: SQRDMLAH escalar por elemento (bit 24 = 1) com size = 00: não alocado, UDF nos dois motores. O
 * despacho escalar do JIT tomava como SCVTF (2misc) por não olhar o bit 24. Achado pelo tools/a64fuzz_host.c
 * (simds.indexed). */
    .text
    .globl main
main:
    .inst 0x7f21d8cb
    mov x0, #0
    ret
