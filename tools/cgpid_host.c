/* Teste de host do CGEventPostToPid (userland/libsystem/cgpid.c real, o mesmo que o cgevent.c chama).
 *   1. o tap local só entra quando o alvo é o próprio processo; nunca há fan-out para outros (antes o evento dirigido
 *      ia para o tap local e para os taps de todos os processos, como um CGEventPost global);
 *   2. todo pacote de DEV_INPUT_TO_PID leva o pid do alvo (c >> 32), em tecla, texto, clique e rolagem;
 *   3. texto vira descida e subida por caractere; Shift sobe a letra; clique leva o botão só na descida;
 *   4. pid inválido, tecla sem mapa e rolagem zero não geram pacote.
 *
 *   gcc -std=c11 -Wall -Wextra -Iuserland/libsystem userland/libsystem/cgpid.c tools/cgpid_host.c \
 *       -o /tmp/cgpid-host && /tmp/cgpid-host
 */
#include "cgpid.h"
#include <stdio.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}
static unsigned type_of(cgpid_pkt p) { return (unsigned)(p.b & 0xFF); }
static unsigned buttons_of(cgpid_pkt p) { return (unsigned)((p.b >> 8) & 7); }
static int wheel_of(cgpid_pkt p) { return (int)(p.b >> 32); }
static int key_of(cgpid_pkt p) { return (int)(unsigned)p.c; }
static int pid_of(cgpid_pkt p) { return (int)(p.c >> 32); }
static int x_of(cgpid_pkt p) { return (int)(unsigned)p.d; }
static int y_of(cgpid_pkt p) { return (int)(p.d >> 32); }

int main(void) {
    enum { SELF = 100, ALVO = 4321 };
    cgpid_pkt pk[64];
    int n, allpid;

    printf("1. taps\n");
    check(!cx_cgpid_taps(ALVO, SELF), "evento para outro processo não passa por tap");
    check(cx_cgpid_taps(SELF, SELF), "evento para o próprio processo passa pelo tap local");
    check(!cx_cgpid_taps(0, SELF) && !cx_cgpid_taps(-1, SELF), "pid inválido não abre tap");

    printf("2. tecla\n");
    cgpid_ev k = {10, 0, 'a', 0, 0, 0, 0, 0};
    n = cx_cgpid_packets(&k, ALVO, pk, 64);
    check(n == 1 && type_of(pk[0]) == 10 && key_of(pk[0]) == 'a' && pid_of(pk[0]) == ALVO, "keyDown 'a' vai ao pid alvo");
    k.mods = 1;
    n = cx_cgpid_packets(&k, ALVO, pk, 64);
    check(n == 1 && key_of(pk[0]) == 'A' && ((pk[0].b >> 16) & 0xFFFF) == 1, "com Shift a letra sobe e o modificador viaja");
    k.key = -1;
    check(cx_cgpid_packets(&k, ALVO, pk, 64) == 0, "tecla sem mapa não gera pacote");

    printf("3. texto\n");
    static const unsigned short txt[] = {'o', 'l', 0xE1, '\n', 0x4E2D};   /* "olá\n" e um CJK fora do Latin-1 */
    cgpid_ev t = {10, 0, -1, txt, 5, 0, 0, 0};
    n = cx_cgpid_packets(&t, ALVO, pk, 64);
    allpid = 1;
    for (int i = 0; i < n; i++) if (pid_of(pk[i]) != ALVO) allpid = 0;
    check(n == 8, "quatro caracteres Latin-1 viram oito pacotes (o CJK fica de fora, como antes)");
    check(type_of(pk[0]) == 10 && type_of(pk[1]) == 11 && key_of(pk[4]) == 0xE1 && key_of(pk[6]) == 10, "descida e subida, 'á' e Enter");
    check(allpid, "todos com o pid alvo");
    t.type = 11;
    check(cx_cgpid_packets(&t, ALVO, pk, 64) == 0, "keyUp com texto não repete o texto");

    printf("4. mouse\n");
    cgpid_ev m = {1, 0, -1, 0, 0, 0, 300, 200};
    n = cx_cgpid_packets(&m, ALVO, pk, 64);
    check(n == 1 && type_of(pk[0]) == 1 && buttons_of(pk[0]) == 1 && x_of(pk[0]) == 300 && y_of(pk[0]) == 200 && pid_of(pk[0]) == ALVO,
          "leftMouseDown em (300, 200) com o botão 1, no pid alvo");
    m.type = 2;
    n = cx_cgpid_packets(&m, ALVO, pk, 64);
    check(n == 1 && buttons_of(pk[0]) == 0, "leftMouseUp sem botão apertado");
    m.type = 3;
    n = cx_cgpid_packets(&m, ALVO, pk, 64);
    check(n == 1 && buttons_of(pk[0]) == 2, "rightMouseDown com o botão 2");
    m.type = 5;
    n = cx_cgpid_packets(&m, ALVO, pk, 64);
    check(n == 1 && type_of(pk[0]) == 5 && buttons_of(pk[0]) == 0, "mouseMoved");
    m.type = 99;
    check(cx_cgpid_packets(&m, ALVO, pk, 64) == 0, "tipo desconhecido não gera pacote");

    printf("5. rolagem\n");
    cgpid_ev s = {22, 0, -1, 0, 0, -3, 10, 20};
    n = cx_cgpid_packets(&s, ALVO, pk, 64);
    check(n == 1 && wheel_of(pk[0]) == -3 && pid_of(pk[0]) == ALVO, "rolagem de -3 linhas no pid alvo");
    s.wheel = 0;
    check(cx_cgpid_packets(&s, ALVO, pk, 64) == 0, "rolagem zero não gera pacote");

    printf("6. pid\n");
    check(cx_cgpid_packets(&m, 0, pk, 64) == 0 && cx_cgpid_packets(&k, -5, pk, 64) == 0, "pid 0 ou negativo não gera pacote");
    cgpid_ev big = {10, 0, -1, txt, 2, 0, 0, 0};
    check(cx_cgpid_packets(&big, ALVO, pk, 3) == 3, "o teto de pacotes de quem chama é respeitado");

    printf("%d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
