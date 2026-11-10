/*
 * Teste de CTLine e CFHost no host Linux (não é Mach-O, não entra no libSystem).
 * Compilação: ver o comando no relatório. -DCT_HOST evita o int 0x80 do kernel do mOS.
 * O que isto prova: layout, quebra, truncamento, tinta no bitmap, getaddrinfo e NSHost.
 * O que isto não prova: machocheck contra libSystem.mos, a VM, nem o raster da Inter.
 */
#include "objc.h"
#include <stdio.h>
#include <string.h>

typedef struct { double x, y, w, h; } ct_rect_t;
typedef struct { long loc, len; } ct_range_t;
typedef struct { long domain; int error; } cf_err;
typedef struct { double a, b, c, d, tx, ty; } ct_tm;

id CTLineCreateWithAttributedString(id astr);
id CTLineCreateTruncatedLine(id line, double width, unsigned type, id token);
void CTLineDraw(id line, void *ctx);
ct_rect_t CTLineGetBoundsWithOptions(id line, unsigned long options);
id CTTypesetterCreateWithAttributedString(id astr);
id CTTypesetterCreateLine(id ts, ct_range_t range);
long CTTypesetterSuggestLineBreak(id ts, long start, double width);
long CTTypesetterSuggestClusterBreak(id ts, long start, double width);

id CFHostCreateWithName(void *alloc, id name);
BOOL CFHostStartInfoResolution(id host, long info, cf_err *err);
id CFHostGetAddressing(id host, BOOL *resolved);
void CFHostScheduleWithRunLoop(id host, void *rl, void *mode);
void CFHostUnscheduleFromRunLoop(id host, void *rl, void *mode);
void CFHostCancelInfoResolution(id host, long info);
typedef struct { long version; void *info; const void *(*retain)(const void *); void (*release)(const void *); const void *(*copyDescription)(const void *); } cfhost_ctx;
typedef void (*cfhost_cb)(id host, long type, const cf_err *err, void *info);
BOOL CFHostSetClient(id host, cfhost_cb cb, cfhost_ctx *ctx);
extern const void *kCFStreamErrorDomainNetDB;

void host_runtime_init(void);
void cx_cfhost_init(void);
id ct_test_font(double size);
id ct_test_color(double r, double g, double b, double a);
id ct_test_astr(const char *s, id font, id color);
void *ct_test_bitmap(int w, int h);
int ct_test_ink(void *bm);
u32 ct_test_fill(void *bm);
extern int ct_host_performs;
ct_tm CGContextGetTextMatrix(void *c);
void CGContextSetTextMatrix(void *c, ct_tm t);
const u8 *CFDataGetBytePtr(id d);
long CFDataGetLength(id d);
void *CFRunLoopGetCurrent(void);
id cx_str(const char *s);
const char *cx_cstr(id o);
cx_class_t *cx_class_named(const char *name);
id objc_retain(id o);
void objc_release(id o);

static int fails;
static void expect(int cond, const char *msg) {
    if (!cond) { fprintf(stderr, "FAIL %s\n", msg); fails++; }
    else printf("ok   %s\n", msg);
}
static int near(double a, double b, double eps) { double d = a - b; if (d < 0) d = -d; return d <= eps; }

static int cb_fires, cb_info, retains, releases;
static void host_cb(id host, long type, const cf_err *err, void *info) {
    (void)host; (void)type; (void)err;
    cb_fires++;
    if (info) *(int *)info = 1;
}
static const void *cb_retain(const void *p) { retains++; return p; }
static void cb_release(const void *p) { (void)p; releases++; }

static const u8 *first_addr(id host, long *len, BOOL *resolved) {
    id arr = CFHostGetAddressing(host, resolved);
    if (!arr || !MSG0(u64, arr, "count")) { *len = 0; return 0; }
    id d = MSG(id, arr, "objectAtIndex:", (u64)0);
    *len = CFDataGetLength(d);
    return CFDataGetBytePtr(d);
}

