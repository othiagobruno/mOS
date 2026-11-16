/*
 * mOS — teste de host das funções de sandbox do libSystem (userland/libsystem/darwin.c, F10).
 *   gcc -O1 -w -DMOS_LIBSYS_SYSCALL -Iuserland/libsystem -c userland/libsystem/darwin.c -o /tmp/darwin_ls.o
 *   gcc -O1 tools/sandbox_host.c /tmp/darwin_ls.o -Wl,--unresolved-symbols=ignore-all -o /tmp/sandbox_host && /tmp/sandbox_host
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
int sandbox_check(int pid, const char *op, int type, ...);
int sandbox_check_by_audit_token(const uint32_t *tok, const char *op, int type, ...);
int sandbox_init(const char *p, uint64_t f, char **e);
char *sandbox_extension_issue_file(const char *c, const char *p, uint32_t f);
char *sandbox_extension_issue_mach(const char *c, const char *n, uint32_t f);
long sandbox_extension_consume(const char *t);
int sandbox_extension_release(long h);
int sandbox_container_path_for_pid(int pid, char *b, size_t n);
void sandbox_spawnattrs_init(void *a);
int sandbox_spawnattrs_setprofilename(void *a, const char *n);
static int err; int *__error(void) { return &err; }
char *fake_argv[] = {"app", 0}; char **NXArgv = fake_argv;
long cx_syscall(long n, long a, long b, long c, long d, long e) { (void)a; (void)b; (void)c; (void)d; (void)e; return n == 2 ? 50 : -38; }
static int fails, oks;
#define CHECK(c, m) do { if (c) oks++; else { fails++; printf("FALHA %d: %s\n", __LINE__, m); } } while (0)
int main(void) {
    uint32_t tok[8] = {0, 0, 0, 0, 0, 50, 0, 0};
    CHECK(sandbox_check(50, 0, 0) == 0, "fora de sandbox");
    CHECK(sandbox_check(50, "file-read-data", 1, "/etc/hosts") == 0, "operação permitida");
    sandbox_init("no-network", 0, 0);
    CHECK(sandbox_check(50, 0, 0) == 1 && sandbox_check_by_audit_token(tok, 0, 0) == 1, "depois do sandbox_init");
    CHECK(sandbox_check(51, 0, 0) == 0, "outro pid");
    char *t = sandbox_extension_issue_mach("com.apple.app-sandbox.mach", "com.ex.svc", 0);
    CHECK(t && !strcmp(t, "com.apple.app-sandbox.mach;com.ex.svc"), "token mach");
    long h1 = sandbox_extension_consume(t), h2 = sandbox_extension_consume(t);
    CHECK(h1 > 0 && h2 > h1 && sandbox_extension_release(h1) == 0, "consume/release");
    CHECK(sandbox_extension_release(99999) == -1 && err == 22, "release inválido EINVAL");
    CHECK(sandbox_extension_consume(0) == -1, "consume nulo");
    char *f = sandbox_extension_issue_file("x", "/tmp/a", 0);
    CHECK(f && sandbox_extension_consume(f) > 0, "token de arquivo");
    char buf[64] = "lixo";
    CHECK(sandbox_container_path_for_pid(50, buf, sizeof buf) == -1 && buf[0] == 0, "sem contêiner");
    char attrs[300];
    sandbox_spawnattrs_init(attrs);
    CHECK(sandbox_spawnattrs_setprofilename(attrs, "renderer") == 0 && !strcmp(attrs, "renderer"), "spawnattrs");
    printf("sandbox: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
