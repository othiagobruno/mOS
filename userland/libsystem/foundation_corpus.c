/*
 * mOS libSystem — classes e constantes do Foundation que o corpus importa e faltavam (docs/lacunas/eduardo.md).
 * Um app que só referencia a classe (classref) ou herda dela quebrava no carregamento; aqui elas existem com o
 * comportamento de um Mac sem os serviços por trás:
 *   - Distributed Objects (NSPort, NSSocketPort, NSPortMessage, NSPortNameServer, NSSocketPortNameServer,
 *     NSConnection, NSDistantObject, NSProtocolChecker): portas locais existem; nenhum servidor de nomes registra ou
 *     acha outro processo, então connectionWithRegisteredName: devolve nil, como sem o servidor rodando;
 *   - NSArchiver/NSUnarchiver (arquivamento antigo): ida e volta pelo NSKeyedArchiver do mOS, dentro do mOS;
 *   - NSURLDownload: o delegate recebe download:didFailWithError: (o Sparkle antigo cai no caminho de erro);
 *   - NSScriptSuiteRegistry (AppleScript): registro vazio; NSPointerFunctions com as opções guardadas;
 *   - constantes: chaves de erro do NSAppleScript, exceções de porta, NSIsNotNilTransformerName, kCFURL* antigas.
 * Registrado por cx_foundation_corpus_init, chamado de cx_foundation_more_init logo depois do NSProxy.
 */
#include "foundation.h"
#include "libc.h"

void cx_object_free(id o);

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
extern cx_class_t NSObject_c __asm__("OBJC_CLASS_$_NSObject");
extern cx_class_t NSProxy_c __asm__("OBJC_CLASS_$_NSProxy");
extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
void cx_add_cmethod(cx_class_t *cls, const char *sel, void *imp);

#define CLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
CLASS(NSPort) CLASS(NSSocketPort) CLASS(NSPortMessage) CLASS(NSPortNameServer) CLASS(NSSocketPortNameServer)
CLASS(NSConnection) CLASS(NSDistantObject) CLASS(NSProtocolChecker) CLASS(NSPortCoder)
CLASS(NSArchiver) CLASS(NSUnarchiver) CLASS(NSURLDownload) CLASS(NSScriptSuiteRegistry) CLASS(NSPointerFunctions)
CLASS(NSDistributedLock)

#define KNV(sym, lit) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)lit, sizeof(lit) - 1 }; id sym = (id)&sym##_o;
#define KNS(sym) KNV(sym, #sym)
KNS(NSAppleScriptErrorNumber) KNS(NSAppleScriptErrorMessage) KNS(NSAppleScriptErrorBriefMessage)
KNS(NSAppleScriptErrorAppName) KNS(NSAppleScriptErrorRange)
KNS(NSPortSendException) KNS(NSInvalidSendPortException) KNS(NSInvalidReceivePortException)
KNS(NSPortTimeoutException) KNS(NSPortReceiveException) KNS(NSObjectInaccessibleException)
KNS(NSObjectNotAvailableException) KNS(NSFailedAuthenticationException) KNS(NSDestinationInvalidException)
KNS(NSConnectionDidInitializeNotification)
extern id NSConnectionDidDieNotification, NSPortDidBecomeInvalidNotification;   /* fdn_consts.c */
KNS(NSConnectionReplyMode)
KNV(NSIsNotNilTransformerName, "NSIsNotNil")
/* chaves antigas do CFURL (CFURLCopyResourcePropertyForKey usa os mesmos nomes do NSURL) */
KNV(kCFURLLocalizedTypeDescriptionKey, "NSURLLocalizedTypeDescriptionKey")
KNV(kCFURLHasHiddenExtensionKey, "NSURLHasHiddenExtensionKey")
KNV(kCFURLFileSecurityKey, "NSURLFileSecurityKey")
KNV(kCFURLVolumeSupportsAdvisoryFileLockingKey, "NSURLVolumeSupportsAdvisoryFileLockingKey")
KNV(kCFURLVolumeIdentifierKey, "NSURLVolumeIdentifierKey")
KNV(kCFURLVolumeLocalizedFormatDescriptionKey, "NSURLVolumeLocalizedFormatDescriptionKey")
KNV(kCFURLVolumeIsReadOnlyKey, "NSURLVolumeIsReadOnlyKey")
KNV(kCFURLVolumeIsEncryptedKey, "NSURLVolumeIsEncryptedKey")
/* propriedades do CFURLCreatePropertyFromResource (API de 10.0) */
KNV(kCFURLFileExists, "kCFURLFileExists") KNV(kCFURLFileDirectoryContents, "kCFURLFileDirectoryContents")
KNV(kCFURLFileLength, "kCFURLFileLength") KNV(kCFURLFileLastModificationTime, "kCFURLFileLastModificationTime")
KNV(kCFURLFilePOSIXMode, "kCFURLFilePOSIXMode") KNV(kCFURLFileOwnerID, "kCFURLFileOwnerID")
KNV(kCFURLHTTPStatusCode, "kCFURLHTTPStatusCode") KNV(kCFURLHTTPStatusLine, "kCFURLHTTPStatusLine")
KNV(kCFStringTransformFullwidthHalfwidth, "Fullwidth-Halfwidth")
KNV(NSMetadataUbiquitousItemDownloadingStatusNotDownloaded, "NSMetadataUbiquitousItemDownloadingStatusNotDownloaded")
KNV(NSMetadataUbiquitousItemDownloadingStatusDownloaded, "NSMetadataUbiquitousItemDownloadingStatusDownloaded")
KNV(NSMetadataQueryUserHomeScope, "NSMetadataQueryUserHomeScope")
KNV(NSMetadataQueryGatheringProgressNotification, "NSMetadataQueryGatheringProgressNotification")
KNV(NSMetadataItemContentTypeTreeKey, "kMDItemContentTypeTree")

