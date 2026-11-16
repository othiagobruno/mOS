/* Host: userland/libsystem/mediaplayer.c com um runtime mínimo (trampolim do objc_msgSend). → "mediaplayer: N/N ok" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { double x, y, w, h; } rc_t;
typedef struct { double w, h; } sz_t;
/* as classes de casca do vscode_fwclasses.c */
char MPNowPlayingInfoCenter_c[256] __asm__("OBJC_CLASS_$_MPNowPlayingInfoCenter"), MPNowPlayingInfoCenter_m[256] __asm__("OBJC_METACLASS_$_MPNowPlayingInfoCenter");
char MPRemoteCommandCenter_c[256] __asm__("OBJC_CLASS_$_MPRemoteCommandCenter"), MPRemoteCommandCenter_m[256] __asm__("OBJC_METACLASS_$_MPRemoteCommandCenter");
char MPMediaItemArtwork_c[256] __asm__("OBJC_CLASS_$_MPMediaItemArtwork"), MPMediaItemArtwork_m[256] __asm__("OBJC_METACLASS_$_MPMediaItemArtwork");
char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
void cx_mediaplayer_init(void);
long cx_mp_remote_command(int kind, double value, long type);
void *cx_mp_now_playing(long long *state);

typedef struct { void *key; const char *name; unsigned extra; void *super; const char *sels[64]; void *imps[64]; int n; } cls_t;
static cls_t tab[32]; static int ntab;
static char NSObj;
static cls_t *Kc(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; tab[ntab].key = c; return &tab[ntab++]; }
static cls_t *Kf(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; return 0; }
void *cx_class_named(const char *n) {
    if (!strcmp(n, "NSObject")) return &NSObj;
    for (int i = 0; i < ntab; i++) if (tab[i].name && !strcmp(tab[i].name, n)) return tab[i].key;
    return 0;
}
void *cx_define_class(void *c, void *m, const char *name, void *super, unsigned extra) { (void)m; cls_t *k = Kc(c); k->name = name; k->super = super; k->extra = extra; return c; }
void cx_add_method(void *c, const char *s, void *f) { cls_t *k = Kc(c); k->sels[k->n] = s; k->imps[k->n++] = f; }
void cx_add_cmethod(void *c, const char *s, void *f) { cx_add_method(c, s, f); }
static unsigned size_of(void *c) { unsigned s = 0; for (cls_t *k = Kf(c); k; k = Kf(k->super)) s += k->extra; return s; }
void *cx_alloc_instance(void *c) { void **o = calloc(1, 16 + size_of(c) + 64); o[0] = c; return o; }
static void *imp(void *c, const char *s) { for (cls_t *k = Kf(c); k; k = Kf(k->super)) for (int i = 0; i < k->n; i++) if (!strcmp(k->sels[i], s)) return k->imps[i]; return 0; }
void *class_getMethodImplementation(void *c, const char *s) { (void)c; (void)s; return 0; }
const char *sel_registerName(const char *s) { return s; }
void *objc_retain(void *o) { return o; }
void objc_release(void *o) { (void)o; }
void *objc_autorelease(void *o) { return o; }
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }
double cx_now(void) { return 123.5; }
/* objetos simples: NSObject (token), dicionário (copy devolve ele mesmo) e um alvo de ação */
static char DICT, TARGET;
static int target_calls; static void *target_event;
static long f_ret0(void) { return 0; }
static void *f_self(void *s, const char *c) { (void)c; return s; }
static void *f_alloc(void *k, const char *c) { (void)k; (void)c; void **o = calloc(1, 32); o[0] = &NSObj; return o; }
static long f_action(void *s, const char *c, void *ev) { (void)s; (void)c; target_calls++; target_event = ev; return 0; }
void *fake_lookup(void *self, const char *sel) {
    if (!self) return (void *)f_ret0;
    void *f = imp(self, sel);
    if (f) return f;
    void *isa = *(void **)self;
    if (self == &NSObj && !strcmp(sel, "alloc")) return (void *)f_alloc;
    if (!strcmp(sel, "init") || (isa == &DICT && !strcmp(sel, "copy"))) return (void *)f_self;
    if (isa == &TARGET && !strcmp(sel, "toggle:")) return (void *)f_action;
    f = imp(isa, sel);
    return f ? f : (void *)f_ret0;
}
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\npush %rbp\nmov %rsp,%rbp\nsub $0x40,%rsp\n"
        "mov %rdi,0(%rsp)\nmov %rsi,8(%rsp)\nmov %rdx,16(%rsp)\nmov %rcx,24(%rsp)\nmov %r8,32(%rsp)\nmov %r9,40(%rsp)\n"
        "movdqu %xmm0,48(%rsp)\ncall fake_lookup\nmov %rax,%r11\n"
        "mov 0(%rsp),%rdi\nmov 8(%rsp),%rsi\nmov 16(%rsp),%rdx\nmov 24(%rsp),%rcx\nmov 32(%rsp),%r8\nmov 40(%rsp),%r9\nmovdqu 48(%rsp),%xmm0\n"
        "leave\njmp *%r11\n");

