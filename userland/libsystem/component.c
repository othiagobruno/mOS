/*
 * mOS libSystem — Component Manager do Carbon (CarbonCore/Components.h).
 *
 * Dois tipos de componente na mesma lista: os que o app registra (RegisterComponent, com a rotina de entrada que
 * recebe ComponentParameters) e as unidades de áudio do sistema (AudioComponent de coreaudio.c). Como no macOS
 * atual, Component e AudioComponent são o mesmo objeto para áudio: OpenAComponent de uma unidade devolve a
 * AudioComponentInstance, que o app passa direto a AudioUnitSetProperty e companhia.
 * Instância de componente registrado: a rotina recebe Open (-1) ao abrir, Close (-2) ao fechar, e o app conversa com
 * ela por CallComponentDispatch. Layout de ComponentParameters no 64 bits: flags u8, paramSize u8, what s16, 4 bytes
 * de preenchimento, params[] de long; a instância vem depois dos parâmetros (params[paramSize / 8]).
 */
#include "foundation.h"
#include "libc.h"

typedef int OSStatus;
typedef short OSErr;
typedef long ComponentResult;
typedef char **Handle;
typedef struct { u32 componentType, componentSubType, componentManufacturer, componentFlags, componentFlagsMask; } cdesc_t;
typedef struct { u8 flags, paramSize; i16 what; u32 padding; long params[]; } cparams_t;
typedef ComponentResult (*croutine_t)(cparams_t *p, Handle storage);
Handle NewHandle(long size);
void DisposeHandle(Handle h);
void SetHandleSize(Handle h, long size);
void *AudioComponentFindNext(void *after, const cdesc_t *d);
OSStatus AudioComponentGetDescription(void *c, cdesc_t *out);
OSStatus AudioComponentCopyName(void *c, id *out);
OSStatus AudioComponentGetVersion(void *c, u32 *v);
OSStatus AudioComponentInstanceNew(void *c, void **out);
OSStatus AudioComponentInstanceDispose(void *u);
void *AudioComponentInstanceGetComponent(void *u);
id objc_retain(id o);
void objc_release(id o);

enum { noErr = 0, paramErr = -50, memFullErr = -108, invalidComponentID = -3000, validInstancesExist = -3001,
       componentNotCaptured = -3002 };
#define badComponentInstance ((ComponentResult)(int)0x80008001)
#define badComponentSelector ((ComponentResult)(int)0x80008002)
enum { kComponentOpenSelect = -1, kComponentCloseSelect = -2, kComponentCanDoSelect = -3, kComponentVersionSelect = -4,
       kComponentRegisterSelect = -5, kComponentTargetSelect = -6, kComponentUnregisterSelect = -7 };
#define cmpWantsRegisterMessage 0x80000000u
#define cmpIsMissing 0x20000000u

typedef struct comp {
    struct comp *next;
    cdesc_t d;
    croutine_t entry;
    u8 name[256], info[256];                                                     /* Str255 */
    long refcon;
    int ninst, captured;
    struct comp *capturer;
} comp_t;
typedef struct inst {
    struct inst *next;
    comp_t *c;
    Handle storage;
    long refcon;
    OSErr err;
    void *target;
} inst_t;
static comp_t *g_comps;
static inst_t *g_insts;
static long g_seed = 1;

static comp_t *reg_of(void *c) { for (comp_t *p = g_comps; p; p = p->next) if (p == c) return p; return 0; }
static inst_t *inst_of(void *i) { for (inst_t *p = g_insts; p; p = p->next) if (p == i) return p; return 0; }
static int is_audio(void *c) { for (void *a = AudioComponentFindNext(0, 0); a; a = AudioComponentFindNext(a, 0)) if (a == c) return 1; return 0; }
static int match(const cdesc_t *want, const cdesc_t *have) {
    if (!want) return 1;
    if (want->componentType && want->componentType != have->componentType) return 0;
    if (want->componentSubType && want->componentSubType != have->componentSubType) return 0;
    if (want->componentManufacturer && want->componentManufacturer != have->componentManufacturer) return 0;
    return (have->componentFlags & want->componentFlagsMask) == (want->componentFlags & want->componentFlagsMask);
}
/* chamada à rotina de um componente registrado */
static ComponentResult call(comp_t *c, Handle storage, i16 what, int n, const long *args, void *self) {
    long buf[8 + 2];
    cparams_t *p = (cparams_t *)buf;
    p->flags = 0;
    p->paramSize = (u8)(n * (int)sizeof(long));
    p->what = what;
    p->padding = 0;
    for (int i = 0; i < n; i++) p->params[i] = args[i];
    p->params[n] = (long)self;
    return c->entry(p, storage);
}

