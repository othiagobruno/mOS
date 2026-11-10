/* Classes de Contacts que o WhatsApp importa, em cima do store e de um
 * controller que já existe. Não cria janela. */
#include "libc.h"
#include "foundation.h"
#include "cn_store.h"

id objc_autorelease(id o);
id objc_retain(id o);
void objc_release(id o);

cx_class_t CNContactRelation_c __asm__("OBJC_CLASS_$_CNContactRelation");
cx_class_t CNContactRelation_m __asm__("OBJC_METACLASS_$_CNContactRelation");
cx_class_t CNInstantMessageAddress_c __asm__("OBJC_CLASS_$_CNInstantMessageAddress");
cx_class_t CNInstantMessageAddress_m __asm__("OBJC_METACLASS_$_CNInstantMessageAddress");
cx_class_t CNSocialProfile_c __asm__("OBJC_CLASS_$_CNSocialProfile");
cx_class_t CNSocialProfile_m __asm__("OBJC_METACLASS_$_CNSocialProfile");
cx_class_t CNContainer_c __asm__("OBJC_CLASS_$_CNContainer");
cx_class_t CNContainer_m __asm__("OBJC_METACLASS_$_CNContainer");
cx_class_t CNContactsUserDefaults_c __asm__("OBJC_CLASS_$_CNContactsUserDefaults");
cx_class_t CNContactsUserDefaults_m __asm__("OBJC_METACLASS_$_CNContactsUserDefaults");
cx_class_t CNContactPickerViewController_c __asm__("OBJC_CLASS_$_CNContactPickerViewController");
cx_class_t CNContactPickerViewController_m __asm__("OBJC_METACLASS_$_CNContactPickerViewController");
cx_class_t CNContactViewController_c __asm__("OBJC_CLASS_$_CNContactViewController");
cx_class_t CNContactViewController_m __asm__("OBJC_METACLASS_$_CNContactViewController");

enum { UI_N = 16 };

typedef struct {
    id obj;
    int used;
    id a, b, c, d, contact, delegate;
} ui_slot_t;

static ui_slot_t g_ui[UI_N];
static id g_defaults;
static id g_container;

static ui_slot_t *ui_of(id o, int make) {
    ui_slot_t *free_s = 0;
    int i;
    if (!o) return 0;
    for (i = 0; i < UI_N; i++) {
        if (g_ui[i].used && g_ui[i].obj == o) return &g_ui[i];
        if (!g_ui[i].used && !free_s) free_s = &g_ui[i];
    }
    if (!make || !free_s) return 0;
    memset(free_s, 0, sizeof *free_s);
    free_s->used = 1;
    free_s->obj = o;
    return free_s;
}

static id s_ar(const char *s) { return objc_autorelease(cx_str(s ? s : "")); }

static id rel_with(id cls, SEL c, id name) {
    id o = cx_alloc_instance((cx_class_t *)cls);
    ui_slot_t *s = ui_of(o, 1);
    (void)c;
    if (s) s->a = name ? objc_retain(name) : 0;
    return objc_autorelease(o);
}
static id rel_name(id self, SEL c) { ui_slot_t *s = ui_of(self, 0); (void)c; return s ? s->a : 0; }

static id im_with(id cls, SEL c, id user, id service) {
    id o = cx_alloc_instance((cx_class_t *)cls);
    ui_slot_t *s = ui_of(o, 1);
    (void)c;
    if (s) {
        s->a = user ? objc_retain(user) : 0;
        s->b = service ? objc_retain(service) : 0;
    }
    return objc_autorelease(o);
}
static id slot_a(id self, SEL c) { ui_slot_t *s = ui_of(self, 0); (void)c; return s ? s->a : 0; }
static id slot_b(id self, SEL c) { ui_slot_t *s = ui_of(self, 0); (void)c; return s ? s->b : 0; }
static id slot_c(id self, SEL c) { ui_slot_t *s = ui_of(self, 0); (void)c; return s ? s->c : 0; }
static id slot_d(id self, SEL c) { ui_slot_t *s = ui_of(self, 0); (void)c; return s ? s->d : 0; }

