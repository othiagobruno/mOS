/* Contacts: CNContactStore, CNMutableContact, CNSaveRequest e o formatador postal.
 * A agenda é o arquivo de cn_store.c. Não fala com a Apple. */
#include "libc.h"
#include "foundation.h"
#include "cn_store.h"

void cx_contacts_ui_init(void);
int cx_tcc_request(const char *service);
int cx_tcc_state(const char *service);
id objc_autorelease(id o);
void objc_release(id o);
char *strtok(char *s, const char *d);

extern cx_class_t CNContact_c __asm__("OBJC_CLASS_$_CNContact");
extern cx_class_t CNContactStore_c __asm__("OBJC_CLASS_$_CNContactStore");
extern cx_class_t CNPostalAddressFormatter_c __asm__("OBJC_CLASS_$_CNPostalAddressFormatter");

cx_class_t CNMutableContact_c __asm__("OBJC_CLASS_$_CNMutableContact");
cx_class_t CNMutableContact_m __asm__("OBJC_METACLASS_$_CNMutableContact");
cx_class_t CNSaveRequest_c __asm__("OBJC_CLASS_$_CNSaveRequest");
cx_class_t CNSaveRequest_m __asm__("OBJC_METACLASS_$_CNSaveRequest");
cx_class_t CNPostalAddress_c __asm__("OBJC_CLASS_$_CNPostalAddress");
cx_class_t CNPostalAddress_m __asm__("OBJC_METACLASS_$_CNPostalAddress");
cx_class_t CNMutablePostalAddress_c __asm__("OBJC_CLASS_$_CNMutablePostalAddress");
cx_class_t CNMutablePostalAddress_m __asm__("OBJC_METACLASS_$_CNMutablePostalAddress");

enum { CN_REQ = 16, CN_OP = 32 };

typedef struct {
    id obj;
    int used, kind;
    cx_cn_contact c;
} cn_slot_t;

typedef struct {
    int kind;
    cx_cn_contact c;
} cn_req_op_t;

typedef struct {
    id obj;
    int used, n;
    cn_req_op_t op[CN_OP];
} cn_req_t;

/* um slot por objeto (contato ou endereço), alocado à parte para o ponteiro não mudar; solto no dealloc */
static cn_slot_t **g_slots;
static int g_nslots, g_capslots;
static cn_req_t g_req[CN_REQ];

static void field_copy(char *d, int cap, id s) {
    const char *p = s ? cx_cstr(s) : "";
    int i = 0;
    if (!p) p = "";
    for (; i < cap - 1 && p[i]; i++) d[i] = p[i];
    d[i] = 0;
}

static id str_ar(const char *s) { return objc_autorelease(cx_str(s ? s : "")); }

static cn_slot_t *slot_of(id o, int make) {
    if (!o) return 0;
    for (int i = 0; i < g_nslots; i++) if (g_slots[i]->obj == o) return g_slots[i];
    if (!make) return 0;
    if (g_nslots == g_capslots) {
        int cap = g_capslots ? g_capslots * 2 : 64;
        cn_slot_t **n = realloc(g_slots, (size_t)cap * sizeof *n);
        if (!n) return 0;
        g_slots = n; g_capslots = cap;
    }
    cn_slot_t *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->used = 1;
    s->obj = o;
    g_slots[g_nslots++] = s;
    return s;
}
static void slot_free(id o) {
    for (int i = 0; i < g_nslots; i++)
        if (g_slots[i]->obj == o) { free(g_slots[i]); g_slots[i] = g_slots[--g_nslots]; return; }
}
static void cn_dealloc(id self, SEL c) {
    slot_free(self);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}

static cn_req_t *req_of(id o, int make) {
    cn_req_t *free_s = 0;
    int i;
    if (!o) return 0;
    for (i = 0; i < CN_REQ; i++) {
        if (g_req[i].used && g_req[i].obj == o) return &g_req[i];
        if (!g_req[i].used && !free_s) free_s = &g_req[i];
    }
    if (!make || !free_s) return 0;
    memset(free_s, 0, sizeof *free_s);
    free_s->used = 1;
    free_s->obj = o;
    return free_s;
}

