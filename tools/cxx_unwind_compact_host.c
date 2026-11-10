/* Host: __unwind_info x86_64. Frameless imediato, indireto, seis registradores
 * na página comprimida, quadro rbp e fallback DWARF (FDE real do .eh_frame).
 * O backtrace vê o callee-saved já restaurado, no quadro do caller. */
#define _GNU_SOURCE
#include <elf.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MODE_RBP = 0x01000000,
    MODE_IMMD = 0x02000000,
    MODE_IND = 0x03000000,
    MODE_DWARF = 0x04000000,
    CU_RBX = 1,
    CU_R15 = 5
};

extern void mos_cu_leaf(void);
extern char mos_cu_immd[], mos_cu_immd_end[];
extern char mos_cu_ind[], mos_cu_ind_end[];
extern char mos_cu_rbp[], mos_cu_rbp_end[];
extern char mos_cu_six[], mos_cu_six_end[];
extern char mos_cu_dwarf[], mos_cu_dwarf_end[];

extern unsigned long long _Unwind_GetIP(void *);
extern unsigned long long _Unwind_GetGR(void *, int);
extern int _Unwind_Backtrace(int (*)(void *, void *), void *);

struct span {
    uintptr_t lo, hi;
    int id;
};
static struct span spans[6];
static int nspans;
static uintptr_t image_base;
static unsigned char section[4096];
static const unsigned char *eh_start;
static unsigned long long exp_rbx, exp_rbp, exp_r12, exp_r13, exp_r14, exp_r15;
static int pending, saw[5];

static void add_span(char *lo, char *hi, int id) {
    spans[nspans].lo = (uintptr_t)lo;
    spans[nspans].hi = (uintptr_t)hi;
    spans[nspans].id = id;
    nspans++;
}

static int span_id(uintptr_t pc) {
    for (int i = 0; i < nspans; i++)
        if (pc >= spans[i].lo && pc < spans[i].hi) return spans[i].id;
    return 0;
}

static uintptr_t enc_read(unsigned char enc, const unsigned char **p, uintptr_t datarel) {
    const unsigned char *at = *p;
    uintptr_t v = 0;
    switch (enc & 0x0f) {
    case 0x01: {
        unsigned shift = 0;
        unsigned char b;
        do {
            b = *(*p)++;
            v |= (uintptr_t)(b & 0x7f) << shift;
            shift += 7;
        } while (b & 0x80);
        break;
    }
    case 0x03: {
        uint32_t u;
        memcpy(&u, *p, 4);
        *p += 4;
        v = u;
        break;
    }
    case 0x0b: {
        int32_t s;
        memcpy(&s, *p, 4);
        *p += 4;
        v = (uintptr_t)(intptr_t)s;
        break;
    }
    default:
        fprintf(stderr, "compact: encoding %u do eh_frame_hdr\n", enc);
        abort();
    }
    if ((enc & 0x70) == 0x10) v += (uintptr_t)at;
    else if ((enc & 0x70) == 0x30) v += datarel;
    return v;
}

struct eh_want {
    uintptr_t pc;
    const unsigned char *eh, *fde;
};

static int on_phdr(struct dl_phdr_info *info, size_t size, void *data) {
    struct eh_want *w = data;
    uintptr_t hdr = 0;
    (void)size;
    uintptr_t base = info->dlpi_addr;
    int covers = 0;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const Elf64_Phdr *ph = &info->dlpi_phdr[i];
        if (ph->p_type != PT_LOAD) continue;
        uintptr_t seg = base + ph->p_vaddr;
        if (w->pc >= seg && w->pc < seg + ph->p_memsz) covers = 1;
    }
    if (!covers) return 0;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const Elf64_Phdr *ph = &info->dlpi_phdr[i];
        if (ph->p_type == PT_GNU_EH_FRAME) hdr = info->dlpi_addr + ph->p_vaddr;
    }
    if (!hdr) return 0;
    const unsigned char *p = (const unsigned char *)hdr;
    if (*p++ != 1) return 0;
    unsigned char penc = *p++, cenc = *p++, tenc = *p++;
    w->eh = (const unsigned char *)enc_read(penc, &p, hdr);
    uintptr_t count = enc_read(cenc, &p, hdr);
    const unsigned char *tab = p;
    uintptr_t lo = 0, hi = count;
    while (lo + 1 < hi) {
        uintptr_t mid = (lo + hi) / 2;
        const unsigned char *e = tab + mid * 8;
        uintptr_t start = enc_read(tenc, &e, hdr);
        if (start <= w->pc) lo = mid;
        else hi = mid;
    }
    const unsigned char *e = tab + lo * 8;
    uintptr_t start = enc_read(tenc, &e, hdr);
    uintptr_t fde = enc_read(tenc, &e, hdr);
    if (!fde || start > w->pc) return 0;
    w->fde = (const unsigned char *)fde;
    return 1;
}