static id social_with(id cls, SEL c, id url, id user, id uid, id service) {
    id o = cx_alloc_instance((cx_class_t *)cls);
    ui_slot_t *s = ui_of(o, 1);
    (void)c;
    if (s) {
        s->a = url ? objc_retain(url) : 0;
        s->b = user ? objc_retain(user) : 0;
        s->c = uid ? objc_retain(uid) : 0;
        s->d = service ? objc_retain(service) : 0;
    }
    return objc_autorelease(o);
}

static id container_local(id cls, SEL c) {
    (void)cls; (void)c;
    if (!g_container) g_container = cx_alloc_instance(&CNContainer_c);
    return g_container;
}
static id container_ident(id self, SEL c) { (void)self; (void)c; return s_ar(cx_cn_container_id()); }
static id container_name(id self, SEL c) { (void)self; (void)c; return s_ar(cx_cn_container_name()); }

static id defaults_shared(id cls, SEL c) {
    (void)cls; (void)c;
    if (!g_defaults) g_defaults = cx_alloc_instance(&CNContactsUserDefaults_c);
    return g_defaults;
}
static long defaults_name(id self, SEL c) { (void)self; (void)c; return cx_cn_name_order(); }
static void defaults_set_name(id self, SEL c, long v) { (void)self; (void)c; cx_cn_set_name_order((int)v); }
static long defaults_sort(id self, SEL c) { (void)self; (void)c; return cx_cn_sort_order(); }
static void defaults_set_sort(id self, SEL c, long v) { (void)self; (void)c; cx_cn_set_sort_order((int)v); }

static void pick_set_del(id self, SEL c, id d) {
    ui_slot_t *s = ui_of(self, 1);
    (void)c;
    if (!s) return;
    if (s->delegate) objc_release(s->delegate);
    s->delegate = d ? objc_retain(d) : 0;
}
static id pick_del(id self, SEL c) { ui_slot_t *s = ui_of(self, 0); (void)c; return s ? s->delegate : 0; }

static long pick_count(id self, SEL c) { (void)self; (void)c; return cx_cn_count(); }

static void pick_choose(id self, SEL c, long index) {
    ui_slot_t *s = ui_of(self, 0);
    cx_cn_contact ct;
    id contact;
    (void)c;
    if (!s || !s->delegate) return;
    if (cx_cn_get((int)index, &ct) != CX_CN_OK) return;
    contact = cx_alloc_instance((cx_class_t *)cx_class_named("CNMutableContact"));
    if (contact) {
        MSG(void, contact, "setGivenName:", cx_str(ct.given));
        MSG(void, contact, "setFamilyName:", cx_str(ct.family));
    }
    if (cx_responds(object_getClass(s->delegate), sel_registerName("contactPicker:didSelectContact:")))
        MSG(void, s->delegate, "contactPicker:didSelectContact:", self, contact);
}

static void pick_cancel(id self, SEL c) {
    ui_slot_t *s = ui_of(self, 0);
    (void)c;
    if (!s || !s->delegate) return;
    if (cx_responds(object_getClass(s->delegate), sel_registerName("contactPickerDidCancel:")))
        MSG(void, s->delegate, "contactPickerDidCancel:", self);
}

static void view_set_contact(id self, SEL c, id contact) {
    ui_slot_t *s = ui_of(self, 1);
    (void)c;
    if (!s) return;
    if (s->contact) objc_release(s->contact);
    s->contact = contact ? objc_retain(contact) : 0;
}
static id view_contact(id self, SEL c) { ui_slot_t *s = ui_of(self, 0); (void)c; return s ? s->contact : 0; }

static id view_title(id self, SEL c) {
    ui_slot_t *s = ui_of(self, 0);
    cx_cn_contact ct;
    char buf[160];
    (void)c;
    if (!s || !s->contact) return s_ar("");
    memset(&ct, 0, sizeof ct);
    {
        id g = MSG0(id, s->contact, "givenName");
        id f = MSG0(id, s->contact, "familyName");
        id p = MSG0(id, s->contact, "namePrefix");
        const char *gs = g ? cx_cstr(g) : "";
        const char *fs = f ? cx_cstr(f) : "";
        const char *ps = p ? cx_cstr(p) : "";
        int i;
        for (i = 0; i < 63 && gs && gs[i]; i++) ct.given[i] = gs[i];
        for (i = 0; i < 63 && fs && fs[i]; i++) ct.family[i] = fs[i];
        for (i = 0; i < 31 && ps && ps[i]; i++) ct.prefix[i] = ps[i];
    }
    cx_cn_display_name(&ct, buf, (int)sizeof buf);
    return s_ar(buf);
}

