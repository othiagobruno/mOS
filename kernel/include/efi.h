/* mOS — definições mínimas da especificação UEFI 2.x usadas pelo estágio de boot */
#pragma once
#include "types.h"

typedef u64   EFI_STATUS;
typedef void *EFI_HANDLE;
typedef u16   CHAR16;
#define EFIAPI __attribute__((ms_abi))
#define EFI_SUCCESS            0
#define EFI_BUFFER_TOO_SMALL   (0x8000000000000005ULL)
#define EFI_ERROR(s)           ((i64)(s) < 0)

typedef struct { u32 a; u16 b, c; u8 d[8]; } EFI_GUID;

typedef struct { u64 Signature; u32 Revision, HeaderSize, CRC32, Reserved; } EFI_TABLE_HEADER;

typedef struct EFI_SIMPLE_TEXT_OUTPUT {
    EFI_STATUS (EFIAPI *Reset)(struct EFI_SIMPLE_TEXT_OUTPUT *, bool);
    EFI_STATUS (EFIAPI *OutputString)(struct EFI_SIMPLE_TEXT_OUTPUT *, CHAR16 *);
    void *TestString, *QueryMode, *SetMode, *SetAttribute;
    EFI_STATUS (EFIAPI *ClearScreen)(struct EFI_SIMPLE_TEXT_OUTPUT *);
} EFI_SIMPLE_TEXT_OUTPUT;

typedef struct {
    u32 Type, Pad;
    u64 PhysicalStart, VirtualStart, NumberOfPages, Attribute;
} EFI_MEMORY_DESCRIPTOR;

enum {
    EfiReservedMemoryType, EfiLoaderCode, EfiLoaderData, EfiBootServicesCode,
    EfiBootServicesData, EfiRuntimeServicesCode, EfiRuntimeServicesData,
    EfiConventionalMemory, EfiUnusableMemory, EfiACPIReclaimMemory,
    EfiACPIMemoryNVS, EfiMemoryMappedIO, EfiMemoryMappedIOPortSpace,
    EfiPalCode, EfiPersistentMemory
};

enum { AllocateAnyPages, AllocateMaxAddress, AllocateAddress };

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *RaiseTPL, *RestoreTPL;
    EFI_STATUS (EFIAPI *AllocatePages)(u32 type, u32 memtype, u64 pages, u64 *addr);
    EFI_STATUS (EFIAPI *FreePages)(u64 addr, u64 pages);
    EFI_STATUS (EFIAPI *GetMemoryMap)(u64 *size, EFI_MEMORY_DESCRIPTOR *map, u64 *key, u64 *descsize, u32 *descver);
    EFI_STATUS (EFIAPI *AllocatePool)(u32 type, u64 size, void **buf);
    EFI_STATUS (EFIAPI *FreePool)(void *buf);
    void *CreateEvent, *SetTimer, *WaitForEvent, *SignalEvent, *CloseEvent, *CheckEvent;
    void *InstallProtocolInterface, *ReinstallProtocolInterface, *UninstallProtocolInterface;
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE, EFI_GUID *, void **);
    void *Reserved, *RegisterProtocolNotify, *LocateHandle, *LocateDevicePath, *InstallConfigurationTable;
    void *LoadImage, *StartImage, *Exit, *UnloadImage;
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, u64 key);
    void *GetNextMonotonicCount;
    EFI_STATUS (EFIAPI *Stall)(u64 us);
    EFI_STATUS (EFIAPI *SetWatchdogTimer)(u64 timeout, u64 code, u64 size, CHAR16 *data);
    void *ConnectController, *DisconnectController, *OpenProtocol, *CloseProtocol, *OpenProtocolInformation;
    void *ProtocolsPerHandle;
    EFI_STATUS (EFIAPI *LocateHandleBuffer)(u32 type, EFI_GUID *proto, void *key, u64 *count, EFI_HANDLE **buf);
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID *, void *reg, void **iface);
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *GetTime, *SetTime, *GetWakeupTime, *SetWakeupTime, *SetVirtualAddressMap, *ConvertPointer;
    void *GetVariable, *GetNextVariableName, *SetVariable, *GetNextHighMonotonicCount;
    void (EFIAPI *ResetSystem)(u32 type, EFI_STATUS status, u64 size, void *data);
} EFI_RUNTIME_SERVICES;

typedef struct { EFI_GUID VendorGuid; void *VendorTable; } EFI_CONFIGURATION_TABLE;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    u32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT *StdErr;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    u64 NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;

/* Graphics Output Protocol */
typedef struct {
    u32 Version, HorizontalResolution, VerticalResolution, PixelFormat;
    u32 RedMask, GreenMask, BlueMask, ReservedMask;
    u32 PixelsPerScanLine;
} EFI_GOP_MODE_INFO;

typedef struct {
    u32 MaxMode, Mode;
    EFI_GOP_MODE_INFO *Info;
    u64 SizeOfInfo;
    u64 FrameBufferBase, FrameBufferSize;
} EFI_GOP_MODE;

typedef struct EFI_GOP {
    EFI_STATUS (EFIAPI *QueryMode)(struct EFI_GOP *, u32 mode, u64 *size, EFI_GOP_MODE_INFO **info);
    EFI_STATUS (EFIAPI *SetMode)(struct EFI_GOP *, u32 mode);
    void *Blt;
    EFI_GOP_MODE *Mode;
} EFI_GOP;

/* Loaded Image + Simple File System (para ler arquivos do pendrive antes de sair do firmware) */
typedef struct {
    u32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    void *FilePath, *Reserved;
    u32 LoadOptionsSize;
    void *LoadOptions;
    void *ImageBase;
    u64 ImageSize;
} EFI_LOADED_IMAGE;

typedef struct EFI_FILE {
    u64 Revision;
    EFI_STATUS (EFIAPI *Open)(struct EFI_FILE *, struct EFI_FILE **, CHAR16 *, u64 mode, u64 attr);
    EFI_STATUS (EFIAPI *Close)(struct EFI_FILE *);
    void *Delete;
    EFI_STATUS (EFIAPI *Read)(struct EFI_FILE *, u64 *size, void *buf);
    EFI_STATUS (EFIAPI *Write)(struct EFI_FILE *, u64 *size, void *buf);
    void *GetPosition, *SetPosition;
    EFI_STATUS (EFIAPI *GetInfo)(struct EFI_FILE *, EFI_GUID *, u64 *size, void *buf);
} EFI_FILE;

typedef struct EFI_SFS {
    u64 Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(struct EFI_SFS *, EFI_FILE **);
} EFI_SFS;

typedef struct {
    u64 Size, FileSize, PhysicalSize;
    u8 CreateTime[16], LastAccessTime[16], ModificationTime[16];
    u64 Attribute;
    CHAR16 FileName[256];
} EFI_FILE_INFO;

#define EFI_FILE_DIRECTORY 0x10
