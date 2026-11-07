/*
 * mOS libSystem — KVO (observação chave-valor) do NSObject.
 *
 * addObserver:forKeyPath:options:context: registra o observador e intercepta o setter da chave ("setValor:") na classe
 * que o define, trocando o IMP por um trampolim que chama willChangeValueForKey:/didChangeValueForKey: em volta do
 * original — o efeito do NSKVONotifying_* do Cocoa, mas sem trocar o isa (uma subclasse ObjC criada em tempo de
 * execução não teria a vtable de uma classe Swift; as propriedades `@objc dynamic` do Swift passam por objc_msgSend,
 * então trocar o método basta). willChange/didChange manuais também notificam.
 *
 * Opções como no Cocoa: New (1), Old (2), Initial (4), Prior (8). Só o primeiro componente de um caminho "a.b" é
 * interceptado. Setters cujo argumento é uma struct (CGRect...) não são interceptados: só notificam por will/did.
 * O objeto observado e o observador não são retidos (como no Cocoa).
 */
#include "objc.h"
#include "libc.h"
#include "foundation.h"

extern id NSKeyValueChangeKindKey, NSKeyValueChangeNewKey, NSKeyValueChangeOldKey, NSKeyValueChangeNotificationIsPriorKey;
extern cx_class_t NSMutableDictionary_c __asm__("OBJC_CLASS_$_NSMutableDictionary");
extern cx_class_t NSNull_c __asm__("OBJC_CLASS_$_NSNull");
extern const char *cx_method_types(cx_class_t *c, SEL sel);
extern IMP class_replaceMethod(cx_class_t *c, SEL s, IMP imp, const char *types);
extern void *class_getInstanceMethod(cx_class_t *c, SEL s);
extern IMP method_getImplementation(void *m);
extern const char *method_getTypeEncoding(void *m);
extern id cx_num_i(i64 v);
extern id cx_num_bool(BOOL b);
extern const char *sel_getName(SEL s);
id objc_retain(id o);
void objc_release(id o);

typedef struct { id obj, obs, path, key; u64 opts; void *ctx; } kvo_reg_t;
static kvo_reg_t *regs;
static int nregs, capregs;

typedef struct { cx_class_t *cls; SEL sel; IMP orig; } kvo_hook_t;
static kvo_hook_t hooks[256];
static int nhooks;

static volatile int kvo_lk;
static void lock(void) { while (__sync_lock_test_and_set(&kvo_lk, 1)) ; }
static void unlock(void) { __sync_lock_release(&kvo_lk); }

static int str_eq(id a, id b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    const char *x = cx_cstr(a), *y = cx_cstr(b);
    return x && y && !strcmp(x, y);
}

/* primeiro componente do caminho, como NSString */
static id first_key(id path) {
    const char *p = cx_cstr(path);
    if (!p) return path;
    const char *dot = strchr(p, '.');
    if (!dot) return path;
    char buf[160];
    u64 n = (u64)(dot - p) < sizeof buf - 1 ? (u64)(dot - p) : sizeof buf - 1;
    memcpy(buf, p, n); buf[n] = 0;
    return cx_str(buf);
}

static int observed(id obj, id key) {
    for (int i = 0; i < nregs; i++) if (regs[i].obj == obj && str_eq(regs[i].key, key)) return 1;
    return 0;
}

static id value_of(id obj, id path) { return MSG(id, obj, "valueForKeyPath:", path); }
static id nsnull(void) { return MSG0(id, (id)&NSNull_c, "null"); }

static id change_dict(id nv, id ov, u64 opts, int prior, int send_new) {
    id ch = MSG0(id, (id)&NSMutableDictionary_c, "dictionary");
    MSG(void, ch, "setObject:forKey:", cx_num_i(1), NSKeyValueChangeKindKey);   /* NSKeyValueChangeSetting */
    if (send_new && (opts & 1)) MSG(void, ch, "setObject:forKey:", nv ? nv : nsnull(), NSKeyValueChangeNewKey);
    if (opts & 2) MSG(void, ch, "setObject:forKey:", ov ? ov : nsnull(), NSKeyValueChangeOldKey);
    if (prior) MSG(void, ch, "setObject:forKey:", cx_num_bool(YES), NSKeyValueChangeNotificationIsPriorKey);
    return ch;
}

