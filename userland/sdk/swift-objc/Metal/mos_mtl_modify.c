/* Chama -didModifyRange: sem o objc_msgSend variádico, que o Swift recusa. */
typedef struct { unsigned long location, length; } nsrange_t;
typedef struct objc_object *id;
typedef struct objc_selector *SEL;
extern SEL sel_registerName(const char *name);
extern void objc_msgSend(id self, SEL sel, nsrange_t range);

void mos_mtlbuffer_did_modify(id buf, long loc, long len) {
    nsrange_t r;
    r.location = (unsigned long)loc;
    r.length = (unsigned long)len;
    objc_msgSend(buf, sel_registerName("didModifyRange:"), r);
}