#define GETF(name, field, cap) \
    static id get_##name(id self, SEL c) { cn_slot_t *s = slot_of(self, 0); (void)c; return str_ar(s ? s->c.field : ""); } \
    static void set_##name(id self, SEL c, id v) { cn_slot_t *s = slot_of(self, 1); (void)c; if (s) field_copy(s->c.field, cap, v); }

GETF(given, given, 64)
GETF(prefix, prefix, 32)
GETF(suffix, suffix, 32)
GETF(previous, previous, 64)
GETF(phonetic_given, phonetic_given, 64)
GETF(phonetic_middle, phonetic_middle, 64)
GETF(phonetic_family, phonetic_family, 64)
GETF(phonetic_org, phonetic_org, 80)
GETF(url, url, 96)
GETF(relation, relation, 48)
GETF(im_user, im_user, 64)
GETF(im_service, im_service, 32)
GETF(social_user, social_user, 64)
GETF(social_service, social_service, 32)
GETF(social_url, social_url, 96)
GETF(social_id, social_id, 64)
GETF(image, image, 68)
GETF(family, family, 64)
GETF(org, org, 80)
GETF(phone, phone, 64)
GETF(email, email, 80)
GETF(ident, id, 40)
GETF(street, street, 96)
GETF(city, city, 48)
GETF(state, state, 32)
GETF(zip, zip, 16)
GETF(country, country, 48)
GETF(iso, iso, 8)

static void set_postal(id self, SEL c, id addr) {
    cn_slot_t *s = slot_of(self, 1);
    cn_slot_t *a = slot_of(addr, 0);
    (void)c;
    if (!s || !a) return;
    s->c.street[0] = 0;
    memcpy(s->c.street, a->c.street, sizeof s->c.street);
    memcpy(s->c.city, a->c.city, sizeof s->c.city);
    memcpy(s->c.state, a->c.state, sizeof s->c.state);
    memcpy(s->c.zip, a->c.zip, sizeof s->c.zip);
    memcpy(s->c.country, a->c.country, sizeof s->c.country);
    memcpy(s->c.iso, a->c.iso, sizeof s->c.iso);
}

static id get_postal(id self, SEL c) {
    cn_slot_t *s = slot_of(self, 0);
    id o;
    cn_slot_t *a;
    (void)c;
    o = cx_alloc_instance(&CNPostalAddress_c);
    a = slot_of(o, 1);
    if (s && a) {
        memcpy(a->c.street, s->c.street, sizeof a->c.street);
        memcpy(a->c.city, s->c.city, sizeof a->c.city);
        memcpy(a->c.state, s->c.state, sizeof a->c.state);
        memcpy(a->c.zip, s->c.zip, sizeof a->c.zip);
        memcpy(a->c.country, s->c.country, sizeof a->c.country);
        memcpy(a->c.iso, s->c.iso, sizeof a->c.iso);
    }
    return objc_autorelease(o);
}

static id contact_from(const cx_cn_contact *c) {
    id o = cx_alloc_instance(&CNMutableContact_c);
    cn_slot_t *s = slot_of(o, 1);
    if (s) s->c = *c;
    return o;
}

static void stage_contact(cn_req_t *r, int kind, id contact) {
    cn_slot_t *s;
    if (!r || r->n >= CN_OP) return;
    s = slot_of(contact, 0);
    if (!s) return;
    r->op[r->n].kind = kind;
    r->op[r->n].c = s->c;
    r->n++;
}