/* ---- busca */
void *FindNextComponent(void *prev, const cdesc_t *d) {
    comp_t *start = g_comps;
    void *audio_after = 0;
    if (prev) {
        comp_t *r = reg_of(prev);
        if (r) start = r->next;
        else { start = 0; audio_after = prev; }
    }
    for (comp_t *c = start; c; c = c->next) if (!(c->d.componentFlags & cmpIsMissing) && !c->captured && match(d, &c->d)) return c;
    for (void *a = AudioComponentFindNext(audio_after, 0); a; a = AudioComponentFindNext(a, 0)) {
        cdesc_t ad;
        if (!AudioComponentGetDescription(a, &ad) && match(d, &ad)) return a;
    }
    return 0;
}
long CountComponents(const cdesc_t *d) { long n = 0; for (void *c = FindNextComponent(0, d); c; c = FindNextComponent(c, d)) n++; return n; }
long GetComponentListModSeed(void) { return g_seed; }
static void pstr_to_handle(Handle h, const u8 *s) {
    if (!h) return;
    SetHandleSize(h, (long)s[0] + 1);
    if (*h) memcpy(*h, s, (size_t)s[0] + 1);
}
OSErr GetComponentInfo(void *c, cdesc_t *d, Handle name, Handle info, Handle icon) {
    (void)icon;
    comp_t *r = reg_of(c);
    if (r) {
        if (d) *d = r->d;
        pstr_to_handle(name, r->name);
        pstr_to_handle(info, r->info);
        return noErr;
    }
    if (!is_audio(c)) return invalidComponentID;
    if (d) { AudioComponentGetDescription(c, d); d->componentFlags = 0; d->componentFlagsMask = 0; }
    if (name || info) {
        id s = 0;
        u8 p[256] = {0};
        if (!AudioComponentCopyName(c, &s) && s) {
            const char *u = cx_cstr(s);
            size_t n = strlen(u);
            if (n > 255) n = 255;
            p[0] = (u8)n;
            memcpy(p + 1, u, n);
            objc_release(s);
        }
        pstr_to_handle(name, p);
        pstr_to_handle(info, p);
    }
    return noErr;
}
long GetComponentVersion(void *inst);
long GetComponentRefcon(void *c) { comp_t *r = reg_of(c); return r ? r->refcon : 0; }
void SetComponentRefcon(void *c, long v) { comp_t *r = reg_of(c); if (r) r->refcon = v; }

/* ---- registro */
void *RegisterComponent(const cdesc_t *d, croutine_t entry, short global, Handle name, Handle info, Handle icon) {
    (void)global; (void)icon;
    if (!d || !entry) return 0;
    comp_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    c->d = *d;
    c->d.componentFlagsMask = 0;
    c->entry = entry;
    if (name && *name) memcpy(c->name, *name, (size_t)(u8)(*name)[0] + 1);
    if (info && *info) memcpy(c->info, *info, (size_t)(u8)(*info)[0] + 1);
    if (d->componentFlags & cmpWantsRegisterMessage) {                          /* o componente pode recusar o registro */
        if (call(c, 0, kComponentRegisterSelect, 0, 0, 0) != 0) { free(c); return 0; }
    }
    comp_t **pp = &g_comps;
    while (*pp) pp = &(*pp)->next;
    *pp = c;
    g_seed++;
    return c;
}
OSErr UnregisterComponent(void *c) {
    comp_t *r = reg_of(c);
    if (!r) return invalidComponentID;
    if (r->ninst) return validInstancesExist;
    for (comp_t **pp = &g_comps; *pp; pp = &(*pp)->next) if (*pp == r) { *pp = r->next; break; }
    free(r);
    g_seed++;
    return noErr;
}

