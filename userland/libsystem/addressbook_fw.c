/*
 * mOS libSystem — AddressBook.framework (API antiga dos contatos, ObjC e C) sobre a mesma agenda do Contacts
 * (cn_store.c, arquivo MOSCN1) e a mesma autorização (kTCCServiceAddressBook): o que um app grava pelo AddressBook
 * aparece no Contacts e vice-versa. ABAddressBook +sharedAddressBook pede a permissão no primeiro uso (como o macOS) e
 * devolve nil se negada. Propriedades: nome, sobrenome, nomes fonéticos, prefixo/sufixo, organização, departamento,
 * cargo, e-mail, telefone, endereço, URL, mensageiro, perfil social e UID; multivalores com rótulo (ABMultiValue).
 * A agenda do mOS não tem grupos nem datas de criação: groups é vazio e esses campos são nil.
 * Busca: ABSearchElement com os operadores de comparação de texto do AddressBook.
 */
#include "foundation.h"
#include "libc.h"
#include "cn_store.h"

id objc_autorelease(id o);
id objc_retain(id o);
void objc_release(id o);
int cx_tcc_request(const char *service);
int cx_tcc_state(const char *service);

static id cls(const char *n) { return (id)cx_class_named(n); }
#define ST(T, o) (*(T **)((u8 *)(o) + 8))

/* ---- constantes (os valores do AddressBook do macOS) */
extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
#define CK(sym, val) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)val, sizeof(val) - 1 }; id sym = (id)&sym##_o;
CK(kABFirstNameProperty, "First") CK(kABLastNameProperty, "Last") CK(kABMiddleNameProperty, "Middle")
CK(kABFirstNamePhoneticProperty, "FirstPhonetic") CK(kABLastNamePhoneticProperty, "LastPhonetic")
CK(kABMiddleNamePhoneticProperty, "MiddlePhonetic") CK(kABTitleProperty, "Title") CK(kABSuffixProperty, "Suffix")
CK(kABNicknameProperty, "Nickname") CK(kABOrganizationProperty, "Organization") CK(kABDepartmentProperty, "ABDepartment")
CK(kABJobTitleProperty, "JobTitle") CK(kABEmailProperty, "Email") CK(kABPhoneProperty, "Phone") CK(kABAddressProperty, "Address")
CK(kABURLsProperty, "URLs") CK(kABHomePageProperty, "HomePage") CK(kABInstantMessageProperty, "InstantMessage")
CK(kABSocialProfileProperty, "SocialProfile") CK(kABNoteProperty, "Note") CK(kABBirthdayProperty, "Birthday")
CK(kABOtherDatesProperty, "ABOtherDates") CK(kABUIDProperty, "UID") CK(kABModificationDateProperty, "Modification")
CK(kABCreationDateProperty, "Creation") CK(kABGroupNameProperty, "GroupName") CK(kABPersonFlags, "ABPersonFlags")
CK(kABPersonRecordType, "ABPerson") CK(kABGroupRecordType, "ABGroup")
CK(kABPhoneWorkLabel, "_$!<Work>!$_") CK(kABPhoneHomeLabel, "_$!<Home>!$_") CK(kABPhoneMobileLabel, "_$!<Mobile>!$_")
CK(kABPhonePagerLabel, "_$!<Pager>!$_") CK(kABPhoneWorkFAXLabel, "_$!<WorkFAX>!$_") CK(kABAddressHomeLabel, "_$!<Home>!$_")
CK(kABAddressWorkLabel, "_$!<Work>!$_") CK(kABAnniversaryLabel, "_$!<Anniversary>!$_") CK(kABOtherLabel, "_$!<Other>!$_")
CK(kABAddressStreetKey, "Street") CK(kABAddressCityKey, "City") CK(kABAddressStateKey, "State") CK(kABAddressZIPKey, "ZIP")
CK(kABAddressCountryKey, "Country") CK(kABAddressCountryCodeKey, "CountryCode")
CK(kABInstantMessageServiceKey, "InstantMessageService") CK(kABInstantMessageUsernameKey, "username")
CK(kABInstantMessageServiceAIM, "AIMInstant") CK(kABInstantMessageServiceFacebook, "FacebookInstant")
CK(kABInstantMessageServiceGaduGadu, "GaduGaduInstant") CK(kABInstantMessageServiceGoogleTalk, "GoogleTalkInstant")
CK(kABInstantMessageServiceICQ, "ICQInstant") CK(kABInstantMessageServiceJabber, "JabberInstant")
CK(kABInstantMessageServiceMSN, "MSNInstant") CK(kABInstantMessageServiceQQ, "QQInstant")
CK(kABInstantMessageServiceSkype, "SkypeInstant") CK(kABInstantMessageServiceYahoo, "YahooInstant")
CK(kABSocialProfileServiceKey, "service") CK(kABSocialProfileUsernameKey, "username") CK(kABSocialProfileURLKey, "url")
CK(kABSocialProfileServiceTwitter, "twitter") CK(kABSocialProfileServiceFacebook, "facebook")
CK(kABSocialProfileServiceLinkedIn, "linkedin") CK(kABSocialProfileServiceFlickr, "flickr")
CK(kABSocialProfileServiceMySpace, "myspace") CK(kABSocialProfileServiceSinaWeibo, "sinaweibo")
CK(kABDatabaseChangedNotification, "ABDatabaseChangedNotification")
CK(kABDatabaseChangedExternallyNotification, "ABDatabaseChangedExternallyNotification")
CK(kABInsertedRecords, "ABInsertedRecords") CK(kABUpdatedRecords, "ABUpdatedRecords") CK(kABDeletedRecords, "ABDeletedRecords")

