/*
 * Teste de host de kernel/compat/chained_ptr.h (decodificação de ponteiros encadeados do carregador).
 *   cc -O2 -std=gnu11 -o /tmp/chained_ptr_test tools/chained_ptr_test.c && /tmp/chained_ptr_test
 *   /tmp/chained_ptr_test <Mach-O>...    # percorre as cadeias de binários reais (fatias fat incluídas)
 * Os valores são montados com structs de bitfields escritas aqui a partir do desenho documentado no
 * cabeçalho (campo a campo, da ordem baixa para a alta, como o clang/gcc dispõem bitfields em little-endian),
 * e não com as mesmas máscaras da função testada.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64; typedef int64_t i64;
#include "../kernel/compat/chained_ptr.h"

struct e_rebase   { u64 target:43, high8:8, next:11, bind:1, auth:1; };
struct e_bind     { u64 ordinal:16, zero:16, addend:19, next:11, bind:1, auth:1; };
struct e_bind24   { u64 ordinal:24, zero:8, addend:19, next:11, bind:1, auth:1; };
struct e_arebase  { u64 target:32, diversity:16, addrDiv:1, key:2, next:11, bind:1, auth:1; };
struct e_abind    { u64 ordinal:16, zero:16, diversity:16, addrDiv:1, key:2, next:11, bind:1, auth:1; };
struct e_abind24  { u64 ordinal:24, zero:8, diversity:16, addrDiv:1, key:2, next:11, bind:1, auth:1; };
struct p_rebase   { u64 target:36, high8:8, reserved:7, next:12, bind:1; };
struct p_bind     { u64 ordinal:24, addend:8, reserved:19, next:12, bind:1; };

#define AS64(x) ({ u64 _v; _Static_assert(sizeof(x) == 8, "8 bytes"); memcpy(&_v, &(x), 8); _v; })

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* Modo arquivo: percorre as cadeias de LC_DYLD_CHAINED_FIXUPS de cada fatia de um Mach-O de verdade com chptr_decode,
 * como o run_chained do carregador, e confere a estrutura: formato aceito, rebase dentro da imagem, ordinal de bind
 * menor que imports_count e cada cadeia presa à própria página do segmento. */
static u32 rd32(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }
static u64 rd64(const u8 *p) { u64 v; memcpy(&v, p, 8); return v; }
static u16 rd16(const u8 *p) { u16 v; memcpy(&v, p, 2); return v; }
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

