/* Teste de host do aviso de app baixado (kernel/compat/gatekeeper.c): leitura e aprovação do com.apple.quarantine.
 *   gcc -std=gnu11 -Wall -Wextra -fsanitize=address,undefined tools/gatekeepertest.c -o /tmp/gk && /tmp/gk */
#include <stdio.h>
#include <string.h>
#define GK_HOST 1
#include "../kernel/compat/gatekeeper.c"

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; else printf("FALHA: %s\n", w); }

int main(void) {
    const char *v = "0083;6738f2a0;Safari;9C7C6B1E-7A3A-4C11-8B5E-1D2E3F405162";
    gk_qtn_t q = gatekeeper_parse(v, (int)strlen(v));
    check(q.ok && q.flags == 0x83 && q.ts == 0x6738f2a0ULL && !strcmp(q.agent, "Safari"), "campos do valor");
    check(!(q.flags & GK_APPROVED), "ainda não aprovado");
    char out[200];
    int n = gatekeeper_approve(v, (int)strlen(v), out, sizeof out);
    check(n > 0 && !strcmp(out, "00c3;6738f2a0;Safari;9C7C6B1E-7A3A-4C11-8B5E-1D2E3F405162"), "aprovar liga 0x40 e mantém o resto");
    q = gatekeeper_parse(out, n);
    check(q.ok && (q.flags & GK_APPROVED), "aprovado");
    q = gatekeeper_parse("0001", 4);
    check(q.ok && q.flags == 1 && q.ts == 0 && !q.agent[0], "só as flags");
    q = gatekeeper_parse("zz;1;x", 6);
    check(!q.ok, "flags inválidas");
    q = gatekeeper_parse("", 0);
    check(!q.ok, "vazio");
    q = gatekeeper_parse("0081;65;Um agente com nome bem comprido que passa de sessenta e quatro letras certamente;u", 90);
    check(q.ok && strlen(q.agent) == 63, "agente cortado no limite");
    check(gatekeeper_approve("x", 1, out, sizeof out) < 0, "aprovar valor inválido falha");
    int Y, M, D;
    gk_civil(0x6738f2a0ULL, &Y, &M, &D);
    check(Y == 2024 && M == 11 && D == 16, "data da hora Unix");
    gk_civil(951782400ULL, &Y, &M, &D);
    check(Y == 2000 && M == 2 && D == 29, "29/02/2000");
    printf("gatekeeper: %d/%d %s\n", ok, total, ok == total ? "ok" : "FALHOU");
    return ok == total ? 0 : 1;
}