static void req_add(id self, SEL c, id contact, id container) {
    (void)c; (void)container;
    stage_contact(req_of(self, 1), 1, contact);
}
static void req_update(id self, SEL c, id contact) {
    (void)c;
    stage_contact(req_of(self, 1), 2, contact);
}
static void req_delete(id self, SEL c, id contact) {
    (void)c;
    stage_contact(req_of(self, 1), 3, contact);
}

static id cn_error(void) {
    cx_class_t *k = cx_class_named("NSError");
    return k ? objc_autorelease(cx_alloc_instance(k)) : cx_str("CNError");
}

static BOOL store_execute(id self, SEL c, id request, id *error) {
    cn_req_t *r = req_of(request, 0);
    int i, rc;
    (void)self; (void)c;
    cx_cn_begin();
    if (!r) { if (error) *error = cn_error(); return NO; }
    for (i = 0; i < r->n; i++) {
        char id[40];
        if (r->op[i].kind == 1) rc = cx_cn_stage_add(&r->op[i].c, id);
        else if (r->op[i].kind == 2) rc = cx_cn_stage_update(&r->op[i].c);
        else rc = cx_cn_stage_delete(r->op[i].c.id);
        if (rc != CX_CN_OK) { if (error) *error = cn_error(); return NO; }
        if (r->op[i].kind == 1) {
            cn_slot_t *s = slot_of(0, 0);
            (void)s;
            memcpy(r->op[i].c.id, id, sizeof id);
        }
    }
    rc = cx_cn_commit();
    if (rc != CX_CN_OK) { if (error) *error = cn_error(); return NO; }
    if (error) *error = 0;
    return YES;
}

static long store_auth(id cls, SEL c, long entity) {
    (void)cls; (void)c; (void)entity;
    return cx_cn_auth();
}

typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, BOOL, id); } acc_blk_t;

void cx_tcc_request_async(const char *service, void (*cb)(int granted, void *ctx), void *ctx);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
/* o bloco volta depois da escolha no diálogo do TCC (ou na hora, se já houver), numa fila global, como no macOS */
static void store_access_done(int granted, void *ctx) {
    acc_blk_t *b = ctx;
    (void)granted;
    if (b->invoke) b->invoke(b, cx_cn_auth() == CX_CN_AUTHORIZED ? YES : NO, cx_cn_auth() == CX_CN_AUTHORIZED ? 0 : cn_error());
    _Block_release(b);
}
static void store_request_access(id self, SEL c, long entity, void *block) {
    (void)self; (void)c; (void)entity;
    if (!block) { cx_tcc_request("kTCCServiceAddressBook"); return; }
    cx_tcc_request_async("kTCCServiceAddressBook", store_access_done, _Block_copy(block));
}

static id store_enumerate(id self, SEL c, id request, id *error, void *block) {
    typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id, BOOL *); } en_blk_t;
    en_blk_t *b = block;
    int n, i;
    (void)self; (void)c; (void)request;
    n = cx_cn_count();
    if (n < 0) { if (error) *error = cn_error(); return 0; }
    for (i = 0; i < n; i++) {
        cx_cn_contact rec;
        BOOL stop = NO;
        id o;
        if (cx_cn_get(i, &rec) != CX_CN_OK) break;
        o = contact_from(&rec);
        if (b && b->invoke) b->invoke(block, o, &stop);
        if (stop) break;
    }
    if (error) *error = 0;
    return 0;
}

static id get_image_data(id self, SEL c) {
    cn_slot_t *s = slot_of(self, 0);
    unsigned char buf[32];
    int n;
    (void)c;
    if (!s || !s->c.image[0]) return 0;
    n = cx_cn_image_bytes(s->c.image, buf, (int)sizeof buf);
    if (n < 0) return 0;
    return objc_autorelease(MSG(id, (id)cx_class_named("NSData"), "dataWithBytes:length:", buf, (unsigned long)n));
}

static id fmt_string(id self, SEL c, id obj) {
    cn_slot_t *s = slot_of(obj, 0);
    char buf[400];
    (void)self; (void)c;
    if (!s) return str_ar("");
    cx_cn_format_postal(&s->c, buf, (int)sizeof buf);
    return str_ar(buf);
}


