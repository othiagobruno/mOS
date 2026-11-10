/* Teste de host do computer use, em dois processos (fork + memória compartilhada).
 * O filho é o app de amostra. O pai é o processo de teste:
 *   (1) captura a janela e confere os pixels,
 *   (2) lista as janelas e acha a geometria,
 *   (3) CGEventPost clica o botão e o app publica o clique,
 *   (4) CGEventPost digita e o AXValue confere.
 * Compila no Linux, sem QEMU e sem login:
 *   gcc -std=c11 -Wall -Wextra -Iuserland/tests-appkit \
 *       userland/tests-appkit/computeruse_core.c userland/tests-appkit/computeruse_host.c \
 *       -o /tmp/computer-use-test && /tmp/computer-use-test
 * No mOS, CGEventPost / CGWindowListCreateImage / CGWindowListCopyWindowInfo / AX* são os
 * símbolos de cgevent.c, screencapturekit.c e appkit_ax.c. Aqui os mesmos nomes chamam o
 * modelo, porque este host não tem o compositor. */
#define _GNU_SOURCE
#include "computeruse_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

static int ok, total;
static cu_world *G;

static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

static int wait_eq(int slot, int want) {
    for (int i = 0; i < 2000; i++) {
        if (cu_load(G, slot) == want) return 1;
        usleep(1000);
    }
    return 0;
}

/* Os nomes são os das APIs. O corpo é o modelo do harness. */
static void *CGEventCreateMouseEvent(void *src, unsigned type, int x, int y, unsigned button) {
    (void)src; (void)button;
    int *e = malloc(4 * sizeof(int));
    e[0] = (int)type; e[1] = x; e[2] = y; e[3] = 0;
    return e;
}
static void *CGEventCreateKeyboardEvent(void *src, unsigned key, int down) {
    (void)src;
    int *e = malloc(4 * sizeof(int));
    e[0] = down ? 10 : 11; e[1] = 0; e[2] = 0; e[3] = (int)key;
    return e;
}
static void CGEventKeyboardSetUnicodeString(void *e, unsigned n, const unsigned short *s) {
    int *ev = e;
    if (n && s) ev[3] = s[0];
}
static void *CGEventCreateScrollEvent(void *src, int dy) {
    (void)src;
    int *e = malloc(4 * sizeof(int));
    e[0] = 22; e[1] = 0; e[2] = 0; e[3] = dy;
    return e;
}
static void CGEventPost(unsigned tap, void *e) {
    int *ev = e;
    if (ev[0] == 22) cu_post_scroll(G, (int)tap, ev[3]);
    else if (ev[0] == 10 || ev[0] == 11) cu_post_key(G, (int)tap, (unsigned)ev[3], ev[0] == 10);
    else cu_post_mouse(G, (int)tap, ev[0], ev[1], ev[2]);
    free(e);
}
static int CGWindowListCreateImage(int win, unsigned *px, int *w, int *h) {
    return cu_capture(G, win, px, w, h);
}
static int CGWindowListCopyWindowInfo(cu_info *out, int cap) { return cu_list(G, out, cap); }
static int AXUIElementCopyAttributeValue(int kind, int id, const char *attr, cu_attr *v) {
    return cu_ax_copy(G, kind, id, attr, v);
}
static int AXUIElementPerformAction(int kind, int id) { return cu_ax_press(G, kind, id); }
static int AXUIElementSetAttributeValue(int kind, int id, const char *value) {
    return cu_ax_set(G, kind, id, "AXValue", value);
}

