/*
 * mOS — Virtualization.framework (Mach-O, em /System/Library/Frameworks).
 * O Claude Desktop (swift_addon.node, 4.23118.1) importa 41 classes VZ*.
 * Não há hipervisor: isSupported é NÃO, validate e start devolvem NSError
 * "não suportado". Nenhum método responde que a VM ligou.
 * tools/build-frameworks-objc.ps1 e tools/build.sh
 */
#import <Foundation/Foundation.h>
#include "vz_policy.h"

@interface NSError (MOSVirtualization)
+ (instancetype)errorWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)userInfo;
@end

static NSError *vz_err(void) {
    return [NSError errorWithDomain:@VZ_ERROR_DOMAIN code:VZ_ERROR_CODE
                            userInfo:@{ @"NSLocalizedDescription": @VZ_UNSUPPORTED_MSG }];
}

typedef struct { void *isa; int flags; int reserved; void (*invoke)(void *, id); } vz_blk_t;

static void vz_fail_block(id handler) {
    vz_blk_t *b = (vz_blk_t *)handler;
    if (b && b->invoke) b->invoke(handler, vz_err());
}

#define VZ_COMMON \
+ (BOOL)isSupported { return vz_is_supported() ? YES : NO; } \
- (BOOL)validateWithError:(NSError **)error { if (error) *error = vz_err(); return NO; } \
- (BOOL)validateSaveRestoreAtURL:(id)url error:(NSError **)error { (void)url; if (error) *error = vz_err(); return NO; }

#define VZ_IFACE(Name, Super) @interface Name : Super @end
#define VZ_IMPL(Name) @implementation Name VZ_COMMON @end

VZ_IFACE(VZConsoleDeviceConfiguration, NSObject)
VZ_IFACE(VZDirectorySharingDeviceConfiguration, NSObject)
VZ_IFACE(VZDiskImageStorageDeviceAttachment, NSObject)
VZ_IFACE(VZEFIBootLoader, NSObject)
VZ_IFACE(VZEFIVariableStore, NSObject)
VZ_IFACE(VZEntropyDeviceConfiguration, NSObject)
VZ_IFACE(VZFileHandleNetworkDeviceAttachment, NSObject)
VZ_IFACE(VZFileHandleSerialPortAttachment, NSObject)
VZ_IFACE(VZGenericMachineIdentifier, NSObject)
VZ_IFACE(VZGenericPlatformConfiguration, NSObject)
VZ_IFACE(VZGraphicsDeviceConfiguration, NSObject)
VZ_IFACE(VZKeyboardConfiguration, NSObject)
VZ_IFACE(VZLinuxBootLoader, NSObject)
VZ_IFACE(VZMACAddress, NSObject)
VZ_IFACE(VZMemoryBalloonDeviceConfiguration, NSObject)
VZ_IFACE(VZMultipleDirectoryShare, NSObject)
VZ_IFACE(VZNATNetworkDeviceAttachment, NSObject)
VZ_IFACE(VZNVMExpressControllerDeviceConfiguration, NSObject)
VZ_IFACE(VZNetworkDeviceConfiguration, NSObject)
VZ_IFACE(VZPointingDeviceConfiguration, NSObject)
VZ_IFACE(VZSharedDirectory, NSObject)
VZ_IFACE(VZSocketDevice, NSObject)
VZ_IFACE(VZSocketDeviceConfiguration, NSObject)
VZ_IFACE(VZStorageDeviceConfiguration, NSObject)
VZ_IFACE(VZUSBKeyboardConfiguration, VZKeyboardConfiguration)
VZ_IFACE(VZUSBScreenCoordinatePointingDeviceConfiguration, VZPointingDeviceConfiguration)
VZ_IFACE(VZVirtioBlockDeviceConfiguration, VZStorageDeviceConfiguration)
VZ_IFACE(VZVirtioConsoleDeviceConfiguration, VZConsoleDeviceConfiguration)
VZ_IFACE(VZVirtioConsolePortConfiguration, NSObject)
VZ_IFACE(VZVirtioEntropyDeviceConfiguration, VZEntropyDeviceConfiguration)
VZ_IFACE(VZVirtioFileSystemDeviceConfiguration, VZDirectorySharingDeviceConfiguration)
VZ_IFACE(VZVirtioGraphicsDeviceConfiguration, VZGraphicsDeviceConfiguration)
VZ_IFACE(VZVirtioGraphicsScanoutConfiguration, NSObject)
VZ_IFACE(VZVirtioNetworkDeviceConfiguration, VZNetworkDeviceConfiguration)
VZ_IFACE(VZVirtioSocketDevice, VZSocketDevice)
VZ_IFACE(VZVirtioSocketDeviceConfiguration, VZSocketDeviceConfiguration)
VZ_IFACE(VZVirtioSocketListener, NSObject)
VZ_IFACE(VZVirtioTraditionalMemoryBalloonDeviceConfiguration, VZMemoryBalloonDeviceConfiguration)
VZ_IFACE(VZVirtualMachineConfiguration, NSObject)
VZ_IFACE(VZVirtualMachine, NSObject)
VZ_IFACE(VZVmnetNetworkDeviceAttachment, NSObject)

