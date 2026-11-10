/*
 * Metadados Swift dos tipos Combine: value witnesses, metadado de struct/classe
 * e os witnesses que a conformance chama. Os descritores estão em combine_meta.S.
 */
#ifdef CX_HOST
#include <string.h>
#else
#include "sys.h"
void *memcpy(void *d, const void *s, size_t n);
int strcmp(const char *a, const char *b);
#endif

typedef struct cx_pub cx_pub;
typedef struct cx_can cx_can;
cx_pub *cx_passthrough(void);
void cx_send(cx_pub *p, long v);
cx_can *cx_sink(cx_pub *p, void (*fn)(long, void *), void *ctx);

#define ASM(mangled) __asm__(mangled)

typedef void *(*vw_copy)(void *dest, void *src, void *self);
typedef void (*vw_destroy)(void *obj, void *self);
typedef unsigned (*vw_tag)(const void *obj, unsigned empty, void *self);
typedef void (*vw_store)(void *obj, unsigned which, unsigned empty, void *self);

typedef struct {
    vw_copy init_buf;
    vw_destroy destroy;
    vw_copy init_copy;
    vw_copy assign_copy;
    vw_copy init_take;
    vw_copy assign_take;
    vw_tag get_tag;
    vw_store store_tag;
    size_t size;
    size_t stride;
    unsigned flags;
    unsigned extra;
} vwt_t;

static void *copy8(void *dest, void *src, void *self) {
    (void)self;
    memcpy(dest, src, 8);
    return dest;
}
static void destroy8(void *obj, void *self) {
    (void)obj;
    (void)self;
}
static unsigned tag0(const void *obj, unsigned empty, void *self) {
    (void)obj;
    (void)empty;
    (void)self;
    return 0;
}
static void store0(void *obj, unsigned which, unsigned empty, void *self) {
    (void)obj;
    (void)which;
    (void)empty;
    (void)self;
}

static const vwt_t vw8 = {
    copy8, destroy8, copy8, copy8, copy8, copy8, tag0, store0,
    8, 8, 7, 0
};

typedef struct {
    const vwt_t *vw;
    unsigned long kind;
    const void *description;
} smd_t;

typedef struct {
    void (*destroy)(void *);
    unsigned long kind;
    void *super;
    void *cache0;
    void *cache1;
    unsigned long data;
    unsigned flags;
    unsigned inst_addr;
    unsigned inst_size;
    unsigned short align_mask;
    unsigned short reserved;
    unsigned class_size;
    unsigned class_addr;
    const void *description;
    void *ivar_destroyer;
} cmd_t;

extern const int d_drop, d_first, d_filt, d_dedup, d_cmap, d_thr, d_recv;
extern const int d_any, d_pub, d_pubp, d_can, d_pass, d_owp;

static int destroyed;

static void note_destroy(void *obj) {
    if (obj) destroyed++;
}

static smd_t md_drop = { &vw8, 0x200, &d_drop };
static smd_t md_first = { &vw8, 0x200, &d_first };
static smd_t md_filt = { &vw8, 0x200, &d_filt };
static smd_t md_dedup = { &vw8, 0x200, &d_dedup };
static smd_t md_cmap = { &vw8, 0x200, &d_cmap };
static smd_t md_thr = { &vw8, 0x200, &d_thr };
static smd_t md_recv = { &vw8, 0x200, &d_recv };
static smd_t md_any = { &vw8, 0x200, &d_any };
static smd_t md_pub = { &vw8, 0x200, &d_pub };
static smd_t md_pubp = { &vw8, 0x200, &d_pubp };
static smd_t md_out = { &vw8, 0x200, &d_pub };

static cmd_t md_can = {
    note_destroy, 0, 0, 0, 0, 0,
    0, 0, 16, 7, 0, 0, 0, &d_can, 0
};
static cmd_t md_pass = {
    note_destroy, 0, 0, 0, 0, 0,
    0, 0, 16, 7, 0, 0, 0, &d_pass, 0
};
static cmd_t md_owp = {
    note_destroy, 0, 0, 0, 0, 0,
    0, 0, 16, 7, 0, 0, 0, &d_owp, 0
};

void *acc_drop(void) { return &md_drop.kind; }
void *acc_first(void) { return &md_first.kind; }
void *acc_filt(void) { return &md_filt.kind; }
void *acc_dedup(void) { return &md_dedup.kind; }
void *acc_cmap(void) { return &md_cmap.kind; }
void *acc_thr(void) { return &md_thr.kind; }
void *acc_recv(void) { return &md_recv.kind; }
void *acc_any(void) { return &md_any.kind; }
void *acc_pub(void) { return &md_pub.kind; }
void *acc_pubp(void) { return &md_pubp.kind; }
void *acc_can(void) { return &md_can.kind; }
void *acc_pass(void) { return &md_pass.kind; }
void *acc_owp(void) { return &md_owp.kind; }

void mos_can_ma(void) ASM("$s7Combine14AnyCancellableCMa");
void mos_pub_ma(void) ASM("$s7Combine9PublishedVMa");
void mos_owc_tl(void) ASM("$s25ObjectWillChangePublisher7Combine010ObservableA0PTl");
void mos_can_ma(void) __attribute__((alias("acc_can")));
void mos_pub_ma(void) __attribute__((alias("acc_pub")));
void mos_owc_tl(void) __attribute__((alias("acc_owp")));

static int recv_n;

