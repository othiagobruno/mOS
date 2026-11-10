/* mOS — interface central do kernel "Nucleo" */
#pragma once
#include "types.h"

#define MOS_NAME    "mOS"
#define MOS_VERSION "0.1 \"Dunas\""

/* ---------- informações entregues pelo estágio UEFI ---------- */
typedef struct {
    u64 base, pages;
    u32 type; /* MEM_* */
} mem_region_t;

enum { MEM_FREE = 1, MEM_RESERVED, MEM_ACPI, MEM_KERNEL, MEM_MMIO, MEM_BOOTSVC };

typedef struct {
    u32 *fb;
    u32 width, height, pitch; /* pitch em pixels */
    bool bgr;
    u64 fb_size;
} framebuffer_t;

typedef struct {
    char path[96];
    u8  *data;
    u64  size;
} boot_file_t;

#define MAX_BOOT_FILES 64

typedef struct {
    framebuffer_t fb;
    mem_region_t  regions[512];
    u32           region_count;
    void         *rsdp;
    void         *runtime_services;
    boot_file_t   files[MAX_BOOT_FILES];
    u32           file_count;
    u64           kernel_base, kernel_size;
    /* caminhos de dispositivo UEFI (bytes brutos, nós até o End Node) capturados antes do ExitBootServices:
     * boot_dp = dispositivo de onde o firmware carregou a imagem do kernel; files_dp = volume que forneceu \mOS\.
     * Vazio (len 0) = desconhecido. Usados por fs/origin.c para identificar a mídia de origem sem heurística. */
    u8            boot_dp[256];
    u16           boot_dp_len;
    u8            files_dp[256];
    u16           files_dp_len;
} boot_info_t;

extern boot_info_t *g_boot;

/* ---------- lib ---------- */
void  *memcpy(void *d, const void *s, usize n);
void  *memmove(void *d, const void *s, usize n);
void  *memset(void *d, int c, usize n);
int    memcmp(const void *a, const void *b, usize n);
usize  strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, usize n);
char  *strcpy(char *d, const char *s);
char  *strncpy(char *d, const char *s, usize n);
char  *strcat(char *d, const char *s);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *h, const char *n);
char  *strdup(const char *s);
int    tolower(int c);
int    toupper(int c);
int    isspace(int c);
int    isdigit(int c);
int    isalpha(int c);
int    isalnum(int c);
int    strcasecmp(const char *a, const char *b);
int    strncasecmp(const char *a, const char *b, usize n);
long   strtol(const char *s, char **end, int base);
int    atoi(const char *s);
int    vsnprintf(char *buf, usize n, const char *fmt, va_list ap);
int    snprintf(char *buf, usize n, const char *fmt, ...);

void   kprintf(const char *fmt, ...);
NORETURN void panic(const char *fmt, ...);

/* math (float) */
float  fsqrt(float x);
float  fabsf_(float x);
float  fsin(float x);
float  fcos(float x);
float  fexp(float x);
float  fpow(float b, float e);
float  ffloor(float x);
float  fceil(float x);
#define PI_F 3.14159265f

u32    rand32(void);
void   srand32(u32 seed);

/* ---------- memória ---------- */
void   pmm_init(void);
u64    pmm_alloc(u64 pages);           /* endereço físico (identidade) ou 0 */
void   pmm_free(u64 addr, u64 pages);
u64    pmm_total_pages(void);
u64    pmm_free_pages(void);
bool   pmm_is_ram(u64 addr, u64 len);   /* buffer do kernel apto a DMA direto (identidade) */
void   paging_init(void);
void   swap_init(void);
bool   swap_is(u64 pte);
void   swap_hold(u64 pte);
void   swap_release(u64 pte);
u64    swap_store(u64 phys, int nx, int jwp);
void   swap_load(u64 pte, u64 page);
u64    swap_floor(void);
int    swap_busy(void);
void   swap_set_busy(int v);
void   mem_pressure(void);
void   pmm_reclaim_boot(void);
void   vmm_map_mmio(u64 phys, u64 size);
void  *kmalloc(usize n);
void  *kcalloc(usize n, usize sz);
void  *krealloc(void *p, usize n);
void   kfree(void *p);
/* dados alinhados à página e liberados página a página (pref[] do process.c): buffers de arquivo grandes, cujas páginas
 * o carregador Mach-O mapeia direto nos processos (__TEXT dos frameworks, macho.c) */
void  *kmalloc_pages(usize n);
bool   kmalloc_is_pages(const void *p);
void   heap_init(void);
usize  heap_used(void);
void  *dma_alloc(usize bytes, u64 *phys); /* memória alinhada a página, zerada */