VZ_IMPL(VZConsoleDeviceConfiguration)
VZ_IMPL(VZDirectorySharingDeviceConfiguration)
VZ_IMPL(VZEntropyDeviceConfiguration)
VZ_IMPL(VZFileHandleNetworkDeviceAttachment)
VZ_IMPL(VZFileHandleSerialPortAttachment)
VZ_IMPL(VZGenericMachineIdentifier)
VZ_IMPL(VZGraphicsDeviceConfiguration)
VZ_IMPL(VZKeyboardConfiguration)
VZ_IMPL(VZMemoryBalloonDeviceConfiguration)
VZ_IMPL(VZMultipleDirectoryShare)
VZ_IMPL(VZNATNetworkDeviceAttachment)
VZ_IMPL(VZNVMExpressControllerDeviceConfiguration)
VZ_IMPL(VZNetworkDeviceConfiguration)
VZ_IMPL(VZPointingDeviceConfiguration)
VZ_IMPL(VZSharedDirectory)
VZ_IMPL(VZSocketDevice)
VZ_IMPL(VZSocketDeviceConfiguration)
VZ_IMPL(VZStorageDeviceConfiguration)
VZ_IMPL(VZUSBKeyboardConfiguration)
VZ_IMPL(VZUSBScreenCoordinatePointingDeviceConfiguration)
VZ_IMPL(VZVirtioBlockDeviceConfiguration)
VZ_IMPL(VZVirtioConsoleDeviceConfiguration)
VZ_IMPL(VZVirtioConsolePortConfiguration)
VZ_IMPL(VZVirtioEntropyDeviceConfiguration)
VZ_IMPL(VZVirtioFileSystemDeviceConfiguration)
VZ_IMPL(VZVirtioGraphicsDeviceConfiguration)
VZ_IMPL(VZVirtioGraphicsScanoutConfiguration)
VZ_IMPL(VZVirtioNetworkDeviceConfiguration)
VZ_IMPL(VZVirtioSocketDevice)
VZ_IMPL(VZVirtioSocketDeviceConfiguration)
VZ_IMPL(VZVirtioSocketListener)
VZ_IMPL(VZVirtioTraditionalMemoryBalloonDeviceConfiguration)
VZ_IMPL(VZVmnetNetworkDeviceAttachment)

@implementation VZDiskImageStorageDeviceAttachment
VZ_COMMON
- (instancetype)initWithURL:(id)url readOnly:(BOOL)ro error:(NSError **)error {
    (void)url; (void)ro;
    if (error) *error = vz_err();
    return nil;
}
@end

@implementation VZEFIVariableStore
VZ_COMMON
- (instancetype)initWithURL:(id)url error:(NSError **)error {
    (void)url;
    if (error) *error = vz_err();
    return nil;
}
@end

@implementation VZEFIBootLoader
VZ_COMMON
@end

@implementation VZGenericPlatformConfiguration
VZ_COMMON
@end

@implementation VZLinuxBootLoader
VZ_COMMON
- (instancetype)initWithKernelURL:(id)url { (void)url; return self; }
@end

@implementation VZMACAddress
VZ_COMMON
+ (instancetype)randomLocallyAdministeredAddress { return nil; }
- (NSString *)string { return nil; }
@end

@implementation VZVirtualMachineConfiguration
VZ_COMMON
@end

@implementation VZVirtualMachine
VZ_COMMON
- (instancetype)initWithConfiguration:(id)configuration { (void)configuration; return self; }
- (NSInteger)state { return 0; } /* VZVirtualMachineStateStopped */
- (BOOL)canStart { return NO; }
- (BOOL)canStop { return NO; }
- (BOOL)canPause { return NO; }
- (BOOL)canResume { return NO; }
- (BOOL)canRequestStop { return NO; }
- (BOOL)requestStopWithError:(NSError **)error { if (error) *error = vz_err(); return NO; }
- (void)startWithCompletionHandler:(id)handler { vz_fail_block(handler); }
- (void)startWithOptions:(NSUInteger)options completionHandler:(id)handler { (void)options; vz_fail_block(handler); }
- (void)stopWithCompletionHandler:(id)handler { vz_fail_block(handler); }
- (void)pauseWithCompletionHandler:(id)handler { vz_fail_block(handler); }
- (void)resumeWithCompletionHandler:(id)handler { vz_fail_block(handler); }
- (void)saveMachineStateToURL:(id)url completionHandler:(id)handler { (void)url; vz_fail_block(handler); }
- (void)restoreMachineStateFromURL:(id)url completionHandler:(id)handler { (void)url; vz_fail_block(handler); }
@end
