/* crt mínimo dos testes do a64test: _start → main(argc, argv) → exit */
    .text
    .globl _start
_start:
    bl main
    mov x16, #1
    svc #0x80
    brk #0
    .globl a64t_write          /* long a64t_write(int fd, const void *b, unsigned long n) */
a64t_write:
    mov x16, #4
    svc #0x80
    ret
    .globl a64t_dump           /* void a64t_dump(void): imprime todo o estado (formato do oráculo) */
a64t_dump:
    mov x16, #0x4000
    svc #0x80
    ret