static uint32_t off_of(const void *fn) { return (uint32_t)((uintptr_t)fn - image_base); }
static void put32(unsigned char *p, uint32_t v) { memcpy(p, &v, 4); }

static void build_section(uint32_t dwarf_off) {
    memset(section, 0, sizeof section);
    char *fns[5] = {mos_cu_immd, mos_cu_ind, mos_cu_rbp, mos_cu_dwarf, mos_cu_six};
    uint32_t enc[5] = {
        MODE_IMMD | (4u << 16) | (2u << 10) | 0u,
        MODE_IND | (3u << 16) | (1u << 13) | (2u << 10) | 12u,
        MODE_RBP | (2u << 16) | (CU_R15 << 3) | CU_RBX,
        MODE_DWARF | dwarf_off,
        MODE_IMMD | (8u << 16) | (6u << 10) | 0u,
    };
    int ord[5] = {0, 1, 2, 3, 4};
    for (int i = 0; i < 5; i++)
        for (int j = i + 1; j < 5; j++)
            if (off_of(fns[ord[j]]) < off_of(fns[ord[i]])) {
                int t = ord[i];
                ord[i] = ord[j];
                ord[j] = t;
            }
    int top = ord[4];
    unsigned char *page_r = section + 256;
    unsigned char *page_c = section + 512;
    unsigned char *index = section + 768;
    put32(section + 0, 1);
    put32(section + 4, 32);
    put32(section + 8, 0);
    put32(section + 12, 48);
    put32(section + 16, 0);
    put32(section + 20, (uint32_t)(index - section));
    put32(section + 24, 3);

    put32(page_r + 0, 2);
    page_r[4] = 8;
    page_r[6] = 4;
    int k = 0;
    for (int i = 0; i < 4; i++) {
        int id = ord[i];
        put32(page_r + 8 + k * 8, off_of(fns[id]));
        put32(page_r + 12 + k * 8, enc[id]);
        k++;
    }
    put32(page_c + 0, 3);
    page_c[4] = 12;
    page_c[6] = 1;
    page_c[8] = 16;
    page_c[10] = 1;
    put32(page_c + 12, 0);
    put32(page_c + 16, enc[top]);

    uint32_t first = off_of(fns[ord[0]]);
    uint32_t six = off_of(fns[top]);
    uint32_t sentinel = six + 256;
    put32(index + 0, first);
    put32(index + 4, (uint32_t)(page_r - section));
    put32(index + 12, six);
    put32(index + 16, (uint32_t)(page_c - section));
    put32(index + 24, sentinel);
    put32(index + 28, (uint32_t)(page_c - section));
}

struct dyld_uw {
    const void *mh, *dwarf;
    unsigned long dwarf_len;
    const void *compact;
    unsigned long compact_len;
};

int _dyld_find_unwind_sections(void *addr, struct dyld_uw *info) {
    if (!span_id((uintptr_t)addr)) return 0;
    memset(info, 0, sizeof *info);
    info->mh = (void *)image_base;
    info->compact = section;
    info->compact_len = sizeof section;
    info->dwarf = eh_start;
    info->dwarf_len = 1u << 20;
    return 1;
}

