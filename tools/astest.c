/* mOS — teste de host do interpretador de AppleScript (userland/libsystem/as_interp.inc) e dos descritores de Apple
 * Event (ae_core.inc e a API C de ae_capi.inc). As ações do sistema são falsas e registram o que o script pediu.
 *   gcc -std=gnu11 -O1 -Wall -Wno-unused-function -Iuserland/libsystem tools/astest.c -o /tmp/astest && /tmp/astest */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;
typedef int16_t i16;
typedef uint64_t u64;
typedef int64_t i64;
typedef size_t usize;
#include <stddef.h>
#include "ae_core.inc"
#include "ae_capi.inc"
#include <stdarg.h>
/* Memory Manager mínimo para o AEBuild/AEPrint (no libSystem é o de cs_legacy.c) */
typedef struct { char *p; long n; } th_t;
char **NewHandle(long n) { th_t *h = calloc(1, sizeof *h); h->p = calloc(1, (size_t)(n ? n : 1)); h->n = n; return &h->p; }
long GetHandleSize(char **h) { return h ? ((th_t *)h)->n : 0; }
#include "ae_build.inc"
#include "as_interp.inc"

static int ok, total;
static void check(int c, const char *what) { total++; if (c) ok++; else printf("FALHA: %s\n", what); }

static char g_events[4096], g_log[4096], g_clip[256] = "copiado", g_notif[512], g_keys[256];
static int f_send(void *x, const char *app, unsigned cls, unsigned id, const as_val *d, as_val *r, char *err, int cap) {
    (void)x;
    char *t = d ? as_to_text(d) : strdup("-");
    size_t l = strlen(g_events);
    snprintf(g_events + l, sizeof g_events - l, "[%s %c%c%c%c/%c%c%c%c %s]", app, cls >> 24, cls >> 16 & 255, cls >> 8 & 255, cls & 255,
             id >> 24, id >> 16 & 255, id >> 8 & 255, id & 255, t);
    free(t);
    if (!strcmp(app, "Inexistente")) { snprintf(err, (size_t)cap, "Aplicativo não encontrado."); return -600; }
    *r = as_missing();
    return 0;
}
static int f_shell(void *x, const char *cmd, char **out, int *st) {
    (void)x;
    if (!strcmp(cmd, "falha")) { *out = strdup("deu ruim"); *st = 1; return 0; }
    char b[512];
    snprintf(b, sizeof b, "saida de <%s>\n", cmd);
    *out = strdup(b);
    *st = 0;
    return 0;
}
static int f_dialog(void *x, const char *t, const char *title, const char **b, int nb, int def, int gu, const char *ain, char **aout) {
    (void)x; (void)title; (void)gu;
    if (strstr(t, "cancela")) return -1;
    if (aout) *aout = strdup(ain && ain[0] ? "resposta" : "");
    return def >= 0 ? def : nb - 1;
    (void)b;
}
static void f_notify(void *x, const char *t, const char *ti, const char *s, const char *snd) { (void)x; (void)snd; snprintf(g_notif, sizeof g_notif, "%s|%s|%s", t, ti ? ti : "", s ? s : ""); }
static char *f_clip_get(void *x) { (void)x; return strdup(g_clip); }
static void f_clip_set(void *x, const char *t) { (void)x; snprintf(g_clip, sizeof g_clip, "%s", t); }
static int f_keys(void *x, const char *t, int code, unsigned mods) { (void)x; size_t l = strlen(g_keys); snprintf(g_keys + l, sizeof g_keys - l, "[%s %d %x]", t ? t : "", code, mods); return 0; }
static char *f_app(void *x, const char *app, const char *what) {
    (void)x;
    if (!strcmp(app, "Inexistente")) return 0;
    if (!strcmp(what, "id")) return strdup("com.exemplo.app");
    if (!strcmp(what, "version")) return strdup("2.1");
    if (!strcmp(what, "running")) return strdup(!strcmp(app, "Finder") ? "true" : "false");
    if (!strcmp(what, "path")) return strdup("/Aplicativos/X.app");
    if (!strcmp(what, "name-for-id")) return strdup("Safari");
    return strdup(app);
}
static char *f_path(void *x, const char *w) { (void)x; if (!strcmp(w, "desktop")) return strdup("/Users/Visitante/Desktop/"); if (!strcmp(w, "home folder")) return strdup("/Users/Visitante/"); return 0; }
static char **f_procs(void *x, int *n) { (void)x; char **p = malloc(2 * sizeof *p); p[0] = strdup("Finder"); p[1] = strdup("Terminal"); *n = 2; return p; }
static void f_log(void *x, const char *t) { (void)x; size_t l = strlen(g_log); snprintf(g_log + l, sizeof g_log - l, "%s;", t); }
static double f_now(void *x) { (void)x; return 816436800.0; }
static as_host H = {0, f_send, f_shell, f_dialog, f_notify, f_clip_get, f_clip_set, f_keys, f_app, f_path, f_procs, 0, f_log, 0, 0, f_now};

