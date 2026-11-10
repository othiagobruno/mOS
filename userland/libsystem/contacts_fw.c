/* Contacts: CNContactStore, CNMutableContact, CNSaveRequest e o formatador postal.
 * A agenda é o arquivo de cn_store.c. Não fala com a Apple. */
#include "libc.h"
#include "foundation.h"
#include "cn_store.h"

int cx_tcc_request(const char *service);
int cx_tcc_state(const char *service);
id objc_autorelease(id o);

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

enum { CN_SLOTS = 48, CN_REQ = 16, CN_OP = 32 };

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

static cn_slot_t g_slot[CN_SLOTS];
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
    cn_slot_t *free_s = 0;
    int i;
    if (!o) return 0;
    for (i = 0; i < CN_SLOTS; i++) {
        if (g_slot[i].used && g_slot[i].obj == o) return &g_slot[i];
        if (!g_slot[i].used && !free_s) free_s = &g_slot[i];
    }
    if (!make || !free_s) return 0;
    memset(free_s, 0, sizeof *free_s);
    free_s->used = 1;
    free_s->obj = o;
    return free_s;
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

static void store_request_access(id self, SEL c, long entity, void *block) {
    acc_blk_t *b = block;
    int st;
    (void)self; (void)c; (void)entity;
    st = cx_tcc_request("kTCCServiceAddressBook");
    (void)st;
    if (b && b->invoke) b->invoke(block, cx_cn_auth() == CX_CN_AUTHORIZED ? YES : NO, cx_cn_auth() == CX_CN_AUTHORIZED ? 0 : cn_error());
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

static id fmt_string(id self, SEL c, id obj) {
    cn_slot_t *s = slot_of(obj, 0);
    char buf[400];
    (void)self; (void)c;
    if (!s) return str_ar("");
    cx_cn_format_postal(&s->c, buf, (int)sizeof buf);
    return str_ar(buf);
}

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

    add_m(&CNSaveRequest_c, "addContact:toContainerWithIdentifier:", (void *)req_add);
    add_m(&CNSaveRequest_c, "updateContact:", (void *)req_update);
    add_m(&CNSaveRequest_c, "deleteContact:", (void *)req_delete);
    add_m(&CNContactStore_c, "executeSaveRequest:error:", (void *)store_execute);
    cx_add_cmethod(&CNContactStore_c, "authorizationStatusForEntityType:", (void *)store_auth);
    add_m(&CNContactStore_c, "requestAccessForEntityType:completionHandler:", (void *)store_request_access);
    add_m(&CNContactStore_c, "enumerateContactsWithFetchRequest:error:usingBlock:", (void *)store_enumerate);
    add_m(&CNPostalAddressFormatter_c, "stringForObjectValue:", (void *)fmt_string);
}