static int child_main(void) {
    int win = cu_open_window(G, (int)getpid(), "Amostra", "Painel", 40, 30, 320, 200, 0xFF224466u);
    int btn = cu_add_button(G, win, "Enviar", 20, 40, 80, 30, 0xFFFF3300u);
    int field = cu_add_field(G, win, 20, 90, 200, 24);
    cu_open_window(G, (int)getpid(), "Outro", "Nada", 400, 30, 100, 80, 0xFF0000FFu);
    cu_store(G, CU_READY, 1);
    int last_c = 0, last_p = 0, last_s = 0, last_n = -1;
    while (!cu_load(G, CU_DONE)) {
        int c = cu_ctl_clicks(G, btn);
        if (c != last_c) { last_c = c; cu_store(G, CU_APP_CLICKS, c); }
        int p = cu_ctl_presses(G, btn);
        if (p != last_p) { last_p = p; cu_store(G, CU_APP_PRESSES, p); }
        int s = cu_win_scroll(G, win);
        if (s != last_s) { last_s = s; cu_store(G, CU_APP_SCROLL, s); }
        char tmp[80];
        int n = cu_read_value(G, field, tmp, 80);
        if (n != last_n) { last_n = n; cu_publish_text(G, tmp, n); }
        usleep(1000);
    }
    return 0;
}

static int find_kid(cu_attr *kids, const char *role, const char *title, cu_attr *box) {
    for (int i = 0; i < kids->nkids; i++) {
        cu_attr a;
        if (AXUIElementCopyAttributeValue(kids->kid_kind[i], kids->kid_id[i], "AXRole", &a)) continue;
        if (strcmp(a.str, role)) continue;
        if (title) {
            cu_attr t;
            if (AXUIElementCopyAttributeValue(kids->kid_kind[i], kids->kid_id[i], "AXTitle", &t)) continue;
            if (strcmp(t.str, title)) continue;
        }
        if (AXUIElementCopyAttributeValue(kids->kid_kind[i], kids->kid_id[i], "AXPosition", box)) return 0;
        box->i = kids->kid_id[i];
        return kids->kid_kind[i];
    }
    return 0;
}

