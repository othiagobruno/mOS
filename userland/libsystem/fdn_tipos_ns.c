/*
 * Lado Objective-C de fdn_tipos.c: NSDate, NSUUID, NSString e NSNotification do Foundation do mOS.
 * Objetos novos voltam com +1 (as pontes _bridgeToObjectiveC devolvem owned).
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_tipos.h"

id objc_retain(id o);
void objc_release(id o);
void arc4random_buf(void *buf, size_t n);

void fdn_ns_retain(void *o) { if (o) objc_retain((id)o); }
void fdn_ns_release(void *o) { if (o) objc_release((id)o); }

const char *fdn_ns_utf8(void *s) {
    if (!s) return 0;
    return MSG0(const char *, (id)s, "UTF8String");
}

void *fdn_ns_string(const char *s) { return cx_str(s ? s : ""); }

double fdn_ns_now1970(void) { return cx_now(); }

void fdn_ns_random(void *buf, size_t n) { arc4random_buf(buf, n); }

void *fdn_ns_date_new(double t1970) {
    cx_class_t *c = cx_class_named("NSDate");
    id d = c ? MSG(id, (id)c, "dateWithTimeIntervalSince1970:", t1970) : 0;
    return d ? objc_retain(d) : 0;
}

double fdn_ns_date_t1970(void *date) { return MSG0(double, (id)date, "timeIntervalSince1970"); }

void *fdn_ns_uuid_new(const uint8_t b[16]) {
    cx_class_t *c = cx_class_named("NSUUID");
    id u = c ? MSG0(id, (id)c, "alloc") : 0;
    return u ? MSG(id, u, "initWithUUIDBytes:", b) : 0;
}

/* Pela UUIDString: o -getUUIDBytes: deste Foundation não escreve no buffer do chamador. */
void fdn_ns_uuid_bytes(void *uuid, uint8_t out[16]) {
    id s = MSG0(id, (id)uuid, "UUIDString");
    const char *c = s ? cx_cstr(s) : 0;
    if (!c || fdn_uuid_parse(c, (long)strlen(c), out) != 0) memset(out, 0, 16);
}

void *fdn_ns_note_new(void *name, void *object, void *info) { return cx_note((id)name, (id)object, (id)info); }
void *fdn_ns_note_name(void *note) { return note ? MSG0(id, (id)note, "name") : 0; }
void *fdn_ns_note_info(void *note) { return note ? MSG0(id, (id)note, "userInfo") : 0; }

/* ---- NSURL */
void *fdn_ns_url_string(const char *s) {
    cx_class_t *c = cx_class_named("NSURL");
    id u = c ? MSG(id, (id)c, "URLWithString:", cx_str(s)) : 0;
    return u ? objc_retain(u) : 0;
}

void *fdn_ns_url_file(const char *path) {
    cx_class_t *c = cx_class_named("NSURL");
    id u = c ? MSG(id, (id)c, "fileURLWithPath:", cx_str(path ? path : "")) : 0;
    return u ? objc_retain(u) : 0;
}

const char *fdn_ns_url_part(void *url, int part) {
    static const char *const sel[] = { "path", "absoluteString", "relativeString", "scheme", "host", "user",
                                       "password", "query", "fragment" };
    id s;
    if (!url || part < 0 || part > FDN_URL_FRAGMENT) return 0;
    s = MSG0(id, (id)url, sel[part]);
    return s ? MSG0(const char *, s, "UTF8String") : 0;
}

int fdn_ns_url_isfile(void *url) { return url && MSG0(BOOL, (id)url, "isFileURL"); }

long fdn_ns_url_port(void *url, int *has) {
    id n = url ? MSG0(id, (id)url, "port") : 0;
    *has = n != 0;
    return n ? MSG0(long, n, "longValue") : 0;
}

void *fdn_ns_url_file_op(void *url, int op) {
    id r = 0;
    if (!url) return 0;
    if (op == FDN_URL_OP_RESOLVE) r = MSG0(id, (id)url, "URLByResolvingSymlinksInPath");
    else if (op == FDN_URL_OP_ABSOLUTE) r = MSG0(id, (id)url, "absoluteURL");
    return r ? objc_retain(r) : 0;
}

int fdn_ns_isdir(const char *path) {
    void *d = path && path[0] ? opendir(path) : 0;
    if (!d) return 0;
    closedir(d);
    return 1;
}

/* ---- NSLocale / NSTimeZone */
const char *fdn_ns_loc_current_id(void) {
    cx_class_t *c = cx_class_named("NSLocale");
    id l = c ? MSG0(id, (id)c, "currentLocale") : 0;
    id s = l ? MSG0(id, l, "localeIdentifier") : 0;
    return s ? MSG0(const char *, s, "UTF8String") : "en_US";
}
void *fdn_ns_loc_obj(const char *ident) {
    cx_class_t *c = cx_class_named("NSLocale");
    id l = c ? MSG(id, (id)c, "localeWithLocaleIdentifier:", cx_str(ident ? ident : "")) : 0;
    return l ? objc_retain(l) : 0;
}
const char *fdn_ns_loc_obj_id(void *loc) {
    id s = loc ? MSG0(id, (id)loc, "localeIdentifier") : 0;
    return s ? MSG0(const char *, s, "UTF8String") : 0;
}
static id tz_class(void) { return (id)cx_class_named("NSTimeZone"); }
static void *tz_ret(id t) { return t ? objc_retain(t) : 0; }
void *fdn_ns_tz_current(void) { id c = tz_class(); return c ? tz_ret(MSG0(id, c, "localTimeZone")) : 0; }
void *fdn_ns_tz_with_name(const char *n) { id c = tz_class(); return c ? tz_ret(MSG(id, c, "timeZoneWithName:", cx_str(n))) : 0; }
void *fdn_ns_tz_with_abbr(const char *a) { id c = tz_class(); return c ? tz_ret(MSG(id, c, "timeZoneWithAbbreviation:", cx_str(a))) : 0; }
void *fdn_ns_tz_with_offset(long s) { id c = tz_class(); return c ? tz_ret(MSG(id, c, "timeZoneForSecondsFromGMT:", s)) : 0; }
const char *fdn_ns_tz_name(void *tz) {
    id s = tz ? MSG0(id, (id)tz, "name") : 0;
    return s ? MSG0(const char *, s, "UTF8String") : 0;
}
long fdn_ns_tz_offset(void *tz, double t1970) {
    id d = (id)fdn_ns_date_new(t1970);
    long r = tz && d ? MSG(long, (id)tz, "secondsFromGMTForDate:", d) : 0;
    fdn_ns_release(d);
    return r;
}
const char *fdn_ns_tz_abbr(void *tz, double t1970) {
    id s;
    (void)t1970;
    s = tz ? MSG0(id, (id)tz, "abbreviation") : 0;
    return s ? MSG0(const char *, s, "UTF8String") : 0;
}

long fdn_ns_hash(void *o) { return o ? MSG0(long, (id)o, "hash") : 0; }
int fdn_ns_equal(void *a, void *b) {
    if (a == b) return 1;
    return a && b && MSG(BOOL, (id)a, "isEqual:", (id)b);
}