/* tipos de propriedade (ABTypedefs.h) */
enum { kABStringProperty = 1, kABIntegerProperty = 2, kABRealProperty = 3, kABDateProperty = 4, kABArrayProperty = 5,
       kABDictionaryProperty = 6, kABDataProperty = 7, kABMultiValueMask = 0x100 };

#define DEF(name) cx_class_t name##_c __asm__("OBJC_CLASS_$_" #name); cx_class_t name##_m __asm__("OBJC_METACLASS_$_" #name);
DEF(ABAddressBook) DEF(ABRecord) DEF(ABPerson) DEF(ABGroup) DEF(ABMultiValue) DEF(ABMutableMultiValue) DEF(ABSearchElement)
DEF(ABPeoplePickerView)

/* ================================================================ multivalor */
typedef struct { id values, labels, ids; long type; } mv_t;
static mv_t *mv(id s) { mv_t *m = ST(mv_t, s); if (!m) { m = calloc(1, sizeof *m); ST(mv_t, s) = m; } return m; }
static id arr(void) { return objc_retain(MSG0(id, cls("NSMutableArray"), "array")); }
static id mv_new(cx_class_t *k, long type) {
    id o = cx_alloc_instance(k);
    mv_t *m = mv(o);
    m->values = arr(); m->labels = arr(); m->ids = arr(); m->type = type;
    return o;
}
static void mv_add(id o, id value, id label) {
    mv_t *m = mv(o);
    if (!value) return;
    MSG(void, m->values, "addObject:", value);
    MSG(void, m->labels, "addObject:", label ? label : kABOtherLabel);
    char b[48];
    snprintf(b, sizeof b, "%lu-%p", (unsigned long)MSG0(u64, m->ids, "count"), (void *)o);
    MSG(void, m->ids, "addObject:", cx_str(b));
}
static u64 mv_count(id s, SEL c) { (void)c; mv_t *m = mv(s); return m->values ? MSG0(u64, m->values, "count") : 0; }
static id mv_value(id s, SEL c, u64 i) { (void)c; return i < mv_count(s, 0) ? MSG(id, mv(s)->values, "objectAtIndex:", i) : 0; }
static id mv_label(id s, SEL c, u64 i) { (void)c; return i < mv_count(s, 0) ? MSG(id, mv(s)->labels, "objectAtIndex:", i) : 0; }
static id mv_ident(id s, SEL c, u64 i) { (void)c; return i < mv_count(s, 0) ? MSG(id, mv(s)->ids, "objectAtIndex:", i) : 0; }
static id mv_primary(id s, SEL c) { (void)c; return mv_count(s, 0) ? mv_ident(s, 0, 0) : 0; }
static u64 mv_index_for(id s, SEL c, id ident) { (void)c; u64 i = mv(s)->ids ? MSG(u64, mv(s)->ids, "indexOfObject:", ident) : (u64)-1; return i; }
static long mv_type(id s, SEL c) { (void)c; return mv(s)->type; }
static id mv_add_m(id s, SEL c, id v, id l) { (void)c; mv_add(s, v, l); return mv_ident(s, 0, mv_count(s, 0) - 1); }
static BOOL mv_remove(id s, SEL c, u64 i) {
    (void)c;
    if (i >= mv_count(s, 0)) return NO;
    mv_t *m = mv(s);
    MSG(void, m->values, "removeObjectAtIndex:", i); MSG(void, m->labels, "removeObjectAtIndex:", i); MSG(void, m->ids, "removeObjectAtIndex:", i);
    return YES;
}
static BOOL mv_replace_v(id s, SEL c, id v, u64 i) { (void)c; if (i >= mv_count(s, 0) || !v) return NO; MSG(void, mv(s)->values, "replaceObjectAtIndex:withObject:", i, v); return YES; }
static BOOL mv_replace_l(id s, SEL c, id l, u64 i) { (void)c; if (i >= mv_count(s, 0) || !l) return NO; MSG(void, mv(s)->labels, "replaceObjectAtIndex:withObject:", i, l); return YES; }
static id mv_copy(id s, SEL c, void *zone) {
    (void)c; (void)zone;
    id o = mv_new(&ABMultiValue_c, mv(s)->type);
    for (u64 i = 0; i < mv_count(s, 0); i++) mv_add(o, mv_value(s, 0, i), mv_label(s, 0, i));
    return o;
}
static id mv_init(id s, SEL c) {
    (void)c;
    mv_t *m = mv(s);
    if (!m->values) { m->values = arr(); m->labels = arr(); m->ids = arr(); m->type = kABMultiValueMask; }
    return s;
}
static id mv_mcopy(id s, SEL c, void *zone) {
    (void)c; (void)zone;
    id o = mv_new(&ABMutableMultiValue_c, mv(s)->type);
    for (u64 i = 0; i < mv_count(s, 0); i++) mv_add(o, mv_value(s, 0, i), mv_label(s, 0, i));
    return o;
}

