/*
 * CFNotificationCenter.
 * Local: o NSNotificationCenter padrão. cx_post entrega os callbacks C (cx_cfnotify_from_ns).
 * Darwin: entrega neste processo (object e userInfo NULL, como a Apple) e notify_post.
 *   O eco do notifyd é engolido por um contador por nome. Limitação: um post externo do
 *   mesmo nome que chegue antes do eco pode ser engolido.
 * Distributed: só neste processo. Não há distnoted; userInfo não atravessa processo.
 * Suspension (Drop/Coalesce/Hold) é aceito e a entrega é sempre síncrona e imediata.
 * No máximo 128 observadores por post (o núcleo tira um snapshot).
 */
#include "foundation.h"
#include "libc.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

#include "cfnotify_core.inc"

id objc_release(id o);
void *dispatch_get_global_queue(long pri, unsigned long flags);
unsigned notify_register_dispatch(const char *name, int *token, void *queue, void *handler);
unsigned notify_post(const char *name);

static char g_darwin_center;
static char g_dist_center;
static cfn_tab *g_tab;

static cfn_tab *tab(void) {
    if (!g_tab) g_tab = cfn_new();
    return g_tab;
}

id CFNotificationCenterGetLocalCenter(void) {
    return MSG0(id, (id)cx_class_named("NSNotificationCenter"), "defaultCenter");
}
id CFNotificationCenterGetDarwinNotifyCenter(void) { return (id)&g_darwin_center; }
id CFNotificationCenterGetDistributedCenter(void) { return (id)&g_dist_center; }

static const char *note_cstr(id name) {
    if (!name) return 0;
    if (cx_is_str(name)) return ((nsstr_t *)name)->s ? ((nsstr_t *)name)->s : "";
    return cx_cstr(name);
}

#define DARWIN_SLOTS 32
static struct { char name[128]; int token; int suppress; } g_slots[DARWIN_SLOTS];

typedef struct { void *isa; int flags; int reserved; void (*invoke)(void *, int); } dn_blk_t;
static void echo_invoke(void *b, int token) {
    (void)b;
    for (int i = 0; i < DARWIN_SLOTS; i++) if (g_slots[i].name[0] && g_slots[i].token == token) {
        if (g_slots[i].suppress > 0) { g_slots[i].suppress--; return; }
        id nm = cx_str(g_slots[i].name);
        cfn_post(tab(), &g_darwin_center, g_slots[i].name, nm, 0, 0);
        if (nm) objc_release(nm);
        return;
    }
}
static dn_blk_t g_echo = { 0, 1 << 28, 0, echo_invoke };

static void darwin_arm(const char *name) {
    if (!name || !name[0]) return;
    for (int i = 0; i < DARWIN_SLOTS; i++) if (!strcmp(g_slots[i].name, name)) return;
    for (int i = 0; i < DARWIN_SLOTS; i++) if (!g_slots[i].name[0]) {
        int tok = 0;
        if (notify_register_dispatch(name, &tok, dispatch_get_global_queue(0, 0), &g_echo) != 0) return;
        snprintf(g_slots[i].name, sizeof g_slots[i].name, "%s", name);
        g_slots[i].token = tok;
        return;
    }
}

static void darwin_post(id name) {
    const char *s = note_cstr(name);
    if (!s) return;
    cfn_post(tab(), &g_darwin_center, s, name, 0, 0);
    for (int i = 0; i < DARWIN_SLOTS; i++) if (g_slots[i].name[0] && !strcmp(g_slots[i].name, s)) {
        g_slots[i].suppress++;
        if (notify_post(s) != 0) g_slots[i].suppress--;
        return;
    }
}

void CFNotificationCenterAddObserver(id c, const void *obs, void *cb, id name, const void *obj, long behavior) {
    (void)behavior;
    if (!c) c = CFNotificationCenterGetLocalCenter();
    const char *n = note_cstr(name);
    cfn_add(tab(), c, (void *)obs, (cfn_fn)cb, n, (void *)obj);
    if (c == (id)&g_darwin_center && n) darwin_arm(n);
}

void CFNotificationCenterRemoveObserver(id c, const void *obs, id name, const void *obj) {
    if (!c) c = CFNotificationCenterGetLocalCenter();
    cfn_remove(tab(), c, (void *)obs, note_cstr(name), (void *)obj, name ? 0 : 1, obj ? 0 : 1);
}

void CFNotificationCenterRemoveEveryObserver(id c, const void *obs) {
    if (!c) c = CFNotificationCenterGetLocalCenter();
    cfn_remove(tab(), c, (void *)obs, 0, 0, 1, 1);
}

void CFNotificationCenterPostNotification(id c, id name, const void *obj, id info, unsigned char now) {
    (void)now;
    if (!c) c = CFNotificationCenterGetLocalCenter();
    if (c == (id)&g_darwin_center) { darwin_post(name); return; }
    if (c == (id)&g_dist_center) {
        cfn_post(tab(), &g_dist_center, note_cstr(name), name, (void *)obj, info);
        return;
    }
    id local = CFNotificationCenterGetLocalCenter();
    if (c != local) cfn_post(tab(), c, note_cstr(name), name, (void *)obj, info);
    id note = cx_note(name, (id)obj, info);
    cx_post(note);
    objc_release(note);
}

void cx_cfnotify_from_ns(id note) {
    if (!note || !g_tab) return;
    id name = MSG0(id, note, "name");
    id obj = MSG0(id, note, "object");
    id info = MSG0(id, note, "userInfo");
    cfn_post(g_tab, CFNotificationCenterGetLocalCenter(), note_cstr(name), name, obj, info);
}
