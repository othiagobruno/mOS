#include <string.h>
/* Teste de host do núcleo do Carbon (EventRef, tipos dos tratadores, HIShape). Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -Wno-multichar -Iuserland/libsystem userland/libsystem/carbon_core.c \
 *       userland/tests-appkit/carbon_host.c -o /tmp/carbon-test && /tmp/carbon-test */
#include "carbon_core.h"
#include <stdio.h>
#include <stdlib.h>
void *cc_alloc(cc_size n) { return calloc(1, n); }
void cc_free(void *p) { free(p); }
static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }
static int near(double a, double b) { return a - b < 1e-9 && b - a < 1e-9; }
int main(void) {
    cc_event *e = cc_event_new('keyb', 5, 1.5, 0);
    unsigned v = 42, got = 0, t = 0; cc_size sz = 0;
    check(cc_event_set(e, 'kcod', 'magn', 4, &v) == 0, "parâmetro guardado");
    check(cc_event_get(e, 'kcod', 'magn', &t, 4, &sz, &got) == 0 && got == 42 && t == 'magn' && sz == 4, "parâmetro lido");
    check(cc_event_get(e, 'kcod', 'long', 0, 4, 0, &got) == -1700, "tipo errado: errAECoercionFail");
    got = 0;
    check(cc_event_get(e, 'kcod', '****', &t, 4, 0, &got) == 0 && got == 42, "tipo curinga aceita");
    check(cc_event_get(e, 'none', 'magn', 0, 4, 0, &got) == -9870, "sem parâmetro: eventParameterNotFoundErr");
    v = 7; cc_event_set(e, 'kcod', 'magn', 4, &v);
    cc_event_get(e, 'kcod', 'magn', 0, 4, 0, &got);
    check(got == 7, "mesmo nome substitui");
    cc_event *c = cc_event_copy(e);
    got = 0; cc_event_get(c, 'kcod', 'magn', 0, 4, 0, &got);
    check(c && c != e && got == 7 && c->cls == 'keyb' && c->kind == 5 && near(c->time, 1.5), "cópia independente");
    e->refs++; cc_event_release(e); check(e->refs == 1, "retain/release conta");
    cc_event_release(e); cc_event_release(c);
    cc_specs s = {0, 0, 0};
    cc_spec a[2] = {{'keyb', 5}, {'keyb', 6}}, b[1] = {{'mous', 1}};
    cc_specs_add(&s, a, 2); cc_specs_add(&s, a, 1);
    check(s.n == 2 && cc_specs_match(&s, 'keyb', 6) && !cc_specs_match(&s, 'keyb', 7), "tipos somados sem repetir");
    cc_specs_add(&s, b, 1);
    check(cc_specs_remove(&s, a, 1) == 0 && !cc_specs_match(&s, 'keyb', 5) && cc_specs_match(&s, 'mous', 1), "remove um tipo");
    check(cc_specs_remove(&s, a, 1) == -50, "remover o que não está: paramErr");
    cc_specs_free(&s);
    cc_shape A = {0, 0, 0}, B = {0, 0, 0}, R = {0, 0, 0};
    cc_shape_set_rect(&A, (cc_rect){0, 0, 10, 10});
    cc_shape_set_rect(&B, (cc_rect){5, 5, 10, 10});
    cc_shape_union(&R, &A, &B);
    check(near(cc_shape_area(&R), 175) && !cc_shape_rectangular(&R), "união em L: área 175, não retangular");
    cc_rect bb = cc_shape_bounds(&R);
    check(near(bb.x, 0) && near(bb.w, 15) && near(bb.h, 15), "limites da união");
    cc_shape_intersect(&R, &A, &B);
    check(near(cc_shape_area(&R), 25) && cc_shape_rectangular(&R), "interseção 5x5");
    cc_shape_diff(&R, &A, &B);
    check(near(cc_shape_area(&R), 75) && cc_shape_contains(&R, 1, 1) && !cc_shape_contains(&R, 7, 7), "diferença tira o canto");
    cc_shape_xor(&R, &A, &B);
    check(near(cc_shape_area(&R), 150), "xor = união − interseção");
    cc_shape_union(&R, &A, &B);
    check(cc_shape_intersects_rect(&R, (cc_rect){12, 12, 1, 1}) && !cc_shape_intersects_rect(&R, (cc_rect){12, 1, 1, 1}), "toca retângulo");
    cc_shape_offset(&R, 100, 0);
    check(cc_shape_contains(&R, 101, 1) && !cc_shape_contains(&R, 1, 1), "desloca");
    cc_shape_set_rect(&R, (cc_rect){0, 0, 10, 10});
    cc_shape_inset(&R, 2, 3);
    bb = cc_shape_bounds(&R);
    check(near(bb.x, 2) && near(bb.y, 3) && near(bb.w, 6) && near(bb.h, 4) && cc_shape_rectangular(&R), "inset de retângulo exato");
    cc_shape_inset(&R, -2, -3);
    check(near(cc_shape_area(&R), 100), "inset negativo volta ao tamanho");
    cc_shape_union(&R, &A, &B);
    cc_shape_inset(&R, 1, 1);
    check(!cc_shape_contains(&R, 0.5, 0.5) && cc_shape_contains(&R, 1.5, 1.5) && !cc_shape_contains(&R, 10.5, 1.5), "inset de L erode as bordas");
    cc_shape_clear(&R);
    check(cc_shape_empty(&R) && cc_shape_rectangular(&R), "vazio");
    cc_thumb th = cc_track_thumb(0, 200, 0, 100, 50, 100);
    check(near(th.len, 100) && near(th.pos, 50), "rolagem: polegar proporcional à parte visível");
    th = cc_track_thumb(0, 200, 0, 100000, 0, 1);
    check(near(th.len, 16) && near(th.pos, 0), "rolagem: polegar mínimo de 16");
    th = cc_track_thumb(1, 115, 0, 10, 10, 0);
    check(near(th.len, 15) && near(th.pos, 100), "slider: polegar no fim");
    th = cc_track_thumb(2, 200, 0, 1, 0.25, 0);
    check(near(th.len, 50), "progresso: preenchido proporcional");
    check(cc_track_hit(0, 200, 0, 100, 50, 100, 10) == 22 && cc_track_hit(0, 200, 0, 100, 50, 100, 120) == 129 &&
          cc_track_hit(0, 200, 0, 100, 0, 100, 150) == 23 && cc_track_hit(0, 200, 0, 100, 0, 100, 250) == 0, "acerto: página, polegar, fora");
    unsigned char fam[64], f2[96], f3[96];
    long fl = cc_icns_set(0, 0, 0x69633037u, (const unsigned char *)"PNGDATA", 7, fam);   /* 'ic07' */
    long off = 0, len = 0;
    check(fl == 23 && cc_icns_find(fam, fl, 0x69633037u, &off, &len) && len == 7 && !memcmp(fam + off, "PNGDATA", 7), "icns: família nova com um elemento");
    long f2l = cc_icns_set(fam, fl, 0x69633038u, (const unsigned char *)"AB", 2, f2);
    check(f2l == 33 && cc_icns_find(f2, f2l, 0x69633038u, &off, &len) && len == 2 && cc_icns_find(f2, f2l, 0x69633037u, 0, 0), "icns: acrescenta");
    long f3l = cc_icns_set(f2, f2l, 0x69633037u, (const unsigned char *)"X", 1, f3);
    check(f3l == 27 && cc_icns_find(f3, f3l, 0x69633037u, &off, &len) && len == 1 && f3[off] == 'X' && cc_icns_find(f3, f3l, 0x69633038u, 0, 0), "icns: troca sem perder os outros");
    check(!cc_icns_find(f3, f3l, 0x69633130u, 0, 0) && !cc_icns_find((const unsigned char *)"xxxxxxxx", 8, 0x69633037u, 0, 0), "icns: ausente e inválido");
    printf("%d/%d\n", ok, total);
    return ok == total ? 0 : 1;
}
