/*
 * UserNotifications: autorização local no mesmo arquivo do TCC (serviço
 * "userNotifications"). Não há APNs. O completion do request volta depois da escolha no diálogo do TCC
 * (cx_tcc_request_async, tcc.c).
 *
 * StoreKit, QuickLookUI, Core Data, Photos e os quatro frameworks do WhatsApp estão em storekit_fw.c,
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
void cx_virtualization_init(void);
void cx_wa_frameworks_init(void);
void cx_mapkit_init(void);

int cx_tcc_request(const char *service);
int cx_tcc_state(const char *service);
int cx_tcc_decide(const char *bundle, const char *service, int granted);


static id g_un;
static id un_current_real(id cls, SEL c) {
    (void)c;
    if (!g_un) g_un = cx_alloc_instance((cx_class_t *)cls);
    return g_un;
}

void cx_tcc_request_async(const char *service, void (*cb)(int granted, void *ctx), void *ctx);
typedef struct { void *isa; int flags; int reserved; void (*invoke)(void *, int, id); } un_blk_t;
static void un_request_done(int granted, void *blk) {
    un_blk_t *b = blk;
    if (b->invoke) b->invoke(blk, granted ? 1 : 0, 0);
    _Block_release(blk);
}
/* requestAuthorizationWithOptions: o diálogo "enviar notificações" é o do TCC (kernel/ui/tcc_prompt.c); o bloco
 * volta com a escolha, ou na hora se ela já existe */
static void un_request(id self, SEL c, unsigned long opts, void *block) {
    (void)self; (void)c; (void)opts;
    if (!block) { cx_tcc_request("userNotifications"); return; }
    cx_tcc_request_async("userNotifications", un_request_done, _Block_copy(block));
}

static long un_auth(id self, SEL c) {
    (void)self; (void)c;
    int st = cx_tcc_state("userNotifications");
    if (st == 2) return 2; /* UNAuthorizationStatusAuthorized */
    if (st == 1) return 1; /* denied */
    return 0;              /* notDetermined, inclusive pending */
}

/* decisão vinda do próprio processo (testes): o bloco pendente volta pelo acompanhamento de cx_tcc_request_async */
void cx_un_decide(int granted) { cx_tcc_decide(0, "userNotifications", granted); }

void cx_userspace_services_init(void) {
    cx_storekit_init();
    cx_quicklook_init();
    cx_coredata_init();
    cx_photos_init();
    cx_virtualization_init();
    cx_wa_frameworks_init();
    cx_mapkit_init();
    cx_add_cmethod(&UNUserNotificationCenter_c, "currentNotificationCenter", (void *)un_current_real);
    cx_add_method(&UNUserNotificationCenter_c, "requestAuthorizationWithOptions:completionHandler:", (void *)un_request);
    cx_add_method(&UNUserNotificationCenter_c, "authorizationStatus", (void *)un_auth);
}