static char *run(const char *src, int *code) {
    as_val r;
    char err[400] = "";
    int line = 0, col = 0;
    int rc = as_run(src, &H, &r, err, sizeof err, &line, &col);
    if (code) *code = rc;
    if (rc) { char *e = malloc(500); snprintf(e, 500, "ERRO %d: %s", rc, err); return e; }
    char *t = as_to_text(&r);
    as_free(&r);
    return t;
}
static void eq(const char *src, const char *want) {
    char *g = run(src, 0);
    char what[700];
    snprintf(what, sizeof what, "%s  =>  [%s] (esperado [%s])", src, g, want);
    check(!strcmp(g, want), what);
    free(g);
}

/* ---- AEBuild, AEPrint, AEStream, AEResolve */
static int acc_calls;
static int acc_doc(u32 cls, const AEDesc *c, u32 ccls, u32 form, const AEDesc *data, AEDesc *tok, long refcon) {
    (void)c; (void)form; (void)refcon;
    acc_calls++;
    if (cls != AE_FCC('d', 'o', 'c', 'u') || ccls != AE_NULL) return -1728;
    i32 idx = 0; u32 ty; long got;
    AEGetDescData(data, &idx, 4); (void)ty; (void)got;
    char name[16]; snprintf(name, sizeof name, "doc%d", idx);
    return AECreateDesc(AE_FCC('D', 'O', 'C', 'T'), name, (long)strlen(name), tok);
}
static int acc_word(u32 cls, const AEDesc *c, u32 ccls, u32 form, const AEDesc *data, AEDesc *tok, long refcon) {
    (void)form; (void)data; (void)refcon;
    acc_calls++;
    if (cls != AE_FCC('c', 'w', 'o', 'r') || ccls != AE_FCC('d', 'o', 'c', 'u') || c->descriptorType != AE_FCC('D', 'O', 'C', 'T')) return -1728;
    char buf[32] = {0};
    AEGetDescData(c, buf, sizeof buf - 1);
    strcat(buf, ":palavra");
    return AECreateDesc(AE_TEXT, buf, (long)strlen(buf), tok);
}
static int coerce_hits;
static int my_coerce(u32 from, const void *p, long n, u32 to, long refcon, AEDesc *r) {
    (void)from; (void)to; (void)refcon; coerce_hits++;
    char b[64]; snprintf(b, sizeof b, "[%.*s]", (int)n, (const char *)p);
    return AECreateDesc(AE_FCC('B', 'R', 'K', 'T'), b, (long)strlen(b), r);
}
static char *printed(const AEDesc *d) { char **h = 0; if (AEPrintDescToHandle(d, &h)) return strdup("?"); return strdup(*h); }
static void aebuild_tests(void) {
    AEDesc d = {0, 0}, x = {0, 0};
    AEBuildError e = {0, 0};
    check(AEBuildDesc(&d, &e, "[1, -2, 'TEXT'(@), {kfoo: \"oi\", bar: abcd}]", 3L, "abc") == 0, "AEBuildDesc lista com @, registro e código");
    long c = 0; AECountItems(&d, &c);
    char *s = printed(&d);
    check(c == 4 && !strcmp(s, "[1, -2, \xe2\x80\x9c" "abc" "\xe2\x80\x9d, {kfoo:\xe2\x80\x9c" "oi" "\xe2\x80\x9d, bar:abcd}]"), "AEPrint da lista");
    if (strcmp(s, "[1, -2, \xe2\x80\x9c" "abc" "\xe2\x80\x9d, {kfoo:\xe2\x80\x9c" "oi" "\xe2\x80\x9d, bar:abcd}]")) printf("  impresso: %s\n", s);
    free(s); AEDisposeDesc(&d);
    check(AEBuildDesc(&d, &e, "'shor'(7)") == 0 && d.descriptorType == AE_SHOR && AEGetDescDataSize(&d) == 2, "coerção de inteiro para shor");
    AEDisposeDesc(&d);
    check(AEBuildDesc(&d, &e, "'utf8'(\xc2\xab" "41 42" "\xc2\xbb)") == 0 && d.descriptorType == AE_UTF8 && AEGetDescDataSize(&d) == 2, "hex coagido");
    AEDisposeDesc(&d);
    check(AEBuildDesc(&d, &e, "\xc2\xab" "41" "\xc2\xbb") == errAEBuildSyntaxError && e.fError == aeBuildSyntaxUncoercedHex, "hex sem coerção = erro 9");
    check(AEBuildDesc(&d, &e, "[1, 2") == errAEBuildSyntaxError && e.fError == aeBuildSyntaxNoCloseBracket, "lista sem ] = erro 14");
    check(AEBuildDesc(&d, &e, "{a: 1 b: 2}") == errAEBuildSyntaxError && e.fError == aeBuildSyntaxNoCloseBrace && e.fErrorPos == 6, "registro sem vírgula: erro 15 na posição 6");
    char **h = NewHandle(3); memcpy(*h, "xyz", 3);
    AEDesc inner = {0, 0}; AECreateDesc(AE_LONG, &(i32){42}, 4, &inner);
    check(AEBuildDesc(&d, &e, "{a: 'TEXT'(@@), b: @}", h, &inner) == 0, "@@ (Handle) e @ (AEDesc*)");
    char buf[8] = {0}; u32 ty; long got = 0; i32 v = 0;
    AEGetKeyPtr(&d, AE_FCC('a', ' ', ' ', ' '), AE_TEXT, &ty, buf, 7, &got);
    AEGetKeyPtr(&d, AE_FCC('b', ' ', ' ', ' '), AE_LONG, &ty, &v, 4, &got);
    check(!strcmp(buf, "xyz") && v == 42, "valores de @@ e @");
    AEDisposeDesc(&d);
    check(AEBuildDesc(&d, &e, "'TEXT'(@@)", (char **)0) == 0, "@@ com Handle nulo");
    AEDisposeDesc(&d);
    check(AEBuildDesc(&d, &e, "@@", h) == errAEBuildSyntaxError && e.fError == aeBuildSyntaxUncoercedDoubleAt, "@@ sem coerção = erro 19");
    u32 sig = AE_FCC('M', 'A', 'C', 'S');
    check(AEBuildAppleEvent(AE_FCC('a', 'e', 'v', 't'), AE_FCC('o', 'd', 'o', 'c'), AE_FCC('s', 'i', 'g', 'n'), &sig, 4, -1, 0, &x, &e,
                            "'----':[\"/tmp/a\", \"/tmp/b\"], ~prdt:'TEXT'(@)", 2L, "hi") == 0, "AEBuildAppleEvent com parâmetros");
    s = printed(&x);
    check(strstr(s, "aevt\\odoc{") == s && strstr(s, "'----':[") && strstr(s, "prdt:"), "AEPrint do evento");
    free(s);
    check(AEBuildParameters(&x, &e, "kmsg: 5") == 0 && AEGetParamPtr(&x, AE_FCC('k', 'm', 's', 'g'), AE_LONG, &ty, &v, 4, &got) == 0 && v == 5, "AEBuildParameters acrescenta");
    AEDisposeDesc(&x);
    /* AEStream: evento com lista e registro aninhados */
    AEStream *st = AEStreamCreateEvent(AE_FCC('c', 'o', 'r', 'e'), AE_FCC('s', 'e', 't', 'd'), AE_FCC('s', 'i', 'g', 'n'), &sig, 4, -1, 0);
    check(st != 0, "AEStreamCreateEvent");
    AEStreamWriteKey(st, AE_FCC('-', '-', '-', '-'));
    AEStreamOpenList(st);
    AEStreamWriteDesc(st, AE_LONG, &(i32){1}, 4);
    AEStreamOpenRecord(st, AE_RECO);
    AEStreamWriteKeyDesc(st, AE_FCC('n', 'a', 'm', 'e'), AE_TEXT, "x", 1);
    AEStreamCloseRecord(st);
    AEStreamCloseList(st);
    AEStreamOpenKeyDesc(st, AE_FCC('d', 'a', 't', 'a'), AE_TEXT);
    AEStreamWriteData(st, "ab", 2); AEStreamWriteData(st, "cd", 2);
    AEStreamCloseDesc(st);
    AEStreamOptionalParam(st, AE_FCC('d', 'a', 't', 'a'));
    check(AEStreamClose(st, &x) == 0, "AEStreamClose");
    AEDesc lst = {0, 0};
    check(AEGetParamDesc(&x, AE_FCC('-', '-', '-', '-'), AE_LIST, &lst) == 0 && (AECountItems(&lst, &c), c == 2), "lista do stream com 2 itens");
    memset(buf, 0, sizeof buf);
    check(AEGetParamPtr(&x, AE_FCC('d', 'a', 't', 'a'), AE_TEXT, &ty, buf, 7, &got) == 0 && !strcmp(buf, "abcd"), "OpenDesc/WriteData em pedaços");
    AEDisposeDesc(&lst); AEDisposeDesc(&x);
    st = AEStreamOpen();
    AEStreamOpenList(st);
    check(AEStreamClose(st, &x) == errAECorruptData, "fechar com lista aberta = erro");
    /* AEResolve: palavra 1 do documento 2 */
    AEObjectInit();
    AEInstallObjectAccessor(AE_FCC('d', 'o', 'c', 'u'), AE_NULL, acc_doc, 0, 0);
    AEInstallObjectAccessor(AE_FCC('c', 'w', 'o', 'r'), AE_FCC('D', 'O', 'C', 'T'), acc_word, 0, 0);
    check(AEBuildDesc(&d, &e, "'obj '{want: type(cwor), from: 'obj '{want: type(docu), from: 'null'(), form: enum(indx), seld: 2}, form: enum(indx), seld: 1}") == 0
          && d.descriptorType == AE_FCC('o', 'b', 'j', ' '), "especificador aninhado pelo AEBuild");
    AEDesc tok = {0, 0};
    int re = AEResolve(&d, 0, &tok);
    memset(buf, 0, sizeof buf);
    char tb[32] = {0};
    AEGetDescData(&tok, tb, sizeof tb - 1);
    check(re == 0 && acc_calls == 2 && !strcmp(tb, "doc2:palavra"), "AEResolve chama os 2 acessores de fora para dentro");
    if (re || strcmp(tb, "doc2:palavra")) printf("  AEResolve: %d '%s' chamadas %d\n", re, tb, acc_calls);
    AEDisposeDesc(&d); AEDisposeToken(&tok);
    /* handler de coerção do app */
    AEInstallCoercionHandler(AE_TEXT, AE_FCC('B', 'R', 'K', 'T'), (void *)my_coerce, 0, 0, 0);
    AECreateDesc(AE_TEXT, "oi", 2, &d);
    memset(tb, 0, sizeof tb);
    check(AECoerceDesc(&d, AE_FCC('B', 'R', 'K', 'T'), &x) == 0 && coerce_hits == 1 && (AEGetDescData(&x, tb, 31), !strcmp(tb, "[oi]")), "AEInstallCoercionHandler é usado pelo AECoerceDesc");
    AEDisposeDesc(&d); AEDisposeDesc(&x);
    AEDesc r1 = {0, 0}, r2 = {0, 0};
    CreateOffsetDescriptor(3, &r1); CreateOffsetDescriptor(5, &r2);
    check(CreateRangeDescriptor(&r1, &r2, 1, &d) == 0 && d.descriptorType == AE_FCC('r', 'a', 'n', 'g'), "CreateRangeDescriptor");
    AEDisposeDesc(&d);
    AEDesc arr[2] = {{0, 0}, {0, 0}};
    AECreateDesc(AE_LONG, &(i32){7}, 4, &arr[0]); AECreateDesc(AE_TEXT, "z", 1, &arr[1]);
    AECreateList(0, 0, 0, &d);
    check(AEPutArray(&d, 3, arr, 0, 0, 2) == 0 && (AECountItems(&d, &c), c == 2), "AEPutArray de AEDesc");
    AEDisposeDesc(&d);
}

