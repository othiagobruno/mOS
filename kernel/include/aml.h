/* mOS — interpretador AML (ACPI 6.5, capítulos 19 e 20)
 *
 * Carrega DSDT/SSDT num namespace, executa métodos e acessa OperationRegions (SystemMemory, SystemIO,
 * PCI_Config, EmbeddedControl, SystemCMOS) pelas funções aml_os_* que o kernel (dev/acpi.c) ou o
 * harness de host (tools/aml_host.c) implementam. O interpretador é de um executor por vez: quem chama
 * serializa (acpi.c usa uma trava). Sem dependência do resto do kernel além de memcpy/memset/memcmp.
 */
#pragma once
#ifdef AML_HOST
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t i8; typedef int16_t i16; typedef int32_t i32; typedef int64_t i64;
#else
#include "types.h"
#endif

typedef struct aml_node aml_node_t;
typedef struct aml_blob aml_blob_t;

/* tipos visíveis pelo ObjectType (ACPI 19.6.97) e internos (>= 20) */
enum {
    AML_UNINIT = 0, AML_INT = 1, AML_STR = 2, AML_BUF = 3, AML_PKG = 4, AML_FIELD = 5, AML_DEVICE = 6,
    AML_EVENT = 7, AML_METHOD = 8, AML_MUTEX = 9, AML_REGION = 10, AML_POWERRES = 11, AML_PROCESSOR = 12,
    AML_THERMAL = 13, AML_BUFFIELD = 14, AML_DDB = 15, AML_DEBUG = 16,
    AML_SCOPE = 20, AML_REF = 21, AML_ALIAS = 22
};
/* tipos de referência (AML_REF) */
enum { AML_REF_NODE, AML_REF_PKG, AML_REF_BUF, AML_REF_STR, AML_REF_SLOT };

/* espaços de endereço de OperationRegion */
enum { AML_AS_MEM = 0, AML_AS_IO = 1, AML_AS_PCI = 2, AML_AS_EC = 3, AML_AS_SMBUS = 4, AML_AS_CMOS = 5,
       AML_AS_PCIBAR = 6, AML_AS_IPMI = 7, AML_AS_GPIO = 8, AML_AS_GSBUS = 9, AML_AS_PCC = 10 };

typedef struct aml_val {
    u8  type;            /* AML_UNINIT, AML_INT, AML_STR, AML_BUF, AML_PKG ou AML_REF */
    u8  rkind;           /* AML_REF_* quando type == AML_REF */
    u32 idx;             /* índice das referências de Index() */
    union { u64 i; aml_blob_t *b; aml_node_t *node; struct aml_val *slot; };
} aml_val_t;

/* corpo de String/Buffer/Package com contagem de referências */
struct aml_blob {
    u32 refs;
    u32 len;             /* bytes (String sem o NUL) ou elementos (Package) */
    aml_val_t *el;       /* Package */
    u8 *d;               /* String/Buffer */
};

typedef int (*aml_native_fn)(aml_val_t *args, int nargs, aml_val_t *ret);

struct aml_node {
    char name[4];
    u8   type;
    u8   dynamic;        /* criado durante um método: some no retorno */
    aml_node_t *parent, *child, *next;
    aml_val_t v;         /* objetos de dados (Name) */
    union {
        struct { const u8 *code; u32 len; u8 flags; aml_native_fn native; } m;
        struct { u8 space; u8 pci_ok; u16 seg; u8 bus, dev, fn; u64 base, len; } r;
        struct { u8 kind, flags, attr; aml_node_t *reg, *idx, *dat, *bank; u64 bankval; u32 bitoff, bitlen; } f;
        struct { aml_blob_t *buf; u32 bitoff, bitlen; } bf;
        struct { u8 sync; u32 depth; } mx;
        struct { u32 count; } ev;
        struct { u8 id; u8 plen; u32 pblk; } cpu;
        struct { u8 level; u16 order; } pr;
        aml_node_t *alias;
    } u;
};

