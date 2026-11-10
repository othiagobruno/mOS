/* Comportamento do Virtualization.framework no mOS.
 * Stub de compatibilidade: as classes existem e cada chamada tem resultado
 * definido. Não há hipervisor. isSupported == 0 é a limitação, não a entrega. */
#pragma once

#define VZ_UNSUPPORTED_MSG "não suportado"
#define VZ_ERROR_DOMAIN "VZErrorDomain"
#define VZ_ERROR_CODE 1
#define VZ_STATE_STOPPED 0

#define VZ_CLASS_LIST(X) \
    X(VZConsoleDeviceConfiguration) \
    X(VZDirectorySharingDeviceConfiguration) \
    X(VZDiskImageStorageDeviceAttachment) \
    X(VZEFIBootLoader) \
    X(VZEFIVariableStore) \
    X(VZEntropyDeviceConfiguration) \
    X(VZFileHandleNetworkDeviceAttachment) \
    X(VZFileHandleSerialPortAttachment) \
    X(VZGenericMachineIdentifier) \
    X(VZGenericPlatformConfiguration) \
    X(VZGraphicsDeviceConfiguration) \
    X(VZKeyboardConfiguration) \
    X(VZLinuxBootLoader) \
    X(VZMACAddress) \
    X(VZMemoryBalloonDeviceConfiguration) \
    X(VZMultipleDirectoryShare) \
    X(VZNATNetworkDeviceAttachment) \
    X(VZNVMExpressControllerDeviceConfiguration) \
    X(VZNetworkDeviceConfiguration) \
    X(VZPointingDeviceConfiguration) \
    X(VZSharedDirectory) \
    X(VZSocketDevice) \
    X(VZSocketDeviceConfiguration) \
    X(VZStorageDeviceConfiguration) \
    X(VZUSBKeyboardConfiguration) \
    X(VZUSBScreenCoordinatePointingDeviceConfiguration) \
    X(VZVirtioBlockDeviceConfiguration) \
    X(VZVirtioConsoleDeviceConfiguration) \
    X(VZVirtioConsolePortConfiguration) \
    X(VZVirtioEntropyDeviceConfiguration) \
    X(VZVirtioFileSystemDeviceConfiguration) \
    X(VZVirtioGraphicsDeviceConfiguration) \
    X(VZVirtioGraphicsScanoutConfiguration) \
    X(VZVirtioNetworkDeviceConfiguration) \
    X(VZVirtioSocketDevice) \
    X(VZVirtioSocketDeviceConfiguration) \
    X(VZVirtioSocketListener) \
    X(VZVirtioTraditionalMemoryBalloonDeviceConfiguration) \
    X(VZVirtualMachine) \
    X(VZVirtualMachineConfiguration) \
    X(VZVmnetNetworkDeviceAttachment)

#define VZ_CLASS_PLUS(N) + 1
enum { VZ_CLASS_COUNT = 0 VZ_CLASS_LIST(VZ_CLASS_PLUS) };

/* 0: o hardware virtual não existe neste sistema. */
static inline int vz_policy_supported(void) { return 0; }

/* 0 e *msg = "não suportado". Não há configuração que passe. */
static inline int vz_policy_validate(const char **msg) {
    if (msg) *msg = VZ_UNSUPPORTED_MSG;
    return 0;
}

/* O completion sempre recebe a mensagem de erro. Não há partida. */
static inline void vz_policy_start(const char **msg) {
    if (msg) *msg = VZ_UNSUPPORTED_MSG;
}

static inline int vz_policy_state(void) { return VZ_STATE_STOPPED; }
static inline int vz_policy_can(void) { return 0; }

/* 0: nenhum objeto de disco ou de variáveis EFI é criado. */
static inline int vz_policy_disk(const char **msg) {
    if (msg) *msg = VZ_UNSUPPORTED_MSG;
    return 0;
}
static inline int vz_policy_efi(const char **msg) {
    if (msg) *msg = VZ_UNSUPPORTED_MSG;
    return 0;
}

/* 0: nenhum endereço MAC é fabricado. */
static inline int vz_policy_mac(void) { return 0; }
