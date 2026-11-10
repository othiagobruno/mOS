/* Host test: throw atravessa dois frames, roda destrutores, captura,
 * current_exception e rethrow_exception. Liga só cxx_unwind.c. */
#include <stdlib.h>
#include <string.h>

extern "C" void *__cxa_allocate_exception(size_t n) {
    unsigned char *p = (unsigned char *)calloc(1, 128 + n);
    return p ? p + 128 : 0;
}
extern "C" void __cxa_free_exception(void *p);

namespace std {
struct exception_ptr {
    void *p;
    exception_ptr();
    exception_ptr(const exception_ptr &);
    ~exception_ptr();
    exception_ptr &operator=(const exception_ptr &);
};
exception_ptr current_exception();
void rethrow_exception(exception_ptr);
}

static int hits;

struct D {
    ~D() { hits++; }
};

/* rbx e r12–r15 vivos atravessam o catch(...). rbp entra no teste de quadro
 * compacto: aqui o compilador pode usá-lo como ponteiro de frame. */
static int regs_ok(void) {
    unsigned long long rbx = 0, r12 = 0, r13 = 0, r14 = 0, r15 = 0;
    int before = hits;
    asm volatile("movq $0xB0B0B0B0B0B0B0B0, %%rbx\n\t"
                 "movq $0x1212121212121212, %%r12\n\t"
                 "movq $0x1313131313131313, %%r13\n\t"
                 "movq $0x1414141414141414, %%r14\n\t"
                 "movq $0x1515151515151515, %%r15"
                 :
                 :
                 : "rbx", "r12", "r13", "r14", "r15", "memory");
    try {
        D d;
        throw 9;
        (void)d;
    } catch (...) {
        asm volatile("movq %%rbx, %0\n\t"
                     "movq %%r12, %1\n\t"
                     "movq %%r13, %2\n\t"
                     "movq %%r14, %3\n\t"
                     "movq %%r15, %4"
                     : "=r"(rbx), "=r"(r12), "=r"(r13), "=r"(r14), "=r"(r15));
        if (rbx != 0xB0B0B0B0B0B0B0B0ULL || r12 != 0x1212121212121212ULL ||
            r13 != 0x1313131313131313ULL || r14 != 0x1414141414141414ULL ||
            r15 != 0x1515151515151515ULL)
            return 11;
        if (hits != before + 1) return 13;
        return 0;
    }
    return 12;
}

static void leaf() {
    D d;
    throw 7;
    (void)d;
}

static void mid() {
    D d;
    leaf();
    (void)d;
}

int main() {
    try {
        mid();
        return 2;
    } catch (int x) {
        if (x != 7 || hits != 2) return 3;
        std::exception_ptr e = std::current_exception();
        try {
            std::rethrow_exception(e);
            return 4;
        } catch (int y) {
            if (y != 7 || hits != 2) return 5;
        }
        try {
            mid();
        } catch (int) {
            try { throw; }
            catch (int z) { return (z == 7 && hits == 4) ? 0 : 7; }
        }
        return 8;
    }
    int rr = regs_ok();
    if (rr) return rr;
    return 6;
}
