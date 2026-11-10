/*
 * UserNotifications: autorização local no mesmo arquivo do TCC (serviço
 * "userNotifications"). Não há APNs. O completion do request só roda quando a
 * Interface chama cx_un_decide.
 *
 * StoreKit, QuickLookUI, Core Data e Photos estão em storekit_fw.c,
 * quicklook_fw.c, coredata_fw.c e photos_fw.c. O init daqui só os registra,
 * depois que as classes de fachada já existem.
 */
#include "foundation.h"
#include "libc.h"

void *_Block_copy(const void *b);
void _Block_release(const void *b);
extern cx_class_t UNUserNotificationCenter_c __asm__("OBJC_CLASS_$_UNUserNotificationCenter");
void cx_storekit_init(void);
void cx_quicklook_init(void);
void cx_coredata_init(void);
void cx_photos_init(void);

int cx_tcc_request(const char *service);
int cx_tcc_state(const char *service);
int cx_tcc_decide(const char *bundle, const char *service, int granted);

static void *g_un_block;

static id g_un;
static id un_current_real(id cls, SEL c) {
    (void)c;
    if (!g_un) g_un = cx_alloc_instance((cx_class_t *)cls);
    return g_un;
}

static void un_request(id self, SEL c, unsigned long opts, void *block) {
    (void)self; (void)c; (void)opts;
    cx_tcc_request("userNotifications");
    if (g_un_block) _Block_release(g_un_block);
    g_un_block = block ? _Block_copy(block) : 0;
}

static long un_auth(id self, SEL c) {
    (void)self; (void)c;
    int st = cx_tcc_state("userNotifications");
    if (st == 2) return 2; /* UNAuthorizationStatusAuthorized */
    if (st == 1) return 1; /* denied */
    return 0;              /* notDetermined, inclusive pending */
}

void cx_un_decide(int granted) {
    cx_tcc_decide(0, "userNotifications", granted);
    if (!g_un_block) return;
    typedef struct { void *isa; int flags; int reserved; void (*invoke)(void *, int, id); } un_blk_t;
    un_blk_t *b = (un_blk_t *)g_un_block;
    if (b->invoke) b->invoke(g_un_block, granted ? 1 : 0, 0);
}

void cx_userspace_services_init(void) {
    cx_storekit_init();
    cx_quicklook_init();
    cx_coredata_init();
    cx_photos_init();
    cx_add_cmethod(&UNUserNotificationCenter_c, "currentNotificationCenter", (void *)un_current_real);
    cx_add_method(&UNUserNotificationCenter_c, "requestAuthorizationWithOptions:completionHandler:", (void *)un_request);
    cx_add_method(&UNUserNotificationCenter_c, "authorizationStatus", (void *)un_auth);
}