/* ---------- arch ---------- */
void   gdt_init(void);
void   idt_init(void);
void   fpu_init(void);                    /* AVX: CR4.OSXSAVE e XCR0 antes do smp_init (os APs copiam do BSP) */
typedef struct {
    u64 r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 vector, error;
    u64 rip, cs, rflags, rsp, ss;
} int_frame_t;
typedef void (*irq_handler_t)(int_frame_t *f);
void   irq_install(int irq, irq_handler_t h);
void   msi_set_handler(void (*fn)(void)); /* vetor 0xE0, MSI/MSI-X (NVMe) */
void   pic_init(void);
void   pic_unmask(int irq);
void   pit_init(u32 hz);
u64    timer_ticks(void);       /* milissegundos desde o boot */
void   sleep_ms(u64 ms);
void   cpu_info(char *vendor, char *brand);
u64    rdtsc(void);

static inline void outb(u16 p, u8 v)  { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(p)); }
static inline void outl(u16 p, u32 v) { __asm__ volatile("outl %0,%1" :: "a"(v), "Nd"(p)); }
static inline u8   inb(u16 p)  { u8 v;  __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline u16  inw(u16 p)  { u16 v; __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline u32  inl(u16 p)  { u32 v; __asm__ volatile("inl %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline void io_wait(void) { outb(0x80, 0); }
static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
void cpu_idle(void);           /* proc/sched.c: o hlt que solta a trava grande se for o desktop no BSP */
static inline void hlt_raw(void) { __asm__ volatile("hlt"); }
static inline void hlt(void) { cpu_idle(); }
static inline u64  read_cr3(void) { u64 v; __asm__ volatile("mov %%cr3,%0" : "=r"(v)); return v; }
static inline void mmio_w32(volatile void *base, u32 off, u32 v) { *(volatile u32 *)((u8 *)base + off) = v; }
static inline u32  mmio_r32(volatile void *base, u32 off) { return *(volatile u32 *)((u8 *)base + off); }
static inline void mmio_w16(volatile void *base, u32 off, u16 v) { *(volatile u16 *)((u8 *)base + off) = v; }
static inline u16  mmio_r16(volatile void *base, u32 off) { return *(volatile u16 *)((u8 *)base + off); }
static inline void mmio_w8(volatile void *base, u32 off, u8 v) { *(volatile u8 *)((u8 *)base + off) = v; }
static inline u8   mmio_r8(volatile void *base, u32 off) { return *(volatile u8 *)((u8 *)base + off); }
static inline u64  irq_save(void) { u64 f; __asm__ volatile("pushfq; popq %0; cli" : "=r"(f) :: "memory"); return f; }
static inline void irq_restore(u64 f) { if (f & 0x200) sti(); }

/* trava de giro (SMP): também desliga as interrupções do núcleo que a segura */
typedef struct { volatile u32 v; } spinlock_t;
static inline u64 spin_lock_irqsave(spinlock_t *l) {
    u64 f = irq_save();
    while (__atomic_exchange_n(&l->v, 1, __ATOMIC_ACQUIRE))
        while (__atomic_load_n(&l->v, __ATOMIC_RELAXED)) __asm__ volatile("pause");
    return f;
}
static inline void spin_unlock_irqrestore(spinlock_t *l, u64 f) { __atomic_store_n(&l->v, 0, __ATOMIC_RELEASE); irq_restore(f); }

/* ---------- SMP: núcleos, trava grande do kernel e TLB (arch/cpu.c, arch/smp.c, proc/sched.c)
 * Cada núcleo tem o próprio TSS na GDT (seletor 0x28 + 16*cpu), então o `str` diz em que núcleo estamos.
 * Trava grande (BKL): todo código do kernel roda com ela, exceto os tratadores de IRQ do PIC (PIT e PS/2, só
 * no BSP), os trabalhos de par_for e os laços ociosos. O ring 3 roda sem ela em qualquer núcleo; quem entra
 * no kernel vindo do usuário (syscall, falta, temporizador) a pega. O desktop a segura o tempo todo e só a
 * solta em cpu_idle() (todo hlt() da thread 0 com interrupções ligadas) — o mesmo ponto em que o modelo
 * antigo deixava threads de usuário rodarem; fora dali, syscalls continuam "com o desktop parado". */
#define MAX_CPUS 64
static inline int this_cpu(void) { u16 s; __asm__ volatile("str %0" : "=r"(s)); return s >= 0x28 ? (s - 0x28) >> 4 : 0; }
void   cpu_tss_init(int cpu, u64 ist_top);      /* AP: descritor do TSS próprio + ltr */
bool   bkl_held(void);                          /* este núcleo segura a trava grande */
void   smp_tlb_shootdown(u64 cr3);              /* invalida o TLB dos OUTROS núcleos com esse CR3 (com a BKL) */
void   smp_send_ipi(int cpu, u8 vec);
u8     smp_apic_id(int cpu);
u64    smp_lapic_base(void);                   /* base física do MADT; 0 se o SMP não achou LAPIC */
int    smp_x2apic(void);

/* ---------- trabalho paralelo nos núcleos extras (arch/smp.c)
 * par_for divide [0, n) em pedaços de `grain` itens e os distribui entre o BSP e os APs; volta só
 * quando tudo terminou. `cpu` (0 = BSP, 1..par_cpus()-1) indexa buffers de rascunho por núcleo.
 * As funções de trabalho não podem alocar memória, imprimir nem chamar par_for de novo. */
typedef void (*par_fn_t)(void *ctx, int begin, int end, int cpu);
void   par_for(int n, int grain, par_fn_t fn, void *ctx);
int    par_cpus(void);
void   par_set_enabled(bool on);       /* false: tudo serial no BSP (para medir o ganho) */
u64    hpet_ns(void);                  /* relógio de alta resolução (0 sem HPET) */

/* ---------- dispositivos ---------- */
void   serial_init(void);
void   serial_putc(char c);
void   serial_write(const char *s);

typedef struct {
    u8  bus, dev, fn;
    u16 vendor, device;
    u8  class_, subclass, progif, irq;
    u32 bar[6];
} pci_dev_t;
void       pci_init(void);
int        pci_count(void);
pci_dev_t *pci_get(int i);
pci_dev_t *pci_find_class(u8 cls, u8 sub, int nth);
pci_dev_t *pci_find(u16 vendor, u16 device);
u32        pci_read(pci_dev_t *d, u8 off);
void       pci_write(pci_dev_t *d, u8 off, u32 v);
void       pci_enable_bus_master(pci_dev_t *d);
u64        pci_bar_addr(pci_dev_t *d, int bar);
const char*pci_class_name(u8 cls, u8 sub);

void   ps2_init(void);
void   acpi_init(void);
void   acpi_poweroff(void);
void   acpi_reboot(void);

typedef struct { int year, month, day, hour, minute, second, weekday; } datetime_t;
void   rtc_now(datetime_t *dt);
void   rtc_now_utc(datetime_t *dt);   /* UTC (certificados, OCSP) */

/* fila de eventos de entrada global */
enum { EV_NONE, EV_KEY_DOWN, EV_KEY_UP, EV_MOUSE, EV_MOUSE_ABS }; /* EV_MOUSE_ABS: posição absoluta de tela em dx/dy */
enum { MOD_SHIFT = 1, MOD_CTRL = 2, MOD_ALT = 4, MOD_CMD = 8, MOD_CAPS = 16 };
enum {
    KEY_ESC = 0x1b, KEY_BACKSPACE = 8, KEY_TAB = 9, KEY_ENTER = 10,
    KEY_UP = 0x100, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_HOME, KEY_END,
    KEY_PGUP, KEY_PGDN, KEY_DELETE, KEY_INSERT,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_LSHIFT, KEY_RSHIFT, KEY_LCTRL, KEY_LALT, KEY_LCMD, KEY_CAPS,
    KEY_RCTRL, KEY_RALT, KEY_RCMD
};
typedef struct {
    u8  type;
    u8  buttons;   /* bit0 esq, bit1 dir, bit2 meio */
    u16 mods;
    i32 key;       /* código / caractere ASCII */
    i32 dx, dy, dz;
} input_event_t;
void   input_push(input_event_t *e);
bool   input_pop(input_event_t *e);
u16    input_mods(void);
void   input_kbd_scancode(u16 code, bool up); /* scancode set 1; bit 0x100 = prefixo E0 */

/* áudio */
bool   audio_init(void);
bool   audio_available(void);
void   audio_play(const i16 *stereo_samples, u32 frames); /* 48 kHz estéreo */
void   audio_set_volume(int pct);
int    audio_get_volume(void);
extern void (*audio_tap)(const i16 *stereo, u32 frames);  /* derivação do mixer (fones Bluetooth); NULL = desligada */
extern bool audio_route_bluetooth;                         /* silencia a saída interna enquanto o fone toca */
const char *audio_device_name(void);
void   audio_poll(void);
bool   audio_input_available(void);
const char *audio_input_name(void);
void   audio_set_input_volume(int pct);                 /* 100 = 0 dB sobre o ganho do hardware */
int    audio_get_input_volume(void);
void   audio_set_input_monitor(bool on);                /* mistura a entrada na saída */
bool   audio_get_input_monitor(void);
u32    audio_capture_read(i16 *stereo_samples, u32 frames); /* 48 kHz estéreo; quadros copiados */
void   sound_startup_chime(void);
void   sound_click(void);
void   sound_alert(void);

/* rede */
void   net_init(void);
void   net_poll(void);
bool   net_link_up(void);
bool   net_configured(void);
const char *net_device_name(void);
void   net_status_string(char *buf, usize n);

/* bluetooth (USB HCI) */
void   bt_init(void);
const char *bt_status(void);

/* armazenamento SATA (AHCI) */
void   ahci_init(void);
void   ahci_smp_ready(void);
int    ahci_disk_count(void);
const char *ahci_disk_desc(int i);
const char *ahci_status(void);
void   storage_mount_all(void);   /* monta volumes HFS+/APFS dos discos */

/* usb */
void   usb_init(void);
void   usb_poll(void);
const char *usb_status(void);

/* sistema de arquivos em memória */
void   vfs_init(void);
void   dfs_mount_all(void);

/* interface */
void   desktop_run(void) NORETURN;
