/*
 * Teste de host de kernel/compat/chained_ptr.h (decodificação de ponteiros encadeados do carregador).
 *   cc -O2 -std=gnu11 -o /tmp/chained_ptr_test tools/chained_ptr_test.c && /tmp/chained_ptr_test
 * Os valores são montados com structs de bitfields escritas aqui a partir do desenho documentado no
 * cabeçalho (campo a campo, da ordem baixa para a alta, como o clang/gcc dispõem bitfields em little-endian),
 * e não com as mesmas máscaras da função testada.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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

int main(void) {
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