/* cópia dos registros que casam (os observadores podem mexer na tabela durante a notificação) */
static int matching(id obj, id key, kvo_reg_t *out, int max) {
    int n = 0;
    lock();
    for (int i = 0; i < nregs && n < max; i++) if (regs[i].obj == obj && str_eq(regs[i].key, key)) out[n++] = regs[i];
    unlock();
    return n;
}

static void notify(kvo_reg_t *r, id change) {
    MSG(void, r->obs, "observeValueForKeyPath:ofObject:change:context:", r->path, r->obj, change, r->ctx);
}

/* pilha de mudanças em curso: will guarda os valores antigos, did casa com o topo */
#define KVO_DEPTH 32
#define KVO_MAXOBS 16
static struct { id obj, key; int n; kvo_reg_t r[KVO_MAXOBS]; id old[KVO_MAXOBS]; } pend[KVO_DEPTH];
static int npend;

static void kvo_will(id self, SEL c, id key) {
    (void)c;
    if (!self || !key) return;
    kvo_reg_t r[KVO_MAXOBS];
    int n = matching(self, key, r, KVO_MAXOBS);
    if (npend >= KVO_DEPTH) return;
    int slot = npend++;
    pend[slot].obj = self; pend[slot].key = key; pend[slot].n = n;
    for (int i = 0; i < n; i++) {
        pend[slot].r[i] = r[i];
        id ov = (r[i].opts & 2) ? objc_retain(value_of(self, r[i].path)) : 0;
        pend[slot].old[i] = ov;
        if (r[i].opts & 8) notify(&r[i], change_dict(0, ov, r[i].opts, 1, 0));
    }
}

static void kvo_did(id self, SEL c, id key) {
    (void)c;
    if (!self || !key) return;
    int slot = -1;
    for (int i = npend - 1; i >= 0; i--) if (pend[i].obj == self && str_eq(pend[i].key, key)) { slot = i; break; }
    if (slot < 0) return;
    int n = pend[slot].n;
    kvo_reg_t r[KVO_MAXOBS];
    id old[KVO_MAXOBS];
    for (int i = 0; i < n; i++) { r[i] = pend[slot].r[i]; old[i] = pend[slot].old[i]; }
    for (int i = slot; i < npend - 1; i++) pend[i] = pend[i + 1];
    npend--;
    for (int i = 0; i < n; i++) {
        /* o observador pode ter saído entre o will e o did */
        int still = 0;
        lock();
        for (int j = 0; j < nregs; j++) if (regs[j].obj == self && regs[j].obs == r[i].obs && str_eq(regs[j].path, r[i].path)) { still = 1; break; }
        unlock();
        if (still) notify(&r[i], change_dict((r[i].opts & 1) ? value_of(self, r[i].path) : 0, old[i], r[i].opts, 0, 1));
        if (old[i]) objc_release(old[i]);
    }
}

/* ---- interceptação do setter. Argumento escalar/ponteiro: chega em rdx (inteiro) ou xmm0 (ponto flutuante);
 * o trampolim recebe os dois e repassa ambos, com a mesma assinatura, ao IMP original. */
typedef void (*setter_t)(id, SEL, u64, u64, u64, u64, double, double);

static IMP orig_for(cx_class_t *cls, SEL s) {
    for (cx_class_t *c = cls; c; c = c->super)
        for (int i = 0; i < nhooks; i++) if (hooks[i].cls == c && hooks[i].sel == s) return hooks[i].orig;
    return 0;
}

/* "setValor:" -> "valor" */
static id key_of_setter(SEL s) {
    const char *n = sel_getName(s);
    char buf[160];
    u64 len = strlen(n);
    if (len < 5 || len - 4 >= sizeof buf) return 0;
    memcpy(buf, n + 3, len - 4); buf[len - 4] = 0;
    /* setURL: -> URL; setValor: -> valor */
    if (!(buf[1] >= 'A' && buf[1] <= 'Z')) buf[0] = (char)tolower((u8)buf[0]);
    return cx_str(buf);
}

static void kvo_setter(id self, SEL s, u64 a, u64 b, u64 c, u64 d, double x, double y) {
    IMP o = orig_for(self->isa, s);
    if (!o) return;
    id key = 0;
    lock();
    int any = 0;
    for (int i = 0; i < nregs; i++) if (regs[i].obj == self) { any = 1; break; }
    unlock();
    if (any) {
        key = key_of_setter(s);
        lock(); int obs = key && observed(self, key); unlock();
        if (!obs) key = 0;
    }
    if (key) kvo_will(self, 0, key);
    ((setter_t)o)(self, s, a, b, c, d, x, y);
    if (key) kvo_did(self, 0, key);
}

