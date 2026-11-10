/*
 * Extrator de pacotes flat do Instalador (.pkg / .mpkg em xar).
 * Tira o Payload (cpio em gzip ou pbzx) e confere o Bom. Não executa scripts.
 * Só chama out->write depois de o pacote inteiro ser aceito. Caminho com ".."
 * ou absoluto é recusa: nada é escrito.
 */
#pragma once
#ifdef MOS_HOST_TEST
#include "fs.h"
#else
#include "kernel.h"
#include "fs.h"
#endif

typedef struct pkg_out {
    /* kind 0 = arquivo, 1 = diretório. rel é relativo ao destino, sem ".." e sem barra inicial.
     * Devolve 0 ou -1. Em -1 o extrator para; o que já foi escrito fica só sob o destino. */
    int (*write)(void *ctx, const char *rel, const void *data, u64 n, int kind);
    void *ctx;
    int files;
    int scripts_skipped;
    char reason[200];
} pkg_out_t;

/* 0 = extraiu. -1 = recusou (nenhuma chamada a write). */
int pkg_extract(const u8 *data, u64 n, pkg_out_t *out);
