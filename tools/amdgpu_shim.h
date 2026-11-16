/* mOS — shim de host para kernel/dev/amdgpu/amdgpu.c (tools/amdgpu_host.c simula o hardware). */
#pragma once
typedef struct {
    u8  bus, dev, fn;
    u16 vendor, device;
    u8  class_, subclass, progif, irq;
    u32 bar[6];
} pci_dev_t;
u32  pci_read(pci_dev_t *d, u8 off);
u64  pci_bar_addr(pci_dev_t *d, int bar);
u64  timer_ticks(void);
void sleep_ms(u64 ms);
void kprintf(const char *fmt, ...);
u32  amd_host_rreg(u32 off);
void amd_host_wreg(u32 off, u32 v);
void *amd_host_phys(u64 phys);          /* endereço físico da VRAM -> memória do teste */
void vmm_map_wc(u64 phys, u64 size);
#define MOS_ENOENT 2
#define MOS_EINVAL 22
#define MOS_ETIMEDOUT 60
#define MOS_EIO 5
#define MOS_EAGAIN 35
#define MOS_ENXIO 6
#define MOS_EBUSY 16
#define MOS_ENOMEM 12
#define MOS_ERANGE 34
const u8 *amd_host_fw(const char *name, u64 *len);
u64  amd_host_ram_page(void);
void amd_host_idle(void);               /* o "hlt" do teste: o chip simulado anda um pouco */
u64  amd_host_ram_alloc(u64 bytes);
void amd_host_doorbell(u32 dw_index, u64 v);  /* escrita de 64 bits num doorbell do BAR2 */      /* RAM alinhada a 4 KiB; o endereço vale como físico */