void cx_contacts_ui_init(void) {
    cx_class_t *obj = cx_class_named("NSObject");
    cx_class_t *vc = cx_class_named("NSSplitViewController");
    if (!vc) vc = cx_class_named("NSTitlebarAccessoryViewController");
    if (!vc) vc = obj;
    cx_define_class(&CNContactRelation_c, &CNContactRelation_m, "CNContactRelation", obj, 0);
    cx_define_class(&CNInstantMessageAddress_c, &CNInstantMessageAddress_m, "CNInstantMessageAddress", obj, 0);
    cx_define_class(&CNSocialProfile_c, &CNSocialProfile_m, "CNSocialProfile", obj, 0);
    cx_define_class(&CNContainer_c, &CNContainer_m, "CNContainer", obj, 0);
    cx_define_class(&CNContactsUserDefaults_c, &CNContactsUserDefaults_m, "CNContactsUserDefaults", obj, 0);
    cx_define_class(&CNContactPickerViewController_c, &CNContactPickerViewController_m, "CNContactPickerViewController", vc, 0);
    cx_define_class(&CNContactViewController_c, &CNContactViewController_m, "CNContactViewController", vc, 0);

    cx_add_cmethod(&CNContactRelation_c, "contactRelationWithName:", (void *)rel_with);
    cx_add_method(&CNContactRelation_c, "name", (void *)rel_name);
    cx_add_cmethod(&CNInstantMessageAddress_c, "instantMessageAddressWithUsername:service:", (void *)im_with);
    cx_add_method(&CNInstantMessageAddress_c, "username", (void *)slot_a);
    cx_add_method(&CNInstantMessageAddress_c, "service", (void *)slot_b);
    cx_add_cmethod(&CNSocialProfile_c, "socialProfileWithUrlString:username:userIdentifier:service:", (void *)social_with);
    cx_add_method(&CNSocialProfile_c, "urlString", (void *)slot_a);
    cx_add_method(&CNSocialProfile_c, "username", (void *)slot_b);
    cx_add_method(&CNSocialProfile_c, "userIdentifier", (void *)slot_c);
    cx_add_method(&CNSocialProfile_c, "service", (void *)slot_d);
    cx_add_cmethod(&CNContainer_c, "localContainer", (void *)container_local);
    cx_add_method(&CNContainer_c, "identifier", (void *)container_ident);
    cx_add_method(&CNContainer_c, "name", (void *)container_name);
    cx_add_cmethod(&CNContactsUserDefaults_c, "sharedDefaults", (void *)defaults_shared);
    cx_add_method(&CNContactsUserDefaults_c, "nameOrder", (void *)defaults_name);
    cx_add_method(&CNContactsUserDefaults_c, "setNameOrder:", (void *)defaults_set_name);
    cx_add_method(&CNContactsUserDefaults_c, "sortOrder", (void *)defaults_sort);
    cx_add_method(&CNContactsUserDefaults_c, "setSortOrder:", (void *)defaults_set_sort);
    cx_add_method(&CNContactPickerViewController_c, "setDelegate:", (void *)pick_set_del);
    cx_add_method(&CNContactPickerViewController_c, "delegate", (void *)pick_del);
    cx_add_method(&CNContactPickerViewController_c, "contactCount", (void *)pick_count);
    cx_add_method(&CNContactPickerViewController_c, "selectContactAtIndex:", (void *)pick_choose);
    cx_add_method(&CNContactPickerViewController_c, "cancel:", (void *)pick_cancel);
    cx_add_method(&CNContactViewController_c, "setContact:", (void *)view_set_contact);
    cx_add_method(&CNContactViewController_c, "contact", (void *)view_contact);
    cx_add_method(&CNContactViewController_c, "displayedName", (void *)view_title);
}