static void hook_setter(cx_class_t *isa, id key) {
    const char *k = cx_cstr(key);
    if (!k || !*k) return;
    char nm[170];
    snprintf(nm, sizeof nm, "set%c%s:", toupper((u8)k[0]), k + 1);
    SEL s = sel_registerName(nm);
    void *m = class_getInstanceMethod(isa, s);
    if (!m) return;
    IMP imp = method_getImplementation(m);
    if (imp == (IMP)kvo_setter) return;
    const char *t = method_getTypeEncoding(m);
    /* argumento: depois de "v<n>@<n>:<n>" */
    const char *p = t;
    for (int i = 0; p && *p && i < 3; i++) {
        while (*p && strchr("rnNoORV", *p)) p++;
        if (*p == '{' || *p == '(') return;
        p++;
        while (*p >= '0' && *p <= '9') p++;
    }
    while (p && *p && strchr("rnNoORV", *p)) p++;
    if (!p || *p == '{' || *p == '(' || *p == '[') return;
    /* a classe que define o método */
    cx_class_t *def = isa;
    for (cx_class_t *c = isa; c; c = c->super) {
        void *mc = class_getInstanceMethod(c, s);
        if (mc != m) break;
        def = c;
    }
    lock();
    if (nhooks >= (int)(sizeof hooks / sizeof hooks[0])) { unlock(); return; }
    hooks[nhooks].cls = def; hooks[nhooks].sel = s; hooks[nhooks].orig = imp;
    nhooks++;
    unlock();
    class_replaceMethod(def, s, (IMP)kvo_setter, t);
}

void cx_kvo_add(id self, SEL c, id obs, id path, u64 opts, void *ctx) {
    (void)c;
    if (!self || !obs || !path) return;
    id key = objc_retain(first_key(path));
    lock();
    if (nregs == capregs) {
        capregs = capregs ? capregs * 2 : 32;
        regs = realloc(regs, (u64)capregs * sizeof *regs);
    }
    regs[nregs].obj = self; regs[nregs].obs = obs; regs[nregs].path = objc_retain(path);
    regs[nregs].key = key; regs[nregs].opts = opts; regs[nregs].ctx = ctx;
    kvo_reg_t r = regs[nregs];
    nregs++;
    unlock();
    hook_setter(self->isa, key);
    if (opts & 4) notify(&r, change_dict(value_of(self, path), 0, opts & 1, 0, 1));
}

void cx_kvo_remove_ctx(id self, SEL c, id obs, id path, void *ctx) {
    (void)c;
    lock();
    for (int i = nregs - 1; i >= 0; i--)
        if (regs[i].obj == self && regs[i].obs == obs && str_eq(regs[i].path, path) && (!ctx || regs[i].ctx == ctx)) {
            objc_release(regs[i].path); objc_release(regs[i].key);
            regs[i] = regs[--nregs];
            if (ctx) break;      /* com contexto, sai um registro só */
        }
    unlock();
}
void cx_kvo_remove(id self, SEL c, id obs, id path) { cx_kvo_remove_ctx(self, c, obs, path, 0); }

/* o objeto observado morreu: os registros dele saem (chamado pelo dealloc do NSObject) */
void cx_kvo_forget(id self) {
    if (!nregs) return;
    lock();
    for (int i = nregs - 1; i >= 0; i--)
        if (regs[i].obj == self) { objc_release(regs[i].path); objc_release(regs[i].key); regs[i] = regs[--nregs]; }
    unlock();
}

static id kvo_info(id self, SEL c) { (void)self; (void)c; return 0; }
static BOOL kvo_auto(id cls, SEL c, id key) { (void)cls; (void)c; (void)key; return YES; }

void cx_kvo_init(cx_class_t *ob) {
    cx_add_method(ob, "addObserver:forKeyPath:options:context:", (void *)cx_kvo_add);
    cx_add_method(ob, "removeObserver:forKeyPath:", (void *)cx_kvo_remove);
    cx_add_method(ob, "removeObserver:forKeyPath:context:", (void *)cx_kvo_remove_ctx);
    cx_add_method(ob, "willChangeValueForKey:", (void *)kvo_will);
    cx_add_method(ob, "didChangeValueForKey:", (void *)kvo_did);
    cx_add_method(ob, "observationInfo", (void *)kvo_info);
    cx_add_cmethod(ob, "automaticallyNotifiesObserversForKey:", (void *)kvo_auto);
}
