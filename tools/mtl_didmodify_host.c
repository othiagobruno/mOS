/* Chama o símbolo do ELF ($s…, sem o '_' do Mach-O) com self em r13. */
#include <stdio.h>
typedef struct { unsigned long location, length; } nsrange_t;
typedef struct objc_object *id;
typedef struct objc_selector *SEL;
static id got_self;
static nsrange_t got;
static const char *got_sel;
SEL sel_registerName(const char *name) { return (SEL)name; }
void objc_msgSend(id self, SEL sel, nsrange_t range) {
    got_self = self;
    got_sel = (const char *)sel;
    got = range;
}
static void did_modify(long lower, long upper, id self_) {
    __asm__ volatile(
        "movq %0, %%r13\n"
        "movq %1, %%rdi\n"
        "movq %2, %%rsi\n"
        "leaq \"$sSo9MTLBufferP5MetalE14didModifyRangeyySnySiGF\"(%%rip), %%rax\n"
        "call *%%rax\n"
        :
        : "r"(self_), "r"(lower), "r"(upper)
        : "rax", "rdi", "rsi", "r13", "memory");
}
int main(void) {
    id buf = (id)(unsigned long)0x42;
    did_modify(10, 14, buf);
    if (got_self != buf || got.location != 10 || got.length != 4) {
        printf("falha self=%p loc=%lu len=%lu\n", (void *)got_self, got.location, got.length);
        return 1;
    }
    if (!got_sel || got_sel[0] != 'd') { printf("falha seletor\n"); return 1; }
    printf("didModifyRange elf ok\n");
    return 0;
}