int main(void) {
    host_runtime_init();
    cx_cfhost_init();
    id font = ct_test_font(13);
    id red = ct_test_color(1, 0, 0, 1);
    id oi = ct_test_astr("Oi", font, red);
    id hi = ct_test_astr("Hi", font, 0);
    id line = CTLineCreateWithAttributedString(oi);
    id line_hi = CTLineCreateWithAttributedString(hi);
    expect(line != 0 && line_hi != 0, "CTLine a partir de attributed string");
    ct_rect_t b = CTLineGetBoundsWithOptions(line, 0);
    ct_rect_t bh = CTLineGetBoundsWithOptions(line_hi, 0);
    printf("    width Oi=%.4f Hi=%.4f h=%.4f y=%.4f\n", b.w, bh.w, b.h, b.y);
    expect(near(b.w, 14.3, 0.05) && near(bh.w, b.w, 0.001), "largura Oi/Hi = 2 * 0,55 * 13");
    expect(near(b.h, 13 * 1.193, 0.05) && near(b.y, -13 * 0.241, 0.05), "caixa tipográfica ascent+descent");
    ct_rect_t bp = CTLineGetBoundsWithOptions(line, 8);
    expect(near(bp.w, b.w, 0.05) && bp.h > 10, "opção 8 (path) sem kernel cai na caixa do avanço");

    void *bm = ct_test_bitmap(80, 80);
    ct_tm m = {1, 0, 0, 1, 4, 30};
    CGContextSetTextMatrix(bm, m);
    CTLineDraw(line, bm);
    ct_tm m2 = CGContextGetTextMatrix(bm);
    printf("    ink=%d fill=%08x tx %.2f -> %.2f\n", ct_test_ink(bm), ct_test_fill(bm), m.tx, m2.tx);
    expect(ct_test_ink(bm) > 0, "CTLineDraw pinta tinta no bitmap");
    expect(ct_test_fill(bm) == 0xFFFF0000u, "cor do atributo NSColor vira o fill");
    expect(near(m2.tx, m.tx + b.w, 0.05), "text matrix avança a largura");

    id hello = ct_test_astr("hi world", font, 0);
    id ts = CTTypesetterCreateWithAttributedString(hello);
    long lb = CTTypesetterSuggestLineBreak(ts, 0, 30);
    long cb = CTTypesetterSuggestClusterBreak(ts, 0, 30);
    printf("    line-break=%ld cluster-break=%ld\n", lb, cb);
    expect(lb == 3 && cb == 4, "quebra de palavra volta ao espaço; cluster parte a palavra");
    id nl = ct_test_astr("ab\ncd", font, 0);
    id tsn = CTTypesetterCreateWithAttributedString(nl);
    expect(CTTypesetterSuggestLineBreak(tsn, 0, 10000) == 3, "ab\\ncd com largura enorme devolve 3");
    ct_range_t rg = {0, 3};
    id part = CTTypesetterCreateLine(ts, rg);
    ct_rect_t br = CTLineGetBoundsWithOptions(part, 0);
    expect(near(br.w, 3 * 13 * 0.55, 0.05), "CreateLine do trecho hi+espaço");
    expect(CTTypesetterSuggestLineBreak(ts, 99, 100) == 0, "índice além do fim devolve 0");
    expect(CTLineCreateWithAttributedString(0) == 0, "attributed string nula");

    id longl = ct_test_astr("Hello", font, 0);
    id full = CTLineCreateWithAttributedString(longl);
    ct_rect_t bf = CTLineGetBoundsWithOptions(full, 0);
    id copy = CTLineCreateTruncatedLine(full, 1000, 1, 0);
    expect(copy != 0 && copy != full, "linha que cabe devolve uma cópia nova");
    id trunc = CTLineCreateTruncatedLine(full, 30, 1, 0);
    objc_release(full);
    ct_rect_t bt = CTLineGetBoundsWithOptions(trunc, 0);
    printf("    full=%.4f trunc=%.4f\n", bf.w, bt.w);
    expect(bt.w < bf.w - 1 && bt.w <= 30.1 && near(bt.w, 28.6, 0.15), "truncamento no fim cabe em 30");
    id bang = ct_test_astr("Hello!!", font, 0);
    id bang_l = CTLineCreateWithAttributedString(bang);
    id mid = CTLineCreateTruncatedLine(bang_l, 43, 2, 0);
    ct_rect_t bmids = CTLineGetBoundsWithOptions(mid, 0);
    ct_rect_t bfull = CTLineGetBoundsWithOptions(bang_l, 0);
    printf("    middle %.4f de %.4f\n", bmids.w, bfull.w);
    expect(bmids.w < bfull.w - 1 && bmids.w <= 43.1 && bmids.w > 20, "truncamento no meio");
    id start = CTLineCreateTruncatedLine(bang_l, 30, 0, 0);
    ct_rect_t bs = CTLineGetBoundsWithOptions(start, 0);
    expect(bs.w <= 30.1 && bs.w > 10, "truncamento no início");

    expect(CFHostCreateWithName(0, 0) == 0, "CFHost com nome nulo");
    expect(CFHostCreateWithName(0, cx_str("")) == 0, "CFHost com nome vazio");
    id host = CFHostCreateWithName(0, cx_str("localhost"));
    cf_err err;
    BOOL scheduled_ok = CFHostStartInfoResolution(host, 0, &err);
    BOOL resolved = NO;
    long alen = 0;
    const u8 *a = first_addr(host, &alen, &resolved);
    expect(scheduled_ok && resolved && a && alen == 16, "localhost resolve");
    expect(a && a[0] == 16 && a[1] == 2 && a[2] == 0 && a[3] == 0 && a[4] == 127 && a[5] == 0 && a[6] == 0 && a[7] == 1,
           "sockaddr IPv4 127.0.0.1 porta 0");
    u64 naddr = MSG0(u64, CFHostGetAddressing(host, &resolved), "count");
    expect(naddr >= 2, "localhost também devolve IPv6");
    objc_release(host);

    id num = CFHostCreateWithName(0, cx_str("10.0.2.2"));
    expect(CFHostStartInfoResolution(num, 0, &err) == YES, "IP numérico resolve");
    a = first_addr(num, &alen, &resolved);
    expect(resolved && a && a[4] == 10 && a[5] == 0 && a[6] == 2 && a[7] == 2, "10.0.2.2 permanece numérico");
    objc_release(num);

    id bad = CFHostCreateWithName(0, cx_str("no.such.host.invalid"));
    BOOL bad_ok = CFHostStartInfoResolution(bad, 0, &err);
    resolved = NO;
    id none = CFHostGetAddressing(bad, &resolved);
    expect(!bad_ok && none == 0 && resolved, "nome inexistente falha e marca resolved");
    expect(err.domain == (long)kCFStreamErrorDomainNetDB && err.error == 8, "erro NetDB EAI_NONAME");
    objc_release(bad);

    id late = CFHostCreateWithName(0, cx_str("localhost"));
    CFHostCancelInfoResolution(late, 0);
    expect(CFHostStartInfoResolution(late, 0, &err) == NO, "cancel antes da Start consome uma tentativa");
    resolved = YES;
    expect(CFHostGetAddressing(late, &resolved) == 0 && resolved == NO, "cancel não resolve");
    expect(CFHostStartInfoResolution(late, 0, &err) == YES, "a Start seguinte resolve");
    objc_release(late);

    id cbh = CFHostCreateWithName(0, cx_str("localhost"));
    cfhost_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.info = &cb_info;
    ctx.retain = cb_retain;
    ctx.release = cb_release;
    expect(CFHostSetClient(cbh, host_cb, &ctx) == YES && retains == 1, "SetClient retém o info");
    int perf = ct_host_performs;
    expect(CFHostStartInfoResolution(cbh, 0, &err) == YES && cb_fires == 0 && ct_host_performs == perf,
           "sem schedule o callback não corre");
    CFHostScheduleWithRunLoop(cbh, 0, 0);
    expect(CFHostStartInfoResolution(cbh, 0, &err) == YES && cb_fires == 1 && cb_info == 1 && ct_host_performs == perf + 1,
           "com schedule o callback corre dentro da Start");
    CFHostUnscheduleFromRunLoop(cbh, CFRunLoopGetCurrent(), 0);
    expect(CFHostStartInfoResolution(cbh, 0, &err) == YES && cb_fires == 1, "unschedule silencia o callback");
    expect(CFHostSetClient(cbh, 0, 0) == YES && releases == 1, "limpar o cliente solta o info");
    objc_release(cbh);

    id cur = MSG0(id, (id)cx_class_named("NSHost"), "currentHost");
    expect(cur && !strcmp(cx_cstr(MSG0(id, cur, "name")), "mos"), "currentHost se chama mos");
    expect(MSG0(u64, MSG0(id, cur, "names"), "count") == 1, "names tem o hostname");
    expect(!strcmp(cx_cstr(MSG0(id, cur, "address")), "127.0.0.1"), "currentHost é 127.0.0.1");
    expect(MSG0(id, cur, "localizedName") == MSG0(id, cur, "name"), "localizedName é o hostname");
    id byname = MSG(id, (id)cx_class_named("NSHost"), "hostWithName:", cx_str("localhost"));
    expect(byname && !strcmp(cx_cstr(MSG0(id, byname, "address")), "127.0.0.1"), "hostWithName:localhost");
    expect(MSG0(u64, MSG0(id, byname, "addresses"), "count") >= 1, "addresses de localhost");
    id byaddr = MSG(id, (id)cx_class_named("NSHost"), "hostWithAddress:", cx_str("10.2.3.4"));
    expect(byaddr && !strcmp(cx_cstr(MSG0(id, byaddr, "name")), "10.2.3.4"), "hostWithAddress sem DNS reverso");
    expect(MSG(BOOL, byaddr, "isEqualToHost:", byaddr) == YES, "isEqualToHost: reflexivo");
    objc_release(byname);
    objc_release(byaddr);

    id ex = CFHostCreateWithName(0, cx_str("example.com"));
    if (CFHostStartInfoResolution(ex, 0, &err)) {
        a = first_addr(ex, &alen, &resolved);
        expect(resolved && a && alen >= 8 && (a[1] == 2 || a[1] == 30), "example.com (DNS do host, melhor esforço)");
    } else {
        printf("ok   example.com sem DNS neste host (%d) — não é falha\n", err.error);
    }
    objc_release(ex);

    printf(fails ? "\n%d falha(s)\n" : "\ntudo certo\n", fails);
    return fails ? 1 : 0;
}
