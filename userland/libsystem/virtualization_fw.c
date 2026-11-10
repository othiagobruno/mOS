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

VZCLS(VZConsoleDeviceConfiguration);
VZCLS(VZDirectorySharingDeviceConfiguration);
VZCLS(VZDiskImageStorageDeviceAttachment);
VZCLS(VZEFIBootLoader);
VZCLS(VZEFIVariableStore);
VZCLS(VZEntropyDeviceConfiguration);
VZCLS(VZFileHandleNetworkDeviceAttachment);
VZCLS(VZFileHandleSerialPortAttachment);
VZCLS(VZGenericMachineIdentifier);
VZCLS(VZGenericPlatformConfiguration);
VZCLS(VZGraphicsDeviceConfiguration);
VZCLS(VZKeyboardConfiguration);
VZCLS(VZLinuxBootLoader);
VZCLS(VZMACAddress);
VZCLS(VZMemoryBalloonDeviceConfiguration);
VZCLS(VZMultipleDirectoryShare);
VZCLS(VZNATNetworkDeviceAttachment);
VZCLS(VZNVMExpressControllerDeviceConfiguration);
VZCLS(VZNetworkDeviceConfiguration);
VZCLS(VZPointingDeviceConfiguration);
VZCLS(VZSharedDirectory);
VZCLS(VZSocketDevice);
VZCLS(VZSocketDeviceConfiguration);
VZCLS(VZStorageDeviceConfiguration);
VZCLS(VZUSBKeyboardConfiguration);
VZCLS(VZUSBScreenCoordinatePointingDeviceConfiguration);
VZCLS(VZVirtioBlockDeviceConfiguration);
VZCLS(VZVirtioConsoleDeviceConfiguration);
VZCLS(VZVirtioConsolePortConfiguration);
VZCLS(VZVirtioEntropyDeviceConfiguration);
VZCLS(VZVirtioFileSystemDeviceConfiguration);
VZCLS(VZVirtioGraphicsDeviceConfiguration);
VZCLS(VZVirtioGraphicsScanoutConfiguration);
VZCLS(VZVirtioNetworkDeviceConfiguration);
VZCLS(VZVirtioSocketDevice);
VZCLS(VZVirtioSocketDeviceConfiguration);
VZCLS(VZVirtioSocketListener);
VZCLS(VZVirtioTraditionalMemoryBalloonDeviceConfiguration);
VZCLS(VZVirtualMachine);
VZCLS(VZVirtualMachineConfiguration);
VZCLS(VZVmnetNetworkDeviceAttachment);

typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id); } vz_blk_t;

static id vz_err(void) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:",
                cx_str(VZ_UNSUPPORTED_MSG), cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:",
               cx_str(VZ_ERROR_DOMAIN), (long)VZ_ERROR_CODE, ui);
}

