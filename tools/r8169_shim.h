/* mOS — shim para compilar kernel/dev/r8169.c no host (tools/r8169_host.c).
 * Os acessos MMIO viram funções do teste, que simula os efeitos colaterais do chip. */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef unsigned long long u64;   /* como em kernel/include/types.h: %llx sem aviso */
typedef size_t   usize;
typedef uintptr_t uptr;

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    u8  bus, dev, fn;
    u16 vendor, device;
    u8  class_, subclass, progif, irq;
    u32 bar[6];
} pci_dev_t;

typedef struct {
    const char *name;
    bool (*send)(const void *frame, u16 len);
    int  (*recv)(void *frame, u16 max);
    bool (*link_up)(void);
    u8   mac[6];
} netdev_t;

int        pci_count(void);
pci_dev_t *pci_get(int i);
u32        pci_read(pci_dev_t *d, u8 off);
void       pci_write(pci_dev_t *d, u8 off, u32 v);
void       pci_enable_bus_master(pci_dev_t *d);
u64        pci_bar_addr(pci_dev_t *d, int bar);
void       vmm_map_mmio(u64 phys, u64 size);
void      *dma_alloc(usize bytes, u64 *phys);
void       sleep_ms(u64 ms);
void       kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
u32        rand32(void);
void       net_register_device(netdev_t *d);

u8   mmio_r8(volatile void *base, u32 off);
u16  mmio_r16(volatile void *base, u32 off);
u32  mmio_r32(volatile void *base, u32 off);
void mmio_w8(volatile void *base, u32 off, u8 v);
void mmio_w16(volatile void *base, u32 off, u16 v);
void mmio_w32(volatile void *base, u32 off, u32 v);

bool r8169_probe(void);
