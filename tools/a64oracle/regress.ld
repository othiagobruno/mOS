/* mOS - testes de regressão do fuzzer (tools/a64tests/regress_*.S): mesmo layout dos outros testes
 * (base 0x200000000) + página de código e rascunho nos endereços fixos de fz.h */
ENTRY(_start)
SECTIONS {
    . = 0x200000000 + SIZEOF_HEADERS;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    . = ALIGN(0x10000);
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    .a64fzcode 0x2E0000000 : { KEEP(*(.a64fzcode)) }
    .a64fzscr 0x2E0100000 : { KEEP(*(.a64fzscr)) }
}
