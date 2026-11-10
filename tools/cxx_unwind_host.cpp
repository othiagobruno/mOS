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
    return 6;
}
