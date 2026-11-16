/*
 * UserNotifications (centro, autorização e banner): usernotifications.c.
 *
 * StoreKit, QuickLookUI, Core Data, Photos e os quatro frameworks do WhatsApp estão em storekit_fw.c,
 * quicklook_fw.c, coredata_fw.c e photos_fw.c. O init daqui só os registra,
 * depois que as classes de fachada já existem.
 */
#include "foundation.h"
#include "libc.h"

void cx_storekit_init(void);
void cx_quicklook_init(void);
void cx_coredata_init(void);
void cx_photos_init(void);
void cx_virtualization_init(void);
void cx_wa_frameworks_init(void);
void cx_mapkit_init(void);



void cx_userspace_services_init(void) {
    cx_storekit_init();
    cx_quicklook_init();
    cx_coredata_init();
    cx_photos_init();
    cx_virtualization_init();
    cx_wa_frameworks_init();
    cx_mapkit_init();
}