/* estado das instâncias: campos genéricos */
typedef struct { cx_class_t *isa; id a, b, c; i64 i, j; u64 opts; } fc_box_t;
#define FB(o) ((fc_box_t *)(o))
#define FEX (u32)(sizeof(fc_box_t) - 8)
static id ret(id o) { return o ? objc_retain(o) : 0; }
static void rel(id o) { if (o) objc_release(o); }
static id ar(id o) { return o ? objc_autorelease(o) : 0; }
static id self_m(id s, SEL c) { (void)c; return s; }
static id nil_m(id s, SEL c) { (void)s; (void)c; return 0; }
static id nil_1(id s, SEL c, id a) { (void)s; (void)c; (void)a; return 0; }
static id nil_2(id s, SEL c, id a, id b) { (void)s; (void)c; (void)a; (void)b; return 0; }
static BOOL no_m(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL no_1(id s, SEL c, id a) { (void)s; (void)c; (void)a; return NO; }
static BOOL no_2(id s, SEL c, id a, id b) { (void)s; (void)c; (void)a; (void)b; return NO; }
static BOOL yes_m(id s, SEL c) { (void)s; (void)c; return YES; }
static void void_m(id s, SEL c) { (void)s; (void)c; }
static void void_1(id s, SEL c, id a) { (void)s; (void)c; (void)a; }
static void void_2(id s, SEL c, id a, id b) { (void)s; (void)c; (void)a; (void)b; }
static id new_of(id cls, SEL c) { (void)c; return ar(cx_alloc_instance((cx_class_t *)cls)); }
static id empty_array(id s, SEL c) { (void)s; (void)c; return MSG0(id, (id)cx_class_named("NSArray"), "array"); }
static id empty_dict(id s, SEL c) { (void)s; (void)c; return MSG0(id, (id)cx_class_named("NSDictionary"), "dictionary"); }
static void box_dealloc(id s, SEL c) {
    (void)c;
    rel(FB(s)->a); rel(FB(s)->b); rel(FB(s)->c);
    cx_object_free(s);
}
static id singleton_of(id cls, SEL c) {
    (void)c;
    static struct { cx_class_t *k; id o; } tab[8];
    cx_class_t *k = (cx_class_t *)cls;
    for (int i = 0; i < 8; i++) {
        if (tab[i].k == k) return tab[i].o;
        if (!tab[i].k) { tab[i].k = k; tab[i].o = ret(cx_alloc_instance(k)); return tab[i].o; }
    }
    return cx_alloc_instance(k);
}

/* ---------------------------------------------------------------- NSPort e família */
static id port_alloc_init(id cls, SEL c) { (void)c; return ar(cx_alloc_instance((cx_class_t *)cls)); }
static id port_delegate(id s, SEL c) { (void)c; return (id)FB(s)->opts; }
static void port_set_delegate(id s, SEL c, id d) { (void)c; FB(s)->opts = (u64)d; }   /* não retém, como o Foundation */
static BOOL port_valid(id s, SEL c) { (void)c; return FB(s)->i == 0; }
static void port_invalidate(id s, SEL c) {
    (void)c;
    if (FB(s)->i) return;
    FB(s)->i = 1;
    id nc = MSG0(id, (id)cx_class_named("NSNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:", NSPortDidBecomeInvalidNotification, s);
}
static void port_sched(id s, SEL c, id rl, id mode) { (void)s; (void)c; (void)rl; (void)mode; }
static u64 port_reserved(id s, SEL c) { (void)s; (void)c; return 0; }
/* -sendBeforeDate:components:from:reserved: — sem outro lado conectado: NO (o Foundation devolve NO sem destino) */
static BOOL port_send(id s, SEL c, id date, id comps, id from, u64 res) { (void)s; (void)c; (void)date; (void)comps; (void)from; (void)res; return NO; }
static BOOL port_send_msgid(id s, SEL c, id date, u32 msgid, id comps, id from, u64 res) {
    (void)s; (void)c; (void)date; (void)msgid; (void)comps; (void)from; (void)res; return NO;
}
/* NSSocketPort: guarda a família, o tipo e o endereço; não abre socket de DO */
static id sp_init_tcp(id s, SEL c, u16 port) { (void)c; FB(s)->j = port; return s; }
static id sp_init_remote(id s, SEL c, u16 port, id host) { (void)c; FB(s)->j = port; FB(s)->a = ret(host); return s; }
static id sp_address(id s, SEL c) { (void)s; (void)c; return 0; }
static int sp_socket(id s, SEL c) { (void)s; (void)c; return -1; }
static int sp_family(id s, SEL c) { (void)s; (void)c; return 2; }
static int sp_type(id s, SEL c) { (void)s; (void)c; return 1; }
static int sp_proto(id s, SEL c) { (void)s; (void)c; return 6; }
/* NSPortMessage */
static id pm_init(id s, SEL c, id send, id recv, id comps) {
    (void)c;
    FB(s)->a = ret(send); FB(s)->b = ret(recv); FB(s)->c = ret(comps);
    return s;
}
static id pm_send(id s, SEL c) { (void)c; return FB(s)->a; }
static id pm_recv(id s, SEL c) { (void)c; return FB(s)->b; }
static id pm_comps(id s, SEL c) { (void)c; return FB(s)->c; }
static u32 pm_msgid(id s, SEL c) { (void)c; return (u32)FB(s)->j; }
static void pm_set_msgid(id s, SEL c, u32 v) { (void)c; FB(s)->j = v; }
static BOOL pm_send_before(id s, SEL c, id date) { (void)s; (void)c; (void)date; return NO; }

/* ---------------------------------------------------------------- NSConnection */
static id conn_with_ports(id cls, SEL c, id recv, id send) {
    (void)c;
    id o = cx_alloc_instance((cx_class_t *)cls);
    FB(o)->a = ret(recv); FB(o)->b = ret(send);
    return ar(o);
}
static id conn_init_ports(id s, SEL c, id recv, id send) { (void)c; FB(s)->a = ret(recv); FB(s)->b = ret(send); return s; }
static id conn_default(id cls, SEL c) {
    (void)c;
    static id d;
    if (!d) d = ret(cx_alloc_instance((cx_class_t *)cls));
    return d;
}
static id conn_new(id cls, SEL c) { (void)c; return ar(cx_alloc_instance((cx_class_t *)cls)); }
static id conn_all(id cls, SEL c) { (void)cls; (void)c; return MSG0(id, (id)cx_class_named("NSArray"), "array"); }
static id conn_recv(id s, SEL c) { (void)c; return FB(s)->a; }
static id conn_send(id s, SEL c) { (void)c; return FB(s)->b; }
static id conn_root(id s, SEL c) { (void)c; return FB(s)->c; }
static void conn_set_root(id s, SEL c, id o) { (void)c; id old = FB(s)->c; FB(s)->c = ret(o); rel(old); }
static BOOL conn_valid(id s, SEL c) { (void)c; return FB(s)->i == 0; }
static void conn_invalidate(id s, SEL c) {
    (void)c;
    if (FB(s)->i) return;
    FB(s)->i = 1;
    id nc = MSG0(id, (id)cx_class_named("NSNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:", NSConnectionDidDieNotification, s);
}
static double conn_timeout(id s, SEL c) { (void)s; (void)c; return 1e10; }
static void conn_set_timeout(id s, SEL c, double t) { (void)s; (void)c; (void)t; }
static id conn_stats(id s, SEL c) { return empty_dict(s, c); }
static id conn_modes(id s, SEL c) { (void)s; (void)c; return MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", cx_str("kCFRunLoopDefaultMode")); }
static id conn_delegate(id s, SEL c) { (void)c; return (id)FB(s)->opts; }
static void conn_set_delegate(id s, SEL c, id d) { (void)c; FB(s)->opts = (u64)d; }

/* ---------------------------------------------------------------- NSDistantObject / NSProtocolChecker (NSProxy) */
static id proxy_alloc(id cls, SEL c) { (void)c; return cx_alloc_instance((cx_class_t *)cls); }
static id chk_with(id cls, SEL c, id target, id proto) {
    (void)c;
    id o = cx_alloc_instance((cx_class_t *)cls);
    FB(o)->a = ret(target); FB(o)->opts = (u64)proto;
    return ar(o);
}
static id chk_init(id s, SEL c, id target, id proto) { (void)c; FB(s)->a = ret(target); FB(s)->opts = (u64)proto; return s; }
static id chk_target(id s, SEL c) { (void)c; return FB(s)->a; }
static id chk_proto(id s, SEL c) { (void)c; return (id)FB(s)->opts; }
/* o encaminhamento do NSProxy do mOS vai pelo forwardingTargetForSelector: */
static id chk_fwd_target(id s, SEL c, SEL sel) { (void)c; (void)sel; return FB(s)->a; }
static id dobj_proxy_with_target(id cls, SEL c, id target, id conn) {
    (void)c;
    id o = cx_alloc_instance((cx_class_t *)cls);
    FB(o)->a = ret(target); FB(o)->b = ret(conn);
    return ar(o);
}
static id dobj_conn(id s, SEL c) { (void)c; return FB(s)->b; }
static void dobj_set_proto(id s, SEL c, id p) { (void)c; FB(s)->opts = (u64)p; }

/* ---------------------------------------------------------------- NSPortNameServer */
static id ns_port_for(id s, SEL c, id name) { (void)s; (void)c; (void)name; return 0; }
static id ns_port_for_host(id s, SEL c, id name, id host) { (void)s; (void)c; (void)name; (void)host; return 0; }
static id ns_port_for_host_port(id s, SEL c, id name, id host, u16 n) { (void)s; (void)c; (void)name; (void)host; (void)n; return 0; }
static BOOL ns_register(id s, SEL c, id port, id name) { (void)s; (void)c; (void)port; (void)name; return NO; }
static BOOL ns_register_n(id s, SEL c, id port, id name, u16 n) { (void)s; (void)c; (void)port; (void)name; (void)n; return NO; }
static u16 ns_default_port(id s, SEL c) { (void)s; (void)c; return 0; }
static void ns_set_default_port(id s, SEL c, u16 p) { (void)s; (void)c; (void)p; }

/* ---------------------------------------------------------------- NSArchiver / NSUnarchiver */
static id keyed(void) { return (id)cx_class_named("NSKeyedArchiver"); }
static id unkeyed(void) { return (id)cx_class_named("NSKeyedUnarchiver"); }
static id arc_data_with_root(id cls, SEL c, id root) {
    (void)cls; (void)c;
    return root ? MSG(id, keyed(), "archivedDataWithRootObject:", root) : 0;
}
static BOOL arc_root_to_file(id cls, SEL c, id root, id path) {
    (void)cls; (void)c;
    id d = root ? MSG(id, keyed(), "archivedDataWithRootObject:", root) : 0;
    return d && path && MSG(BOOL, d, "writeToFile:atomically:", path, YES);
}
static id arc_init_for_writing(id s, SEL c, id mdata) { (void)c; FB(s)->a = ret(mdata); return s; }
static void arc_encode_root(id s, SEL c, id root) {
    (void)c;
    id d = root ? MSG(id, keyed(), "archivedDataWithRootObject:", root) : 0;
    if (d && FB(s)->a) MSG(void, FB(s)->a, "setData:", d);
}
static id arc_data(id s, SEL c) { (void)c; return FB(s)->a; }
static id unarc_obj_with_data(id cls, SEL c, id data) {
    (void)cls; (void)c;
    return data ? MSG(id, unkeyed(), "unarchiveObjectWithData:", data) : 0;
}
static id unarc_obj_with_file(id cls, SEL c, id path) {
    (void)cls; (void)c;
    id d = path ? MSG(id, (id)cx_class_named("NSData"), "dataWithContentsOfFile:", path) : 0;
    return d ? MSG(id, unkeyed(), "unarchiveObjectWithData:", d) : 0;
}
static id unarc_init(id s, SEL c, id data) { (void)c; FB(s)->a = ret(data); return data ? s : 0; }
static id unarc_decode(id s, SEL c) { (void)c; return FB(s)->a ? MSG(id, unkeyed(), "unarchiveObjectWithData:", FB(s)->a) : 0; }
static BOOL unarc_at_end(id s, SEL c) { (void)c; return FB(s)->i != 0; }
static void unarc_decode_class(id cls, SEL c, id a, id b) { (void)cls; (void)c; (void)a; (void)b; }
static u32 arc_version(id s, SEL c, id name) { (void)s; (void)c; (void)name; return 0; }

/* ---------------------------------------------------------------- NSURLDownload */
static void dl_fail(id s) {
    id del = (id)FB(s)->opts;
    if (!del || FB(s)->i) return;
    FB(s)->i = 1;
    SEL sel = sel_registerName("download:didFailWithError:");
    if (!cx_responds(object_getClass(del), sel)) return;
    id info = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:",
                  cx_str("NSURLDownload não existe no mOS; use NSURLSession"), cx_str("NSLocalizedDescription"));
    id err = MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSURLErrorDomain"), (i64)-1002, info);
    ((void (*)(id, SEL, id, id))objc_msgSend)(del, sel, s, err);
}
static void dl_fire(id s, SEL c, id t) { (void)c; (void)t; dl_fail(s); }
static id dl_init(id s, SEL c, id req, id del) {
    (void)c;
    FB(s)->a = ret(req); FB(s)->opts = (u64)del;       /* o delegate não é retido */
    /* o erro chega no próximo giro do run loop, como uma falha de rede assíncrona */
    MSG(void, s, "performSelector:withObject:afterDelay:", sel_registerName("_mosFail:"), (id)0, 0.0);
    return s;
}
static id dl_init_resume(id s, SEL c, id data, id del, id path) { (void)data; (void)path; return dl_init(s, c, 0, del); }
static void dl_cancel(id s, SEL c) { (void)c; FB(s)->i = 1; }
static id dl_request(id s, SEL c) { (void)c; return FB(s)->a; }
static id dl_resume_data(id s, SEL c) { (void)s; (void)c; return 0; }
static void dl_set_dest(id s, SEL c, id path, BOOL allow) { (void)c; (void)allow; id o = FB(s)->c; FB(s)->c = ret(path); rel(o); }
static void dl_set_delete(id s, SEL c, BOOL v) { (void)s; (void)c; (void)v; }
static BOOL dl_can_resume(id cls, SEL c, id mime) { (void)cls; (void)c; (void)mime; return NO; }

/* ---------------------------------------------------------------- NSScriptSuiteRegistry */
static id ssr_class_desc(id s, SEL c, id name) { (void)s; (void)c; (void)name; return 0; }
static void ssr_set_shared(id cls, SEL c, id r) { (void)cls; (void)c; (void)r; }

/* ---------------------------------------------------------------- NSPointerFunctions */
static id pf_with(id cls, SEL c, u64 opts) { (void)c; id o = cx_alloc_instance((cx_class_t *)cls); FB(o)->opts = opts; return ar(o); }
static id pf_init(id s, SEL c, u64 opts) { (void)c; FB(s)->opts = opts; return s; }
#define PF_PTR(get, set, slot) \
    static void *get(id s, SEL c) { (void)c; return (void *)(uintptr_t)FB(s)->slot; } \
    static void set(id s, SEL c, void *v) { (void)c; FB(s)->slot = (i64)(uintptr_t)v; }
PF_PTR(pf_hash, pf_set_hash, i)
PF_PTR(pf_eq, pf_set_eq, j)
/* descrição, tamanho, aquisição e liberação: guardados num bloco à parte (os campos de objeto do box são soltos no
 * dealloc) */
typedef struct { void *desc, *size, *acq, *rel; } pf_more_t;
static pf_more_t *pf_more(id s) {
    if (!FB(s)->c) {
        id d = MSG(id, (id)cx_class_named("NSMutableData"), "dataWithLength:", (u64)sizeof(pf_more_t));
        FB(s)->c = d ? objc_retain(d) : 0;
    }
    return FB(s)->c ? (pf_more_t *)MSG0(void *, FB(s)->c, "mutableBytes") : 0;
}
#define PF_MORE(get, set, f) \
    static void *get(id s, SEL c) { (void)c; pf_more_t *m = FB(s)->c ? (pf_more_t *)MSG0(void *, FB(s)->c, "mutableBytes") : 0; return m ? m->f : 0; } \
    static void set(id s, SEL c, void *v) { (void)c; pf_more_t *m = pf_more(s); if (m) m->f = v; }
PF_MORE(pf_desc, pf_set_desc, desc)
PF_MORE(pf_size, pf_set_size, size)
PF_MORE(pf_acq, pf_set_acq, acq)
PF_MORE(pf_rel, pf_set_rel, rel)
static BOOL pf_weak(id s, SEL c) { (void)c; return (FB(s)->opts & 5) == 5; }
static void pf_set_weak(id s, SEL c, BOOL v) { (void)s; (void)c; (void)v; }
static id pf_copy(id s, SEL c, void *zone) {
    (void)c; (void)zone;
    id o = cx_alloc_instance(object_getClass(s));
    *FB(o) = *FB(s);
    FB(o)->a = FB(o)->b = 0;
    FB(o)->c = FB(s)->c ? MSG0(id, FB(s)->c, "mutableCopy") : 0;     /* +1, dono próprio */
    return o;
}

/* ---------------------------------------------------------------- NSDistributedLock: arquivo de trava (mkdir atômico) */
int mkdir(const char *p, int mode);
int rmdir(const char *p);
static id dlock_with(id cls, SEL c, id path) {
    (void)c;
    id o = cx_alloc_instance((cx_class_t *)cls);
    FB(o)->a = ret(path);
    return ar(o);
}
static id dlock_init(id s, SEL c, id path) { (void)c; FB(s)->a = ret(path); return s; }
static BOOL dlock_try(id s, SEL c) {
    (void)c;
    if (!FB(s)->a) return NO;
    if (mkdir(cx_cstr(FB(s)->a), 0755) != 0) return NO;
    FB(s)->i = 1;
    return YES;
}
static void dlock_unlock(id s, SEL c) { (void)c; if (FB(s)->a && FB(s)->i) { rmdir(cx_cstr(FB(s)->a)); FB(s)->i = 0; } }
static void dlock_break(id s, SEL c) { (void)c; if (FB(s)->a) rmdir(cx_cstr(FB(s)->a)); FB(s)->i = 0; }
static id dlock_date(id s, SEL c) { (void)s; (void)c; return 0; }

#define M(cls, s, f) cx_add_method(&cls##_c, s, (void *)(f))
#define CM(cls, s, f) cx_add_cmethod(&cls##_c, s, (void *)(f))
static void def(cx_class_t *c, cx_class_t *m, const char *n, cx_class_t *s) {
    cx_define_class(c, m, n, s, FEX);
    cx_add_method(c, "dealloc", (void *)box_dealloc);
}
/* ---------------------------------------------------------------- NSData: leitura com opções e erro (o Postico lê a
 * licença com +dataWithContentsOfURL:options:error:). O arquivo inteiro vem do dataWithContentsOfFile: do libSystem;
 * falhando, o erro é NSCocoaErrorDomain 260 (NSFileReadNoSuchFileError) com o caminho, como no macOS. */
static id data_err(const char *path, id *err) {
    if (err) {
        id info = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(path ? path : ""), cx_str("NSFilePath"));
        *err = MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (long)260, info);
    }
    return 0;
}
static id data_file_opts(id cls, SEL s, id path, u64 opts, id *err) {
    (void)s; (void)opts;
    if (err) *err = 0;
    id d = path ? MSG(id, cls, "dataWithContentsOfFile:", path) : 0;
    return d ? d : data_err(path ? cx_cstr(path) : 0, err);
}
static id data_url_opts(id cls, SEL s, id url, u64 opts, id *err) {
    id path = url && MSG0(BOOL, url, "isFileURL") ? MSG0(id, url, "path") : 0;
    if (!path) { if (err) *err = 0; return data_err(0, err); }
    return data_file_opts(cls, s, path, opts, err);
}
static id data_init_file_opts(id self, SEL s, id path, u64 opts, id *err) {
    id d = data_file_opts((id)object_getClass(self), s, path, opts, err);
    objc_release(self);
    return d ? objc_retain(d) : 0;
}
static id data_init_url_opts(id self, SEL s, id url, u64 opts, id *err) {
    id d = data_url_opts((id)object_getClass(self), s, url, opts, err);
    objc_release(self);
    return d ? objc_retain(d) : 0;
}
static void data_opts_init(void) {
    cx_class_t *d = cx_class_named("NSData");
    if (!d) return;
    cx_class_t *meta = object_getClass((id)d);
    struct { cx_class_t *c; const char *sel; void *imp; } t[] = {
        {meta, "dataWithContentsOfURL:options:error:", (void *)data_url_opts},
        {meta, "dataWithContentsOfFile:options:error:", (void *)data_file_opts},
        {d, "initWithContentsOfURL:options:error:", (void *)data_init_url_opts},
        {d, "initWithContentsOfFile:options:error:", (void *)data_init_file_opts},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
        if (!cx_lookup(t[i].c, sel_registerName(t[i].sel))) cx_add_method(t[i].c, t[i].sel, t[i].imp);   /* só se faltar */
}

void cx_foundation_corpus_init(void) {
    data_opts_init();
    cx_class_t *ob = &NSObject_c;
    /* portas */
    def(&NSPort_c, &NSPort_m, "NSPort", ob);
    CM(NSPort, "port", port_alloc_init);
    M(NSPort, "init", self_m); M(NSPort, "delegate", port_delegate); M(NSPort, "setDelegate:", port_set_delegate);
    M(NSPort, "isValid", port_valid); M(NSPort, "invalidate", port_invalidate);
    M(NSPort, "scheduleInRunLoop:forMode:", port_sched); M(NSPort, "removeFromRunLoop:forMode:", port_sched);
    M(NSPort, "reservedSpaceLength", port_reserved);
    M(NSPort, "sendBeforeDate:components:from:reserved:", port_send);
    M(NSPort, "sendBeforeDate:msgid:components:from:reserved:", port_send_msgid);
    def(&NSSocketPort_c, &NSSocketPort_m, "NSSocketPort", &NSPort_c);
    M(NSSocketPort, "initWithTCPPort:", sp_init_tcp); M(NSSocketPort, "initRemoteWithTCPPort:host:", sp_init_remote);
    M(NSSocketPort, "address", sp_address); M(NSSocketPort, "socket", sp_socket);
    M(NSSocketPort, "protocolFamily", sp_family); M(NSSocketPort, "socketType", sp_type); M(NSSocketPort, "protocol", sp_proto);
    def(&NSPortMessage_c, &NSPortMessage_m, "NSPortMessage", ob);
    M(NSPortMessage, "initWithSendPort:receivePort:components:", pm_init);
    M(NSPortMessage, "sendPort", pm_send); M(NSPortMessage, "receivePort", pm_recv); M(NSPortMessage, "components", pm_comps);
    M(NSPortMessage, "msgid", pm_msgid); M(NSPortMessage, "setMsgid:", pm_set_msgid);
    M(NSPortMessage, "sendBeforeDate:", pm_send_before);
    def(&NSPortNameServer_c, &NSPortNameServer_m, "NSPortNameServer", ob);
    CM(NSPortNameServer, "systemDefaultPortNameServer", singleton_of);
    M(NSPortNameServer, "portForName:", ns_port_for); M(NSPortNameServer, "portForName:host:", ns_port_for_host);
    M(NSPortNameServer, "registerPort:name:", ns_register); M(NSPortNameServer, "removePortForName:", no_1);
    def(&NSSocketPortNameServer_c, &NSSocketPortNameServer_m, "NSSocketPortNameServer", &NSPortNameServer_c);
    CM(NSSocketPortNameServer, "sharedInstance", singleton_of);
    M(NSSocketPortNameServer, "portForName:", ns_port_for); M(NSSocketPortNameServer, "portForName:host:", ns_port_for_host);
    M(NSSocketPortNameServer, "portForName:host:nameServerPortNumber:", ns_port_for_host_port);
    M(NSSocketPortNameServer, "registerPort:name:", ns_register);
    M(NSSocketPortNameServer, "registerPort:name:nameServerPortNumber:", ns_register_n);
    M(NSSocketPortNameServer, "removePortForName:", no_1);
    M(NSSocketPortNameServer, "defaultNameServerPortNumber", ns_default_port);
    M(NSSocketPortNameServer, "setDefaultNameServerPortNumber:", ns_set_default_port);
    /* conexões */
    def(&NSConnection_c, &NSConnection_m, "NSConnection", ob);
    CM(NSConnection, "connectionWithReceivePort:sendPort:", conn_with_ports);
    CM(NSConnection, "defaultConnection", conn_default); CM(NSConnection, "new", conn_new);
    CM(NSConnection, "allConnections", conn_all); CM(NSConnection, "currentConversation", nil_m);
    CM(NSConnection, "connectionWithRegisteredName:host:", nil_2);
    CM(NSConnection, "rootProxyForConnectionWithRegisteredName:host:", nil_2);
    CM(NSConnection, "serviceConnectionWithName:rootObject:", nil_2);
    M(NSConnection, "initWithReceivePort:sendPort:", conn_init_ports);
    M(NSConnection, "receivePort", conn_recv); M(NSConnection, "sendPort", conn_send);
    M(NSConnection, "rootObject", conn_root); M(NSConnection, "setRootObject:", conn_set_root);
    M(NSConnection, "rootProxy", nil_m); M(NSConnection, "isValid", conn_valid); M(NSConnection, "invalidate", conn_invalidate);
    M(NSConnection, "registerName:", no_1); M(NSConnection, "registerName:withNameServer:", no_2);
    M(NSConnection, "requestTimeout", conn_timeout); M(NSConnection, "replyTimeout", conn_timeout);
    M(NSConnection, "setRequestTimeout:", conn_set_timeout); M(NSConnection, "setReplyTimeout:", conn_set_timeout);
    M(NSConnection, "statistics", conn_stats); M(NSConnection, "requestModes", conn_modes);
    M(NSConnection, "addRequestMode:", void_1); M(NSConnection, "removeRequestMode:", void_1);
    M(NSConnection, "runInNewThread", void_m); M(NSConnection, "enableMultipleThreads", void_m);
    M(NSConnection, "multipleThreadsEnabled", no_m); M(NSConnection, "independentConversationQueueing", no_m);
    M(NSConnection, "setIndependentConversationQueueing:", void_1);
    M(NSConnection, "addRunLoop:", void_1); M(NSConnection, "removeRunLoop:", void_1);
    M(NSConnection, "delegate", conn_delegate); M(NSConnection, "setDelegate:", conn_set_delegate);
    M(NSConnection, "localObjects", empty_array); M(NSConnection, "remoteObjects", empty_array);
    /* proxies */
    def(&NSDistantObject_c, &NSDistantObject_m, "NSDistantObject", &NSProxy_c);
    CM(NSDistantObject, "alloc", proxy_alloc);
    CM(NSDistantObject, "proxyWithLocal:connection:", dobj_proxy_with_target);
    CM(NSDistantObject, "proxyWithTarget:connection:", dobj_proxy_with_target);
    M(NSDistantObject, "connectionForProxy", dobj_conn); M(NSDistantObject, "setProtocolForProxy:", dobj_set_proto);
    M(NSDistantObject, "forwardingTargetForSelector:", chk_fwd_target);
    def(&NSProtocolChecker_c, &NSProtocolChecker_m, "NSProtocolChecker", &NSProxy_c);
    CM(NSProtocolChecker, "alloc", proxy_alloc);
    CM(NSProtocolChecker, "protocolCheckerWithTarget:protocol:", chk_with);
    M(NSProtocolChecker, "initWithTarget:protocol:", chk_init);
    M(NSProtocolChecker, "target", chk_target); M(NSProtocolChecker, "protocol", chk_proto);
    M(NSProtocolChecker, "forwardingTargetForSelector:", chk_fwd_target);
    def(&NSPortCoder_c, &NSPortCoder_m, "NSPortCoder", (cx_class_t *)cx_class_named("NSCoder") ? (cx_class_t *)cx_class_named("NSCoder") : ob);
    M(NSPortCoder, "connection", nil_m); M(NSPortCoder, "isBycopy", no_m); M(NSPortCoder, "isByref", no_m);
    /* arquivamento antigo */
    cx_class_t *coder = cx_class_named("NSCoder");
    def(&NSArchiver_c, &NSArchiver_m, "NSArchiver", coder ? coder : ob);
    CM(NSArchiver, "archivedDataWithRootObject:", arc_data_with_root);
    CM(NSArchiver, "archiveRootObject:toFile:", arc_root_to_file);
    M(NSArchiver, "initForWritingWithMutableData:", arc_init_for_writing);
    M(NSArchiver, "encodeRootObject:", arc_encode_root); M(NSArchiver, "archiverData", arc_data);
    M(NSArchiver, "classNameEncodedForTrueClassName:", nil_1);
    M(NSArchiver, "encodeClassName:intoClassName:", void_2);
    M(NSArchiver, "replaceObject:withObject:", void_2);
    def(&NSUnarchiver_c, &NSUnarchiver_m, "NSUnarchiver", coder ? coder : ob);
    CM(NSUnarchiver, "unarchiveObjectWithData:", unarc_obj_with_data);
    CM(NSUnarchiver, "unarchiveObjectWithFile:", unarc_obj_with_file);
    CM(NSUnarchiver, "decodeClassName:asClassName:", unarc_decode_class);
    CM(NSUnarchiver, "classNameDecodedForArchiveClassName:", nil_1);
    M(NSUnarchiver, "initForReadingWithData:", unarc_init);
    M(NSUnarchiver, "decodeObject", unarc_decode); M(NSUnarchiver, "isAtEnd", unarc_at_end);
    M(NSUnarchiver, "systemVersion", arc_version); M(NSUnarchiver, "versionForClassName:", arc_version);
    M(NSUnarchiver, "decodeClassName:asClassName:", void_2);
    M(NSUnarchiver, "replaceObject:withObject:", void_2);
    /* NSURLDownload */
    def(&NSURLDownload_c, &NSURLDownload_m, "NSURLDownload", ob);
    CM(NSURLDownload, "canResumeDownloadDecodedWithEncodingMIMEType:", dl_can_resume);
    M(NSURLDownload, "initWithRequest:delegate:", dl_init);
    M(NSURLDownload, "initWithResumeData:delegate:path:", dl_init_resume);
    M(NSURLDownload, "_mosFail:", dl_fire);
    M(NSURLDownload, "cancel", dl_cancel); M(NSURLDownload, "request", dl_request); M(NSURLDownload, "resumeData", dl_resume_data);
    M(NSURLDownload, "setDestination:allowOverwrite:", dl_set_dest);
    M(NSURLDownload, "setDeletesFileUponFailure:", dl_set_delete); M(NSURLDownload, "deletesFileUponFailure", yes_m);
    /* AppleScript */
    def(&NSScriptSuiteRegistry_c, &NSScriptSuiteRegistry_m, "NSScriptSuiteRegistry", ob);
    CM(NSScriptSuiteRegistry, "sharedScriptSuiteRegistry", singleton_of);
    CM(NSScriptSuiteRegistry, "setSharedScriptSuiteRegistry:", ssr_set_shared);
    M(NSScriptSuiteRegistry, "suiteNames", empty_array);
    M(NSScriptSuiteRegistry, "classDescriptionWithAppleEventCode:", ssr_class_desc);
    M(NSScriptSuiteRegistry, "commandDescriptionWithAppleEventClass:andAppleEventCode:", nil_2);
    M(NSScriptSuiteRegistry, "classDescriptionsInSuite:", empty_dict);
    M(NSScriptSuiteRegistry, "commandDescriptionsInSuite:", empty_dict);
    M(NSScriptSuiteRegistry, "appleEventCodeForSuite:", nil_1);
    M(NSScriptSuiteRegistry, "bundleForSuite:", nil_1);
    M(NSScriptSuiteRegistry, "suiteForAppleEventCode:", nil_1);
    M(NSScriptSuiteRegistry, "loadSuitesFromBundle:", void_1);
    M(NSScriptSuiteRegistry, "loadSuiteWithDictionary:fromBundle:", void_2);
    M(NSScriptSuiteRegistry, "registerClassDescription:", void_1);
    M(NSScriptSuiteRegistry, "registerCommandDescription:", void_1);
    M(NSScriptSuiteRegistry, "aeteResource:", nil_1);
    /* NSPointerFunctions */
    def(&NSPointerFunctions_c, &NSPointerFunctions_m, "NSPointerFunctions", ob);
    CM(NSPointerFunctions, "pointerFunctionsWithOptions:", pf_with);
    M(NSPointerFunctions, "initWithOptions:", pf_init); M(NSPointerFunctions, "copyWithZone:", pf_copy);
    M(NSPointerFunctions, "hashFunction", pf_hash); M(NSPointerFunctions, "setHashFunction:", pf_set_hash);
    M(NSPointerFunctions, "isEqualFunction", pf_eq); M(NSPointerFunctions, "setIsEqualFunction:", pf_set_eq);
    M(NSPointerFunctions, "descriptionFunction", pf_desc); M(NSPointerFunctions, "setDescriptionFunction:", pf_set_desc);
    M(NSPointerFunctions, "sizeFunction", pf_size); M(NSPointerFunctions, "setSizeFunction:", pf_set_size);
    M(NSPointerFunctions, "acquireFunction", pf_acq); M(NSPointerFunctions, "setAcquireFunction:", pf_set_acq);
    M(NSPointerFunctions, "relinquishFunction", pf_rel); M(NSPointerFunctions, "setRelinquishFunction:", pf_set_rel);
    M(NSPointerFunctions, "usesWeakReadAndWriteBarriers", pf_weak);
    M(NSPointerFunctions, "setUsesWeakReadAndWriteBarriers:", pf_set_weak);
    M(NSPointerFunctions, "usesStrongWriteBarrier", no_m);
    M(NSPointerFunctions, "setUsesStrongWriteBarrier:", pf_set_weak);
    /* NSDistributedLock */
    def(&NSDistributedLock_c, &NSDistributedLock_m, "NSDistributedLock", ob);
    CM(NSDistributedLock, "lockWithPath:", dlock_with);
    M(NSDistributedLock, "initWithPath:", dlock_init); M(NSDistributedLock, "tryLock", dlock_try);
    M(NSDistributedLock, "unlock", dlock_unlock); M(NSDistributedLock, "breakLock", dlock_break);
    M(NSDistributedLock, "lockDate", dlock_date);
    (void)new_of;
}

/* ---------------------------------------------------------------- CF que vinha junto */
/* CFUserNotification: o mOS não tem o daemon que mostra o alerta fora de um app; a criação falha como sem o
 * serviço (o chamador cai no NSAlert ou ignora) */
void *CFUserNotificationCreate(void *alloc, double timeout, u64 flags, i32 *err, id dict) {
    (void)alloc; (void)timeout; (void)flags; (void)dict;
    if (err) *err = -1;
    return 0;
}
i32 CFUserNotificationReceiveResponse(void *n, double timeout, u64 *resp) { (void)n; (void)timeout; if (resp) *resp = 3; return -1; }   /* kCFUserNotificationCancelResponse */
i32 CFUserNotificationCancel(void *n) { (void)n; return 0; }
i32 CFUserNotificationUpdate(void *n, double timeout, u64 flags, id dict) { (void)n; (void)timeout; (void)flags; (void)dict; return -1; }
id CFUserNotificationGetResponseDictionary(void *n) { (void)n; return 0; }
id CFUserNotificationGetResponseValue(void *n, id key, long idx) { (void)n; (void)key; (void)idx; return 0; }
void *CFUserNotificationCreateRunLoopSource(void *a, void *n, void *cb, long order) { (void)a; (void)n; (void)cb; (void)order; return 0; }
u64 CFUserNotificationGetTypeID(void) { return 0x2D0; }
/* aviso simples: vai para o NSAlert do AppKit quando o app tem um, senão para o stderr */
i32 CFUserNotificationDisplayNotice(double timeout, u64 flags, id icon, id sound, id loc, id header, id msg, id def_btn) {
    (void)timeout; (void)flags; (void)icon; (void)sound; (void)loc; (void)def_btn;
    cx_class_t *alert = cx_class_named("NSAlert");
    if (alert) {
        id a = MSG0(id, MSG0(id, (id)alert, "alloc"), "init");
        if (a) {
            if (header) MSG(void, a, "setMessageText:", header);
            if (msg) MSG(void, a, "setInformativeText:", msg);
            MSG0(i64, a, "runModal");
            objc_release(a);
            return 0;
        }
    }
    cx_fdprintf(2, "%s: %s\n", header ? cx_cstr(header) : "", msg ? cx_cstr(msg) : "");
    return 0;
}
/* CFReadStreamCreate com callbacks de stream próprio (CFReadStreamCallBacks): o CFStream do mOS só tem os de
 * arquivo, dados e socket; um stream de app devolve NULL, como alocação recusada */
void *CFReadStreamCreate(void *alloc, const void *cb, void *info) { (void)alloc; (void)cb; (void)info; return 0; }
void *CFWriteStreamCreate(void *alloc, const void *cb, void *info) { (void)alloc; (void)cb; (void)info; return 0; }
/* CFURLCreatePropertyFromResource (10.0): existência, tamanho, data e modo de arquivos locais */
id CFURLCreatePropertyFromResource(void *alloc, id url, id prop, i32 *err) {
    (void)alloc;
    if (err) *err = 0;
    if (!url || !prop) { if (err) *err = -15; return 0; }
    id path = MSG0(id, url, "path");
    if (!path) { if (err) *err = -12; return 0; }    /* kCFURLUnknownSchemeError */
    id fm = MSG0(id, (id)cx_class_named("NSFileManager"), "defaultManager");
    id at = MSG(id, fm, "attributesOfItemAtPath:error:", path, (id)0);
    const char *p = cx_cstr(prop);
    if (!strcmp(p, "kCFURLFileExists")) return objc_retain(MSG(id, (id)cx_class_named("NSNumber"), "numberWithBool:", (BOOL)(at != 0)));
    if (!at) { if (err) *err = -10; return 0; }      /* kCFURLResourceNotFoundError */
    const char *k = !strcmp(p, "kCFURLFileLength") ? "NSFileSize" : !strcmp(p, "kCFURLFilePOSIXMode") ? "NSFilePosixPermissions" :
                    !strcmp(p, "kCFURLFileOwnerID") ? "NSFileOwnerAccountID" :
                    !strcmp(p, "kCFURLFileLastModificationTime") ? "NSFileModificationDate" : 0;
    if (k) {
        id v = MSG(id, at, "objectForKey:", cx_str(k));
        if (!v && err) *err = -17;
        return v ? objc_retain(v) : 0;
    }
    if (!strcmp(p, "kCFURLFileDirectoryContents")) {
        id names = MSG(id, fm, "contentsOfDirectoryAtPath:error:", path, (id)0);
        if (!names) { if (err) *err = -10; return 0; }
        id out = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
        long n = MSG0(long, names, "count");
        for (long i = 0; i < n; i++) {
            id nm = MSG(id, names, "objectAtIndex:", i);
            id u = MSG(id, url, "URLByAppendingPathComponent:", nm);
            if (u) MSG(void, out, "addObject:", u);
        }
        return objc_retain(out);
    }
    if (err) *err = -17;                             /* kCFURLPropertyKeyUnavailableError */
    return 0;
}
id CFErrorCopyRecoverySuggestion(id e) {
    if (!e) return 0;
    if (cx_responds(object_getClass(e), sel_registerName("localizedRecoverySuggestion"))) {
        id v = MSG0(id, e, "localizedRecoverySuggestion");
        if (v) return objc_retain(v);
    }
    id info = MSG0(id, e, "userInfo");
    id v = info ? MSG(id, info, "objectForKey:", cx_str("NSLocalizedRecoverySuggestion")) : 0;
    return v ? objc_retain(v) : 0;
}