static void walk_slice(const u8 *m, u64 len, const char *nome) {
    u32 ncmds = rd32(m + 16), cpu = rd32(m + 4), sub = rd32(m + 8);
    const u8 *c = m + 32, *ch = 0;
    u64 vmin = ~0ull, vmax = 0;
    struct { u64 vmaddr, vmsize, fileoff; } seg[32];
    int nseg = 0;
    for (u32 i = 0; i < ncmds && c + 8 <= m + len; i++) {
        u32 cmd = rd32(c), csz = rd32(c + 4);
        if (cmd == 0x19 && nseg < 32) {
            seg[nseg].vmaddr = rd64(c + 24); seg[nseg].vmsize = rd64(c + 32); seg[nseg].fileoff = rd64(c + 40);
            if (memcmp(c + 8, "__PAGEZERO", 10) && seg[nseg].vmsize) {
                if (seg[nseg].vmaddr < vmin) vmin = seg[nseg].vmaddr;
                if (seg[nseg].vmaddr + seg[nseg].vmsize > vmax) vmax = seg[nseg].vmaddr + seg[nseg].vmsize;
            }
            nseg++;
        }
        if (cmd == 0x80000034) ch = c;
        if (csz < 8) break;
        c += csz;
    }
    const char *arch = cpu == 0x01000007 ? "x86_64" : (sub & 0xFF) == 2 ? "arm64e" : "arm64";
    if (!ch) { printf("%s [%s]: sem chained fixups\n", nome, arch); return; }
    const u8 *h = m + rd32(ch + 8);
    u32 starts = rd32(h + 4), nimp = rd32(h + 16);
    const u8 *si = h + starts;
    u32 segc = rd32(si);
    u64 nreb = 0, nbind = 0, nauth = 0, bad = 0;
    unsigned fmts = 0;
    for (u32 s = 0; s < segc && s < (u32)nseg; s++) {
        u32 so = rd32(si + 4 + 4 * s);
        if (!so) continue;
        const u8 *ss = si + so;
        u16 psz = rd16(ss + 4), fmt = rd16(ss + 6), pc = rd16(ss + 20);
        u64 segoff = rd64(ss + 8);
        fmts |= 1u << fmt;
        CHECK(chptr_supported(fmt), "%s [%s]: formato %u recusado", nome, arch, fmt);
        if (!chptr_supported(fmt)) continue;
        u32 stride = chptr_stride(fmt);
        for (u16 pg = 0; pg < pc; pg++) {
            u16 st = rd16(ss + 22 + 2 * pg);
            if (st == 0xFFFF) continue;
            u64 pgva = vmin + segoff + (u64)pg * psz, va = pgva + st;
            for (int guard = 0; guard < 100000; guard++) {
                int k = -1;
                for (int j = 0; j < nseg; j++) if (va >= seg[j].vmaddr && va + 8 <= seg[j].vmaddr + seg[j].vmsize) k = j;
                if (k < 0 || seg[k].fileoff + (va - seg[k].vmaddr) + 8 > len) { bad++; break; }
                u64 v = rd64(m + seg[k].fileoff + (va - seg[k].vmaddr));
                chptr_t d = chptr_decode(fmt, v);
                if (d.bind) { nbind++; if (d.ordinal >= nimp) bad++; }
                else {
                    nreb++;
                    if (chptr_is_arm64e(fmt) && (v >> 63)) nauth++;
                    u64 t = d.target_is_vmaddr ? d.target : vmin + d.target;
                    if (t < vmin || t >= vmax) bad++;
                }
                if (!d.next) break;
                va += (u64)d.next * stride;
                if (va >= pgva + psz) { bad++; break; }       /* a cadeia não sai da página */
            }
        }
    }
    printf("%s [%s]: formatos 0x%x, %llu rebases (%llu autenticados), %llu binds, %llu fora da regra\n", nome, arch, fmts,
           (unsigned long long)nreb, (unsigned long long)nauth, (unsigned long long)nbind, (unsigned long long)bad);
    CHECK(bad == 0 && nreb + nbind > 0, "%s [%s]: cadeias com %llu ponteiros fora da regra", nome, arch, (unsigned long long)bad);
}

static int file_mode(int argc, char **argv) {
    for (int a = 1; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb");
        if (!f) { CHECK(0, "não abriu %s", argv[a]); continue; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        static u8 *b; b = (u8 *)realloc(b, (size_t)n);
        if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); CHECK(0, "leitura de %s", argv[a]); continue; }
        fclose(f);
        u32 mg = be32(b);
        if (mg == 0xCAFEBABE) {
            u32 nf = be32(b + 4);
            for (u32 i = 0; i < nf && i < 16; i++) {
                u32 off = be32(b + 8 + 20 * i + 8), sz = be32(b + 8 + 20 * i + 12);
                if ((u64)off + sz <= (u64)n && rd32(b + off) == 0xFEEDFACF) walk_slice(b + off, sz, argv[a]);
            }
        } else if (rd32(b) == 0xFEEDFACF) walk_slice(b, (u64)n, argv[a]);
        else CHECK(0, "%s não é Mach-O 64", argv[a]);
    }
    printf("chained-ptr (arquivos): %d/%d ok\n", checks - fails, checks);
    return fails != 0;
}