static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
#define CALL(o, sel, R, ...) ((R (*)())imp(*(void **)(o), sel))(o, sel, ##__VA_ARGS__)
#define CCALL(k, sel, R, ...) ((R (*)())imp(k, sel))(k, sel, ##__VA_ARGS__)
static int blk_calls; static void *blk_event; static long blk_ret;
static long blk_inv(void *b, void *ev) { (void)b; blk_calls++; blk_event = ev; return blk_ret; }
static struct { void *isa; int fl, re; long (*inv)(void *, void *); } blk = {0, 0, 0, blk_inv};
static void *art_inv(void *b, sz_t z) { (void)b; return z.w == 64 ? (void *)0x1234 : 0; }
static struct { void *isa; int fl, re; void *(*inv)(void *, sz_t); } art_blk = {0, 0, 0, art_inv};

int main(void) {
    cx_mediaplayer_init();
    void *cc = CCALL(MPRemoteCommandCenter_c, "sharedCommandCenter", void *);
    expect(cc && cc == CCALL(MPRemoteCommandCenter_c, "sharedCommandCenter", void *), "sharedCommandCenter único");
    void *toggle = CALL(cc, "togglePlayPauseCommand", void *), *play = CALL(cc, "playCommand", void *);
    expect(toggle && play && toggle != play && toggle == CALL(cc, "togglePlayPauseCommand", void *), "comandos distintos e estáveis");
    expect(cx_mp_remote_command(3, 0, 0) == 120, "sem handler: MPRemoteCommandHandlerStatusCommandFailed... (120)");
    void *tok = CALL(toggle, "addTargetWithHandler:", void *, &blk);
    blk_ret = 0;
    long st = cx_mp_remote_command(3, 0, 0);
    expect(tok && st == 0 && blk_calls == 1 && CALL(blk_event, "command", void *) == toggle &&
           ((double (*)(void *, const char *))imp(*(void **)blk_event, "timestamp"))(blk_event, "timestamp") == 123.5, "handler em bloco recebe o evento");
    CALL(toggle, "setEnabled:", void, (signed char)0);
    expect(cx_mp_remote_command(3, 0, 0) == 120 && blk_calls == 1, "comando desligado não chama");
    CALL(toggle, "setEnabled:", void, (signed char)1);
    CALL(toggle, "removeTarget:", void, tok);
    expect(cx_mp_remote_command(3, 0, 0) == 120 && blk_calls == 1, "removeTarget: tira o handler");
    void *target = calloc(1, 16); *(void **)target = &TARGET;
    CALL(toggle, "addTarget:action:", void, target, "toggle:");
    expect(cx_mp_remote_command(3, 0, 0) == 0 && target_calls == 1 && target_event, "alvo/ação");
    void *pos = CALL(cc, "changePlaybackPositionCommand", void *);
    CALL(pos, "addTargetWithHandler:", void *, &blk);
    cx_mp_remote_command(9, 42.5, 0);
    expect(blk_calls == 2 && ((double (*)(void *, const char *))imp(*(void **)blk_event, "positionTime"))(blk_event, "positionTime") == 42.5, "evento de posição");
    void *npc = CCALL(MPNowPlayingInfoCenter_c, "defaultCenter", void *);
    void *dict = calloc(1, 16); *(void **)dict = &DICT;
    CALL(npc, "setNowPlayingInfo:", void, dict);
    CALL(npc, "setPlaybackState:", void, 1L);
    long long state = 0;
    expect(CALL(npc, "nowPlayingInfo", void *) == dict && CALL(npc, "playbackState", long) == 1 && cx_mp_now_playing(&state) == dict && state == 1, "tocando agora e estado");
    void *art = cx_alloc_instance(MPMediaItemArtwork_c);
    sz_t z = {300, 200};
    ((void *(*)(void *, const char *, sz_t, void *))imp(MPMediaItemArtwork_c, "initWithBoundsSize:requestHandler:"))(art, "", z, &art_blk);
    sz_t q = {64, 64};
    void *img = ((void *(*)(void *, const char *, sz_t))imp(MPMediaItemArtwork_c, "imageWithSize:"))(art, "", q);
    rc_t b = ((rc_t (*)(void *, const char *))imp(MPMediaItemArtwork_c, "bounds"))(art, "");
    expect(img == (void *)0x1234 && b.w == 300 && b.h == 200, "MPMediaItemArtwork com requestHandler");
    printf("mediaplayer: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