/* ================================================================ registro (pessoa) */
typedef struct { cx_cn_contact c; int is_new, dirty, deleted; id extra; } rec_t;   /* extra: propriedades fora da agenda */
static rec_t *rec(id s) { rec_t *r = ST(rec_t, s); if (!r) { r = calloc(1, sizeof *r); ST(rec_t, s) = r; } return r; }
static id str_or_nil(const char *s) { return s && s[0] ? cx_str(s) : 0; }
static void put(char *dst, size_t cap, id v) {
    const char *s = v && cx_is_str(v) ? cx_cstr(v) : "";
    size_t n = strlen(s);
    if (n >= cap) n = cap - 1;
    memcpy(dst, s, n);
    dst[n] = 0;
}
static const struct { const char *prop; size_t off, cap; } simple[] = {
    {"First", offsetof(cx_cn_contact, given), 64}, {"Last", offsetof(cx_cn_contact, family), 64},
    {"Organization", offsetof(cx_cn_contact, org), 80}, {"Title", offsetof(cx_cn_contact, prefix), 32},
    {"Suffix", offsetof(cx_cn_contact, suffix), 32}, {"FirstPhonetic", offsetof(cx_cn_contact, phonetic_given), 64},
    {"MiddlePhonetic", offsetof(cx_cn_contact, phonetic_middle), 64}, {"LastPhonetic", offsetof(cx_cn_contact, phonetic_family), 64},
    {"UID", offsetof(cx_cn_contact, id), 40},
};
static id rec_value(id s, SEL c, id prop) {
    (void)c;
    rec_t *r = rec(s);
    const char *p = prop && cx_is_str(prop) ? cx_cstr(prop) : "";
    for (unsigned i = 0; i < sizeof simple / sizeof simple[0]; i++)
        if (!strcmp(p, simple[i].prop)) return str_or_nil((char *)&r->c + simple[i].off);
    if (!strcmp(p, "Email") && r->c.email[0]) { id m = mv_new(&ABMultiValue_c, kABMultiValueMask | kABStringProperty); mv_add(m, cx_str(r->c.email), kABPhoneWorkLabel); return objc_autorelease(m); }
    if (!strcmp(p, "Phone") && r->c.phone[0]) { id m = mv_new(&ABMultiValue_c, kABMultiValueMask | kABStringProperty); mv_add(m, cx_str(r->c.phone), kABPhoneMobileLabel); return objc_autorelease(m); }
    if (!strcmp(p, "URLs") && r->c.url[0]) { id m = mv_new(&ABMultiValue_c, kABMultiValueMask | kABStringProperty); mv_add(m, cx_str(r->c.url), kABOtherLabel); return objc_autorelease(m); }
    if (!strcmp(p, "HomePage")) return str_or_nil(r->c.url);
    if (!strcmp(p, "Address") && (r->c.street[0] || r->c.city[0] || r->c.zip[0] || r->c.country[0])) {
        id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
        const char *kv[][2] = {{"Street", r->c.street}, {"City", r->c.city}, {"State", r->c.state}, {"ZIP", r->c.zip}, {"Country", r->c.country}, {"CountryCode", r->c.iso}};
        for (int i = 0; i < 6; i++) if (kv[i][1][0]) MSG(void, d, "setObject:forKey:", cx_str(kv[i][1]), cx_str(kv[i][0]));
        id m = mv_new(&ABMultiValue_c, kABMultiValueMask | kABDictionaryProperty);
        mv_add(m, d, kABAddressHomeLabel);
        return objc_autorelease(m);
    }
    if (!strcmp(p, "InstantMessage") && r->c.im_user[0]) {
        id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
        MSG(void, d, "setObject:forKey:", cx_str(r->c.im_user), kABInstantMessageUsernameKey);
        if (r->c.im_service[0]) MSG(void, d, "setObject:forKey:", cx_str(r->c.im_service), kABInstantMessageServiceKey);
        id m = mv_new(&ABMultiValue_c, kABMultiValueMask | kABDictionaryProperty);
        mv_add(m, d, kABOtherLabel);
        return objc_autorelease(m);
    }
    if (!strcmp(p, "SocialProfile") && r->c.social_user[0]) {
        id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
        MSG(void, d, "setObject:forKey:", cx_str(r->c.social_user), kABSocialProfileUsernameKey);
        if (r->c.social_service[0]) MSG(void, d, "setObject:forKey:", cx_str(r->c.social_service), kABSocialProfileServiceKey);
        if (r->c.social_url[0]) MSG(void, d, "setObject:forKey:", cx_str(r->c.social_url), kABSocialProfileURLKey);
        id m = mv_new(&ABMultiValue_c, kABMultiValueMask | kABDictionaryProperty);
        mv_add(m, d, kABOtherLabel);
        return objc_autorelease(m);
    }
    return r->extra ? MSG(id, r->extra, "objectForKey:", prop) : 0;   /* nota, cargo, apelido...: só nesta sessão */
}
static id mv_first(id v, id key) {
    if (!v) return 0;
    if (cx_is_str(v)) return v;
    if (object_getClass(v) == &ABMultiValue_c || object_getClass(v) == &ABMutableMultiValue_c) v = mv_count(v, 0) ? mv_value(v, 0, 0) : 0;
    if (v && key && !cx_is_str(v) && MSG(BOOL, v, "respondsToSelector:", sel_registerName("objectForKey:"))) v = MSG(id, v, "objectForKey:", key);
    return v;
}
static BOOL rec_set(id s, SEL c, id value, id prop) {
    (void)c;
    rec_t *r = rec(s);
    const char *p = prop && cx_is_str(prop) ? cx_cstr(prop) : "";
    for (unsigned i = 0; i < sizeof simple / sizeof simple[0]; i++)
        if (!strcmp(p, simple[i].prop)) {
            if (!strcmp(p, "UID")) return NO;                                   /* o identificador é da agenda */
            put((char *)&r->c + simple[i].off, simple[i].cap, value);
            r->dirty = 1;
            return YES;
        }
    if (!strcmp(p, "Email")) { put(r->c.email, sizeof r->c.email, mv_first(value, 0)); r->dirty = 1; return YES; }
    if (!strcmp(p, "Phone")) { put(r->c.phone, sizeof r->c.phone, mv_first(value, 0)); r->dirty = 1; return YES; }
    if (!strcmp(p, "URLs") || !strcmp(p, "HomePage")) { put(r->c.url, sizeof r->c.url, mv_first(value, 0)); r->dirty = 1; return YES; }
    if (!strcmp(p, "Address")) {
        put(r->c.street, sizeof r->c.street, mv_first(value, kABAddressStreetKey)); put(r->c.city, sizeof r->c.city, mv_first(value, kABAddressCityKey));
        put(r->c.state, sizeof r->c.state, mv_first(value, kABAddressStateKey)); put(r->c.zip, sizeof r->c.zip, mv_first(value, kABAddressZIPKey));
        put(r->c.country, sizeof r->c.country, mv_first(value, kABAddressCountryKey)); put(r->c.iso, sizeof r->c.iso, mv_first(value, kABAddressCountryCodeKey));
        r->dirty = 1;
        return YES;
    }
    if (!strcmp(p, "InstantMessage")) {
        put(r->c.im_user, sizeof r->c.im_user, mv_first(value, kABInstantMessageUsernameKey));
        put(r->c.im_service, sizeof r->c.im_service, mv_first(value, kABInstantMessageServiceKey));
        r->dirty = 1;
        return YES;
    }
    if (!strcmp(p, "SocialProfile")) {
        put(r->c.social_user, sizeof r->c.social_user, mv_first(value, kABSocialProfileUsernameKey));
        put(r->c.social_service, sizeof r->c.social_service, mv_first(value, kABSocialProfileServiceKey));
        put(r->c.social_url, sizeof r->c.social_url, mv_first(value, kABSocialProfileURLKey));
        r->dirty = 1;
        return YES;
    }
    if (!r->extra) r->extra = objc_retain(MSG0(id, cls("NSMutableDictionary"), "dictionary"));
    if (value) MSG(void, r->extra, "setObject:forKey:", value, prop); else MSG(void, r->extra, "removeObjectForKey:", prop);
    return YES;
}
static BOOL rec_remove_value(id s, SEL c, id prop) { return rec_set(s, c, 0, prop); }
static id rec_uid(id s, SEL c) { (void)c; return str_or_nil(rec(s)->c.id); }
static id rec_init(id s, SEL c) { (void)c; rec(s)->is_new = 1; return s; }
static id rec_parents(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSArray"), "array"); }   /* a agenda não tem grupos */
static BOOL rec_is_readonly(id s, SEL c) { (void)s; (void)c; return NO; }

/* ================================================================ busca */
enum { kABEqual, kABNotEqual, kABLessThan, kABLessThanOrEqual, kABGreaterThan, kABGreaterThanOrEqual, kABEqualCaseInsensitive,
       kABContainsSubString, kABContainsSubStringCaseInsensitive, kABPrefixMatch, kABPrefixMatchCaseInsensitive,
       kABBitsInBitFieldMatch, kABDoesNotContainSubString, kABDoesNotContainSubStringCaseInsensitive, kABNotEqualCaseInsensitive,
       kABSuffixMatch, kABSuffixMatchCaseInsensitive };
typedef struct { id prop, label, key, value; long cmp; } se_t;
static id se_make(id k, SEL c, id prop, id label, id key, id value, long cmp) {
    (void)k; (void)c;
    id o = cx_alloc_instance(&ABSearchElement_c);
    se_t *e = calloc(1, sizeof *e);
    e->prop = prop ? objc_retain(prop) : 0; e->label = label ? objc_retain(label) : 0; e->key = key ? objc_retain(key) : 0;
    e->value = value ? objc_retain(value) : 0; e->cmp = cmp;
    ST(se_t, o) = e;
    return objc_autorelease(o);
}
static long text_cmp(id a, id b, int ci) { return (long)MSG(long, a, "compare:options:", b, (u64)(ci ? 1 : 0)); }
static BOOL test_one(id v, se_t *e) {
    if (!v || !e->value) return e->cmp == kABNotEqual || e->cmp == kABDoesNotContainSubString || e->cmp == kABDoesNotContainSubStringCaseInsensitive;
    if (!cx_is_str(v) || !cx_is_str(e->value)) return MSG(BOOL, v, "isEqual:", e->value) == (e->cmp != kABNotEqual);
    u64 ci = 1, none = 0;
    switch (e->cmp) {
    case kABEqual: return text_cmp(v, e->value, 0) == 0;
    case kABNotEqual: return text_cmp(v, e->value, 0) != 0;
    case kABEqualCaseInsensitive: return text_cmp(v, e->value, 1) == 0;
    case kABNotEqualCaseInsensitive: return text_cmp(v, e->value, 1) != 0;
    case kABLessThan: return text_cmp(v, e->value, 0) < 0;
    case kABLessThanOrEqual: return text_cmp(v, e->value, 0) <= 0;
    case kABGreaterThan: return text_cmp(v, e->value, 0) > 0;
    case kABGreaterThanOrEqual: return text_cmp(v, e->value, 0) >= 0;
    case kABContainsSubString: case kABDoesNotContainSubString: case kABContainsSubStringCaseInsensitive: case kABDoesNotContainSubStringCaseInsensitive: {
        int ins = e->cmp == kABContainsSubStringCaseInsensitive || e->cmp == kABDoesNotContainSubStringCaseInsensitive;
        typedef struct { u64 loc, len; } rg_t;
        rg_t r = MSG(rg_t, v, "rangeOfString:options:", e->value, ins ? ci : none);
        int found = r.loc != (u64)0x7fffffffffffffffULL;
        return e->cmp == kABContainsSubString || e->cmp == kABContainsSubStringCaseInsensitive ? found : !found;
    }
    case kABPrefixMatch: return MSG(BOOL, v, "hasPrefix:", e->value);
    case kABPrefixMatchCaseInsensitive: return MSG(BOOL, MSG0(id, v, "lowercaseString"), "hasPrefix:", MSG0(id, e->value, "lowercaseString"));
    case kABSuffixMatch: return MSG(BOOL, v, "hasSuffix:", e->value);
    case kABSuffixMatchCaseInsensitive: return MSG(BOOL, MSG0(id, v, "lowercaseString"), "hasSuffix:", MSG0(id, e->value, "lowercaseString"));
    }
    return NO;
}
static BOOL se_matches(id s, SEL c, id record) {
    (void)c;
    se_t *e = ST(se_t, s);
    if (!e || !record) return NO;
    id v = rec_value(record, 0, e->prop);
    if (v && (object_getClass(v) == &ABMultiValue_c)) {
        for (u64 i = 0; i < mv_count(v, 0); i++) {
            if (e->label && !MSG(BOOL, mv_label(v, 0, i), "isEqual:", e->label)) continue;
            id x = mv_value(v, 0, i);
            if (e->key && !cx_is_str(x)) x = MSG(id, x, "objectForKey:", e->key);
            if (test_one(x, e)) return YES;
        }
        return NO;
    }
    return test_one(v, e);
}

/* ================================================================ o livro */
typedef struct { id people; int loaded; } book_t;
static id g_book;
static int access_ok(void) {
    int st = cx_cn_auth();
    if (st == CX_CN_NOT_DETERMINED) { cx_tcc_request("kTCCServiceAddressBook"); st = cx_cn_auth(); }
    return st == CX_CN_AUTHORIZED;
}
static book_t *bk(id s) { return ST(book_t, s); }
static void load(id s) {
    book_t *b = bk(s);
    if (b->loaded) return;
    b->loaded = 1;
    int n = cx_cn_count();
    for (int i = 0; i < n; i++) {
        id p = cx_alloc_instance(&ABPerson_c);
        rec_t *r = rec(p);
        if (cx_cn_get(i, &r->c) != CX_CN_OK) { objc_release(p); continue; }
        MSG(void, b->people, "addObject:", p);
        objc_release(p);
    }
}
static id book_shared(id k, SEL c) {
    (void)k; (void)c;
    if (!access_ok()) return 0;                                                 /* negado: nil, como no macOS */
    if (!g_book) {
        g_book = cx_alloc_instance(&ABAddressBook_c);
        book_t *b = calloc(1, sizeof *b);
        b->people = arr();
        ST(book_t, g_book) = b;
    }
    return g_book;
}
static id book_people(id s, SEL c) {
    (void)c;
    load(s);
    id out = MSG0(id, cls("NSMutableArray"), "array");
    id all = bk(s)->people;
    for (u64 i = 0; i < MSG0(u64, all, "count"); i++) { id p = MSG(id, all, "objectAtIndex:", i); if (!rec(p)->deleted) MSG(void, out, "addObject:", p); }
    return out;
}
static id book_groups(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSArray"), "array"); }
static id book_me(id s, SEL c) { (void)s; (void)c; return 0; }                    /* a agenda do mOS não marca o "eu" */
static id book_record_for(id s, SEL c, id uid) {
    (void)c;
    id all = book_people(s, 0);
    for (u64 i = 0; uid && i < MSG0(u64, all, "count"); i++) {
        id p = MSG(id, all, "objectAtIndex:", i);
        if (!strcmp(rec(p)->c.id, cx_cstr(uid))) return p;
    }
    return 0;
}
static BOOL book_add(id s, SEL c, id record) {
    (void)c;
    if (!record || object_getClass(record) != &ABPerson_c) return NO;           /* grupos não existem na agenda do mOS */
    load(s);
    rec_t *r = rec(record);
    r->is_new = 1; r->dirty = 1; r->deleted = 0;
    if (!MSG(BOOL, bk(s)->people, "containsObject:", record)) MSG(void, bk(s)->people, "addObject:", record);
    return YES;
}
static BOOL book_remove(id s, SEL c, id record) {
    (void)s; (void)c;
    if (!record || object_getClass(record) != &ABPerson_c) return NO;
    rec(record)->deleted = 1;
    return YES;
}
static BOOL book_has_changes(id s, SEL c) {
    (void)c;
    id all = bk(s)->people;
    for (u64 i = 0; i < MSG0(u64, all, "count"); i++) { rec_t *r = rec(MSG(id, all, "objectAtIndex:", i)); if (r->dirty || r->deleted) return YES; }
    return NO;
}
static BOOL book_save(id s, SEL c) {
    (void)c;
    id all = bk(s)->people;
    cx_cn_begin();
    for (u64 i = 0; i < MSG0(u64, all, "count"); i++) {
        rec_t *r = rec(MSG(id, all, "objectAtIndex:", i));
        if (r->deleted) { if (!r->is_new && r->c.id[0]) cx_cn_stage_delete(r->c.id); }
        else if (r->dirty && r->is_new) { char nid[40]; if (cx_cn_stage_add(&r->c, nid) == CX_CN_OK) memcpy(r->c.id, nid, 40); }
        else if (r->dirty) cx_cn_stage_update(&r->c);
    }
    if (cx_cn_commit() != CX_CN_OK) return NO;
    for (u64 i = 0; i < MSG0(u64, all, "count");) {
        id p = MSG(id, all, "objectAtIndex:", i);
        rec_t *r = rec(p);
        if (r->deleted) { MSG(void, all, "removeObjectAtIndex:", i); continue; }
        r->dirty = r->is_new = 0;
        i++;
    }
    id nc = MSG0(id, cls("NSNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:", kABDatabaseChangedNotification, s);
    return YES;
}
static BOOL book_save_err(id s, SEL c, id *e) { BOOL ok = book_save(s, c); if (e) *e = 0; return ok; }
static id book_search(id s, SEL c, id elem) {
    (void)c;
    id out = MSG0(id, cls("NSMutableArray"), "array"), all = book_people(s, 0);
    for (u64 i = 0; i < MSG0(u64, all, "count"); i++) { id p = MSG(id, all, "objectAtIndex:", i); if (se_matches(elem, 0, p)) MSG(void, out, "addObject:", p); }
    return out;
}

/* ================================================================ API em C (ABAddressBookC.h) */
id ABGetSharedAddressBook(void) { return book_shared(0, 0); }
id ABCopyArrayOfAllPeople(id book) { return book ? objc_retain(book_people(book, 0)) : 0; }
id ABCopyArrayOfAllGroups(id book) { return book ? objc_retain(book_groups(book, 0)) : 0; }
id ABRecordCopyValue(id record, id prop) { id v = record ? rec_value(record, 0, prop) : 0; return v ? objc_retain(v) : 0; }
u8 ABRecordSetValue(id record, id prop, id value) { return record && rec_set(record, 0, value, prop); }
u8 ABSave(id book) { return book && book_save(book, 0); }
long ABMultiValueCount(id mvv) { return mvv ? (long)mv_count(mvv, 0) : 0; }
id ABMultiValueCopyValueAtIndex(id mvv, long i) { id v = mvv && i >= 0 ? mv_value(mvv, 0, (u64)i) : 0; return v ? objc_retain(v) : 0; }
id ABMultiValueCopyLabelAtIndex(id mvv, long i) { id v = mvv && i >= 0 ? mv_label(mvv, 0, (u64)i) : 0; return v ? objc_retain(v) : 0; }
long ABMultiValuePropertyType(id mvv) { return mvv ? mv(mvv)->type : 0; }
long ABTypeOfProperty(id book, id recordType, id prop) {
    (void)book; (void)recordType;
    const char *p = prop && cx_is_str(prop) ? cx_cstr(prop) : "";
    if (!strcmp(p, "Email") || !strcmp(p, "Phone") || !strcmp(p, "URLs")) return kABMultiValueMask | kABStringProperty;
    if (!strcmp(p, "Address") || !strcmp(p, "InstantMessage") || !strcmp(p, "SocialProfile")) return kABMultiValueMask | kABDictionaryProperty;
    if (!strcmp(p, "Birthday") || !strcmp(p, "Modification") || !strcmp(p, "Creation")) return kABDateProperty;
    if (!strcmp(p, "ABOtherDates")) return kABMultiValueMask | kABDateProperty;
    if (!strcmp(p, "ABPersonFlags")) return kABIntegerProperty;
    return kABStringProperty;
}
id ABCopyArrayOfPropertiesForRecordType(id book, id recordType) {
    (void)book;
    const char *t = recordType && cx_is_str(recordType) ? cx_cstr(recordType) : "";
    if (!strcmp(t, "ABGroup")) return objc_retain(MSG(id, cls("NSArray"), "arrayWithObjects:count:", ((id[]){kABGroupNameProperty, kABUIDProperty}), (u64)2));
    id p[] = {kABFirstNameProperty, kABLastNameProperty, kABMiddleNameProperty, kABFirstNamePhoneticProperty, kABLastNamePhoneticProperty,
              kABMiddleNamePhoneticProperty, kABTitleProperty, kABSuffixProperty, kABNicknameProperty, kABOrganizationProperty,
              kABDepartmentProperty, kABJobTitleProperty, kABEmailProperty, kABPhoneProperty, kABAddressProperty, kABURLsProperty,
              kABInstantMessageProperty, kABSocialProfileProperty, kABNoteProperty, kABBirthdayProperty, kABUIDProperty};
    return objc_retain(MSG(id, cls("NSArray"), "arrayWithObjects:count:", p, (u64)(sizeof p / sizeof p[0])));
}
id ABGroupCopyArrayOfAllMembers(id group) { (void)group; return objc_retain(MSG0(id, cls("NSArray"), "array")); }
/* rótulos "_$!<Work>!$_" viram "Work"; propriedades viram o nome em português */
id ABCopyLocalizedPropertyOrLabel(id key) {
    const char *k = key && cx_is_str(key) ? cx_cstr(key) : 0;
    if (!k) return 0;
    static const char *const map[][2] = {{"First", "Nome"}, {"Last", "Sobrenome"}, {"Email", "E-mail"}, {"Phone", "Telefone"},
        {"Address", "Endereço"}, {"Organization", "Empresa"}, {"JobTitle", "Cargo"}, {"Note", "Nota"}, {"Birthday", "Aniversário"},
        {"_$!<Work>!$_", "trabalho"}, {"_$!<Home>!$_", "casa"}, {"_$!<Mobile>!$_", "celular"}, {"_$!<Pager>!$_", "pager"},
        {"_$!<WorkFAX>!$_", "fax do trabalho"}, {"_$!<Other>!$_", "outro"}, {"_$!<Anniversary>!$_", "aniversário de casamento"}};
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) if (!strcmp(map[i][0], k)) return objc_retain(cx_str(map[i][1]));
    return objc_retain(key);
}
id ABLocalizedPropertyOrLabel(id key) { id v = ABCopyLocalizedPropertyOrLabel(key); return v ? objc_autorelease(v) : 0; }
static id loc_m(id k, SEL c, id key) { (void)k; (void)c; return ABLocalizedPropertyOrLabel(key); }

void cx_addressbook_init(void) {
    cx_class_t *ns = cx_class_named("NSObject");
    if (cx_class_named("ABAddressBook")) return;
    cx_define_class(&ABAddressBook_c, &ABAddressBook_m, "ABAddressBook", ns, 8);
    cx_add_cmethod(&ABAddressBook_c, "sharedAddressBook", (void *)book_shared);
    cx_add_cmethod(&ABAddressBook_c, "addressBook", (void *)book_shared);
    cx_add_method(&ABAddressBook_c, "people", (void *)book_people);
    cx_add_method(&ABAddressBook_c, "groups", (void *)book_groups);
    cx_add_method(&ABAddressBook_c, "me", (void *)book_me);
    cx_add_method(&ABAddressBook_c, "recordForUniqueId:", (void *)book_record_for);
    cx_add_method(&ABAddressBook_c, "addRecord:", (void *)book_add);
    cx_add_method(&ABAddressBook_c, "addRecord:error:", (void *)book_add);
    cx_add_method(&ABAddressBook_c, "removeRecord:", (void *)book_remove);
    cx_add_method(&ABAddressBook_c, "removeRecord:error:", (void *)book_remove);
    cx_add_method(&ABAddressBook_c, "save", (void *)book_save);
    cx_add_method(&ABAddressBook_c, "saveAndReturnError:", (void *)book_save_err);
    cx_add_method(&ABAddressBook_c, "hasUnsavedChanges", (void *)book_has_changes);
    cx_add_method(&ABAddressBook_c, "recordsMatchingSearchElement:", (void *)book_search);
    cx_add_cmethod(&ABAddressBook_c, "localizedPropertyOrLabel:", (void *)loc_m);
    cx_define_class(&ABRecord_c, &ABRecord_m, "ABRecord", ns, 8);
    cx_add_method(&ABRecord_c, "init", (void *)rec_init);
    cx_add_method(&ABRecord_c, "valueForProperty:", (void *)rec_value);
    cx_add_method(&ABRecord_c, "setValue:forProperty:", (void *)rec_set);
    cx_add_method(&ABRecord_c, "setValue:forProperty:error:", (void *)rec_set);
    cx_add_method(&ABRecord_c, "removeValueForProperty:", (void *)rec_remove_value);
    cx_add_method(&ABRecord_c, "uniqueId", (void *)rec_uid);
    cx_add_method(&ABRecord_c, "isReadOnly", (void *)rec_is_readonly);
    cx_define_class(&ABPerson_c, &ABPerson_m, "ABPerson", &ABRecord_c, 8);
    cx_add_method(&ABPerson_c, "parentGroups", (void *)rec_parents);
    cx_add_cmethod(&ABPerson_c, "searchElementForProperty:label:key:value:comparison:", (void *)se_make);
    cx_define_class(&ABGroup_c, &ABGroup_m, "ABGroup", &ABRecord_c, 8);
    cx_add_method(&ABGroup_c, "members", (void *)rec_parents);
    cx_add_method(&ABGroup_c, "subgroups", (void *)rec_parents);
    cx_define_class(&ABMultiValue_c, &ABMultiValue_m, "ABMultiValue", ns, 8);
    cx_add_method(&ABMultiValue_c, "count", (void *)mv_count);
    cx_add_method(&ABMultiValue_c, "valueAtIndex:", (void *)mv_value);
    cx_add_method(&ABMultiValue_c, "labelAtIndex:", (void *)mv_label);
    cx_add_method(&ABMultiValue_c, "identifierAtIndex:", (void *)mv_ident);
    cx_add_method(&ABMultiValue_c, "indexForIdentifier:", (void *)mv_index_for);
    cx_add_method(&ABMultiValue_c, "primaryIdentifier", (void *)mv_primary);
    cx_add_method(&ABMultiValue_c, "propertyType", (void *)mv_type);
    cx_add_method(&ABMultiValue_c, "copyWithZone:", (void *)mv_copy);
    cx_add_method(&ABMultiValue_c, "mutableCopyWithZone:", (void *)mv_mcopy);
    cx_define_class(&ABMutableMultiValue_c, &ABMutableMultiValue_m, "ABMutableMultiValue", &ABMultiValue_c, 8);
    cx_add_method(&ABMutableMultiValue_c, "init", (void *)mv_init);
    cx_add_method(&ABMutableMultiValue_c, "addValue:withLabel:", (void *)mv_add_m);
    cx_add_method(&ABMutableMultiValue_c, "removeValueAndLabelAtIndex:", (void *)mv_remove);
    cx_add_method(&ABMutableMultiValue_c, "replaceValueAtIndex:withValue:", (void *)mv_replace_v);
    cx_add_method(&ABMutableMultiValue_c, "replaceLabelAtIndex:withLabel:", (void *)mv_replace_l);
    cx_define_class(&ABSearchElement_c, &ABSearchElement_m, "ABSearchElement", ns, 8);
    cx_add_method(&ABSearchElement_c, "matchesRecord:", (void *)se_matches);
    cx_class_t *view = cx_class_named("NSView");
    cx_define_class(&ABPeoplePickerView_c, &ABPeoplePickerView_m, "ABPeoplePickerView", view ? view : ns, 8);
}