int main(void) {
    /* ---------------- descritores */
    ae_node *ev = ae_event(AE_FCC('a', 'e', 'v', 't'), AE_FCC('o', 'd', 'o', 'c'), ae_scalar(AE_BUND, "com.apple.finder", 16), -1, 0);
    ae_node *l = ae_list();
    ae_list_put(l, 0, ae_scalar(AE_FURL, "file:///tmp/a.txt", 17));
    ae_list_put(l, 0, ae_text("olá"));
    ae_put_key(ev, AE_DIRECT, l);
    ae_put_attr(ev, AE_FCC('s', 'u', 'b', 'j'), ae_i32(7));
    u8 *wire; usize wn;
    check(ae_serialize(ev, &wire, &wn) == 0, "serializa");
    ae_node *back = ae_parse(wire, wn);
    check(back && back->ev_class == ev->ev_class && back->ev_id == ev->ev_id, "evento volta do fio");
    check(back && ae_count(ae_get_key(back, AE_DIRECT)) == 2, "lista no parâmetro direto");
    char *s2 = back ? ae_utf8_of(ae_nth(ae_get_key(back, AE_DIRECT), 2)) : 0;
    check(s2 && !strcmp(s2, "olá"), "texto UTF-8 na lista");
    free(s2);
    check(back && back->target && back->target->n == 16 && !memcmp(back->target->data, "com.apple.finder", 16), "alvo bund");
    check(ae_parse(wire, wn - 1) == 0, "fio truncado é recusado");
    ae_node *u = ae_coerce(ae_nth(l, 2), AE_UTXT), *u8b = u ? ae_coerce(u, AE_UTF8) : 0;
    check(u && u->n == 6 && u8b && u8b->n == 4 && !memcmp(u8b->data, "olá", 4), "utf8 <-> utxt");
    ae_node *x42 = ae_text("42"), *n1 = ae_coerce(x42, AE_LONG);
    i32 v42 = 0;
    if (n1) memcpy(&v42, n1->data, 4);
    check(v42 == 42, "texto -> long");
    ae_node *f25 = ae_f64(2.5), *t1 = ae_coerce(f25, AE_TEXT), *tx = ae_text("x");
    check(t1 && t1->n == 3 && !memcmp(t1->data, "2.5", 3), "doub -> TEXT");
    check(ae_coerce(tx, AE_LONG) == 0, "coerção impossível");
    check(ae_coerce(f25, AE_LONG) == 0, "2.5 não vira long");
    ae_node *one = ae_list();
    ae_list_put(one, 0, ae_i32(9));
    ae_node *c9 = ae_coerce(one, AE_TEXT);
    check(c9 && c9->n == 1 && c9->data[0] == '9', "lista de um item coage como o item");
    ae_node *rec = ae_record();
    ae_put_key(rec, AE_FCC('n', 'a', 'm', 'e'), ae_text("a"));
    ae_put_key(rec, AE_FCC('n', 'a', 'm', 'e'), ae_text("b"));
    check(ae_count(rec) == 1 && ae_get_key(rec, AE_FCC('n', 'a', 'm', 'e'))->data[0] == 'b', "registro substitui a chave");
    ae_free(ev); ae_free(back); free(wire); ae_free(u); ae_free(u8b); ae_free(x42); ae_free(n1); ae_free(f25); ae_free(t1);
    ae_free(tx); ae_free(one); ae_free(c9); ae_free(rec);

    /* ---------------- API C (AEDesc) */
    check(sizeof(AEDesc) == 12 && offsetof(AEDesc, dataHandle) == 4, "AEDesc com o layout da Apple (12 bytes)");
    AEDesc d, lst, evd, tmp, rcd, spec;
    check(AECreateDesc(AE_UTF8, "42", 2, &d) == noErr && d.descriptorType == AE_UTF8 && AEGetDescDataSize(&d) == 2, "AECreateDesc");
    check(AECoerceDesc(&d, AE_LONG, &tmp) == noErr && tmp.descriptorType == AE_LONG, "AECoerceDesc texto -> long");
    i32 iv = 0;
    AEGetDescData(&tmp, &iv, 4);
    check(iv == 42, "AEGetDescData");
    AEDisposeDesc(&tmp);
    check(tmp.descriptorType == AE_NULL && tmp.dataHandle == 0, "AEDisposeDesc zera");
    AEDesc bad;
    AECreateDesc(AE_UTF8, "x", 1, &bad);
    check(AECoerceDesc(&bad, AE_LONG, &tmp) == errAECoercionFail, "AECoerceDesc impossível");
    AEDisposeDesc(&bad);
    AECreateList(0, 0, 0, &lst);
    i32 seven = 7;
    AEPutPtr(&lst, 0, AE_LONG, &seven, 4);
    AEPutDesc(&lst, 0, &d);
    long cnt = 0;
    AECountItems(&lst, &cnt);
    u32 got_t = 0;
    long act = 0;
    i32 second = 0;
    check(cnt == 2 && AEGetNthPtr(&lst, 2, AE_LONG, 0, &got_t, &second, 4, &act) == noErr && second == 42 && act == 4, "lista e AEGetNthPtr com coerção");
    check(AEGetNthPtr(&lst, 3, AE_LONG, 0, 0, &second, 4, 0) == errAEIllegalIndex, "índice fora da lista");
    AEDesc pidd;
    int pid = 1234;
    AECreateDesc(AE_KPID, &pid, 4, &pidd);
    AECreateAppleEvent(AE_FCC('a', 'e', 'v', 't'), AE_FCC('o', 'd', 'o', 'c'), &pidd, -1, 0, &evd);
    i32 rid = 0;
    u32 evcl = 0;
    check(AEGetAttributePtr(&evd, AE_FCC('r', 't', 'i', 'd'), AE_LONG, 0, &rid, 4, 0) == noErr && rid > 0, "returnID automático");
    check(AEGetAttributePtr(&evd, AE_FCC('e', 'v', 'c', 'l'), AE_TYPE, 0, &evcl, 4, 0) == noErr && evcl == AE_FCC('a', 'e', 'v', 't'), "atributo evcl");
    check(AEPutParamDesc(&evd, AE_DIRECT, &lst) == noErr, "AEPutParamDesc");
    long psz = 0;
    u32 pty = 0;
    check(AESizeOfParam(&evd, AE_DIRECT, &pty, &psz) == noErr && pty == AE_LIST, "AESizeOfParam");
    AEDesc dup;
    AEDuplicateDesc(&evd, &dup);
    AEDeleteParam(&evd, AE_DIRECT);
    check(AEGetParamDesc(&evd, AE_DIRECT, AE_WILD, &tmp) == errAEDescNotFound, "AEDeleteParam");
    check(AEGetParamDesc(&dup, AE_DIRECT, AE_WILD, &tmp) == noErr && tmp.descriptorType == AE_LIST, "AEDuplicateDesc é independente");
    AEDisposeDesc(&tmp);
    AECreateList(0, 0, 1, &rcd);
    AEPutKeyPtr(&rcd, AE_FCC('n', 'a', 'm', 'e'), AE_UTF8, "mOS", 3);
    char nm[8] = {0};
    check(AECheckIsRecord(&rcd) && AEGetKeyPtr(&rcd, AE_FCC('n', 'a', 'm', 'e'), AE_UTF8, 0, nm, 7, 0) == noErr && !strcmp(nm, "mOS"), "registro");
    AEDesc key;
    i32 idx = 1;
    AECreateDesc(AE_LONG, &idx, 4, &key);
    check(CreateObjSpecifier(AE_FCC('c', 'w', 'i', 'n'), 0, AE_FCC('i', 'n', 'd', 'x'), &key, 1, &spec) == noErr && spec.descriptorType == AE_FCC('o', 'b', 'j', ' ')
          && key.dataHandle == 0, "CreateObjSpecifier");
    u32 want = 0;
    check(AEGetKeyPtr(&spec, AE_FCC('w', 'a', 'n', 't'), AE_TYPE, 0, &want, 4, 0) == noErr && want == AE_FCC('c', 'w', 'i', 'n'), "obj: want");
    AEDisposeDesc(&d); AEDisposeDesc(&lst); AEDisposeDesc(&evd); AEDisposeDesc(&dup); AEDisposeDesc(&rcd); AEDisposeDesc(&spec);
    AEDisposeDesc(&pidd);

    /* ---------------- linguagem */
    eq("1 + 2 * 3", "7");
    eq("10 / 4", "2.5");
    eq("7 div 2", "3");
    eq("7 mod 3", "1");
    eq("2 ^ 10", "1024.0");
    eq("\"a\" & \"b\" & 3", "ab3");
    eq("{1, 2} & 3", "1, 2, 3");
    eq("set x to 5\nset x to x + 1\nx", "6");
    eq("length of \"olá mundo\"", "9");
    eq("count of {1, 2, 3}", "3");
    eq("item 2 of {\"a\", \"b\", \"c\"}", "b");
    eq("item -1 of {\"a\", \"b\", \"c\"}", "c");
    eq("items 2 thru 3 of {1, 2, 3, 4}", "2, 3");
    eq("character 1 of \"Brasil\"", "B");
    eq("characters 2 thru 4 of \"Brasil\"", "ras");
    eq("words of \"um dois, três\"", "um, dois, três");
    eq("paragraph 2 of (\"a\" & return & \"b\")", "b");
    eq("set AppleScript's text item delimiters to \",\"\ntext items of \"a,b,c\"", "a, b, c");
    eq("set text item delimiters to \"-\"\n{\"a\", \"b\"} as text", "a-b");
    eq("\"12\" as integer", "12");
    eq("3.7 as integer", "4");
    eq("\"abc\" contains \"B\"", "true");
    eq("{1, 2, 3} contains 2", "true");
    eq("\"abc\" starts with \"ab\" and \"abc\" ends with \"bc\"", "true");
    eq("3 is greater than 2", "true");
    eq("3 ≥ 4", "false");
    eq("\"Abc\" = \"abc\"", "true");
    eq("not (1 = 2)", "true");
    eq("2 is in {1, 2}", "true");
    eq("first item of {9, 8}", "9");
    eq("last item of {9, 8}", "8");
    eq("rest of {1, 2, 3}", "2, 3");
    eq("reverse of {1, 2, 3}", "3, 2, 1");
    eq("set r to {nome:\"Ana\", idade:30}\nidade of r", "30");
    eq("set {a, b} to {1, 2}\na + b", "3");
    eq("set l to {1, 2, 3}\nset item 2 of l to 9\nl", "1, 9, 3");
    eq("if 1 < 2 then\nset y to \"sim\"\nelse\nset y to \"não\"\nend if\ny", "sim");
    eq("if 1 > 2 then\nset y to 1\nelse if 2 > 1 then\nset y to 2\nelse\nset y to 3\nend if\ny", "2");
    eq("if 1 > 2 then set z to 1\n1", "1");
    eq("set s to 0\nrepeat with i from 1 to 10\nset s to s + i\nend repeat\ns", "55");
    eq("set s to 0\nrepeat with i from 10 to 1 by -3\nset s to s + i\nend repeat\ns", "22");
    eq("set t to \"\"\nrepeat with x in {\"a\", \"b\"}\nset t to t & x\nend repeat\nt", "ab");
    eq("set n to 0\nrepeat 4 times\nset n to n + 1\nend repeat\nn", "4");
    eq("set n to 0\nrepeat while n < 3\nset n to n + 1\nend repeat\nn", "3");
    eq("set n to 0\nrepeat until n = 5\nset n to n + 1\nend repeat\nn", "5");
    eq("set n to 0\nrepeat\nset n to n + 1\nif n = 7 then exit repeat\nend repeat\nn", "7");
    eq("on dobro(x)\nreturn x * 2\nend dobro\ndobro(21)", "42");
    eq("fat(5)\non fat(n)\nif n ≤ 1 then return 1\nreturn n * fat(n - 1)\nend fat", "120");
    eq("try\nerror \"boom\" number 42\non error m number k\nset r to m & \"/\" & k\nend try\nr", "boom/42");
    eq("try\nset x to 1 / 0\non error m\nset x to \"div\"\nend try\nx", "div");
    eq("-- comentário\n# outro\n(* bloco\n (* aninhado *) *)\n2 + ¬\n 2", "4");
    eq("POSIX path of (path to desktop)", "/Users/Visitante/Desktop/");
    eq("quoted form of \"it's\"", "'it'\\''s'");
    eq("do shell script \"echo oi\"", "saida de <echo oi>");
    eq("the clipboard", "copiado");
    eq("set the clipboard to \"novo\"\nthe clipboard", "novo");
    eq("id of application \"Mail\"", "com.exemplo.app");
    eq("version of application \"Mail\"", "2.1");
    eq("running of application \"Finder\"", "true");
    eq("button returned of (display dialog \"Oi\" buttons {\"Não\", \"Sim\"} default button \"Sim\")", "Sim");
    eq("text returned of (display dialog \"Nome?\" default answer \"x\")", "resposta");
    eq("current date", "15/11/2026 12:00:00");
    eq("{1, {2, 3}}", "1, 2, 3");
    eq("missing value", "missing value");

    int code;
    char *r;
    g_events[0] = 0;
    r = run("tell application \"Finder\" to activate", &code);
    check(code == 0 && strstr(g_events, "[Finder misc/actv -]"), "tell ... to activate manda misc/actv");
    free(r);
    g_events[0] = 0;
    r = run("tell application \"TextEdit\"\nactivate\nopen POSIX file \"/tmp/a.txt\"\nquit\nend tell", &code);
    check(code == 0 && strstr(g_events, "[TextEdit misc/actv -][TextEdit aevt/odoc /tmp/a.txt][TextEdit aevt/quit -]"), "tell em bloco: activate, open, quit");
    free(r);
    g_events[0] = 0;
    r = run("tell application \"Safari\" to open location \"https://example.com\"", &code);
    check(code == 0 && strstr(g_events, "[Safari GURL/GURL https://example.com]"), "open location manda GURL");
    free(r);
    g_events[0] = 0;
    r = run("tell application id \"com.apple.Safari\" to activate", &code);
    check(code == 0 && strstr(g_events, "[Safari misc/actv -]"), "application id");
    free(r);
    r = run("tell application \"Inexistente\" to activate", &code);
    check(code == -600 && strstr(r, "não encontrado"), "erro do app volta como erro do script");
    free(r);
    r = run("display dialog \"cancela\"", &code);
    check(code == -128, "Cancel = erro -128");
    free(r);
    r = run("do shell script \"falha\"", &code);
    check(code == 1 && strstr(r, "deu ruim"), "do shell script com status != 0");
    free(r);
    g_log[0] = 0;
    r = run("log \"oi\"\nlog 3", &code);
    check(!strcmp(g_log, "(*oi*);(*3*);"), "log");
    free(r);
    r = run("display notification \"corpo\" with title \"Título\" subtitle \"sub\"", &code);
    check(code == 0 && !strcmp(g_notif, "corpo|Título|sub"), "display notification");
    free(r);
    g_keys[0] = 0;
    r = run("tell application \"System Events\"\nkeystroke \"a\" using {command down, shift down}\nkey code 36\nend tell", &code);
    check(code == 0 && !strcmp(g_keys, "[a -1 120000][ 36 0]"), "keystroke e key code");
    free(r);
    r = run("keystroke \"x\"", &code);
    check(code == -1708, "keystroke fora do System Events");
    free(r);
    r = run("tell application \"System Events\" to get name of every process", &code);
    check(code == 0 && !strcmp(r, "Finder, Terminal"), "name of every process");
    free(r);
    r = run("tell application \"System Events\" to exists process \"Terminal\"", &code);
    check(code == 0 && !strcmp(r, "true"), "exists process");
    free(r);
    r = run("set x to \nend", &code);
    check(code == -2741, "erro de sintaxe");
    free(r);
    g_events[0] = 0;
    r = run("tell application \"Finder\" to activate\nset x to (", &code);
    check(code == -2741 && !g_events[0], "erro de sintaxe não executa nada antes");
    free(r);
    r = run("xyz", &code);
    check(code == -2753, "variável indefinida");
    free(r);
    r = run("\"a\" + 1", &code);
    check(code == -1700, "coerção impossível");
    free(r);

    r = run("property contador : 5\nset contador to contador + 1\ncontador", &code);
    check(code == 0 && !strcmp(r, "6"), "property");
    free(r);
    r = run("global g\nset g to 2\non dobra()\nset g to g * 2\nend dobra\ndobra()\ng", &code);
    check(code == 0 && !strcmp(r, "4"), "global visto no handler");
    free(r);
    r = run("set x to 1\non f()\nlocal x\nset x to 9\nreturn x\nend f\nf() & \"-\" & x", &code);
    check(code == 0 && !strcmp(r, "9-1"), "local não muda o de fora");
    free(r);
    r = run("on run\nreturn \"rodou\"\nend run", &code);
    check(code == 0 && !strcmp(r, "rodou"), "on run explícito");
    free(r);
    {
        as_val a[2] = {as_txt("um"), as_txt("dois")}, res;
        char err[200];
        int rc = as_run_ex("on run argv\nreturn (count of argv) & \":\" & item 2 of argv\nend run", &H, 0, a, 2, &res, err, sizeof err, 0, 0);
        char *t = as_to_text(&res);
        check(rc == 0 && !strcmp(t, "2:dois"), "on run argv");
        free(t); as_free(&res);
        rc = as_run_ex("property base : 10\nset base to 99\non soma(a, b)\nreturn base + a + b\nend soma", &H, "soma", (as_val[]){as_num(1), as_num(2)}, 2, &res, err, sizeof err, 0, 0);
        t = as_to_text(&res);
        check(rc == 0 && !strcmp(t, "13"), "handler chamado de fora (só property roda)");
        free(t); as_free(&res);
        rc = as_run_ex("on a()\nend a", &H, "nada", 0, 0, &res, err, sizeof err, 0, 0);
        check(rc == -1708, "handler inexistente");
        as_free(&res); as_free(&a[0]); as_free(&a[1]);
    }

    aebuild_tests();
    printf("astest: %d/%d %s\n", ok, total, ok == total ? "ok" : "FALHOU");
    return ok == total ? 0 : 1;
}