static BOOL vz_supported(id cls, SEL c) { (void)cls; (void)c; return vz_is_supported() ? YES : NO; }
static BOOL vz_validate(id self, SEL c, id *err) {
    (void)self; (void)c;
    if (err) *err = vz_err();
    return NO;
}
static BOOL vz_validate_url(id self, SEL c, id url, id *err) {
    (void)url;
    return vz_validate(self, c, err);
}
static void vz_fail_block(id handler) {
    vz_blk_t *b = (vz_blk_t *)handler;
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
static long vz_state(id self, SEL c) { (void)self; (void)c; return 0; }
static BOOL vz_no(id self, SEL c) { (void)self; (void)c; return NO; }
static BOOL vz_reqstop(id self, SEL c, id *err) { return vz_validate(self, c, err); }
static id vz_disk_init(id self, SEL c, id url, BOOL ro, id *err) {
    (void)self; (void)c; (void)url; (void)ro;
    if (err) *err = vz_err();
    return nil;
}
static id vz_efi_init(id self, SEL c, id url, id *err) {
    (void)self; (void)c; (void)url;
    if (err) *err = vz_err();
    return nil;
}
static id vz_kernel_init(id self, SEL c, id url) { (void)c; (void)url; return self; }
static id vz_nil(id self, SEL c) { (void)self; (void)c; return nil; }

static void vz_def(cx_class_t *c, cx_class_t *m, const char *name, cx_class_t *super) {
    cx_define_class(c, m, name, super ? super : cx_class_named("NSObject"), 0);
    cx_mark_stub_class(c);
    cx_add_cmethod(c, "isSupported", (void *)vz_supported);
    cx_add_method(c, "validateWithError:", (void *)vz_validate);
    cx_add_method(c, "validateSaveRestoreAtURL:error:", (void *)vz_validate_url);
}

void cx_virtualization_init(void) {
    cx_class_t *ns = cx_class_named("NSObject");
    vz_def(&VZConsoleDeviceConfiguration_c, &VZConsoleDeviceConfiguration_m, "VZConsoleDeviceConfiguration", ns);
    vz_def(&VZDirectorySharingDeviceConfiguration_c, &VZDirectorySharingDeviceConfiguration_m, "VZDirectorySharingDeviceConfiguration", ns);
    vz_def(&VZDiskImageStorageDeviceAttachment_c, &VZDiskImageStorageDeviceAttachment_m, "VZDiskImageStorageDeviceAttachment", ns);
    vz_def(&VZEFIBootLoader_c, &VZEFIBootLoader_m, "VZEFIBootLoader", ns);
    vz_def(&VZEFIVariableStore_c, &VZEFIVariableStore_m, "VZEFIVariableStore", ns);
    vz_def(&VZEntropyDeviceConfiguration_c, &VZEntropyDeviceConfiguration_m, "VZEntropyDeviceConfiguration", ns);
    vz_def(&VZFileHandleNetworkDeviceAttachment_c, &VZFileHandleNetworkDeviceAttachment_m, "VZFileHandleNetworkDeviceAttachment", ns);
    vz_def(&VZFileHandleSerialPortAttachment_c, &VZFileHandleSerialPortAttachment_m, "VZFileHandleSerialPortAttachment", ns);
    vz_def(&VZGenericMachineIdentifier_c, &VZGenericMachineIdentifier_m, "VZGenericMachineIdentifier", ns);
    vz_def(&VZGenericPlatformConfiguration_c, &VZGenericPlatformConfiguration_m, "VZGenericPlatformConfiguration", ns);
    vz_def(&VZGraphicsDeviceConfiguration_c, &VZGraphicsDeviceConfiguration_m, "VZGraphicsDeviceConfiguration", ns);
    vz_def(&VZKeyboardConfiguration_c, &VZKeyboardConfiguration_m, "VZKeyboardConfiguration", ns);
    vz_def(&VZLinuxBootLoader_c, &VZLinuxBootLoader_m, "VZLinuxBootLoader", ns);
    vz_def(&VZMACAddress_c, &VZMACAddress_m, "VZMACAddress", ns);
    vz_def(&VZMemoryBalloonDeviceConfiguration_c, &VZMemoryBalloonDeviceConfiguration_m, "VZMemoryBalloonDeviceConfiguration", ns);
    vz_def(&VZMultipleDirectoryShare_c, &VZMultipleDirectoryShare_m, "VZMultipleDirectoryShare", ns);
    vz_def(&VZNATNetworkDeviceAttachment_c, &VZNATNetworkDeviceAttachment_m, "VZNATNetworkDeviceAttachment", ns);
    vz_def(&VZNVMExpressControllerDeviceConfiguration_c, &VZNVMExpressControllerDeviceConfiguration_m, "VZNVMExpressControllerDeviceConfiguration", ns);
    vz_def(&VZNetworkDeviceConfiguration_c, &VZNetworkDeviceConfiguration_m, "VZNetworkDeviceConfiguration", ns);
    vz_def(&VZPointingDeviceConfiguration_c, &VZPointingDeviceConfiguration_m, "VZPointingDeviceConfiguration", ns);
    vz_def(&VZSharedDirectory_c, &VZSharedDirectory_m, "VZSharedDirectory", ns);
    vz_def(&VZSocketDevice_c, &VZSocketDevice_m, "VZSocketDevice", ns);
    vz_def(&VZSocketDeviceConfiguration_c, &VZSocketDeviceConfiguration_m, "VZSocketDeviceConfiguration", ns);
    vz_def(&VZStorageDeviceConfiguration_c, &VZStorageDeviceConfiguration_m, "VZStorageDeviceConfiguration", ns);
    vz_def(&VZUSBKeyboardConfiguration_c, &VZUSBKeyboardConfiguration_m, "VZUSBKeyboardConfiguration", &VZKeyboardConfiguration_c);
    vz_def(&VZUSBScreenCoordinatePointingDeviceConfiguration_c, &VZUSBScreenCoordinatePointingDeviceConfiguration_m, "VZUSBScreenCoordinatePointingDeviceConfiguration", &VZPointingDeviceConfiguration_c);
    vz_def(&VZVirtioBlockDeviceConfiguration_c, &VZVirtioBlockDeviceConfiguration_m, "VZVirtioBlockDeviceConfiguration", &VZStorageDeviceConfiguration_c);
    vz_def(&VZVirtioConsoleDeviceConfiguration_c, &VZVirtioConsoleDeviceConfiguration_m, "VZVirtioConsoleDeviceConfiguration", &VZConsoleDeviceConfiguration_c);
    vz_def(&VZVirtioConsolePortConfiguration_c, &VZVirtioConsolePortConfiguration_m, "VZVirtioConsolePortConfiguration", ns);
    vz_def(&VZVirtioEntropyDeviceConfiguration_c, &VZVirtioEntropyDeviceConfiguration_m, "VZVirtioEntropyDeviceConfiguration", &VZEntropyDeviceConfiguration_c);
    vz_def(&VZVirtioFileSystemDeviceConfiguration_c, &VZVirtioFileSystemDeviceConfiguration_m, "VZVirtioFileSystemDeviceConfiguration", &VZDirectorySharingDeviceConfiguration_c);
    vz_def(&VZVirtioGraphicsDeviceConfiguration_c, &VZVirtioGraphicsDeviceConfiguration_m, "VZVirtioGraphicsDeviceConfiguration", &VZGraphicsDeviceConfiguration_c);
    vz_def(&VZVirtioGraphicsScanoutConfiguration_c, &VZVirtioGraphicsScanoutConfiguration_m, "VZVirtioGraphicsScanoutConfiguration", ns);
    vz_def(&VZVirtioNetworkDeviceConfiguration_c, &VZVirtioNetworkDeviceConfiguration_m, "VZVirtioNetworkDeviceConfiguration", &VZNetworkDeviceConfiguration_c);
    vz_def(&VZVirtioSocketDevice_c, &VZVirtioSocketDevice_m, "VZVirtioSocketDevice", &VZSocketDevice_c);
    vz_def(&VZVirtioSocketDeviceConfiguration_c, &VZVirtioSocketDeviceConfiguration_m, "VZVirtioSocketDeviceConfiguration", &VZSocketDeviceConfiguration_c);
    vz_def(&VZVirtioSocketListener_c, &VZVirtioSocketListener_m, "VZVirtioSocketListener", ns);
    vz_def(&VZVirtioTraditionalMemoryBalloonDeviceConfiguration_c, &VZVirtioTraditionalMemoryBalloonDeviceConfiguration_m, "VZVirtioTraditionalMemoryBalloonDeviceConfiguration", &VZMemoryBalloonDeviceConfiguration_c);
    vz_def(&VZVirtualMachineConfiguration_c, &VZVirtualMachineConfiguration_m, "VZVirtualMachineConfiguration", ns);
    vz_def(&VZVirtualMachine_c, &VZVirtualMachine_m, "VZVirtualMachine", ns);
    vz_def(&VZVmnetNetworkDeviceAttachment_c, &VZVmnetNetworkDeviceAttachment_m, "VZVmnetNetworkDeviceAttachment", ns);

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
