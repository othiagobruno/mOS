/* mOS — oráculo do tradutor arm64: monitor bare-metal carregado com -kernel em 0x40100000 */
ENTRY(_start)
SECTIONS {
    . = 0x40100000;
    .text : { KEEP(*(.text.boot)) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    . = ALIGN(16);
    __bss_start = .;
    .bss : { *(.bss*) *(COMMON) }
    . = ALIGN(16);
    __bss_end = .;
    . = ALIGN(4096);
    . = . + 0x40000;
    __stack_top = .;
}