/* ================================================================ CNContactVCardSerialization (vCard 3.0) */
cx_class_t CNContactVCardSerialization_c __asm__("OBJC_CLASS_$_CNContactVCardSerialization");
cx_class_t CNContactVCardSerialization_m __asm__("OBJC_METACLASS_$_CNContactVCardSerialization");
typedef struct { char *p; size_t n, cap; } vbuf_t;
static void vput(vbuf_t *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap * 2 : 1024;
        while (c < b->n + n + 1) c *= 2;
        char *q = realloc(b->p, c);
        if (!q) return;
        b->p = q; b->cap = c;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}
static void vstr(vbuf_t *b, const char *s) { vput(b, s, strlen(s)); }
static void vesc(vbuf_t *b, const char *s) {                                    /* \ ; , e quebra de linha */
    for (; *s; s++) {
        if (*s == '\\' || *s == ';' || *s == ',') { vput(b, "\\", 1); vput(b, s, 1); }
        else if (*s == '\n') vput(b, "\\n", 2);
        else if (*s != '\r') vput(b, s, 1);
    }
}
static void vline(vbuf_t *b, const char *name, const char *val) { if (val[0]) { vstr(b, name); vesc(b, val); vstr(b, "\r\n"); } }
static void vcard_of(vbuf_t *b, const cx_cn_contact *c) {
    vstr(b, "BEGIN:VCARD\r\nVERSION:3.0\r\nPRODID:-//mOS//Contatos//PT\r\nN:");
    vesc(b, c->family); vstr(b, ";"); vesc(b, c->given); vstr(b, ";;"); vesc(b, c->prefix); vstr(b, ";"); vesc(b, c->suffix); vstr(b, "\r\n");
    char fn[200];
    snprintf(fn, sizeof fn, "%s%s%s", c->given, c->given[0] && c->family[0] ? " " : "", c->family);
    vline(b, "FN:", fn[0] ? fn : c->org);
    vline(b, "ORG:", c->org);
    vline(b, "TEL;type=pref:", c->phone);
    vline(b, "EMAIL;type=INTERNET;type=pref:", c->email);
    if (c->street[0] || c->city[0] || c->state[0] || c->zip[0] || c->country[0]) {
        vstr(b, "ADR;type=pref:;;"); vesc(b, c->street); vstr(b, ";"); vesc(b, c->city); vstr(b, ";"); vesc(b, c->state); vstr(b, ";");
        vesc(b, c->zip); vstr(b, ";"); vesc(b, c->country); vstr(b, "\r\n");
    }
    vline(b, "URL:", c->url);
    vline(b, "X-PHONETIC-FIRST-NAME:", c->phonetic_given);
    vline(b, "X-PHONETIC-MIDDLE-NAME:", c->phonetic_middle);
    vline(b, "X-PHONETIC-LAST-NAME:", c->phonetic_family);
    vline(b, "X-PHONETIC-ORG:", c->phonetic_org);
    vline(b, "X-MAIDENNAME:", c->previous);
    if (c->im_user[0]) { char v[128]; snprintf(v, sizeof v, "%s:%s", c->im_service[0] ? c->im_service : "x-apple", c->im_user); vstr(b, "IMPP:"); vstr(b, v); vstr(b, "\r\n"); }
    if (c->social_url[0] || c->social_user[0]) {
        vstr(b, "X-SOCIALPROFILE;type="); vesc(b, c->social_service[0] ? c->social_service : "other");
        if (c->social_user[0]) { vstr(b, ";x-user="); vesc(b, c->social_user); }
        vstr(b, ":"); vesc(b, c->social_url[0] ? c->social_url : c->social_user); vstr(b, "\r\n");
    }
    vline(b, "X-ABRELATEDNAMES:", c->relation);
    vline(b, "UID:", c->id);
    vstr(b, "END:VCARD\r\n");
}
static id vcard_data(id cls, SEL c, id contacts, id *err) {
    (void)cls; (void)c;
    if (err) *err = 0;
    vbuf_t b = {0};
    u64 n = contacts ? MSG0(u64, contacts, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        cn_slot_t *s = slot_of(MSG(id, contacts, "objectAtIndex:", i), 0);
        cx_cn_contact empty;
        memset(&empty, 0, sizeof empty);
        vcard_of(&b, s ? &s->c : &empty);
    }
    id d = MSG(id, (id)cx_class_named("NSData"), "dataWithBytes:length:", b.p ? b.p : "", (u64)b.n);
    free(b.p);
    return d;
}
/* valor de vCard sem os escapes; comp = qual componente separado por ';' (-1 = tudo) */
static void vunesc(const char *v, int comp, char *out, size_t cap) {
    int k = 0;
    size_t o = 0;
    for (const char *p = v; *p; p++) {
        if (*p == '\\' && p[1]) {
            p++;
            char ch = *p == 'n' || *p == 'N' ? '\n' : *p;
            if ((comp < 0 || k == comp) && o + 1 < cap) out[o++] = ch;
            continue;
        }
        if (*p == ';' && comp >= 0) { if (k == comp) break; k++; continue; }
        if (comp < 0 || k == comp) { if (o + 1 < cap) out[o++] = *p; }
    }
    out[o] = 0;
}
static void put_field(char *dst, size_t cap, const char *v, int comp) { char t[512]; vunesc(v, comp, t, sizeof t); snprintf(dst, cap, "%s", t); }
static void vcard_prop(cx_cn_contact *c, char *line, int *have) {
    char *colon = strchr(line, ':');
    if (!colon) return;
    *colon = 0;
    char *val = colon + 1, *name = line, *dot = strchr(name, '.');
    char *semi = strchr(name, ';');
    if (dot && (!semi || dot < semi)) name = dot + 1;                            /* "item1.EMAIL" */
    char params[256] = "";
    semi = strchr(name, ';');
    if (semi) { snprintf(params, sizeof params, "%s", semi + 1); *semi = 0; }
    for (char *q = name; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
    #define ONCE(bit) if (*have & (bit)) return; *have |= (bit)
    if (!strcmp(name, "N")) { put_field(c->family, sizeof c->family, val, 0); put_field(c->given, sizeof c->given, val, 1);
                              put_field(c->prefix, sizeof c->prefix, val, 3); put_field(c->suffix, sizeof c->suffix, val, 4); *have |= 1; }
    else if (!strcmp(name, "FN")) { if (!(*have & 1)) { char t[200]; vunesc(val, -1, t, sizeof t); char *sp = strrchr(t, ' ');
                                      if (sp) { *sp = 0; snprintf(c->given, sizeof c->given, "%s", t); snprintf(c->family, sizeof c->family, "%s", sp + 1); }
                                      else snprintf(c->given, sizeof c->given, "%s", t); } }
    else if (!strcmp(name, "ORG")) put_field(c->org, sizeof c->org, val, 0);
    else if (!strcmp(name, "TEL")) { ONCE(2); put_field(c->phone, sizeof c->phone, val, -1); }
    else if (!strcmp(name, "EMAIL")) { ONCE(4); put_field(c->email, sizeof c->email, val, -1); }
    else if (!strcmp(name, "ADR")) { ONCE(8); put_field(c->street, sizeof c->street, val, 2); put_field(c->city, sizeof c->city, val, 3);
                                     put_field(c->state, sizeof c->state, val, 4); put_field(c->zip, sizeof c->zip, val, 5);
                                     put_field(c->country, sizeof c->country, val, 6); }
    else if (!strcmp(name, "URL")) { ONCE(16); put_field(c->url, sizeof c->url, val, -1); }
    else if (!strcmp(name, "X-PHONETIC-FIRST-NAME")) put_field(c->phonetic_given, sizeof c->phonetic_given, val, -1);
    else if (!strcmp(name, "X-PHONETIC-MIDDLE-NAME")) put_field(c->phonetic_middle, sizeof c->phonetic_middle, val, -1);
    else if (!strcmp(name, "X-PHONETIC-LAST-NAME")) put_field(c->phonetic_family, sizeof c->phonetic_family, val, -1);
    else if (!strcmp(name, "X-PHONETIC-ORG")) put_field(c->phonetic_org, sizeof c->phonetic_org, val, -1);
    else if (!strcmp(name, "X-MAIDENNAME")) put_field(c->previous, sizeof c->previous, val, -1);
    else if (!strcmp(name, "X-ABRELATEDNAMES")) { ONCE(32); put_field(c->relation, sizeof c->relation, val, -1); }
    else if (!strcmp(name, "UID")) put_field(c->id, sizeof c->id, val, -1);
    else if (!strcmp(name, "IMPP")) {
        ONCE(64);
        char t[160]; vunesc(val, -1, t, sizeof t);
        char *k = strchr(t, ':');
        if (k) { *k = 0; snprintf(c->im_service, sizeof c->im_service, "%s", t); snprintf(c->im_user, sizeof c->im_user, "%s", k + 1); }
        else snprintf(c->im_user, sizeof c->im_user, "%s", t);
    } else if (!strcmp(name, "X-SOCIALPROFILE")) {
        ONCE(128);
        put_field(c->social_url, sizeof c->social_url, val, -1);
        for (char *t = strtok(params, ";"); t; t = strtok(0, ";")) {
            if (!strncasecmp(t, "type=", 5)) snprintf(c->social_service, sizeof c->social_service, "%s", t + 5);
            else if (!strncasecmp(t, "x-user=", 7)) snprintf(c->social_user, sizeof c->social_user, "%s", t + 7);
        }
    }
    #undef ONCE
}
static id vcard_contacts(id cls, SEL c, id data, id *err) {
    (void)cls; (void)c;
    if (err) *err = 0;
    size_t n = data ? (size_t)MSG0(u64, data, "length") : 0;
    const char *src = data ? MSG0(const char *, data, "bytes") : 0;
    id out = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    if (!src) return out;
    char *line = malloc(n + 1);
    size_t ln = 0;
    int in = 0, have = 0;
    cx_cn_contact cur;
    for (size_t i = 0; i <= n; i++) {
        int eol = i == n || src[i] == '\n';
        if (!eol) { if (src[i] != '\r') line[ln++] = src[i]; continue; }
        if (i + 1 < n && (src[i + 1] == ' ' || src[i + 1] == '\t')) { i++; continue; }   /* linha dobrada */
        line[ln] = 0;
        if (!strncasecmp(line, "BEGIN:VCARD", 11)) { in = 1; have = 0; memset(&cur, 0, sizeof cur); }
        else if (!strncasecmp(line, "END:VCARD", 9) && in) {
            id o = contact_from(&cur);
            MSG(void, out, "addObject:", o);
            objc_release(o);
            in = 0;
        } else if (in && ln) vcard_prop(&cur, line, &have);
        ln = 0;
    }
    free(line);
    if (in || (n && !MSG0(u64, out, "count"))) {                               /* cartão sem fim ou nenhum cartão */
        if (err) *err = MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("CNErrorDomain"), (long)700 /* CNErrorCodeVCardMalformed */, (id)0);
        return in ? 0 : out;
    }
    return out;
}
static id vcard_desc_key(id cls, SEL c, id contacts) { (void)cls; (void)c; (void)contacts; return cx_str("CNContactVCardSerializationDescriptor"); }
static id vcard_desc(id cls, SEL c) { (void)cls; (void)c; return cx_str("CNContactVCardSerializationDescriptor"); }

