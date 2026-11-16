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

/*
 * MTLRenderCommandEncoder.setVertexBuffers(_:offsets:range:) do overlay Swift:
 * _$sSo23MTLRenderCommandEncoderP5MetalE16setVertexBuffers_7offsets5rangeySaySo9MTLBuffer_pSgG_SaySiGSnySiGtF.
 * rdi = [MTLBuffer?], rsi = [Int] (objetos Array do Swift: elementos a partir de +32), rdx/rcx = Range<Int>,
 * self em r13. Vira -setVertexBuffers:offsets:withRange:, como a versão do Objective-C.
 */
extern void objc_msgSend_vb(id self, SEL sel, const id *b, const unsigned long *o, nsrange_t r) __asm__("objc_msgSend");
void mos_mtl_set_vbufs_sysv(const char *bufs, const char *offs, long lower, long upper, id self_) {
    nsrange_t r;
    if (!self_ || !bufs || !offs || upper < lower) return;
    r.location = (unsigned long)lower;
    r.length = (unsigned long)(upper - lower);
    objc_msgSend_vb(self_, sel_registerName("setVertexBuffers:offsets:withRange:"), (const id *)(bufs + 32),
                    (const unsigned long *)(offs + 32), r);
}

#if defined(__x86_64__)
__asm__(
    ".globl \"$sSo23MTLRenderCommandEncoderP5MetalE16setVertexBuffers_7offsets5rangeySaySo9MTLBuffer_pSgG_SaySiGSnySiGtF\"\n"
    "\"$sSo23MTLRenderCommandEncoderP5MetalE16setVertexBuffers_7offsets5rangeySaySo9MTLBuffer_pSgG_SaySiGSnySiGtF\":\n"
    "  movq %r13, %r8\n"
    "  jmp mos_mtl_set_vbufs_sysv\n"
);
#endif
