/*
 * mOS — estágio de boot UEFI.
 * O kernel inteiro é um executável PE/UEFI (EFI/BOOT/BOOTX64.EFI). Este arquivo roda
 * enquanto o firmware ainda está ativo: escolhe o modo de vídeo, encontra a ACPI,
 * copia os arquivos de \mOS\ do pendrive para a RAM e então chama ExitBootServices.
 * A partir daí o firmware some e o kernel assume a máquina sozinho.
 */
#include "efi.h"
#include "kernel.h"

static EFI_SYSTEM_TABLE *ST;
static EFI_BOOT_SERVICES *BS;
static boot_info_t s_boot;
boot_info_t *g_boot = &s_boot;

static EFI_GUID GOP_GUID   = {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
static EFI_GUID ACPI2_GUID = {0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}};
static EFI_GUID ACPI1_GUID = {0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};
static EFI_GUID LIP_GUID   = {0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b}};
static EFI_GUID SFS_GUID   = {0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
static EFI_GUID FINFO_GUID = {0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};

extern void kmain(void);
extern void boot_switch_stack(u64 stack_top, void (*fn)(void));

static void puts16(const char *s) {
    CHAR16 buf[160];
    int i = 0;
    while (*s && i < 158) {
        if (*s == '\n') buf[i++] = '\r';
        buf[i++] = (CHAR16)*s++;
    }
    buf[i] = 0;
    ST->ConOut->OutputString(ST->ConOut, buf);
}

static bool guid_eq(EFI_GUID *a, EFI_GUID *b) { return memcmp(a, b, sizeof(EFI_GUID)) == 0; }

static void setup_video(void) {
    EFI_GOP *gop = NULL;
    if (EFI_ERROR(BS->LocateProtocol(&GOP_GUID, NULL, (void **)&gop)) || !gop) {
        puts16("mOS: sem Graphics Output Protocol\n");
        return;
    }
    /* Preferimos resoluções de notebook em 16:10, como os Macs; senão a maior até 1920x1200. */
    static const u32 pref[][2] = {{1440, 900}, {1680, 1050}, {1280, 800}, {1920, 1080}, {1920, 1200}, {1366, 768}, {1280, 1024}, {1024, 768}};
    i32 best = -1, best_rank = 1 << 30;
    u64 best_area = 0;
    for (u32 m = 0; m < gop->Mode->MaxMode; m++) {
        EFI_GOP_MODE_INFO *info;
        u64 sz;
        if (EFI_ERROR(gop->QueryMode(gop, m, &sz, &info))) continue;
        if (info->PixelFormat > 1) continue; /* só RGB/BGR 32 bits diretos */
        u32 w = info->HorizontalResolution, h = info->VerticalResolution;
        i32 rank = 100;
        for (u32 i = 0; i < ARRAY_LEN(pref); i++)
            if (pref[i][0] == w && pref[i][1] == h) { rank = i; break; }
        u64 area = (u64)w * h;
        if (rank < best_rank || (rank == 100 && best_rank == 100 && area > best_area && w <= 1920)) {
            best = m; best_rank = rank; best_area = area;
        }
    }
    if (best >= 0 && (u32)best != gop->Mode->Mode) gop->SetMode(gop, best);
    EFI_GOP_MODE_INFO *i = gop->Mode->Info;
    s_boot.fb.fb      = (u32 *)gop->Mode->FrameBufferBase;
    s_boot.fb.width   = i->HorizontalResolution;
    s_boot.fb.height  = i->VerticalResolution;
    s_boot.fb.pitch   = i->PixelsPerScanLine;
    s_boot.fb.bgr     = i->PixelFormat == 1;
    s_boot.fb.fb_size = gop->Mode->FrameBufferSize;
}

static void find_acpi(void) {
    for (u64 i = 0; i < ST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *t = &ST->ConfigurationTable[i];
        if (guid_eq(&t->VendorGuid, &ACPI2_GUID)) { s_boot.rsdp = t->VendorTable; return; }
        if (guid_eq(&t->VendorGuid, &ACPI1_GUID)) s_boot.rsdp = t->VendorTable;
    }
}

/* ---- carrega recursivamente \mOS\ do volume de boot para a memória ---- */
static void load_dir(EFI_FILE *dir, const char *prefix, int depth) {
    if (depth > 6) return;
    u8 buf[sizeof(EFI_FILE_INFO) + 64] ALIGNED(8);
    for (;;) {
        u64 sz = sizeof(buf);
        if (EFI_ERROR(dir->Read(dir, &sz, buf)) || sz == 0) break;
        EFI_FILE_INFO *fi = (EFI_FILE_INFO *)buf;
        char name[96];
        int n = 0;
        for (int k = 0; fi->FileName[k] && n < 60; k++) name[n++] = (char)fi->FileName[k];
        name[n] = 0;
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        char path[96];
        snprintf(path, sizeof(path), "%s/%s", prefix, name);
        EFI_FILE *f;
        if (EFI_ERROR(dir->Open(dir, &f, fi->FileName, 1, 0))) continue;
        if (fi->Attribute & EFI_FILE_DIRECTORY) {
            if (s_boot.file_count < MAX_BOOT_FILES) { /* registra o diretório (size = ~0) */
                boot_file_t *bf = &s_boot.files[s_boot.file_count++];
                strncpy(bf->path, path, sizeof(bf->path) - 1);
                bf->data = NULL; bf->size = ~0ULL;
            }
            load_dir(f, path, depth + 1);
        } else if (s_boot.file_count < MAX_BOOT_FILES && fi->FileSize < 512ULL * 1024 * 1024) {
            u64 pages = (fi->FileSize + 4095) / 4096 + 1, addr = 0;
            if (!EFI_ERROR(BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, &addr))) {
                u64 rd = fi->FileSize;
                f->Read(f, &rd, (void *)addr);
                boot_file_t *bf = &s_boot.files[s_boot.file_count++];
                strncpy(bf->path, path, sizeof(bf->path) - 1);
                bf->data = (u8 *)addr; bf->size = rd;
            }
        }
        f->Close(f);
    }
}