static int on_bt(void *ctx, void *arg) {
    (void)arg;
    uintptr_t ip = (uintptr_t)_Unwind_GetIP(ctx);
    if (pending) {
        unsigned long long rbx = _Unwind_GetGR(ctx, 3);
        unsigned long long rbp = _Unwind_GetGR(ctx, 6);
        unsigned long long r12 = _Unwind_GetGR(ctx, 12);
        unsigned long long r13 = _Unwind_GetGR(ctx, 13);
        unsigned long long r14 = _Unwind_GetGR(ctx, 14);
        unsigned long long r15 = _Unwind_GetGR(ctx, 15);
        if (rbx != exp_rbx || rbp != exp_rbp || r12 != exp_r12 || r13 != exp_r13 || r14 != exp_r14 || r15 != exp_r15) {
            fprintf(stderr, "compact: restaurado rbx=%llx rbp=%llx r12=%llx r13=%llx r14=%llx r15=%llx\n",
                    rbx, rbp, r12, r13, r14, r15);
            fprintf(stderr, "compact: esperado  rbx=%llx rbp=%llx r12=%llx r13=%llx r14=%llx r15=%llx\n",
                    exp_rbx, exp_rbp, exp_r12, exp_r13, exp_r14, exp_r15);
            abort();
        }
        if (pending < 1 || pending > 5) abort();
        saw[pending - 1] = 1;
        pending = 0;
    }
    int id = span_id(ip);
    if (id) pending = id;
    return 0;
}

void mos_cu_leaf(void) {
    int rc = _Unwind_Backtrace(on_bt, 0);
    if (rc != 5) abort();
    if (pending) abort();
}

static void plant(unsigned long long rbx, unsigned long long rbp, unsigned long long r12, unsigned long long r13,
                  unsigned long long r14, unsigned long long r15) {
    exp_rbx = rbx;
    exp_rbp = rbp;
    exp_r12 = r12;
    exp_r13 = r13;
    exp_r14 = r14;
    exp_r15 = r15;
}

extern void mos_cu_call_immd(void);
extern void mos_cu_call_ind(void);
extern void mos_cu_call_rbp(void);
extern void mos_cu_call_six(void);
extern void mos_cu_call_dwarf(void);

static void caller_immd(void) {
    plant(0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666);
    mos_cu_call_immd();
}
static void caller_ind(void) {
    plant(0x1010, 0x2020, 0x3030, 0x4040, 0x5050, 0x6060);
    mos_cu_call_ind();
}
static void caller_rbp(void) {
    plant(0xaaaa, 0xbbbb, 0xcccc, 0xdddd, 0xeeee, 0xffff);
    mos_cu_call_rbp();
}
static void caller_six(void) {
    plant(0x61, 0x62, 0x63, 0x64, 0x65, 0x66);
    mos_cu_call_six();
}
static void caller_dwarf(void) {
    plant(0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6);
    mos_cu_call_dwarf();
}

int main(void) {
    struct eh_want w = {(uintptr_t)(mos_cu_dwarf + 8), 0, 0};
    if (!dl_iterate_phdr(on_phdr, &w) || !w.eh || !w.fde) {
        fprintf(stderr, "compact: sem .eh_frame\n");
        return 1;
    }
    eh_start = w.eh;
    if (w.fde < eh_start || (uintptr_t)(w.fde - eh_start) > 0x00ffffffu) {
        fprintf(stderr, "compact: FDE fora do __eh_frame\n");
        return 1;
    }
    uintptr_t lo = ~(uintptr_t)0;
    char *fns[] = {mos_cu_immd, mos_cu_ind, mos_cu_rbp, mos_cu_six, mos_cu_dwarf};
    for (int i = 0; i < 5; i++)
        if ((uintptr_t)fns[i] < lo) lo = (uintptr_t)fns[i];
    image_base = lo & ~0xfffULL;
    if (lo - image_base < 16) image_base -= 0x1000;
    build_section((uint32_t)(w.fde - eh_start));
    add_span(mos_cu_immd, mos_cu_immd_end, 1);
    add_span(mos_cu_ind, mos_cu_ind_end, 2);
    add_span(mos_cu_rbp, mos_cu_rbp_end, 3);
    add_span(mos_cu_six, mos_cu_six_end, 4);
    add_span(mos_cu_dwarf, mos_cu_dwarf_end, 5);
    caller_immd();
    caller_ind();
    caller_rbp();
    caller_six();
    caller_dwarf();
    for (int i = 0; i < 5; i++) {
        if (!saw[i]) {
            fprintf(stderr, "compact: quadro %d não apareceu\n", i + 1);
            return 1;
        }
    }
    printf("compact immd ind rbp six dwarf ok\n");
    return 0;
}

