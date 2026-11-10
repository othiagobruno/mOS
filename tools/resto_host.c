/* Host: fchmod, vfork (tabela compartilhada), commpage, swap e FILEPORT_DUP2. */
#include "cred.h"
#include "commpage.h"
#include "fdshare.h"
#include "fileport.h"
#include "syscall.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

void swap_use(void *mem, unsigned n);
unsigned long long swap_store(unsigned long long phys, int nx, int jwp);
void swap_load(unsigned long long pte, unsigned long long page);

static int falhas;
static void check(int cond, const char *msg) {
    if (!cond) { printf("FALHA: %s\n", msg); falhas++; }
}

int main(void) {
    unsigned vnode = 0, fdmode = 0;
    fchmod_apply(&vnode, &fdmode, 0600);
    check(mode_stat(vnode, 0) == 0600 && fdmode == 0600, "fchmod grava o mode que o stat lê");
    fchmod_apply(&vnode, &fdmode, 0);
    check(mode_stat(vnode, 0) == 0 && fdmode == 0, "fchmod 0000 fica gravado");

    typedef struct { int used; char path[8]; } slot_t;
    slot_t pai[4] = {0}, filho_own[4] = {0};
    slot_t *tab = fd_share_tab(pai);
    check(fd_tab_shared(tab, filho_own), "vfork compartilha a tabela");
    pai[1].used = 1;
    memcpy(pai[1].path, "pai", 4);
    check(tab[1].used == 1, "o filho vê o fd do pai");
    tab[1].used = 0;
    check(pai[1].used == 0, "fechar no filho fecha no pai");
    pai[2].used = 1;
    fd_unshare_tab(filho_own, (void **)&tab, sizeof filho_own);
    check(!fd_tab_shared(tab, filho_own) && filho_own[2].used == 1, "exec copia a tabela para o filho");
    filho_own[2].used = 0;
    check(pai[2].used == 1, "depois do exec o pai conserva o fd");
    check(fd_tab_shared(fd_share_tab(pai), filho_own), "no _exit a tabela ainda é a do pai, então não fecha");

    unsigned char page[4096];
    commpage_fill(page);
    check(memcmp(page, "commpage 64-bit", 15) == 0 && page[9] == '6', "commpage tem a assinatura");
    check(*(unsigned short *)(page + 0x01E) == 7, "commpage versão 7");
    check(page[0x022] == 1 && page[0x025] == 14, "commpage: 1 cpu e página de 16 KiB");
    check(*(unsigned short *)(page + 0x026) == 64, "commpage: linha de cache 64");

    unsigned char part[2 * 4096], src[4096], dst[4096];
    memset(src, 0x5A, sizeof src);
    src[0] = 0x11; src[4095] = 0x22;
    swap_use(part, 2);
    unsigned long long pte = swap_store((unsigned long long)(uintptr_t)src, 1, 0);
    memset(src, 0, sizeof src);
    memset(dst, 0, sizeof dst);
    swap_load(pte, (unsigned long long)(uintptr_t)dst);
    check(pte != 0 && dst[0] == 0x11 && dst[4095] == 0x22 && dst[100] == 0x5A, "swap traz a página com o mesmo conteúdo");
    check((pte & (1ULL << 63)) != 0, "swap conserva NX");

    unsigned char used = 0;
    int flags = 0;
    unsigned mode = 0;
    char path[32];
    check(fileport_dup2(&used, &flags, &mode, path, sizeof path, 1, MOS_O_CLOEXEC | 2, 0640, "/tmp/porta") == 0, "FILEPORT_DUP2 instala");
    check(used && mode == 0640 && strcmp(path, "/tmp/porta") == 0 && (flags & MOS_O_CLOEXEC) == 0, "FILEPORT_DUP2 copia path e mode sem CLOEXEC");
    check(fileport_dup2(&used, &flags, &mode, path, sizeof path, 0, 0, 0, 0) == -1, "FILEPORT_DUP2 sem porta falha");

    if (falhas) { printf("resto_host: %d falhas\n", falhas); return 1; }
    printf("resto_host: ok\n");
    return 0;
}