static bool try_volume(EFI_SFS *sfs) {
    EFI_FILE *root, *dir;
    if (EFI_ERROR(sfs->OpenVolume(sfs, &root))) return false;
    EFI_STATUS st = root->Open(root, &dir, (CHAR16 *)L"\\mOS", 1, 0);
    bool ok = !EFI_ERROR(st);
    if (ok) {
        load_dir(dir, "", 0);
        dir->Close(dir);
    }
    root->Close(root);
    return ok;
}

static void load_files(EFI_HANDLE image) {
    EFI_LOADED_IMAGE *li = NULL;
    EFI_SFS *sfs;
    bool ok = false;
    /* 1) volume de onde o kernel foi carregado */
    if (!EFI_ERROR(BS->HandleProtocol(image, &LIP_GUID, (void **)&li)) && li) {
        s_boot.kernel_base = (u64)li->ImageBase;
        s_boot.kernel_size = li->ImageSize;
        if (!EFI_ERROR(BS->HandleProtocol(li->DeviceHandle, &SFS_GUID, (void **)&sfs))) ok = try_volume(sfs);
    }
    /* 2) qualquer volume FAT que contenha \mOS */
    if (!ok) {
        u64 n = 0;
        EFI_HANDLE *hs = NULL;
        if (!EFI_ERROR(BS->LocateHandleBuffer(2 /* ByProtocol */, &SFS_GUID, NULL, &n, &hs))) {
            for (u64 i = 0; i < n && !ok; i++)
                if (!EFI_ERROR(BS->HandleProtocol(hs[i], &SFS_GUID, (void **)&sfs))) ok = try_volume(sfs);
            BS->FreePool(hs);
        }
    }
    if (!ok && !EFI_ERROR(BS->LocateProtocol(&SFS_GUID, NULL, (void **)&sfs))) ok = try_volume(sfs);
    char msg[80];
    snprintf(msg, sizeof msg, "mOS: %s, %u entradas carregadas de \\mOS\n", ok ? "volume encontrado" : "volume NAO encontrado", s_boot.file_count);
    puts16(msg);
    (void)FINFO_GUID;
}

static u32 map_type(u32 t) {
    switch (t) {
    case EfiConventionalMemory: return MEM_FREE;
    case EfiBootServicesCode: case EfiBootServicesData: return MEM_BOOTSVC;
    case EfiLoaderCode: case EfiLoaderData: return MEM_KERNEL;
    case EfiACPIReclaimMemory: case EfiACPIMemoryNVS: return MEM_ACPI;
    case EfiMemoryMappedIO: case EfiMemoryMappedIOPortSpace: return MEM_MMIO;
    default: return MEM_RESERVED;
    }
}

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    ST = st;
    BS = st->BootServices;
    BS->SetWatchdogTimer(0, 0, 0, NULL);
    ST->ConOut->ClearScreen(ST->ConOut);
    puts16("mOS: iniciando o Nucleo...\n");

    setup_video();
    find_acpi();
    load_files(image);
    s_boot.runtime_services = st->RuntimeServices;

    /* pilha do kernel: 1 MiB em memória de loader (não será reutilizada) */
    u64 stack = 0;
    BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 256, &stack);

    u64 msize = 0, key = 0, dsize = 0;
    u32 dver = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    for (int tries = 0; tries < 8; tries++) {
        msize = 0;
        BS->GetMemoryMap(&msize, NULL, &key, &dsize, &dver);
        msize += 8 * dsize;
        if (map) BS->FreePool(map);
        if (EFI_ERROR(BS->AllocatePool(EfiLoaderData, msize, (void **)&map))) continue;
        if (EFI_ERROR(BS->GetMemoryMap(&msize, map, &key, &dsize, &dver))) continue;
        /* nada de chamadas ao firmware entre GetMemoryMap e ExitBootServices */
        if (!EFI_ERROR(BS->ExitBootServices(image, key))) goto exited;
    }
    puts16("mOS: falha em ExitBootServices\n");
    for (;;) hlt();

exited:
    for (u64 off = 0; off < msize && s_boot.region_count < ARRAY_LEN(s_boot.regions); off += dsize) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR *)((u8 *)map + off);
        u32 t = map_type(d->Type);
        mem_region_t *last = s_boot.region_count ? &s_boot.regions[s_boot.region_count - 1] : NULL;
        if (last && last->type == t && last->base + last->pages * 4096 == d->PhysicalStart) {
            last->pages += d->NumberOfPages;
            continue;
        }
        mem_region_t *r = &s_boot.regions[s_boot.region_count++];
        r->base = d->PhysicalStart; r->pages = d->NumberOfPages; r->type = t;
    }
    boot_switch_stack(stack + 256 * 4096 - 64, kmain);
    for (;;) hlt();
}