static void parent_main(int child_pid) {
    check(wait_eq(CU_READY, 1), "o app de amostra publicou a janela");

    unsigned *px = calloc((size_t)CU_W * CU_H, sizeof(unsigned));
    int iw = 0, ih = 0;
    check(CGWindowListCreateImage(1, px, &iw, &ih) == -25211, "captura negada sem kTCCServiceScreenCapture");
    check(cu_post_mouse(G, 0, 1, 100, 85) == -25211, "CGEventPost negado sem kTCCServiceAccessibility");
    check(cu_load(G, CU_APP_CLICKS) == 0, "o clique negado não mudou o app");

    /* O mesmo papel de cx_tcc_decide: a Interface concede os dois serviços. */
    cu_grant(G, "kTCCServiceScreenCapture");
    cu_grant(G, "kTCCServiceAccessibility");

    cu_info wins[4];
    int n = CGWindowListCopyWindowInfo(wins, 4);
    check(n == 2, "CGWindowListCopyWindowInfo devolve as duas janelas");
    int target = -1;
    for (int i = 0; i < n; i++)
        if (!strcmp(wins[i].owner, "Amostra") && !strcmp(wins[i].name, "Painel")) target = i;
    check(target >= 0, "a lista acha a janela do app alvo");
    if (target >= 0) {
        check(wins[target].pid == child_pid, "OwnerPID é o do outro processo");
        check(wins[target].x == 40 && wins[target].y == 30 && wins[target].w == 320 && wins[target].h == 200,
              "kCGWindowBounds da janela alvo");
        check(wins[target].onscreen == 1, "kCGWindowIsOnscreen");
        int other = target == 0 ? 1 : 0;
        check(n == 2 && (wins[other].x != 40 || wins[other].w != 320), "a outra janela não é confundida com a alvo");
    } else {
        check(0, "OwnerPID é o do outro processo");
        check(0, "kCGWindowBounds da janela alvo");
        check(0, "kCGWindowIsOnscreen");
        check(0, "a outra janela não é confundida com a alvo");
    }

    int win_id = target >= 0 ? wins[target].id : 0;
    iw = ih = 0;
    check(CGWindowListCreateImage(win_id, px, &iw, &ih) == 0, "CGWindowListCreateImage da janela alvo");
    check(iw == 320 && ih == 200, "a captura tem o tamanho da janela, não o da tela");
    int local_x = 20 + 40, local_y = 40 + 15;
    unsigned button_px = (iw == 320 && ih == 200) ? px[(unsigned)local_y * (unsigned)iw + (unsigned)local_x] : 0;
    unsigned corner_px = (iw == 320 && ih == 200) ? px[0] : 0;
    check(button_px == 0xFFFF3300u, "o pixel do botão é o vermelho pintado pelo outro app");
    check(corner_px == 0xFF224466u, "o pixel do fundo da janela é o do outro app");
    check(button_px != 0xFF0000FFu, "o pixel do botão não é o da janela decoy");

    cu_attr kids, box, field_box;
    memset(&box, 0, sizeof box);
    memset(&field_box, 0, sizeof field_box);
    int st = AXUIElementCopyAttributeValue(CU_KIND_WIN, win_id, "AXChildren", &kids);
    check(st == 0 && kids.nkids == 2, "AXChildren da janela alvo");
    int btn_kind = find_kid(&kids, "AXButton", "Enviar", &box);
    int field_kind = find_kid(&kids, "AXTextField", 0, &field_box);
    check(btn_kind == CU_KIND_CTL, "AXRole do botão é AXButton");
    check(field_kind == CU_KIND_CTL, "AXRole do campo é AXTextField");
    check(box.x == 60 && box.y == 70 && box.w == 80 && box.h == 30, "AXPosition/AXSize do botão");

    int cx = box.x + box.w / 2, cy = box.y + box.h / 2;
    CGEventPost(0 /* kCGHIDEventTap */, CGEventCreateMouseEvent(0, 1, cx, cy, 0));
    CGEventPost(0, CGEventCreateMouseEvent(0, 2, cx, cy, 0));
    check(wait_eq(CU_APP_CLICKS, 1), "o app alvo viu o clique do CGEventPost");

    CGEventPost(1 /* kCGSessionEventTap */, CGEventCreateScrollEvent(0, 3));
    check(wait_eq(CU_APP_SCROLL, 3), "scroll no tap de sessão chegou na janela sob o cursor");

    int fx = field_box.x + field_box.w / 2, fy = field_box.y + field_box.h / 2;
    CGEventPost(1, CGEventCreateMouseEvent(0, 1, fx, fy, 0));
    CGEventPost(1, CGEventCreateMouseEvent(0, 2, fx, fy, 0));
    unsigned short oi[2] = {'O', 'i'};
    for (int i = 0; i < 2; i++) {
        void *down = CGEventCreateKeyboardEvent(0, 0, 1);
        CGEventKeyboardSetUnicodeString(down, 1, &oi[i]);
        CGEventPost(0, down);
        CGEventPost(0, CGEventCreateKeyboardEvent(0, oi[i], 0));
    }
    check(wait_eq(CU_APP_TEXT_N, 2), "o app alvo recebeu o texto");
    char published[80];
    cu_copy_text(G, published, 80);
    check(strcmp(published, "Oi") == 0, "o estado do campo no app é Oi");
    cu_attr val;
    st = AXUIElementCopyAttributeValue(field_kind, field_box.i, "AXValue", &val);
    check(st == 0 && strcmp(val.str, "Oi") == 0, "AXValue do campo é Oi");

    check(AXUIElementPerformAction(btn_kind, box.i) == 0, "AXPress no botão");
    check(wait_eq(CU_APP_PRESSES, 1), "o app alvo viu o AXPress");
    check(AXUIElementSetAttributeValue(field_kind, field_box.i, "Zed") == 0, "AXSetAttributeValue no campo");
    check(wait_eq(CU_APP_TEXT_N, 3), "o app alvo viu o valor escrito por AX");
    st = AXUIElementCopyAttributeValue(field_kind, field_box.i, "AXValue", &val);
    check(st == 0 && strcmp(val.str, "Zed") == 0, "AXValue depois do set é Zed");

    free(px);
    cu_store(G, CU_DONE, 1);
}

int main(void) {
    size_t bytes = cu_world_bytes_size();
    void *mem = mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { perror("mmap"); return 1; }
    G = cu_world_bytes(mem);
    cu_init(G);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) { int rc = child_main(); _exit(rc); }
    parent_main((int)pid);
    int status = 1;
    waitpid(pid, &status, 0);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "o processo do app de amostra terminou");
    printf("computer-use: %d/%d ok\n", ok, total);
    return ok == total && total > 0 ? 0 : 1;
}