/* ---- instâncias */
OSErr OpenAComponent(void *c, void **out) {
    if (!out) return paramErr;
    *out = 0;
    comp_t *r = reg_of(c);
    if (!r) {
        if (!is_audio(c)) return invalidComponentID;
        OSStatus st = AudioComponentInstanceNew(c, out);
        return st ? (OSErr)(st < -32768 || st > 32767 ? invalidComponentID : st) : noErr;
    }
    inst_t *i = calloc(1, sizeof *i);
    if (!i) return memFullErr;
    i->c = r;
    i->next = g_insts; g_insts = i;
    r->ninst++;
    long self = (long)i;
    ComponentResult res = call(r, 0, kComponentOpenSelect, 1, &self, i);
    if (res) {
        call(r, i->storage, kComponentCloseSelect, 1, &self, i);
        for (inst_t **pp = &g_insts; *pp; pp = &(*pp)->next) if (*pp == i) { *pp = i->next; break; }
        r->ninst--;
        free(i);
        return (OSErr)res;
    }
    *out = i;
    return noErr;
}
void *OpenComponent(void *c) { void *i = 0; return OpenAComponent(c, &i) ? 0 : i; }
OSErr CloseComponent(void *ci) {
    inst_t *i = inst_of(ci);
    if (!i) {
        if (ci && AudioComponentInstanceGetComponent(ci)) return AudioComponentInstanceDispose(ci) ? (OSErr)paramErr : noErr;
        return (OSErr)badComponentInstance;
    }
    long self = (long)i;
    call(i->c, i->storage, kComponentCloseSelect, 1, &self, i);
    for (inst_t **pp = &g_insts; *pp; pp = &(*pp)->next) if (*pp == i) { *pp = i->next; break; }
    i->c->ninst--;
    free(i);
    return noErr;
}
OSErr OpenADefaultComponent(u32 type, u32 subtype, void **out) {
    cdesc_t d = {type, subtype, 0, 0, 0};
    void *c = FindNextComponent(0, &d);
    if (!c) { if (out) *out = 0; return invalidComponentID; }
    return OpenAComponent(c, out);
}
void *OpenDefaultComponent(u32 type, u32 subtype) { void *i = 0; return OpenADefaultComponent(type, subtype, &i) ? 0 : i; }
Handle GetComponentInstanceStorage(void *ci) { inst_t *i = inst_of(ci); return i ? i->storage : 0; }
void SetComponentInstanceStorage(void *ci, Handle h) { inst_t *i = inst_of(ci); if (i) i->storage = h; }
OSErr GetComponentInstanceError(void *ci) { inst_t *i = inst_of(ci); if (!i) return (OSErr)badComponentInstance; OSErr e = i->err; i->err = 0; return e; }
void SetComponentInstanceError(void *ci, OSErr e) { inst_t *i = inst_of(ci); if (i) i->err = e; }
long GetComponentInstanceA5(void *ci) { (void)ci; return 0; }
void SetComponentInstanceA5(void *ci, long a5) { (void)ci; (void)a5; }
/* ---- chamadas */
ComponentResult CallComponentDispatch(cparams_t *p) {
    if (!p) return badComponentInstance;
    void *ci = (void *)p->params[(p->paramSize + sizeof(long) - 1) / sizeof(long)];
    inst_t *i = inst_of(ci);
    if (i) return i->c->entry(p, i->storage);
    if (ci && AudioComponentInstanceGetComponent(ci)) {                          /* unidade de áudio: só os seletores gerais */
        u32 v = 0;
        switch (p->what) {
        case kComponentVersionSelect: AudioComponentGetVersion(AudioComponentInstanceGetComponent(ci), &v); return (ComponentResult)v;
        case kComponentCanDoSelect: {
            i16 sel = (i16)p->params[0];
            return sel == kComponentOpenSelect || sel == kComponentCloseSelect || sel == kComponentCanDoSelect || sel == kComponentVersionSelect;
        }
        }
        return badComponentSelector;
    }
    return badComponentInstance;
}
long GetComponentVersion(void *ci) {
    inst_t *i = inst_of(ci);
    if (i) return call(i->c, i->storage, kComponentVersionSelect, 0, 0, i);
    if (ci && AudioComponentInstanceGetComponent(ci)) { u32 v = 0; AudioComponentGetVersion(AudioComponentInstanceGetComponent(ci), &v); return (long)v; }
    return badComponentInstance;
}
long ComponentFunctionImplemented(void *ci, short sel) {
    inst_t *i = inst_of(ci);
    long arg = sel;
    if (i) return call(i->c, i->storage, kComponentCanDoSelect, 1, &arg, i);
    if (ci && AudioComponentInstanceGetComponent(ci))
        return sel == kComponentOpenSelect || sel == kComponentCloseSelect || sel == kComponentCanDoSelect || sel == kComponentVersionSelect;
    return 0;
}
/* um componente delega a outro (o "target") */
ComponentResult DelegateComponentCall(cparams_t *p, void *ci) {
    inst_t *i = inst_of(ci);
    if (!i || !p) return badComponentInstance;
    long *slot = &p->params[(p->paramSize + sizeof(long) - 1) / sizeof(long)];
    long saved = *slot;
    *slot = (long)i;
    ComponentResult r = i->c->entry(p, i->storage);
    *slot = saved;
    return r;
}
ComponentResult ComponentSetTarget(void *ci, void *target) {
    inst_t *i = inst_of(ci);
    if (!i) return badComponentInstance;
    i->target = target;
    long arg = (long)target;
    return call(i->c, i->storage, kComponentTargetSelect, 1, &arg, i);
}
void *CaptureComponent(void *captured, void *capturing) {
    comp_t *a = reg_of(captured), *b = reg_of(capturing);
    if (!a || a->captured) return 0;
    a->captured = 1; a->capturer = b;
    return a;
}
OSErr UncaptureComponent(void *c) {
    comp_t *r = reg_of(c);
    if (!r) return invalidComponentID;
    if (!r->captured) return componentNotCaptured;
    r->captured = 0; r->capturer = 0;
    return noErr;
}
void *ResolveComponentAlias(void *c) { return reg_of(c) || is_audio(c) ? c : 0; }
