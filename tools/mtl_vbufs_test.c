/* Convenção Swift de MTLRenderCommandEncoder.setVertexBuffers(_:offsets:range:) → -setVertexBuffers:offsets:withRange:.
 *   zig cc -O1 -w -o /tmp/vbt tools/mtl_vbufs_test.c userland/libsystem/mtl_didmodify.c && /tmp/vbt */
#include <stdio.h>
typedef struct { unsigned long location, length; } nsrange_t;
typedef struct objc_object *id;
typedef struct objc_selector *SEL;
static id gs; static const id *gb; static const unsigned long *go; static nsrange_t gr; static const char *gsel;
SEL sel_registerName(const char *n) { return (SEL)n; }
void objc_msgSend(id s, SEL sel, const id *b, const unsigned long *o, nsrange_t r) { gs = s; gsel = (const char *)sel; gb = b; go = o; gr = r; }
__attribute__((swiftcall)) void vb(const char *, const char *, long, long, id __attribute__((swift_context)))
    __asm__("$sSo23MTLRenderCommandEncoderP5MetalE16setVertexBuffers_7offsets5rangeySaySo9MTLBuffer_pSgG_SaySiGSnySiGtF");
int main(void) {
    static unsigned long ab[4 + 2] = {0, 0, 2, 4, 0x111, 0}, ao[4 + 2] = {0, 0, 2, 4, 16, 32};
    vb((const char *)ab, (const char *)ao, 3, 5, (id)0x42);
    int ok = gs == (id)0x42 && gb[0] == (id)0x111 && gb[1] == 0 && go[1] == 32 && gr.location == 3 && gr.length == 2 && gsel[3] == 'V';
    printf(ok ? "setVertexBuffers ok\n" : "falha\n");
    return !ok;
}