static void add_m(cx_class_t *k, const char *sel, void *imp) {
    if (k && !cx_responds(k, sel_registerName(sel))) cx_add_method(k, sel, imp);
}

void cx_contacts_init(void) {
    cx_class_t *obj = cx_class_named("NSObject");
    cx_define_class(&CNPostalAddress_c, &CNPostalAddress_m, "CNPostalAddress", obj, 0);
    cx_define_class(&CNMutablePostalAddress_c, &CNMutablePostalAddress_m, "CNMutablePostalAddress", &CNPostalAddress_c, 0);
    if (!cx_class_named("CNMutableContact"))
        cx_define_class(&CNMutableContact_c, &CNMutableContact_m, "CNMutableContact", &CNContact_c, 0);
    if (!cx_class_named("CNSaveRequest"))
        cx_define_class(&CNSaveRequest_c, &CNSaveRequest_m, "CNSaveRequest", obj, 0);

    cx_add_method(&CNPostalAddress_c, "dealloc", (void *)cn_dealloc);
    cx_add_method(&CNContact_c, "dealloc", (void *)cn_dealloc);
    cx_define_class(&CNContactVCardSerialization_c, &CNContactVCardSerialization_m, "CNContactVCardSerialization", obj, 0);
    cx_add_cmethod(&CNContactVCardSerialization_c, "dataWithContacts:error:", (void *)vcard_data);
    cx_add_cmethod(&CNContactVCardSerialization_c, "contactsWithData:error:", (void *)vcard_contacts);
    cx_add_cmethod(&CNContactVCardSerialization_c, "descriptorForRequiredKeys", (void *)vcard_desc);
    cx_add_cmethod(&CNContactVCardSerialization_c, "descriptorForRequiredKeysForContacts:", (void *)vcard_desc_key);
    add_m(&CNPostalAddress_c, "street", (void *)get_street);
    add_m(&CNMutablePostalAddress_c, "setStreet:", (void *)set_street);
    add_m(&CNPostalAddress_c, "city", (void *)get_city);
    add_m(&CNMutablePostalAddress_c, "setCity:", (void *)set_city);
    add_m(&CNPostalAddress_c, "state", (void *)get_state);
    add_m(&CNMutablePostalAddress_c, "setState:", (void *)set_state);
    add_m(&CNPostalAddress_c, "postalCode", (void *)get_zip);
    add_m(&CNMutablePostalAddress_c, "setPostalCode:", (void *)set_zip);
    add_m(&CNPostalAddress_c, "country", (void *)get_country);
    add_m(&CNMutablePostalAddress_c, "setCountry:", (void *)set_country);
    add_m(&CNPostalAddress_c, "ISOCountryCode", (void *)get_iso);
    add_m(&CNMutablePostalAddress_c, "setISOCountryCode:", (void *)set_iso);

    add_m(&CNContact_c, "identifier", (void *)get_ident);
    add_m(&CNContact_c, "givenName", (void *)get_given);
    add_m(&CNMutableContact_c, "setGivenName:", (void *)set_given);
    add_m(&CNContact_c, "familyName", (void *)get_family);
    add_m(&CNMutableContact_c, "setFamilyName:", (void *)set_family);
    add_m(&CNContact_c, "organizationName", (void *)get_org);
    add_m(&CNMutableContact_c, "setOrganizationName:", (void *)set_org);
    add_m(&CNContact_c, "phoneNumberString", (void *)get_phone);
    add_m(&CNMutableContact_c, "setPhoneNumberString:", (void *)set_phone);
    add_m(&CNContact_c, "emailAddressString", (void *)get_email);
    add_m(&CNMutableContact_c, "setEmailAddressString:", (void *)set_email);
    add_m(&CNContact_c, "postalAddress", (void *)get_postal);
    add_m(&CNMutableContact_c, "setPostalAddress:", (void *)set_postal);
    add_m(&CNContact_c, "namePrefix", (void *)get_prefix);
    add_m(&CNMutableContact_c, "setNamePrefix:", (void *)set_prefix);
    add_m(&CNContact_c, "nameSuffix", (void *)get_suffix);
    add_m(&CNMutableContact_c, "setNameSuffix:", (void *)set_suffix);
    add_m(&CNContact_c, "previousFamilyName", (void *)get_previous);
    add_m(&CNMutableContact_c, "setPreviousFamilyName:", (void *)set_previous);
    add_m(&CNContact_c, "phoneticGivenName", (void *)get_phonetic_given);
    add_m(&CNMutableContact_c, "setPhoneticGivenName:", (void *)set_phonetic_given);
    add_m(&CNContact_c, "phoneticMiddleName", (void *)get_phonetic_middle);
    add_m(&CNMutableContact_c, "setPhoneticMiddleName:", (void *)set_phonetic_middle);
    add_m(&CNContact_c, "phoneticFamilyName", (void *)get_phonetic_family);
    add_m(&CNMutableContact_c, "setPhoneticFamilyName:", (void *)set_phonetic_family);
    add_m(&CNContact_c, "phoneticOrganizationName", (void *)get_phonetic_org);
    add_m(&CNMutableContact_c, "setPhoneticOrganizationName:", (void *)set_phonetic_org);
    add_m(&CNContact_c, "urlAddressString", (void *)get_url);
    add_m(&CNMutableContact_c, "setUrlAddressString:", (void *)set_url);
    add_m(&CNContact_c, "relationLabel", (void *)get_relation);
    add_m(&CNMutableContact_c, "setRelationLabel:", (void *)set_relation);
    add_m(&CNContact_c, "instantMessageUsername", (void *)get_im_user);
    add_m(&CNMutableContact_c, "setInstantMessageUsername:", (void *)set_im_user);
    add_m(&CNContact_c, "instantMessageService", (void *)get_im_service);
    add_m(&CNMutableContact_c, "setInstantMessageService:", (void *)set_im_service);
    add_m(&CNContact_c, "socialProfileUsername", (void *)get_social_user);
    add_m(&CNMutableContact_c, "setSocialProfileUsername:", (void *)set_social_user);
    add_m(&CNContact_c, "socialProfileService", (void *)get_social_service);
    add_m(&CNMutableContact_c, "setSocialProfileService:", (void *)set_social_service);
    add_m(&CNContact_c, "socialProfileURLString", (void *)get_social_url);
    add_m(&CNMutableContact_c, "setSocialProfileURLString:", (void *)set_social_url);
    add_m(&CNContact_c, "socialProfileUserIdentifier", (void *)get_social_id);
    add_m(&CNMutableContact_c, "setSocialProfileUserIdentifier:", (void *)set_social_id);
    add_m(&CNContact_c, "imageDataHex", (void *)get_image);
    add_m(&CNMutableContact_c, "setImageDataHex:", (void *)set_image);
    add_m(&CNContact_c, "imageData", (void *)get_image_data);

    add_m(&CNSaveRequest_c, "addContact:toContainerWithIdentifier:", (void *)req_add);
    add_m(&CNSaveRequest_c, "updateContact:", (void *)req_update);
    add_m(&CNSaveRequest_c, "deleteContact:", (void *)req_delete);
    add_m(&CNContactStore_c, "executeSaveRequest:error:", (void *)store_execute);
    cx_add_cmethod(&CNContactStore_c, "authorizationStatusForEntityType:", (void *)store_auth);
    add_m(&CNContactStore_c, "requestAccessForEntityType:completionHandler:", (void *)store_request_access);
    add_m(&CNContactStore_c, "enumerateContactsWithFetchRequest:error:usingBlock:", (void *)store_enumerate);
    add_m(&CNPostalAddressFormatter_c, "stringForObjectValue:", (void *)fmt_string);
    cx_contacts_ui_init();
}
