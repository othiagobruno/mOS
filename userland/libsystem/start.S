/* mOS libSystem — rotinas que precisam de ABI especial */
    .text

/* Thread-local (macOS): o código chama *(desc) com rdi = descritor e espera em rax o endereço
 * da variável, com TODOS os outros registradores preservados. Processos do mOS têm uma thread,
 * então o loader põe em desc->key o endereço do bloco TLS da imagem: endereço = key + offset. */
    .globl _tlv_bootstrap
_tlv_bootstrap:
    mov 8(%rdi), %rax
    add 16(%rdi), %rax
    ret

/* Sonda de pilha do clang (rax = bytes). A pilha do processo já está toda mapeada. */
    .globl ___chkstk_darwin
___chkstk_darwin:
    ret
    .globl __chkstk_darwin
__chkstk_darwin:
    ret