int main(int argc, char **argv) {
    if (argc > 1) return file_mode(argc, argv);
    /* aceitos e recusados */
    unsigned ok_fmts[] = {1, 2, 6, 9, 12};
    for (unsigned i = 0; i < 5; i++) CHECK(chptr_supported(ok_fmts[i]), "formato %u devia ser aceito", ok_fmts[i]);
    unsigned bad[] = {0, 3, 4, 5, 7, 8, 10, 11, 13, 14};
    for (unsigned i = 0; i < 10; i++) CHECK(!chptr_supported(bad[i]), "formato %u devia ser recusado", bad[i]);
    CHECK(chptr_stride(1) == 8 && chptr_stride(9) == 8 && chptr_stride(12) == 8, "passo arm64e = 8");
    CHECK(chptr_stride(2) == 4 && chptr_stride(6) == 4, "passo 64 = 4");

    unsigned e_fmts[] = {CHPTR_ARM64E, CHPTR_ARM64E_USERLAND, CHPTR_ARM64E_USERLAND24};
    for (unsigned k = 0; k < 3; k++) {
        unsigned f = e_fmts[k];
        /* rebase comum: 1 = vmaddr, 9/12 = deslocamento; high8 vai para 63:56 */
        struct e_rebase r = { .target = 0x100004000ULL, .high8 = 0xA5, .next = 0x3FF, .bind = 0, .auth = 0 };
        chptr_t c = chptr_decode(f, AS64(r));
        CHECK(!c.bind && c.target == 0x100004000ULL && c.high8 == 0xA5ULL << 56 && c.next == 0x3FF, "fmt %u rebase", f);
        CHECK(c.target_is_vmaddr == (f == CHPTR_ARM64E), "fmt %u: rebase por vmaddr só no 1", f);

        /* rebase autenticado: sempre deslocamento de 32 bits, diversidade/chave ignoradas */
        struct e_arebase ar = { .target = 0xDEADBEE0, .diversity = 0x1234, .addrDiv = 1, .key = 3, .next = 7, .bind = 0, .auth = 1 };
        c = chptr_decode(f, AS64(ar));
        CHECK(!c.bind && c.target == 0xDEADBEE0 && !c.target_is_vmaddr && c.high8 == 0 && c.next == 7, "fmt %u auth rebase", f);

        /* bind com addend negativo de 19 bits */
        if (f == CHPTR_ARM64E_USERLAND24) {
            struct e_bind24 b = { .ordinal = 0xABCDE, .addend = (u64)(-5) & 0x7FFFF, .next = 2, .bind = 1, .auth = 0 };
            c = chptr_decode(f, AS64(b));
            CHECK(c.bind && c.ordinal == 0xABCDE && c.addend == -5 && c.next == 2, "fmt 12 bind24: ord %x add %lld", c.ordinal, (long long)c.addend);
            struct e_abind24 ab = { .ordinal = 0x12345, .diversity = 0xFFFF, .addrDiv = 1, .key = 2, .next = 1, .bind = 1, .auth = 1 };
            c = chptr_decode(f, AS64(ab));
            CHECK(c.bind && c.ordinal == 0x12345 && c.addend == 0 && c.next == 1, "fmt 12 auth bind24: ord %x", c.ordinal);
        } else {
            struct e_bind b = { .ordinal = 0xBEEF, .addend = 0x3FFFF, .next = 2, .bind = 1, .auth = 0 };
            c = chptr_decode(f, AS64(b));
            CHECK(c.bind && c.ordinal == 0xBEEF && c.addend == 0x3FFFF && c.next == 2, "fmt %u bind", f);
            struct e_bind bn = { .ordinal = 1, .addend = 0x40000, .bind = 1 };     /* -262144 */
            c = chptr_decode(f, AS64(bn));
            CHECK(c.addend == -262144, "fmt %u addend mínimo: %lld", f, (long long)c.addend);
            struct e_abind ab = { .ordinal = 0xFFFF, .diversity = 0xAAAA, .addrDiv = 1, .key = 1, .next = 0, .bind = 1, .auth = 1 };
            c = chptr_decode(f, AS64(ab));
            CHECK(c.bind && c.ordinal == 0xFFFF && c.addend == 0 && c.next == 0, "fmt %u auth bind", f);
        }
    }
    /* regressão do bug antigo: formato 12 cortava o ordinal em 16 bits */
    {
        struct e_bind24 b = { .ordinal = 0x10001, .bind = 1 };
        CHECK(chptr_decode(12, AS64(b)).ordinal == 0x10001, "ordinal 0x10001 no formato 12");
    }

    /* 64 e 64_OFFSET */
    for (unsigned f = 2; f <= 6; f += 4) {
        struct p_rebase r = { .target = 0x8FFFFFFFFULL, .high8 = 0x80, .next = 0xFFF, .bind = 0 };
        chptr_t c = chptr_decode(f, AS64(r));
        CHECK(!c.bind && c.target == 0x8FFFFFFFFULL && c.high8 == 0x80ULL << 56 && c.next == 0xFFF, "fmt %u rebase", f);
        CHECK(c.target_is_vmaddr == (f == 2), "fmt %u vmaddr", f);
        struct p_bind b = { .ordinal = 0xFFFFFF, .addend = 0xFF, .next = 3, .bind = 1 };
        c = chptr_decode(f, AS64(b));
        CHECK(c.bind && c.ordinal == 0xFFFFFF && c.addend == 0xFF && c.next == 3, "fmt %u bind", f);
    }

    printf("chained-ptr: %d/%d ok\n", checks - fails, checks);
    return fails != 0;
}
