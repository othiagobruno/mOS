/* Virtualization no libSystem.
 * Mesma política de userland/frameworks/Virtualization: sem hipervisor.
 * Se o Mach-O do framework estiver no disco, o loader usa ele (as classes
 * daqui são stub e perdem o cx_class_named). Sem o arquivo, o bind cai aqui.
 * +isSupported devolve NÃO. validate* e start* devolvem NSError "não suportado".
 */
#include "foundation.h"
#include "libc.h"
#include "../frameworks/Virtualization/vz_policy.h"

void cx_mark_stub_class(cx_class_t *c);

#define VZCLS(N) \
    cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); \
    static cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N)
#define VZ_DEF_SYM(N) VZCLS(N);
VZ_CLASS_LIST(VZ_DEF_SYM)
#undef VZ_DEF_SYM

typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id); } vz_blk_t;

static id vz_err(void) {
    const char *msg = 0;
    id ui;
    vz_policy_validate(&msg);
    ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:",
             cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:",
               cx_str(VZ_ERROR_DOMAIN), (long)VZ_ERROR_CODE, ui);
}

static BOOL vz_supported(id cls, SEL c) { (void)cls; (void)c; return vz_policy_supported() ? YES : NO; }
static BOOL vz_validate(id self, SEL c, id *err) {
    const char *msg = 0;
    (void)self; (void)c;
    if (vz_policy_validate(&msg)) return YES;
    if (err) *err = vz_err();
    return NO;
}
static BOOL vz_validate_url(id self, SEL c, id url, id *err) {
    (void)url;
    return vz_validate(self, c, err);
}
static void vz_fail_block(id handler) {
    const char *msg = 0;
    vz_blk_t *b = (vz_blk_t *)handler;
    vz_policy_start(&msg);
    if (b && b->invoke) b->invoke(handler, vz_err());
}
static void vz_start(id self, SEL c, id handler) { (void)self; (void)c; vz_fail_block(handler); }
static void vz_start_opt(id self, SEL c, unsigned long opts, id handler) {
    (void)self; (void)c; (void)opts; vz_fail_block(handler);
}
static void vz_fail_url(id self, SEL c, id url, id handler) {
    (void)self; (void)c; (void)url; vz_fail_block(handler);
}
static id vz_init_cfg(id self, SEL c, id cfg) { (void)c; (void)cfg; return self; }
static long vz_state(id self, SEL c) { (void)self; (void)c; return vz_policy_state(); }
static BOOL vz_no(id self, SEL c) { (void)self; (void)c; return vz_policy_can() ? YES : NO; }
static BOOL vz_reqstop(id self, SEL c, id *err) { return vz_validate(self, c, err); }
static id vz_disk_init(id self, SEL c, id url, BOOL ro, id *err) {
    const char *msg = 0;
    (void)c; (void)url; (void)ro;
    if (vz_policy_disk(&msg)) return self;
    if (err) *err = vz_err();
    return nil;
}
static id vz_efi_init(id self, SEL c, id url, id *err) {
    const char *msg = 0;
    (void)c; (void)url;
    if (vz_policy_efi(&msg)) return self;
    if (err) *err = vz_err();
    return nil;
}
static id vz_kernel_init(id self, SEL c, id url) { (void)c; (void)url; return self; }
static id vz_nil(id self, SEL c) { (void)self; (void)c; return vz_policy_mac() ? self : nil; }

static void vz_def(cx_class_t *c, cx_class_t *m, const char *name, cx_class_t *super) {
    cx_define_class(c, m, name, super ? super : cx_class_named("NSObject"), 0);
    cx_mark_stub_class(c);
    cx_add_cmethod(c, "isSupported", (void *)vz_supported);
    cx_add_method(c, "validateWithError:", (void *)vz_validate);
    cx_add_method(c, "validateSaveRestoreAtURL:error:", (void *)vz_validate_url);
}

