/*
 * mOS — macho_load no Linux, nos mesmos endereços do processo (tools/host_proc.h).
 *   machoload_host <raiz-do-vfs> <caminho-mos-do-executável>
 * MOS_ARCH=arm64 escolhe a fatia arm64. MACHOTEST_TOLERANT=1 segue com biblioteca de sistema ausente.
 * A contagem sai em stderr: "macho: N imagens carregadas".
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "compat.h"

long long g_live_allocs, g_live_bytes;

static const char *g_root;

bool uvm_map(process_t *p, u64 va, u64 len) {
    u64 lo, hi;
    void *m;
    (void)p;
    if (!len) return true;
    lo = va & ~0xFFFULL;
    hi = (va + len + 0xFFFULL) & ~0xFFFULL;
    m = mmap((void *)(uintptr_t)lo, (size_t)(hi - lo), PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (m == MAP_FAILED) {
        fprintf(stderr, "mmap(%llx, %llx) falhou: %s\n", (unsigned long long)va, (unsigned long long)len, strerror(errno));
        return false;
    }
    return true;
}

bool uvm_copy_out(process_t *p, u64 va, const void *src, usize len) {
    (void)p;
    memcpy((void *)(uintptr_t)va, src, len);
    return true;
}

bool uvm_copy_in(process_t *p, void *dst, u64 va, usize len) {
    (void)p;
    memcpy(dst, (void *)(uintptr_t)va, len);
    return true;
}

const u8 *host_read_file(const char *path, usize *len) {
    char hp[1200];
    FILE *f;
    long n;
    u8 *b;
    snprintf(hp, sizeof hp, "%s%s", g_root, path);
    f = fopen(hp, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    b = malloc(n ? (size_t)n : 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = (usize)n;
    return b;
}

int main(int argc, char **argv) {
    process_t p;
    usize len = 0;
    const u8 *img;
    char err[512];
    u64 entry = 0, rsp = 0, rdi = 0;
    if (argc != 3) {
        fprintf(stderr, "uso: machoload_host <raiz-do-vfs> <caminho-mos-do-executável>\n");
        return 2;
    }
    g_root = argv[1];
    memset(&p, 0, sizeof p);
    snprintf(p.path, sizeof p.path, "%s", argv[2]);
    img = host_read_file(p.path, &len);
    if (!img) {
        fprintf(stderr, "não abriu %s%s\n", g_root, p.path);
        return 1;
    }
    err[0] = 0;
    if (!macho_load(&p, img, len, "", &entry, &rsp, &rdi, err, sizeof err)) {
        fprintf(stderr, "macho_load: %s\n", err[0] ? err : "(sem mensagem)");
        return 1;
    }
    fprintf(stderr, "macho_load: ok entry=%llx\n", (unsigned long long)entry);
    return 0;
}