void *recv_witness(void *pub, void *sub) {
    (void)sub;
    recv_n++;
    return pub;
}

const void *output_witness(const void *type, void *wtable) {
    (void)type;
    (void)wtable;
    return &md_out.kind;
}

static cx_pub *owp_pub;
static long owp_got;

static void owp_on(long v, void *ctx) {
    (void)ctx;
    owp_got = v;
}

void *owp_witness(void *self) {
    (void)self;
    if (!owp_pub) {
        owp_pub = cx_passthrough();
        cx_sink(owp_pub, owp_on, 0);
    }
    return owp_pub;
}

static const char *field_name(const int *desc) {
    return (const char *)&desc[2] + desc[2];
}

static int name_is(const int *desc, const char *s) {
    const char *n = field_name(desc);
    return n && strcmp(n, s) == 0;
}

static const int *rel_at(const int *field) {
    return (const int *)((const char *)field + *field);
}

static int conf_ok(const int *mc, const char *type_name) {
    const int *proto = rel_at(&mc[0]);
    const int *type = rel_at(&mc[1]);
    const void **wt = (const void **)((const char *)&mc[2] + mc[2]);
    void *(*recv)(void *, void *);
    const void *(*out)(const void *, void *);
    const unsigned long *md;
    int before = recv_n;
    if (!name_is(proto, "Publisher")) return 0;
    if (!name_is(type, type_name)) return 0;
    recv = (void *(*)(void *, void *))wt[0];
    out = (const void *(*)(const void *, void *))wt[1];
    if (recv((void *)1, (void *)2) != (void *)1) return 0;
    if (recv_n != before + 1) return 0;
    md = (const unsigned long *)out(0, 0);
    if (!md || md[0] != 0x200) return 0;
    return 1;
}

int mos_combine_meta_check(void) {
    unsigned char src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    unsigned char dst[8] = {0};
    const vwt_t *vw;
    void *md;
    cmd_t *cls;
    void *pub;
    extern const int d_mod, d_pubs, d_publisher, d_oo, d_oo_tn, d_oo_tq;
    extern const int mc_cmap, mc_dedup, mc_drop, mc_first, mc_filt;
    extern const int mc_thr, mc_recv, mc_any, mc_pubp;
    extern const void *wt_owp[];

    if ((d_mod & 31) != 0 || !name_is(&d_mod, "Combine")) return 1;
    if ((d_pubs & 31) != 18 || !name_is(&d_pubs, "Publishers")) return 2;
    if (rel_at(&d_pubs + 1) != &d_mod) return 3;
    if (!name_is(&d_cmap, "CompactMap") || *(const unsigned short *)((const char *)&d_cmap + 20) != 2)
        return 4;
    if (!name_is(&d_dedup, "RemoveDuplicates")) return 5;
    if (!name_is(&d_drop, "Drop") || !name_is(&d_first, "First")) return 6;
    if (!name_is(&d_filt, "Filter") || !name_is(&d_thr, "Throttle")) return 7;
    if (!name_is(&d_recv, "ReceiveOn") || !name_is(&d_any, "AnyPublisher")) return 8;
    if (!name_is(&d_pub, "Published") || !name_is(&d_pubp, "Publisher")) return 9;
    if ((d_can & 31) != 16 || !name_is(&d_can, "AnyCancellable")) return 10;
    if ((d_pass & 31) != 16 || !name_is(&d_pass, "PassthroughSubject")) return 11;
    if (!name_is(&d_owp, "ObservableObjectPublisher")) return 12;
    if ((d_oo & 31) != 3 || !name_is(&d_oo, "ObservableObject")) return 13;
    if ((d_oo_tn & 15) != 7 || (d_oo_tq & 15) != 3) return 14;
    if ((d_publisher & 31) != 3) return 15;

    md = acc_pub();
    if (*(unsigned long *)md != 0x200) return 16;
    vw = ((const vwt_t **)md)[-1];
    if (vw->size != 8) return 17;
    if (vw->init_copy(dst, src, md) != (void *)dst || dst[7] != 8) return 18;

    md = acc_can();
    cls = (cmd_t *)((char *)md - __builtin_offsetof(cmd_t, kind));
    if (cls->inst_size != 16 || cls->description != &d_can) return 19;
    destroyed = 0;
    cls->destroy(cls);
    if (destroyed != 1) return 20;

    if (!conf_ok(&mc_cmap, "CompactMap")) return 21;
    if (!conf_ok(&mc_dedup, "RemoveDuplicates")) return 22;
    if (!conf_ok(&mc_drop, "Drop")) return 23;
    if (!conf_ok(&mc_first, "First")) return 24;
    if (!conf_ok(&mc_filt, "Filter")) return 25;
    if (!conf_ok(&mc_thr, "Throttle")) return 26;
    if (!conf_ok(&mc_recv, "ReceiveOn")) return 27;
    if (!conf_ok(&mc_any, "AnyPublisher")) return 28;
    if (!conf_ok(&mc_pubp, "Publisher")) return 29;

    pub = ((void *(*)(void *))wt_owp[0])(0);
    if (!pub) return 30;
    cx_send((cx_pub *)pub, 42);
    if (owp_got != 42) return 31;
    md = acc_owp();
    cls = (cmd_t *)((char *)md - __builtin_offsetof(cmd_t, kind));
    if (cls->description != &d_owp || cls->kind != 0) return 32;
    return 0;
}
