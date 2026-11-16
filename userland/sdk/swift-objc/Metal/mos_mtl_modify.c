/* Chama -didModifyRange: sem o objc_msgSend variádico, que o Swift recusa. */
typedef struct { unsigned long location, length; } nsrange_t;
typedef struct objc_object *id;
typedef struct objc_selector *SEL;
extern SEL sel_registerName(const char *name);
extern void objc_msgSend(id self, SEL sel, nsrange_t range);

void mos_mtlbuffer_did_modify(id buf, long loc, long len) {
    nsrange_t r;
    if (len < 0) len = 0;
    r.location = (unsigned long)loc;
    r.length = (unsigned long)len;
    objc_msgSend(buf, sel_registerName("didModifyRange:"), r);
}

/* OpenBot importa este nome de libswiftMetal.dylib.
 * Range<Int> entra em rdi/rsi (início, fim). self do método Swift fica em r13. */
__attribute__((swiftcall, visibility("default")))
void mos_mtlbuffer_did_modify_swift(long lower, long upper, id self_ __attribute__((swift_context)))
    __asm__("_$sSo9MTLBufferP5MetalE14didModifyRangeyySnySiGF");

__attribute__((swiftcall, visibility("default")))
void mos_mtlbuffer_did_modify_swift(long lower, long upper, id self_ __attribute__((swift_context))) {
    long len = upper - lower;
    mos_mtlbuffer_did_modify(self_, lower, len);
}

/* MTLRenderCommandEncoder.setVertexBuffers(_:offsets:range:): [MTLBuffer?] e [Int] chegam como objetos Array do
 * Swift (elementos a partir de +32), Range<Int> em dois registradores, self em r13. */
extern void objc_msgSend_vb(id self, SEL sel, const id *b, const unsigned long *o, nsrange_t r) __asm__("objc_msgSend");
__attribute__((swiftcall, visibility("default")))
void mos_mtl_set_vbufs_swift(const char *bufs, const char *offs, long lower, long upper, id self_ __attribute__((swift_context)))
    __asm__("_$sSo23MTLRenderCommandEncoderP5MetalE16setVertexBuffers_7offsets5rangeySaySo9MTLBuffer_pSgG_SaySiGSnySiGtF");
__attribute__((swiftcall, visibility("default")))
void mos_mtl_set_vbufs_swift(const char *bufs, const char *offs, long lower, long upper, id self_ __attribute__((swift_context))) {
    nsrange_t r;
    if (!self_ || !bufs || !offs || upper < lower) return;
    r.location = (unsigned long)lower;
    r.length = (unsigned long)(upper - lower);
    objc_msgSend_vb(self_, sel_registerName("setVertexBuffers:offsets:withRange:"), (const id *)(bufs + 32),
                    (const unsigned long *)(offs + 32), r);
}