static cx_class_t *vz_super(const char *name) {
    if (!strcmp(name, "VZUSBKeyboardConfiguration")) return &VZKeyboardConfiguration_c;
    if (!strcmp(name, "VZUSBScreenCoordinatePointingDeviceConfiguration")) return &VZPointingDeviceConfiguration_c;
    if (!strcmp(name, "VZVirtioBlockDeviceConfiguration")) return &VZStorageDeviceConfiguration_c;
    if (!strcmp(name, "VZVirtioConsoleDeviceConfiguration")) return &VZConsoleDeviceConfiguration_c;
    if (!strcmp(name, "VZVirtioEntropyDeviceConfiguration")) return &VZEntropyDeviceConfiguration_c;
    if (!strcmp(name, "VZVirtioFileSystemDeviceConfiguration")) return &VZDirectorySharingDeviceConfiguration_c;
    if (!strcmp(name, "VZVirtioGraphicsDeviceConfiguration")) return &VZGraphicsDeviceConfiguration_c;
    if (!strcmp(name, "VZVirtioNetworkDeviceConfiguration")) return &VZNetworkDeviceConfiguration_c;
    if (!strcmp(name, "VZVirtioSocketDevice")) return &VZSocketDevice_c;
    if (!strcmp(name, "VZVirtioSocketDeviceConfiguration")) return &VZSocketDeviceConfiguration_c;
    if (!strcmp(name, "VZVirtioTraditionalMemoryBalloonDeviceConfiguration")) return &VZMemoryBalloonDeviceConfiguration_c;
    return 0;
}

void cx_virtualization_init(void) {
#define VZ_DEF_ONE(N) vz_def(&N##_c, &N##_m, #N, vz_super(#N));
    VZ_CLASS_LIST(VZ_DEF_ONE)
#undef VZ_DEF_ONE

    cx_add_method(&VZVirtualMachine_c, "initWithConfiguration:", (void *)vz_init_cfg);
    cx_add_method(&VZVirtualMachine_c, "state", (void *)vz_state);
    cx_add_method(&VZVirtualMachine_c, "canStart", (void *)vz_no);
    cx_add_method(&VZVirtualMachine_c, "canStop", (void *)vz_no);
    cx_add_method(&VZVirtualMachine_c, "canPause", (void *)vz_no);
    cx_add_method(&VZVirtualMachine_c, "canResume", (void *)vz_no);
    cx_add_method(&VZVirtualMachine_c, "canRequestStop", (void *)vz_no);
    cx_add_method(&VZVirtualMachine_c, "requestStopWithError:", (void *)vz_reqstop);
    cx_add_method(&VZVirtualMachine_c, "startWithCompletionHandler:", (void *)vz_start);
    cx_add_method(&VZVirtualMachine_c, "startWithOptions:completionHandler:", (void *)vz_start_opt);
    cx_add_method(&VZVirtualMachine_c, "stopWithCompletionHandler:", (void *)vz_start);
    cx_add_method(&VZVirtualMachine_c, "pauseWithCompletionHandler:", (void *)vz_start);
    cx_add_method(&VZVirtualMachine_c, "resumeWithCompletionHandler:", (void *)vz_start);
    cx_add_method(&VZVirtualMachine_c, "saveMachineStateToURL:completionHandler:", (void *)vz_fail_url);
    cx_add_method(&VZVirtualMachine_c, "restoreMachineStateFromURL:completionHandler:", (void *)vz_fail_url);
    cx_add_method(&VZDiskImageStorageDeviceAttachment_c, "initWithURL:readOnly:error:", (void *)vz_disk_init);
    cx_add_method(&VZEFIVariableStore_c, "initWithURL:error:", (void *)vz_efi_init);
    cx_add_method(&VZLinuxBootLoader_c, "initWithKernelURL:", (void *)vz_kernel_init);
    cx_add_cmethod(&VZMACAddress_c, "randomLocallyAdministeredAddress", (void *)vz_nil);
    cx_add_method(&VZMACAddress_c, "string", (void *)vz_nil);
}
