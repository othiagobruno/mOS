/*
 * MTLBuffer.didModifyRange(_:) do OpenBot.
 * O import é _$sSo9MTLBufferP5MetalE14didModifyRangeyySnySiGF em
 * /usr/lib/swift/libswiftMetal.dylib. O machocheck tira um '_' e procura no
 * ELF do libSystem, então o nome aqui não leva o '_' do Mach-O. O stub dessa
 * dylib não exporta o nome: senão o carregador para no ret e não chega aqui.
 * Range<Int> entra em rdi/rsi; self Swift em r13. Vira -didModifyRange:.
 */
typedef struct { unsigned long location, length; } nsrange_t;
typedef struct objc_object *id;
typedef struct objc_selector *SEL;
extern SEL sel_registerName(const char *name);
extern void objc_msgSend(id self, SEL sel, nsrange_t range);

void mos_mtl_did_modify_sysv(long lower, long upper, id self_) {
    nsrange_t r;
    long len = upper - lower;
    if (!self_) return;
    if (len < 0) len = 0;
    r.location = (unsigned long)lower;
    r.length = (unsigned long)len;
    objc_msgSend(self_, sel_registerName("didModifyRange:"), r);
}

#if defined(__x86_64__)
__asm__(
    ".globl \"$sSo9MTLBufferP5MetalE14didModifyRangeyySnySiGF\"\n"
    "\"$sSo9MTLBufferP5MetalE14didModifyRangeyySnySiGF\":\n"
    "  movq %r13, %rdx\n"
    "  jmp mos_mtl_did_modify_sysv\n"
);
#endif