/* ---- serviços que o ambiente fornece ---- */
void *aml_os_alloc(u32 n);                       /* memória zerada */
void  aml_os_free(void *p);
void  aml_os_log(const char *fmt, ...);
bool  aml_os_mem(u64 addr, u32 bits, u64 *v, bool write);
bool  aml_os_io(u16 port, u32 bits, u64 *v, bool write);
bool  aml_os_pci(u16 seg, u8 bus, u8 dev, u8 fn, u16 off, u32 bits, u64 *v, bool write);
bool  aml_os_ec(u8 addr, u8 *v, bool write);
bool  aml_os_cmos(u8 addr, u8 *v, bool write);
bool  aml_os_region_other(u8 space, u64 addr, u32 bits, u64 *v, bool write);
void  aml_os_sleep_ms(u64 ms);
void  aml_os_stall_us(u64 us);
u64   aml_os_timer_100ns(void);
void  aml_os_notify(aml_node_t *n, u64 value);
bool  aml_os_global_lock(bool acquire);          /* trava global da FACS */
const void *aml_os_find_table(const char *sig, const char *oem, const char *oemtab, int nth);

/* ---- API ---- */
#define AML_OK          0
#define AML_ENOTFOUND  (-1)
#define AML_EINVAL     (-2)
#define AML_ENOMEM     (-3)
#define AML_EFAULT     (-4)
#define AML_EDEPTH     (-5)
#define AML_ETYPE      (-6)

void        aml_init(void);                                  /* namespace com \_SB, \_GPE, \_OSI... */
int         aml_load_table(const void *sdt);                 /* tabela com cabeçalho SDT (DSDT/SSDT) */
aml_node_t *aml_root(void);
aml_node_t *aml_find(aml_node_t *scope, const char *path);   /* "\\_SB.PCI0" absoluto, "_STA" relativo exato */
int         aml_eval(aml_node_t *n, const aml_val_t *args, int nargs, aml_val_t *ret);
int         aml_eval_path(aml_node_t *scope, const char *path, const aml_val_t *args, int nargs, aml_val_t *ret);
int         aml_eval_int(aml_node_t *scope, const char *path, u64 *out);
int         aml_call_int1(aml_node_t *scope, const char *path, u64 arg, u64 *out);
void        aml_val_free(aml_val_t *v);
aml_val_t   aml_int(u64 v);
int         aml_val_to_int(const aml_val_t *v, u64 *out);
void        aml_path(aml_node_t *n, char *buf, u32 size);
u32         aml_node_count(void);
u32         aml_int_bits(void);
void        aml_walk(aml_node_t *start, int (*cb)(aml_node_t *n, int depth, void *ctx), void *ctx);
int         aml_init_devices(void);                          /* \_SB._INI, _STA e _INI de cada dispositivo */
void        aml_run_reg(u8 space);                           /* _REG(space, 1) nos dispositivos com _REG */
bool        aml_device_match(aml_node_t *dev, const char *hid); /* _HID/_CID ("PNP0C0C", "ACPI0007") */
int         aml_device_sta(aml_node_t *dev, u64 *sta);
const char *aml_strerror(int e);
u32         aml_errors(void);                                /* erros de execução desde o boot */

/* ---- descritores de recurso (_CRS, ACPI 6.4.2/6.4.3) ---- */
enum { AML_RES_IRQ = 1, AML_RES_IO, AML_RES_MEM, AML_RES_DMA, AML_RES_GPIO, AML_RES_ADDR, AML_RES_OTHER };
typedef struct {
    u8  kind;
    u8  type_spec;           /* ADDR: 0 memória, 1 E/S, 2 número de barramento */
    u8  flags;               /* IRQ: bit0 borda, bit3 ativo baixo, bit4 compartilhado */
    u8  count;               /* nº de IRQs/pinos em irq[] */
    u64 min, max, len;       /* IO/MEM/ADDR */
    u32 irq[16];             /* IRQ/GPIO */
} aml_res_t;
int aml_res_walk(const u8 *buf, u32 len, int (*cb)(const aml_res_t *r, void *ctx), void *ctx);
